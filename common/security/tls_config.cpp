#include "tls_config.h"
#include "common/config/config_manager.h"
#include "common/logger/logger.h"

namespace pacs {
namespace common {
namespace security {

TLSConfig::TLSConfig() = default;

TLSConfig::TLSConfig(const std::string& certificatePath, const std::string& privateKeyPath)
    : certificatePath_(certificatePath), privateKeyPath_(privateKeyPath) {
}

TLSConfig& TLSConfig::setCertificatePath(const std::string& path) {
    certificatePath_ = path;
    return *this;
}

TLSConfig& TLSConfig::setPrivateKeyPath(const std::string& path) {
    privateKeyPath_ = path;
    return *this;
}

TLSConfig& TLSConfig::setCACertificatePath(const std::string& path) {
    caCertificatePath_ = path;
    return *this;
}

TLSConfig& TLSConfig::setCACertificateDir(const std::string& path) {
    caCertificateDir_ = path;
    return *this;
}

TLSConfig& TLSConfig::setVerificationMode(TLSVerificationMode mode) {
    verificationMode_ = mode;
    return *this;
}

TLSConfig& TLSConfig::setMinimumProtocolVersion(TLSProtocolVersion version) {
    minimumProtocolVersion_ = version;
    return *this;
}

TLSConfig& TLSConfig::setCipherList(const std::string& cipherList) {
    cipherList_ = cipherList;
    return *this;
}

TLSConfig& TLSConfig::setUseClientAuthentication(bool useClientAuth) {
    useClientAuthentication_ = useClientAuth;
    return *this;
}

TLSConfig& TLSConfig::addTrustedCertificate(const std::string& certPath) {
    trustedCertificates_.push_back(certPath);
    return *this;
}

TLSConfig& TLSConfig::loadFromConfig() {
    try {
        auto& configManager = config::ConfigManager::getInstance();
        const auto& serviceConfig = configManager.getServiceConfig();
        
        // Check if TLS is enabled
        if (serviceConfig.useTLS && *serviceConfig.useTLS) {
            // Get certificate and key paths
            if (serviceConfig.tlsCertificatePath && serviceConfig.tlsPrivateKeyPath) {
                certificatePath_ = *serviceConfig.tlsCertificatePath;
                privateKeyPath_ = *serviceConfig.tlsPrivateKeyPath;
                
                // Get CA certificate configuration
                auto caCertPath = configManager.getValue("tls.ca.certificate");
                if (!caCertPath.empty()) {
                    caCertificatePath_ = caCertPath;
                }
                
                auto caCertDir = configManager.getValue("tls.ca.directory");
                if (!caCertDir.empty()) {
                    caCertificateDir_ = caCertDir;
                }
                
                // Get verification mode
                auto verifyMode = configManager.getValue("tls.verification.mode", "required");
                if (verifyMode == "none") {
                    verificationMode_ = TLSVerificationMode::None;
                } else if (verifyMode == "relaxed") {
                    verificationMode_ = TLSVerificationMode::Relaxed;
                } else {
                    verificationMode_ = TLSVerificationMode::Required;
                }
                
                // Get minimum protocol version
                auto minProtocol = configManager.getValue("tls.min.protocol", "tlsv1.2");
                if (minProtocol == "tlsv1.0") {
                    minimumProtocolVersion_ = TLSProtocolVersion::TLSv1_0;
                } else if (minProtocol == "tlsv1.1") {
                    minimumProtocolVersion_ = TLSProtocolVersion::TLSv1_1;
                } else if (minProtocol == "tlsv1.3") {
                    minimumProtocolVersion_ = TLSProtocolVersion::TLSv1_3;
                } else if (minProtocol == "auto") {
                    minimumProtocolVersion_ = TLSProtocolVersion::Auto;
                } else {
                    minimumProtocolVersion_ = TLSProtocolVersion::TLSv1_2;
                }
                
                // Get cipher list
                auto ciphers = configManager.getValue("tls.ciphers");
                if (!ciphers.empty()) {
                    cipherList_ = ciphers;
                }
                
                // Get client authentication setting
                auto clientAuth = configManager.getValue("tls.client.authentication", "false");
                useClientAuthentication_ = (clientAuth == "true" || clientAuth == "1");
                
                // Get trusted certificates
                for (int i = 1; ; i++) {
                    auto certPath = configManager.getValue("tls.trusted.certificate." + std::to_string(i));
                    if (certPath.empty()) {
                        break;
                    }
                    trustedCertificates_.push_back(certPath);
                }
                
                logger::logInfo("TLS configuration loaded from config manager");
                logger::logDebug("TLS certificate: {}", certificatePath_);
                logger::logDebug("TLS private key: {}", privateKeyPath_);
                logger::logDebug("TLS verification mode: {}", static_cast<int>(verificationMode_));
                logger::logDebug("TLS minimum protocol: {}", static_cast<int>(minimumProtocolVersion_));
                logger::logDebug("TLS client authentication: {}", useClientAuthentication_ ? "enabled" : "disabled");
            } else {
                logger::logError("TLS is enabled but certificate or private key path is missing");
            }
        } else {
            logger::logInfo("TLS is disabled in configuration");
        }
    }
    catch (const std::exception& ex) {
        logger::logError("Failed to load TLS configuration: {}", ex.what());
    }
    
    return *this;
}

bool TLSConfig::isEnabled() const {
    return !certificatePath_.empty() && !privateKeyPath_.empty();
}

const std::string& TLSConfig::getCertificatePath() const {
    return certificatePath_;
}

const std::string& TLSConfig::getPrivateKeyPath() const {
    return privateKeyPath_;
}

const std::optional<std::string>& TLSConfig::getCACertificatePath() const {
    return caCertificatePath_;
}

const std::optional<std::string>& TLSConfig::getCACertificateDir() const {
    return caCertificateDir_;
}

TLSVerificationMode TLSConfig::getVerificationMode() const {
    return verificationMode_;
}

TLSProtocolVersion TLSConfig::getMinimumProtocolVersion() const {
    return minimumProtocolVersion_;
}

const std::string& TLSConfig::getCipherList() const {
    return cipherList_;
}

bool TLSConfig::useClientAuthentication() const {
    return useClientAuthentication_;
}

const std::vector<std::string>& TLSConfig::getTrustedCertificates() const {
    return trustedCertificates_;
}

} // namespace security
} // namespace common
} // namespace pacs