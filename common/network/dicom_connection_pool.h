#pragma once

#include "connection_pool.h"
#include "retry_policy.h"
#include "../dicom/dicom_definitions.h"
#include <memory>
#include <string>
#include <map>

// Forward declarations for DCMTK
class DcmAssociation;
class DcmSCU;

namespace pacs {
namespace common {
namespace network {

/**
 * @brief DICOM connection wrapper for pooling
 */
class DicomConnection : public PooledConnection {
public:
    /**
     * @brief Connection parameters
     */
    struct Parameters {
        std::string remoteHost;
        uint16_t remotePort;
        std::string remoteAeTitle;
        std::string localAeTitle;
        uint32_t maxPduSize = 16384;
        std::chrono::seconds timeout{30};
    };
    
    explicit DicomConnection(const Parameters& params);
    ~DicomConnection() override;
    
    /**
     * @brief Connect to remote DICOM node
     * @return True if connection successful
     */
    bool connect();
    
    /**
     * @brief Disconnect from remote node
     */
    void disconnect();
    
    /**
     * @brief Check if connection is alive
     * @return True if connection is valid
     */
    bool isAlive() const override;
    
    /**
     * @brief Reset connection state
     */
    void reset() override;
    
    /**
     * @brief Get unique connection ID
     * @return Connection identifier
     */
    std::string getId() const override;
    
    /**
     * @brief Get the underlying DICOM SCU
     * @return DICOM SCU pointer
     */
    DcmSCU* getSCU() const { return static_cast<DcmSCU*>(scu_); }
    
    /**
     * @brief Get connection parameters
     * @return Connection parameters
     */
    const Parameters& getParameters() const { return params_; }
    
    /**
     * @brief Send C-ECHO to verify connection
     * @return True if echo successful
     */
    bool sendEcho();

private:
    Parameters params_;
    // Using void* to avoid incomplete type issues
    // Will be properly cast in implementation
    void* scu_;
    std::string connectionId_;
    bool connected_{false};
    mutable std::mutex mutex_;
};

/**
 * @brief DICOM connection factory
 */
class DicomConnectionFactory : public ConnectionFactory<DicomConnection> {
public:
    explicit DicomConnectionFactory(const DicomConnection::Parameters& params)
        : params_(params) {}
    
    /**
     * @brief Create a new DICOM connection
     * @return New connection or nullptr on failure
     */
    std::unique_ptr<DicomConnection> createConnection() override;
    
    /**
     * @brief Validate a DICOM connection
     * @param conn Connection to validate
     * @return True if connection is valid
     */
    bool validateConnection(DicomConnection* conn) override;

private:
    DicomConnection::Parameters params_;
};

/**
 * @brief DICOM-specific connection pool
 */
class DicomConnectionPool {
public:
    /**
     * @brief Constructor
     * @param params Connection parameters
     * @param poolConfig Pool configuration
     */
    DicomConnectionPool(const DicomConnection::Parameters& params,
                        const ConnectionPool<DicomConnection>::Config& poolConfig = {});
    
    /**
     * @brief Initialize the connection pool
     * @return Result indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Shutdown the connection pool
     */
    void shutdown();
    
    /**
     * @brief Execute a DICOM operation with automatic connection management
     * @tparam Func Function type
     * @param operation Operation name for logging
     * @param func Function to execute with DICOM connection
     * @return Result of the operation
     */
    template<typename Func>
    auto executeWithConnection(const std::string& operation, Func&& func) 
        -> decltype(func(std::declval<DicomConnection*>())) {
        
        using ReturnType = decltype(func(std::declval<DicomConnection*>()));
        
        // Use resilient executor for retry and circuit breaker
        return executor_->execute([this, &func, &operation]() -> ReturnType {
            // Borrow connection from pool
            auto connResult = pool_->borrowConnection();
            
            if (!connResult.isOk()) {
                return ReturnType::error("Failed to get connection from pool: " + 
                                       connResult.error());
            }
            
            auto conn = connResult.value();
            
            try {
                // Execute the operation
                logger::logDebug("Executing DICOM operation: {} on connection {}", 
                               operation, conn->getId());
                
                auto result = func(conn.get());
                
                if (result.isOk()) {
                    logger::logDebug("DICOM operation {} completed successfully", operation);
                } else {
                    logger::logWarning("DICOM operation {} failed: {}", 
                                     operation, result.getError());
                }
                
                return result;
            }
            catch (const std::exception& ex) {
                logger::logError("Exception during DICOM operation {}: {}", 
                               operation, ex.what());
                return ReturnType::error("Exception: " + std::string(ex.what()));
            }
        });
    }
    
    /**
     * @brief Get pool statistics
     * @return Pool statistics
     */
    ConnectionPool<DicomConnection>::PoolStats getPoolStats() const;
    
    /**
     * @brief Get circuit breaker statistics
     * @return Circuit breaker statistics
     */
    CircuitBreaker::Stats getCircuitBreakerStats() const;

private:
    DicomConnection::Parameters params_;
    std::unique_ptr<ConnectionPool<DicomConnection>> pool_;
    std::unique_ptr<ResilientExecutor> executor_;
};

/**
 * @brief Global DICOM connection pool manager
 */
class DicomConnectionPoolManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the manager
     */
    static DicomConnectionPoolManager& getInstance();
    
    /**
     * @brief Create or get a connection pool for a remote node
     * @param remoteAeTitle Remote AE title as pool identifier
     * @param params Connection parameters
     * @param poolConfig Pool configuration
     * @return Connection pool
     */
    std::shared_ptr<DicomConnectionPool> getPool(
        const std::string& remoteAeTitle,
        const DicomConnection::Parameters& params,
        const ConnectionPool<DicomConnection>::Config& poolConfig = {});
    
    /**
     * @brief Remove a connection pool
     * @param remoteAeTitle Remote AE title
     */
    void removePool(const std::string& remoteAeTitle);
    
    /**
     * @brief Shutdown all connection pools
     */
    void shutdown();
    
    /**
     * @brief Get statistics for all pools
     * @return Map of pool name to statistics
     */
    std::map<std::string, ConnectionPool<DicomConnection>::PoolStats> getAllPoolStats() const;

private:
    DicomConnectionPoolManager() = default;
    
    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<DicomConnectionPool>> pools_;
};

} // namespace network
} // namespace common
} // namespace pacs