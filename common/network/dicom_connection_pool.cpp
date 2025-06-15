#include "dicom_connection_pool.h"
#include "common/logger/logger.h"

#ifdef DCMTK_AVAILABLE
#include <dcmtk/dcmnet/scu.h>
#include <dcmtk/dcmnet/diutil.h>
#include <dcmtk/dcmdata/dcuid.h>
#endif

#include <sstream>
#include <iomanip>

namespace pacs {
namespace common {
namespace network {

// DicomConnection implementation
DicomConnection::DicomConnection(const Parameters& params)
    : params_(params) {
    // Generate unique connection ID
    std::stringstream ss;
    ss << params_.remoteAeTitle << "@" << params_.remoteHost << ":" 
       << params_.remotePort << "_" << std::hex << this;
    connectionId_ = ss.str();
}

DicomConnection::~DicomConnection() {
    disconnect();
}

bool DicomConnection::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        return true;
    }
    
#ifdef DCMTK_AVAILABLE
    try {
        // Create SCU object
        scu_ = std::make_unique<DcmSCU>();
        
        // Set connection parameters
        scu_->setAETitle(params_.localAeTitle.c_str());
        scu_->setPeerAETitle(params_.remoteAeTitle.c_str());
        scu_->setPeerHostName(params_.remoteHost.c_str());
        scu_->setPeerPort(params_.remotePort);
        
        // Set timeouts
        scu_->setConnectionTimeout(static_cast<Uint32>(params_.timeout.count()));
        scu_->setDIMSETimeout(static_cast<Uint32>(params_.timeout.count()));
        scu_->setACSETimeout(static_cast<Uint32>(params_.timeout.count()));
        
        // Set max PDU size
        scu_->setMaxReceivePDULength(params_.maxPduSize);
        
        // Add presentation contexts for common SOP classes
        OFList<OFString> transferSyntaxes;
        transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        transferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
        transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
        
        // Verification SOP Class
        scu_->addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
        
        // Storage SOP Classes
        scu_->addPresentationContext(UID_CTImageStorage, transferSyntaxes);
        scu_->addPresentationContext(UID_MRImageStorage, transferSyntaxes);
        scu_->addPresentationContext(UID_SecondaryCaptureImageStorage, transferSyntaxes);
        
        // Query/Retrieve SOP Classes
        scu_->addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        scu_->addPresentationContext(UID_MOVEStudyRootQueryRetrieveInformationModel, transferSyntaxes);
        
        // Initialize network
        OFCondition result = scu_->initNetwork();
        if (result.bad()) {
            logger::logError("Failed to initialize network: {}", result.text());
            return false;
        }
        
        // Negotiate association
        result = scu_->negotiateAssociation();
        if (result.bad()) {
            logger::logError("Failed to negotiate association: {}", result.text());
            return false;
        }
        
        connected_ = true;
        updateActivity();
        
        logger::logInfo("DICOM connection established: {}", connectionId_);
        return true;
    }
    catch (const std::exception& ex) {
        logger::logError("Exception during DICOM connection: {}", ex.what());
        return false;
    }
#else
    logger::logError("DCMTK not available");
    return false;
#endif
}

void DicomConnection::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return;
    }
    
#ifdef DCMTK_AVAILABLE
    if (scu_) {
        // Release association
        scu_->releaseAssociation();
        
        // Drop network
        scu_->dropNetwork();
        
        scu_.reset();
    }
#endif
    
    connected_ = false;
    logger::logInfo("DICOM connection closed: {}", connectionId_);
}

bool DicomConnection::isAlive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_ || !scu_) {
        return false;
    }
    
#ifdef DCMTK_AVAILABLE
    // Check if association is still valid
    if (!scu_->isConnected()) {
        return false;
    }
#endif
    
    return true;
}

void DicomConnection::reset() {
    // Reset connection state
    // For DICOM, we might want to keep the connection alive
    updateActivity();
}

std::string DicomConnection::getId() const {
    return connectionId_;
}

bool DicomConnection::sendEcho() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_ || !scu_) {
        return false;
    }
    
#ifdef DCMTK_AVAILABLE
    try {
        // Send C-ECHO
        OFCondition result = scu_->sendECHORequest(0);
        
        if (result.good()) {
            updateActivity();
            return true;
        } else {
            logger::logWarning("C-ECHO failed: {}", result.text());
            return false;
        }
    }
    catch (const std::exception& ex) {
        logger::logError("Exception during C-ECHO: {}", ex.what());
        return false;
    }
#else
    return false;
#endif
}

