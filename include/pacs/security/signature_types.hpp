/**
 * @file signature_types.hpp
 * @brief Digital signature types and structures for DICOM PS3.15 compliance
 *
 * This file defines the fundamental types used in DICOM digital signature
 * operations, including signature algorithms, status codes, and signature
 * information structures.
 *
 * @see DICOM PS3.15 - Security and System Management Profiles
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::security {

/**
 * @brief Signature algorithms supported for DICOM digital signatures
 *
 * Per DICOM PS3.15, the following algorithms are defined:
 * - RSA with SHA-256 (recommended)
 * - RSA with SHA-384
 * - RSA with SHA-512
 * - ECDSA with SHA-256 (for smaller key sizes)
 * - ECDSA with SHA-384
 */
enum class signature_algorithm {
    rsa_sha256,     ///< RSA with SHA-256 (recommended for most use cases)
    rsa_sha384,     ///< RSA with SHA-384
    rsa_sha512,     ///< RSA with SHA-512 (highest security)
    ecdsa_sha256,   ///< ECDSA with SHA-256 (compact signatures)
    ecdsa_sha384    ///< ECDSA with SHA-384
};

/**
 * @brief Convert signature_algorithm to string representation
 * @param algo The algorithm to convert
 * @return String representation of the algorithm
 */
constexpr std::string_view to_string(signature_algorithm algo) {
    switch (algo) {
        case signature_algorithm::rsa_sha256: return "RSA-SHA256";
        case signature_algorithm::rsa_sha384: return "RSA-SHA384";
        case signature_algorithm::rsa_sha512: return "RSA-SHA512";
        case signature_algorithm::ecdsa_sha256: return "ECDSA-SHA256";
        case signature_algorithm::ecdsa_sha384: return "ECDSA-SHA384";
    }
    return "Unknown";
}

/**
 * @brief Parse signature_algorithm from string
 * @param str The string to parse
 * @return Optional containing the algorithm, or nullopt if parsing fails
 */
inline std::optional<signature_algorithm> parse_signature_algorithm(std::string_view str) {
    if (str == "RSA-SHA256") return signature_algorithm::rsa_sha256;
    if (str == "RSA-SHA384") return signature_algorithm::rsa_sha384;
    if (str == "RSA-SHA512") return signature_algorithm::rsa_sha512;
    if (str == "ECDSA-SHA256") return signature_algorithm::ecdsa_sha256;
    if (str == "ECDSA-SHA384") return signature_algorithm::ecdsa_sha384;
    return std::nullopt;
}

/**
 * @brief Status of signature verification
 */
enum class signature_status {
    valid,              ///< Signature is valid and trusted
    invalid,            ///< Signature verification failed (tampered data)
    expired,            ///< Signer certificate has expired
    untrusted_signer,   ///< Signer certificate is not trusted
    revoked,            ///< Signer certificate has been revoked
    no_signature        ///< No signature present in dataset
};

/**
 * @brief Convert signature_status to string representation
 * @param status The status to convert
 * @return String representation of the status
 */
constexpr std::string_view to_string(signature_status status) {
    switch (status) {
        case signature_status::valid: return "Valid";
        case signature_status::invalid: return "Invalid";
        case signature_status::expired: return "Expired";
        case signature_status::untrusted_signer: return "UntrustedSigner";
        case signature_status::revoked: return "Revoked";
        case signature_status::no_signature: return "NoSignature";
    }
    return "Unknown";
}

/**
 * @brief Information about a digital signature
 *
 * Contains metadata extracted from a DICOM Digital Signature Sequence
 * (0400,0561) item.
 */
struct signature_info {
    std::string signature_uid;                      ///< Digital Signature UID (0400,0100)
    std::string signer_name;                        ///< Name of the signer (extracted from certificate)
    std::string signer_organization;                ///< Organization of the signer
    std::chrono::system_clock::time_point timestamp; ///< Digital Signature DateTime (0400,0105)
    signature_algorithm algorithm;                   ///< Algorithm used for signing
    std::vector<std::uint32_t> signed_tags;         ///< List of tags that were signed
    std::string certificate_thumbprint;             ///< SHA-256 thumbprint of signer certificate

    bool operator==(const signature_info&) const = default;
};

/**
 * @brief MAC algorithm identifiers per DICOM PS3.15
 *
 * These identifiers are used in the MAC Algorithm (0400,0015) attribute.
 */
enum class mac_algorithm {
    ripemd160,      ///< RIPEMD-160 (legacy, not recommended)
    sha1,           ///< SHA-1 (deprecated, avoid for new signatures)
    md5,            ///< MD5 (deprecated, avoid for new signatures)
    sha256,         ///< SHA-256 (recommended)
    sha384,         ///< SHA-384
    sha512          ///< SHA-512
};

/**
 * @brief Convert mac_algorithm to DICOM UID string
 * @param algo The MAC algorithm
 * @return DICOM defined term for the algorithm
 */
constexpr std::string_view to_dicom_uid(mac_algorithm algo) {
    switch (algo) {
        case mac_algorithm::ripemd160: return "1.2.840.113549.2.5";
        case mac_algorithm::sha1: return "1.3.14.3.2.26";
        case mac_algorithm::md5: return "1.2.840.113549.2.5";
        case mac_algorithm::sha256: return "2.16.840.1.101.3.4.2.1";
        case mac_algorithm::sha384: return "2.16.840.1.101.3.4.2.2";
        case mac_algorithm::sha512: return "2.16.840.1.101.3.4.2.3";
    }
    return "";
}

/**
 * @brief Certificate type for DICOM signatures
 *
 * Specifies the type of certificate used in the signature.
 */
enum class certificate_type {
    x509_certificate,       ///< X.509 certificate (most common)
    x509_certificate_chain  ///< Full X.509 certificate chain
};

/**
 * @brief Convert certificate_type to DICOM defined term
 * @param type The certificate type
 * @return DICOM defined term
 */
constexpr std::string_view to_dicom_term(certificate_type type) {
    switch (type) {
        case certificate_type::x509_certificate: return "X509_1993_SIG";
        case certificate_type::x509_certificate_chain: return "X509_1993_SIG_CHAIN";
    }
    return "";
}

} // namespace pacs::security
