/**
 * @file certificate.hpp
 * @brief X.509 Certificate and Private Key handling for DICOM digital signatures
 *
 * This file provides classes for loading, parsing, and managing X.509
 * certificates and private keys used in DICOM digital signature operations.
 *
 * @see DICOM PS3.15 - Security and System Management Profiles
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <kcenon/common/patterns/result.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::security {

// Forward declarations for PIMPL
class certificate_impl;
class private_key_impl;

/**
 * @brief Represents an X.509 certificate
 *
 * This class wraps an X.509 certificate and provides methods to access
 * certificate information such as subject, issuer, validity period, and
 * public key.
 *
 * Thread Safety: This class is thread-safe for read operations.
 *
 * @example
 * @code
 * auto cert_result = certificate::load_from_pem("/path/to/cert.pem");
 * if (cert_result.is_ok()) {
 *     auto& cert = cert_result.value();
 *     std::cout << "Subject: " << cert.subject_name() << "\n";
 *     std::cout << "Issuer: " << cert.issuer_name() << "\n";
 *     std::cout << "Valid: " << cert.is_valid() << "\n";
 * }
 * @endcode
 */
class certificate {
public:
    /**
     * @brief Default constructor - creates an empty certificate
     */
    certificate();

    /**
     * @brief Copy constructor
     */
    certificate(const certificate& other);

    /**
     * @brief Move constructor
     */
    certificate(certificate&& other) noexcept;

    /**
     * @brief Copy assignment
     */
    auto operator=(const certificate& other) -> certificate&;

    /**
     * @brief Move assignment
     */
    auto operator=(certificate&& other) noexcept -> certificate&;

    /**
     * @brief Destructor
     */
    ~certificate();

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Load certificate from PEM file
     * @param path Path to PEM-encoded certificate file
     * @return Result containing certificate or error
     */
    [[nodiscard]] static auto load_from_pem(std::string_view path)
        -> kcenon::common::Result<certificate>;

    /**
     * @brief Load certificate from PEM string
     * @param pem_data PEM-encoded certificate data
     * @return Result containing certificate or error
     */
    [[nodiscard]] static auto load_from_pem_string(std::string_view pem_data)
        -> kcenon::common::Result<certificate>;

    /**
     * @brief Load certificate from DER-encoded bytes
     * @param der_data DER-encoded certificate bytes
     * @return Result containing certificate or error
     */
    [[nodiscard]] static auto load_from_der(std::span<const std::uint8_t> der_data)
        -> kcenon::common::Result<certificate>;

    // ========================================================================
    // Certificate Information
    // ========================================================================

    /**
     * @brief Get the subject distinguished name
     * @return Subject name (e.g., "CN=John Doe,O=Hospital,C=US")
     */
    [[nodiscard]] auto subject_name() const -> std::string;

    /**
     * @brief Get the common name from the subject
     * @return Common name (CN) or empty string if not present
     */
    [[nodiscard]] auto subject_common_name() const -> std::string;

    /**
     * @brief Get the organization from the subject
     * @return Organization (O) or empty string if not present
     */
    [[nodiscard]] auto subject_organization() const -> std::string;

    /**
     * @brief Get the issuer distinguished name
     * @return Issuer name
     */
    [[nodiscard]] auto issuer_name() const -> std::string;

    /**
     * @brief Get the certificate serial number
     * @return Serial number as hex string
     */
    [[nodiscard]] auto serial_number() const -> std::string;

    /**
     * @brief Get the certificate thumbprint (SHA-256)
     * @return Thumbprint as hex string
     */
    [[nodiscard]] auto thumbprint() const -> std::string;

    // ========================================================================
    // Validity
    // ========================================================================

    /**
     * @brief Get the not-before date
     * @return Start of validity period
     */
    [[nodiscard]] auto not_before() const -> std::chrono::system_clock::time_point;

    /**
     * @brief Get the not-after date
     * @return End of validity period
     */
    [[nodiscard]] auto not_after() const -> std::chrono::system_clock::time_point;

    /**
     * @brief Check if the certificate is currently valid
     * @return true if current time is within validity period
     */
    [[nodiscard]] auto is_valid() const -> bool;

    /**
     * @brief Check if the certificate has expired
     * @return true if certificate has expired
     */
    [[nodiscard]] auto is_expired() const -> bool;

    // ========================================================================
    // Export
    // ========================================================================

    /**
     * @brief Export certificate as PEM string
     * @return PEM-encoded certificate
     */
    [[nodiscard]] auto to_pem() const -> std::string;

    /**
     * @brief Export certificate as DER bytes
     * @return DER-encoded certificate
     */
    [[nodiscard]] auto to_der() const -> std::vector<std::uint8_t>;

    // ========================================================================
    // Internal Access
    // ========================================================================

