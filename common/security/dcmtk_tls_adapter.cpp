#include "dcmtk_tls_adapter.h"
#include "common/logger/logger.h"

// Include DCMTK headers for TLS functionality
#ifdef WITH_OPENSSL
#include <dcmtk/dcmtls/tlslayer.h>
#include <dcmtk/dcmtls/tlsopt.h>
#include <dcmtk/dcmnet/assoc.h>
#else
// If DCMTK was built without OpenSSL, provide dummy implementations
#define SSL_FILETYPE_PEM 1
#endif

using Result = pacs::core::Result<void>;

namespace pacs {
namespace common {
namespace security {

DcmtkTlsAdapter::DcmtkTlsAdapter(const TLSConfig& config)
    : config_(config), tlsLayer_(nullptr), initialized_(false), isServer_(false) {
}

DcmtkTlsAdapter::~DcmtkTlsAdapter() = default;

pacs::core::Result<void> DcmtkTlsAdapter::initialize(bool isServer) {
#ifdef WITH_OPENSSL
    try {
        if (!config_.isEnabled()) {
            logger::logInfo("TLS is not enabled, skipping initialization");
            return Result::ok();
        }
        
        isServer_ = isServer;
        
        // Create TLS transport layer
        tlsLayer_.reset(new DcmTLSTransportLayer(isServer_ ? DICOM_APPLICATION_ACCEPTOR : DICOM_APPLICATION_REQUESTOR));
        
        if (!tlsLayer_) {
            return Result::error("Failed to create TLS transport layer");
        }
        
        logger::logInfo("Initializing TLS layer as {}", isServer_ ? "server" : "client");
        
        // Set certificate and private key
        if (!tlsLayer_->setPrivateKeyFile(config_.getPrivateKeyPath().c_str(), SSL_FILETYPE_PEM)) {
            return Result::error("Failed to load private key file: " + config_.getPrivateKeyPath());
        }
        
        if (!tlsLayer_->setCertificateFile(config_.getCertificatePath().c_str(), SSL_FILETYPE_PEM)) {
            return Result::error("Failed to load certificate file: " + config_.getCertificatePath());
        }
        
        // Set CA certificate if configured
        if (config_.getCACertificatePath().has_value()) {
            if (!tlsLayer_->addTrustedCertificateFile(config_.getCACertificatePath().value().c_str(), SSL_FILETYPE_PEM)) {
                logger::logWarning("Failed to load CA certificate file: {}", config_.getCACertificatePath().value());
            }
        }
        
        // Set CA certificate directory if configured
        if (config_.getCACertificateDir().has_value()) {
            if (!tlsLayer_->addTrustedCertificateDir(config_.getCACertificateDir().value().c_str(), SSL_FILETYPE_PEM)) {
                logger::logWarning("Failed to load CA certificate directory: {}", config_.getCACertificateDir().value());
            }
        }
        
        // Add trusted certificates
        for (const auto& certPath : config_.getTrustedCertificates()) {
            if (!tlsLayer_->addTrustedCertificateFile(certPath.c_str(), SSL_FILETYPE_PEM)) {
                logger::logWarning("Failed to load trusted certificate file: {}", certPath);
            }
        }
        
        // Set verification mode
        DcmCertificateVerification verifyMode;
        switch (config_.getVerificationMode()) {
            case TLSVerificationMode::None:
                verifyMode = DCV_ignoreCertificate;
                break;
            case TLSVerificationMode::Relaxed:
                verifyMode = DCV_checkCertificate;
                break;
            case TLSVerificationMode::Required:
            default:
                verifyMode = DCV_requireCertificate;
                break;
        }
        
        tlsLayer_->setCertificateVerification(verifyMode);
        
        // Set cipher suites
        if (!tlsLayer_->setCipherSuites(config_.getCipherList().c_str())) {
            logger::logWarning("Failed to set cipher suites: {}", config_.getCipherList());
        }
        
        // Set TLS protocol
        std::string protocolStr = getProtocolString(config_.getMinimumProtocolVersion());
        if (!tlsLayer_->setTLSProfile(protocolStr.c_str())) {
            logger::logWarning("Failed to set TLS protocol version: {}", protocolStr);
        }
        
        initialized_ = true;
        logger::logInfo("TLS layer initialized successfully");
        
        return Result::ok();
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to initialize TLS layer: " + std::string(ex.what()));
    }
#else
    logger::logWarning("TLS support is not available in this build (DCMTK was built without OpenSSL)");
    return Result::error("TLS support is not available in this build");
#endif
}

pacs::core::Result<void> DcmtkTlsAdapter::applyToNetwork(T_ASC_Network* net) {
#ifdef WITH_OPENSSL
    if (!config_.isEnabled() || !initialized_ || !tlsLayer_) {
        return Result::ok();
    }
    
    try {
        if (ASC_setTransportLayer(net, tlsLayer_.get(), 0) != EC_Normal) {
            return Result::error("Failed to set transport layer for network");
        }
        
        logger::logInfo("TLS transport layer applied to network");
        return Result::ok();
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to apply TLS to network: " + std::string(ex.what()));
    }
#else
    return Result::error("TLS support is not available in this build");
#endif
}

pacs::core::Result<void> DcmtkTlsAdapter::applyToAssociation(T_ASC_Parameters* params) {
#ifdef WITH_OPENSSL
    if (!config_.isEnabled() || !initialized_ || !tlsLayer_) {
        return Result::ok();
    }
    
    try {
        if (ASC_setTransportLayerType(params, isServer_ ? OFTrue : OFFalse) != EC_Normal) {
            return Result::error("Failed to set transport layer type for association");
        }
        
        logger::logInfo("TLS settings applied to association");
        return Result::ok();
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to apply TLS to association: " + std::string(ex.what()));
    }
#else
    return Result::error("TLS support is not available in this build");
#endif
}

bool DcmtkTlsAdapter::isEnabled() const {
#ifdef WITH_OPENSSL
    return config_.isEnabled() && initialized_;
#else
    return false;
#endif
}

DcmTLSTransportLayer* DcmtkTlsAdapter::getTLSLayer() {
#ifdef WITH_OPENSSL
    return tlsLayer_.get();
#else
    return nullptr;
#endif
}

std::string DcmtkTlsAdapter::getProtocolString(TLSProtocolVersion version) {
    switch (version) {
        case TLSProtocolVersion::TLSv1_0:
            return "TLSv1";
        case TLSProtocolVersion::TLSv1_1:
            return "TLSv1_1";
        case TLSProtocolVersion::TLSv1_2:
            return "TLSv1_2";
        case TLSProtocolVersion::TLSv1_3:
            return "TLSv1_3";
        case TLSProtocolVersion::Auto:
        default:
            return "DEFAULT";
    }
}

} // namespace security
} // namespace common
} // namespace pacs