// DicomConnectionFactory implementation
std::unique_ptr<DicomConnection> DicomConnectionFactory::createConnection() {
    auto conn = std::make_unique<DicomConnection>(params_);
    
    if (conn->connect()) {
        logger::logInfo("Created new DICOM connection to {}:{}", 
                       params_.remoteHost, params_.remotePort);
        return conn;
    }
    
    logger::logError("Failed to create DICOM connection to {}:{}", 
                    params_.remoteHost, params_.remotePort);
    return nullptr;
}

bool DicomConnectionFactory::validateConnection(DicomConnection* conn) {
    if (!conn) {
        return false;
    }
    
    // First check if connection reports as alive
    if (!conn->isAlive()) {
        return false;
    }
    
    // Send C-ECHO to verify connection
    return conn->sendEcho();
}

// DicomConnectionPool implementation
DicomConnectionPool::DicomConnectionPool(
    const DicomConnection::Parameters& params,
    const ConnectionPool<DicomConnection>::Config& poolConfig)
    : params_(params) {
    
    // Create connection factory
    auto factory = std::make_unique<DicomConnectionFactory>(params);
    
    // Create connection pool
    pool_ = std::make_unique<ConnectionPool<DicomConnection>>(
        std::move(factory), poolConfig);
    
    // Configure retry policy
    RetryConfig retryConfig;
    retryConfig.maxAttempts = 3;
    retryConfig.initialDelay = std::chrono::milliseconds(1000);
    retryConfig.maxDelay = std::chrono::seconds(10);
    retryConfig.strategy = RetryStrategy::ExponentialJitter;
    
    // Add retryable errors
    retryConfig.addRetryableError("timeout");
    retryConfig.addRetryableError("connection");
    retryConfig.addRetryableError("network");
    retryConfig.addRetryableError("association");
    
    // Configure circuit breaker
    CircuitBreaker::Config cbConfig;
    cbConfig.failureThreshold = 5;
    cbConfig.successThreshold = 2;
    cbConfig.openDuration = std::chrono::seconds(60);
    
    // Create resilient executor
    std::string executorName = "DICOM_" + params.remoteAeTitle;
    executor_ = std::make_unique<ResilientExecutor>(
        executorName, retryConfig, cbConfig);
}

core::Result<void> DicomConnectionPool::initialize() {
    return pool_->initialize();
}

void DicomConnectionPool::shutdown() {
    pool_->shutdown();
}

ConnectionPool<DicomConnection>::PoolStats DicomConnectionPool::getPoolStats() const {
    return pool_->getStats();
}

CircuitBreaker::Stats DicomConnectionPool::getCircuitBreakerStats() const {
    // Note: This would need to be implemented in ResilientExecutor
    return CircuitBreaker::Stats{};
}

// DicomConnectionPoolManager implementation
DicomConnectionPoolManager& DicomConnectionPoolManager::getInstance() {
    static DicomConnectionPoolManager instance;
    return instance;
}

std::shared_ptr<DicomConnectionPool> DicomConnectionPoolManager::getPool(
    const std::string& remoteAeTitle,
    const DicomConnection::Parameters& params,
    const ConnectionPool<DicomConnection>::Config& poolConfig) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if pool already exists
    auto it = pools_.find(remoteAeTitle);
    if (it != pools_.end()) {
        return it->second;
    }
    
    // Create new pool
    auto pool = std::make_shared<DicomConnectionPool>(params, poolConfig);
    
    // Initialize pool
    auto result = pool->initialize();
    if (!result.isOk()) {
        logger::logError("Failed to initialize connection pool for {}: {}", 
                        remoteAeTitle, result.error());
        return nullptr;
    }
    
    // Store pool
    pools_[remoteAeTitle] = pool;
    
    logger::logInfo("Created connection pool for {}", remoteAeTitle);
    return pool;
}

void DicomConnectionPoolManager::removePool(const std::string& remoteAeTitle) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pools_.find(remoteAeTitle);
    if (it != pools_.end()) {
        it->second->shutdown();
        pools_.erase(it);
        logger::logInfo("Removed connection pool for {}", remoteAeTitle);
    }
}

void DicomConnectionPoolManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [name, pool] : pools_) {
        pool->shutdown();
    }
    
    pools_.clear();
    logger::logInfo("All connection pools shut down");
}

std::map<std::string, ConnectionPool<DicomConnection>::PoolStats> 
DicomConnectionPoolManager::getAllPoolStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, ConnectionPool<DicomConnection>::PoolStats> stats;
    
    for (const auto& [name, pool] : pools_) {
        stats[name] = pool->getPoolStats();
    }
    
    return stats;
}

} // namespace network
} // namespace common
} // namespace pacs