    /**
     * @brief Check if certificate is loaded
     * @return true if certificate data is loaded
     */
    [[nodiscard]] auto is_loaded() const noexcept -> bool;

    /**
     * @brief Get internal implementation (for internal use only)
     */
    [[nodiscard]] auto impl() const noexcept -> const certificate_impl*;
    [[nodiscard]] auto impl() noexcept -> certificate_impl*;

private:
    friend class certificate_chain;  // Allow certificate_chain to access impl_
    std::unique_ptr<certificate_impl> impl_;
};

/**
 * @brief Represents a private key for signing operations
 *
 * This class wraps a private key (RSA or EC) and provides methods to
 * load and use the key for digital signature creation.
 *
 * Thread Safety: This class is thread-safe for read operations.
 *
 * @example
 * @code
 * auto key_result = private_key::load_from_pem("/path/to/key.pem", "password");
 * if (key_result.is_ok()) {
 *     auto& key = key_result.value();
 *     std::cout << "Key type: " << key.algorithm_name() << "\n";
 *     std::cout << "Key size: " << key.key_size() << " bits\n";
 * }
 * @endcode
 */
class private_key {
public:
    /**
     * @brief Default constructor - creates an empty key
     */
    private_key();

    /**
     * @brief Copy constructor (deleted - private keys should not be copied)
     */
    private_key(const private_key&) = delete;

    /**
     * @brief Move constructor
     */
    private_key(private_key&& other) noexcept;

    /**
     * @brief Copy assignment (deleted)
     */
    auto operator=(const private_key&) -> private_key& = delete;

    /**
     * @brief Move assignment
     */
    auto operator=(private_key&& other) noexcept -> private_key&;

    /**
     * @brief Destructor - securely erases key material
     */
    ~private_key();

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Load private key from PEM file
     * @param path Path to PEM-encoded private key file
     * @param password Optional password for encrypted keys (empty for unencrypted)
     * @return Result containing private key or error
     */
    [[nodiscard]] static auto load_from_pem(
        std::string_view path,
        std::string_view password = ""
    ) -> kcenon::common::Result<private_key>;

    /**
     * @brief Load private key from PEM string
     * @param pem_data PEM-encoded private key data
     * @param password Optional password for encrypted keys
     * @return Result containing private key or error
     */
    [[nodiscard]] static auto load_from_pem_string(
        std::string_view pem_data,
        std::string_view password = ""
    ) -> kcenon::common::Result<private_key>;

    // ========================================================================
    // Key Information
    // ========================================================================

    /**
     * @brief Get the algorithm name
     * @return Algorithm name (e.g., "RSA", "EC")
     */
    [[nodiscard]] auto algorithm_name() const -> std::string;

    /**
     * @brief Get the key size in bits
     * @return Key size (e.g., 2048, 4096 for RSA; 256, 384 for EC)
     */
    [[nodiscard]] auto key_size() const -> int;

    /**
     * @brief Check if key is loaded
     * @return true if key data is loaded
     */
    [[nodiscard]] auto is_loaded() const noexcept -> bool;

    // ========================================================================
    // Internal Access
    // ========================================================================

    /**
     * @brief Get internal implementation (for internal use only)
     */
    [[nodiscard]] auto impl() const noexcept -> const private_key_impl*;
    [[nodiscard]] auto impl() noexcept -> private_key_impl*;

private:
    std::unique_ptr<private_key_impl> impl_;
};

/**
 * @brief Represents a certificate chain for validation
 *
 * A certificate chain consists of the end-entity certificate, intermediate
 * certificates, and optionally the root certificate.
 */
class certificate_chain {
public:
    /**
     * @brief Default constructor - creates an empty chain
     */
    certificate_chain() = default;

    /**
     * @brief Add a certificate to the chain
     * @param cert Certificate to add
     */
    void add(certificate cert);

    /**
     * @brief Get the end-entity (leaf) certificate
     * @return End-entity certificate or nullptr if chain is empty
     */
    [[nodiscard]] auto end_entity() const -> const certificate*;

    /**
     * @brief Get all certificates in the chain
     * @return Vector of certificates (end-entity first, root last)
     */
    [[nodiscard]] auto certificates() const -> const std::vector<certificate>&;

    /**
     * @brief Check if chain is empty
     * @return true if no certificates in chain
     */
    [[nodiscard]] auto empty() const noexcept -> bool;

    /**
     * @brief Get number of certificates in chain
     * @return Certificate count
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Load certificate chain from PEM file
     *
     * PEM file may contain multiple certificates. The first certificate
     * is treated as the end-entity certificate.
     *
     * @param path Path to PEM file containing certificate chain
     * @return Result containing certificate chain or error
     */
    [[nodiscard]] static auto load_from_pem(std::string_view path)
        -> kcenon::common::Result<certificate_chain>;

private:
    std::vector<certificate> certs_;
};

} // namespace pacs::security
