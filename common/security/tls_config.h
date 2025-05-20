#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace pacs {
namespace common {
namespace security {

/**
 * @brief TLS verification mode options
 */
enum class TLSVerificationMode {
    None,         ///< No verification
    Required,     ///< Verification required, fail if certificate is invalid
    Relaxed       ///< Verification attempted, but continue on failure
};

/**
 * @brief TLS protocol version options
 */
enum class TLSProtocolVersion {
    TLSv1_0,      ///< TLS 1.0 (not recommended for security reasons)
    TLSv1_1,      ///< TLS 1.1 (not recommended for security reasons)
    TLSv1_2,      ///< TLS 1.2
    TLSv1_3,      ///< TLS 1.3 (if supported by OpenSSL)
    Auto          ///< Use highest available version
};

/**
 * @brief Configuration for TLS connections
 * 
 * This class contains configuration parameters for TLS connections,
 * including certificate paths, verification modes, and protocol options.
 */
class TLSConfig {
public:
    /**
     * @brief Default constructor
     */
    TLSConfig();
    
    /**
     * @brief Constructor with certificate files
     * @param certificatePath Path to certificate file
     * @param privateKeyPath Path to private key file
     */
    TLSConfig(const std::string& certificatePath, const std::string& privateKeyPath);
    
    /**
     * @brief Set the certificate file path
     * @param path Path to the certificate file
     * @return Reference to this object for chaining
     */
    TLSConfig& setCertificatePath(const std::string& path);
    
    /**
     * @brief Set the private key file path
     * @param path Path to the private key file
     * @return Reference to this object for chaining
     */
    TLSConfig& setPrivateKeyPath(const std::string& path);
    
    /**
     * @brief Set the CA certificate file path
     * @param path Path to the CA certificate file
     * @return Reference to this object for chaining
     */
    TLSConfig& setCACertificatePath(const std::string& path);
    
    /**
     * @brief Set the CA certificate directory path
     * @param path Path to the directory containing CA certificates
     * @return Reference to this object for chaining
     */
    TLSConfig& setCACertificateDir(const std::string& path);
    
    /**
     * @brief Set the verification mode
     * @param mode Verification mode
     * @return Reference to this object for chaining
     */
    TLSConfig& setVerificationMode(TLSVerificationMode mode);
    
    /**
     * @brief Set the minimum TLS protocol version
     * @param version Protocol version
     * @return Reference to this object for chaining
     */
    TLSConfig& setMinimumProtocolVersion(TLSProtocolVersion version);
    
    /**
     * @brief Set the cipher list
     * @param cipherList OpenSSL cipher list string
     * @return Reference to this object for chaining
     */
    TLSConfig& setCipherList(const std::string& cipherList);
    
    /**
     * @brief Set whether to use client authentication
     * @param useClientAuth Whether to request client certificate
     * @return Reference to this object for chaining
     */
    TLSConfig& setUseClientAuthentication(bool useClientAuth);
    
    /**
     * @brief Add a trusted certificate
     * @param certPath Path to trusted certificate file
     * @return Reference to this object for chaining
     */
    TLSConfig& addTrustedCertificate(const std::string& certPath);
    
    /**
     * @brief Load configuration from the global ConfigManager
     * @return Reference to this object for chaining
     */
    TLSConfig& loadFromConfig();
    
    /**
     * @brief Check if TLS is enabled
     * @return True if TLS is enabled, false otherwise
     */
    bool isEnabled() const;
    
    /**
     * @brief Get the certificate file path
     * @return Certificate file path
     */
    const std::string& getCertificatePath() const;
    
    /**
     * @brief Get the private key file path
     * @return Private key file path
     */
    const std::string& getPrivateKeyPath() const;
    
    /**
     * @brief Get the CA certificate file path
     * @return CA certificate file path
     */
    const std::optional<std::string>& getCACertificatePath() const;
    
    /**
     * @brief Get the CA certificate directory path
     * @return CA certificate directory path
     */
    const std::optional<std::string>& getCACertificateDir() const;
    
    /**
     * @brief Get the verification mode
     * @return Verification mode
     */
    TLSVerificationMode getVerificationMode() const;
    
    /**
     * @brief Get the minimum TLS protocol version
     * @return Protocol version
     */
    TLSProtocolVersion getMinimumProtocolVersion() const;
    
    /**
     * @brief Get the cipher list
     * @return Cipher list string
     */
    const std::string& getCipherList() const;
    
    /**
     * @brief Check if client authentication is enabled
     * @return True if client authentication is enabled, false otherwise
     */
    bool useClientAuthentication() const;
    
    /**
     * @brief Get the list of trusted certificates
     * @return Vector of paths to trusted certificate files
     */
    const std::vector<std::string>& getTrustedCertificates() const;
    
private:
    std::string certificatePath_;
    std::string privateKeyPath_;
    std::optional<std::string> caCertificatePath_;
    std::optional<std::string> caCertificateDir_;
    TLSVerificationMode verificationMode_ = TLSVerificationMode::Required;
    TLSProtocolVersion minimumProtocolVersion_ = TLSProtocolVersion::TLSv1_2;
    std::string cipherList_ = "HIGH:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK";
    bool useClientAuthentication_ = false;
    std::vector<std::string> trustedCertificates_;
};

} // namespace security
} // namespace common
} // namespace pacs