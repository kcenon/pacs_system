/**
 * @file digital_signature.cpp
 * @brief Implementation of DICOM digital signature operations
 *
 * @copyright Copyright (c) 2025
 */

#include "pacs/security/digital_signature.hpp"

#include <pacs/core/dicom_element.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace pacs::security {

// Forward declaration of impl classes from certificate.cpp
class certificate_impl;
class private_key_impl;

namespace {

// DICOM Digital Signature tags
constexpr core::dicom_tag digital_signature_sequence_tag{0x0400, 0x0561};
constexpr core::dicom_tag signature_uid_tag{0x0400, 0x0100};
constexpr core::dicom_tag signature_datetime_tag{0x0400, 0x0105};
constexpr core::dicom_tag certificate_type_tag{0x0400, 0x0110};
constexpr core::dicom_tag certificate_of_signer_tag{0x0400, 0x0115};
constexpr core::dicom_tag signature_tag{0x0400, 0x0120};
constexpr core::dicom_tag mac_id_number_tag{0x0400, 0x0005};
constexpr core::dicom_tag mac_calculation_transfer_syntax_tag{0x0400, 0x0010};
constexpr core::dicom_tag mac_algorithm_tag{0x0400, 0x0015};
constexpr core::dicom_tag data_elements_signed_tag{0x0400, 0x0020};

/**
 * @brief Get OpenSSL error string
 */
auto get_openssl_error() -> std::string {
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return "Unknown error";
    }
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}

/**
 * @brief RAII wrapper for EVP_MD_CTX
 */
struct evp_md_ctx_deleter {
    void operator()(EVP_MD_CTX* ctx) const {
        if (ctx) EVP_MD_CTX_free(ctx);
    }
};
using evp_md_ctx_ptr = std::unique_ptr<EVP_MD_CTX, evp_md_ctx_deleter>;

/**
 * @brief Get EVP_MD for signature algorithm
 */
auto get_evp_md(signature_algorithm algo) -> const EVP_MD* {
    switch (algo) {
        case signature_algorithm::rsa_sha256:
        case signature_algorithm::ecdsa_sha256:
            return EVP_sha256();
        case signature_algorithm::rsa_sha384:
        case signature_algorithm::ecdsa_sha384:
            return EVP_sha384();
        case signature_algorithm::rsa_sha512:
            return EVP_sha512();
    }
    return EVP_sha256();
}

/**
 * @brief Get EVP_MD for MAC algorithm
 */
auto get_mac_evp_md(mac_algorithm algo) -> const EVP_MD* {
    switch (algo) {
        case mac_algorithm::ripemd160:
            return EVP_ripemd160();
        case mac_algorithm::sha1:
            return EVP_sha1();
        case mac_algorithm::md5:
            return EVP_md5();
        case mac_algorithm::sha256:
            return EVP_sha256();
        case mac_algorithm::sha384:
            return EVP_sha384();
        case mac_algorithm::sha512:
            return EVP_sha512();
    }
    return EVP_sha256();
}

/**
 * @brief Get mac_algorithm from signature_algorithm
 */
auto get_mac_algorithm(signature_algorithm algo) -> mac_algorithm {
    switch (algo) {
        case signature_algorithm::rsa_sha256:
        case signature_algorithm::ecdsa_sha256:
            return mac_algorithm::sha256;
        case signature_algorithm::rsa_sha384:
        case signature_algorithm::ecdsa_sha384:
            return mac_algorithm::sha384;
        case signature_algorithm::rsa_sha512:
            return mac_algorithm::sha512;
    }
    return mac_algorithm::sha256;
}

/**
 * @brief Generate DICOM UID
 */
auto generate_uid() -> std::string {
    // Use DICOM private root + random component
    // Format: 1.2.840.XXXXX.NNNN.timestamp.random
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    unsigned char random_bytes[4];
    RAND_bytes(random_bytes, sizeof(random_bytes));

    std::ostringstream oss;
    oss << "1.2.840.10008.5.1.4.1.1.66."
        << timestamp << "."
        << static_cast<unsigned int>(random_bytes[0])
        << static_cast<unsigned int>(random_bytes[1])
        << static_cast<unsigned int>(random_bytes[2])
        << static_cast<unsigned int>(random_bytes[3]);

    return oss.str();
}

/**
 * @brief Format current time as DICOM DateTime
 */
auto format_dicom_datetime() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_now{};
    gmtime_r(&time_t_now, &tm_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y%m%d%H%M%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count()
        << "+0000";

    return oss.str();
}

/**
 * @brief Parse DICOM DateTime to time_point
 */
auto parse_dicom_datetime(std::string_view dt_str)
    -> std::chrono::system_clock::time_point {
    if (dt_str.size() < 14) {
        return std::chrono::system_clock::time_point{};
    }

    std::tm tm{};
    std::istringstream ss(std::string(dt_str.substr(0, 14)));
    ss >> std::get_time(&tm, "%Y%m%d%H%M%S");

    if (ss.fail()) {
        return std::chrono::system_clock::time_point{};
    }

    time_t time = timegm(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

/**
 * @brief Serialize dataset elements to bytes for MAC calculation
 */
auto serialize_elements_for_mac(
    const core::dicom_dataset& dataset,
    std::span<const core::dicom_tag> tags
) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> result;

    for (const auto& tag : tags) {
        const auto* elem = dataset.get(tag);
        if (!elem) {
            continue;
        }

        // Add tag (4 bytes, little endian)
        uint16_t group = tag.group();
        uint16_t element = tag.element();

        result.push_back(static_cast<uint8_t>(group & 0xFF));
        result.push_back(static_cast<uint8_t>((group >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(element & 0xFF));
        result.push_back(static_cast<uint8_t>((element >> 8) & 0xFF));

        // Add element value
        const auto& data = elem->raw_data();
        for (auto byte : data) {
            result.push_back(byte);
        }
    }

    return result;
}

/**
 * @brief Get all non-signature tags from dataset
 */
auto get_signable_tags(const core::dicom_dataset& dataset)
    -> std::vector<core::dicom_tag> {
    std::vector<core::dicom_tag> tags;

    for (const auto& [tag, elem] : dataset) {
        // Skip signature-related tags
        if (tag.group() == 0x0400) {
            continue;
        }
        tags.push_back(tag);
    }

    std::sort(tags.begin(), tags.end());
    return tags;
}

// External declaration to access OpenSSL handles
extern "C" {
    X509* get_x509_from_certificate(const certificate& cert);
    EVP_PKEY* get_pkey_from_private_key(const private_key& key);
}

} // anonymous namespace

// Helper to access OpenSSL X509 handle
X509* get_x509_from_certificate(const certificate& cert) {
    auto* impl = cert.impl();
    if (!impl) return nullptr;
    // Access x509 through the impl
    // This is a workaround - in production, expose a proper internal API
    return reinterpret_cast<X509*>(const_cast<void*>(
        static_cast<const void*>(impl)));
}

// ============================================================================
// digital_signature implementation
// ============================================================================

auto digital_signature::sign(
    core::dicom_dataset& dataset,
    const certificate& cert,
    const private_key& key,
    signature_algorithm algo
) -> kcenon::common::VoidResult {
    // Get all signable tags
    auto tags = get_signable_tags(dataset);
    return sign_tags(dataset, cert, key, tags, algo);
}

auto digital_signature::sign_tags(
    core::dicom_dataset& dataset,
    const certificate& cert,
    const private_key& key,
    std::span<const core::dicom_tag> tags_to_sign,
    signature_algorithm algo
) -> kcenon::common::VoidResult {
    if (!cert.is_loaded()) {
        return kcenon::common::make_error<std::monostate>(
            1, "Certificate not loaded", "digital_signature");
    }

    if (!key.is_loaded()) {
        return kcenon::common::make_error<std::monostate>(
            2, "Private key not loaded", "digital_signature");
    }

    if (!cert.is_valid()) {
        return kcenon::common::make_error<std::monostate>(
            3, "Certificate is not valid (expired or not yet valid)", "digital_signature");
    }

    // Compute MAC of the data
    auto mac_algo = get_mac_algorithm(algo);
    auto mac = compute_mac(dataset, tags_to_sign, mac_algo);

    // Sign the MAC
    auto signature_result = sign_mac(mac, key, algo);
    if (signature_result.is_err()) {
        return kcenon::common::make_error<std::monostate>(
            signature_result.error().code,
            signature_result.error().message,
            "digital_signature");
    }

    const auto& signature_bytes = signature_result.value();

    // Create Digital Signature Sequence item
    // For simplicity, we'll add the signature data directly to the dataset
    // In a full implementation, this would create a proper sequence item

    // Generate signature UID
    std::string sig_uid = generate_signature_uid();

    // Get certificate DER
    auto cert_der = cert.to_der();

    // Create signed tags list (as AT type - list of tags)
    std::vector<std::uint8_t> signed_tags_data;
    for (const auto& tag : tags_to_sign) {
        uint16_t group = tag.group();
        uint16_t element = tag.element();
        signed_tags_data.push_back(static_cast<uint8_t>(group & 0xFF));
        signed_tags_data.push_back(static_cast<uint8_t>((group >> 8) & 0xFF));
        signed_tags_data.push_back(static_cast<uint8_t>(element & 0xFF));
        signed_tags_data.push_back(static_cast<uint8_t>((element >> 8) & 0xFF));
    }

    // Add signature elements to dataset
    // Note: In a full DICOM implementation, these would be in a sequence item
    dataset.set_string(signature_uid_tag, encoding::vr_type::UI, sig_uid);
    dataset.set_string(signature_datetime_tag, encoding::vr_type::DT, format_dicom_datetime());
    dataset.set_string(certificate_type_tag, encoding::vr_type::CS, "X509_1993_SIG");

    // Add certificate data
    dataset.insert(core::dicom_element(
        certificate_of_signer_tag,
        encoding::vr_type::OB,
        cert_der));

    // Add signature
    dataset.insert(core::dicom_element(
        signature_tag,
        encoding::vr_type::OB,
        signature_bytes));

    // Add MAC algorithm
    dataset.set_string(mac_algorithm_tag, encoding::vr_type::CS,
        std::string(to_dicom_uid(mac_algo)));

    // Add signed tags
    dataset.insert(core::dicom_element(
        data_elements_signed_tag,
        encoding::vr_type::AT,
        signed_tags_data));

    return kcenon::common::ok();
}

auto digital_signature::verify(
    const core::dicom_dataset& dataset
) -> kcenon::common::Result<signature_status> {
    // Check if signature exists
    if (!has_signature(dataset)) {
        return signature_status::no_signature;
    }

    // Get signature info
    auto info = get_signature_info(dataset);
    if (!info) {
        return kcenon::common::make_error<signature_status>(
            1, "Failed to extract signature info", "digital_signature");
    }

    // Get certificate from dataset
    const auto* cert_elem = dataset.get(certificate_of_signer_tag);
    if (!cert_elem) {
        return kcenon::common::make_error<signature_status>(
            2, "No certificate in signature", "digital_signature");
    }

    auto cert_result = certificate::load_from_der(cert_elem->raw_data());
    if (cert_result.is_err()) {
        return kcenon::common::make_error<signature_status>(
            3, "Failed to load certificate: " + cert_result.error().message,
            "digital_signature");
    }

    const auto& cert = cert_result.value();

    // Check certificate validity
    if (cert.is_expired()) {
        return signature_status::expired;
    }

    if (!cert.is_valid()) {
        return signature_status::untrusted_signer;
    }

    // Get signed tags
    const auto* signed_tags_elem = dataset.get(data_elements_signed_tag);
    std::vector<core::dicom_tag> signed_tags;

    if (signed_tags_elem) {
        const auto& tag_data = signed_tags_elem->raw_data();
        for (size_t i = 0; i + 3 < tag_data.size(); i += 4) {
            uint16_t group = static_cast<uint16_t>(tag_data[i]) |
                           (static_cast<uint16_t>(tag_data[i + 1]) << 8);
            uint16_t element = static_cast<uint16_t>(tag_data[i + 2]) |
                             (static_cast<uint16_t>(tag_data[i + 3]) << 8);
            signed_tags.emplace_back(group, element);
        }
    } else {
        // If no signed tags specified, use all non-signature tags
        signed_tags = get_signable_tags(dataset);
    }

    // Compute MAC
    auto mac = compute_mac(dataset, signed_tags, mac_algorithm::sha256);

    // Get signature
    const auto* sig_elem = dataset.get(signature_tag);
    if (!sig_elem) {
        return kcenon::common::make_error<signature_status>(
            4, "No signature data found", "digital_signature");
    }

    // Verify signature
    bool valid = verify_mac_signature(
        mac,
        sig_elem->raw_data(),
        cert,
        info->algorithm);

    return valid ? signature_status::valid : signature_status::invalid;
}

auto digital_signature::verify_with_trust(
    const core::dicom_dataset& dataset,
    const std::vector<certificate>& trusted_certs
) -> kcenon::common::Result<signature_status> {
    // First do basic verification
    auto basic_result = verify(dataset);
    if (basic_result.is_err()) {
        return basic_result;
    }

    auto status = basic_result.value();
    if (status != signature_status::valid && status != signature_status::untrusted_signer) {
        return status;
    }

    // Get signer certificate
    const auto* cert_elem = dataset.get(certificate_of_signer_tag);
    if (!cert_elem) {
        return signature_status::untrusted_signer;
    }

    auto cert_result = certificate::load_from_der(cert_elem->raw_data());
    if (cert_result.is_err()) {
        return signature_status::untrusted_signer;
    }

    const auto& signer_cert = cert_result.value();
    std::string signer_thumbprint = signer_cert.thumbprint();

    // Check if signer is in trusted list
    for (const auto& trusted : trusted_certs) {
        if (trusted.thumbprint() == signer_thumbprint) {
            return signature_status::valid;
        }
    }

    return signature_status::untrusted_signer;
}

auto digital_signature::get_signature_info(
    const core::dicom_dataset& dataset
) -> std::optional<signature_info> {
    if (!has_signature(dataset)) {
        return std::nullopt;
    }

    signature_info info;

    // Get signature UID
    info.signature_uid = dataset.get_string(signature_uid_tag);

    // Get timestamp
    std::string dt_str = dataset.get_string(signature_datetime_tag);
    if (!dt_str.empty()) {
        info.timestamp = parse_dicom_datetime(dt_str);
    }

    // Get certificate and extract signer info
    const auto* cert_elem = dataset.get(certificate_of_signer_tag);
    if (cert_elem) {
        auto cert_result = certificate::load_from_der(cert_elem->raw_data());
        if (cert_result.is_ok()) {
            const auto& cert = cert_result.value();
            info.signer_name = cert.subject_common_name();
            info.signer_organization = cert.subject_organization();
            info.certificate_thumbprint = cert.thumbprint();
        }
    }

    // Default algorithm (could be extracted from MAC algorithm tag)
    info.algorithm = signature_algorithm::rsa_sha256;

    // Get signed tags
    const auto* signed_tags_elem = dataset.get(data_elements_signed_tag);
    if (signed_tags_elem) {
        const auto& tag_data = signed_tags_elem->raw_data();
        for (size_t i = 0; i + 3 < tag_data.size(); i += 4) {
            uint16_t group = static_cast<uint16_t>(tag_data[i]) |
                           (static_cast<uint16_t>(tag_data[i + 1]) << 8);
            uint16_t element = static_cast<uint16_t>(tag_data[i + 2]) |
                             (static_cast<uint16_t>(tag_data[i + 3]) << 8);
            info.signed_tags.push_back(
                (static_cast<uint32_t>(group) << 16) | element);
        }
    }

    return info;
}

auto digital_signature::get_all_signatures(
    const core::dicom_dataset& dataset
) -> std::vector<signature_info> {
    // For this simplified implementation, we only support one signature
    std::vector<signature_info> result;

    auto info = get_signature_info(dataset);
    if (info) {
        result.push_back(std::move(*info));
    }

    return result;
}

auto digital_signature::has_signature(
    const core::dicom_dataset& dataset
) -> bool {
    // Check for signature tag or signature sequence
    return dataset.contains(signature_tag) ||
           dataset.contains(digital_signature_sequence_tag);
}

auto digital_signature::remove_signatures(
    core::dicom_dataset& dataset
) -> bool {
    bool removed = false;

    // Remove all signature-related tags
    removed |= dataset.remove(digital_signature_sequence_tag);
    removed |= dataset.remove(signature_uid_tag);
    removed |= dataset.remove(signature_datetime_tag);
    removed |= dataset.remove(certificate_type_tag);
    removed |= dataset.remove(certificate_of_signer_tag);
    removed |= dataset.remove(signature_tag);
    removed |= dataset.remove(mac_id_number_tag);
    removed |= dataset.remove(mac_calculation_transfer_syntax_tag);
    removed |= dataset.remove(mac_algorithm_tag);
    removed |= dataset.remove(data_elements_signed_tag);

    return removed;
}

auto digital_signature::generate_signature_uid() -> std::string {
    return generate_uid();
}

auto digital_signature::compute_mac(
    const core::dicom_dataset& dataset,
    std::span<const core::dicom_tag> tags,
    mac_algorithm algo
) -> std::vector<std::uint8_t> {
    // Serialize elements
    auto data = serialize_elements_for_mac(dataset, tags);

    // Compute hash
    const EVP_MD* md = get_mac_evp_md(algo);
    unsigned int md_len = EVP_MD_size(md);

    std::vector<std::uint8_t> result(md_len);

    evp_md_ctx_ptr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        return {};
    }

    if (EVP_DigestInit_ex(ctx.get(), md, nullptr) != 1) {
        return {};
    }

    if (EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) {
        return {};
    }

    if (EVP_DigestFinal_ex(ctx.get(), result.data(), &md_len) != 1) {
        return {};
    }

    result.resize(md_len);
    return result;
}

auto digital_signature::sign_mac(
    std::span<const std::uint8_t> mac_data,
    const private_key& key,
    signature_algorithm algo
) -> kcenon::common::Result<std::vector<std::uint8_t>> {
    if (!key.is_loaded()) {
        return kcenon::common::make_error<std::vector<std::uint8_t>>(
            1, "Private key not loaded", "digital_signature");
    }

    // Get EVP_PKEY from private_key
    // Note: This requires internal access to the private_key implementation
    auto* key_impl = key.impl();
    if (!key_impl) {
        return kcenon::common::make_error<std::vector<std::uint8_t>>(
            2, "Invalid private key", "digital_signature");
    }

    // Access the EVP_PKEY through the impl
    // In production, this would use a proper internal API
    EVP_PKEY* pkey = reinterpret_cast<EVP_PKEY*>(const_cast<void*>(
        static_cast<const void*>(key_impl)));

    const EVP_MD* md = get_evp_md(algo);

    evp_md_ctx_ptr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        return kcenon::common::make_error<std::vector<std::uint8_t>>(
            3, "Failed to create context: " + get_openssl_error(), "digital_signature");
    }

    if (EVP_DigestSignInit(ctx.get(), nullptr, md, nullptr, pkey) != 1) {
        return kcenon::common::make_error<std::vector<std::uint8_t>>(
            4, "Failed to init sign: " + get_openssl_error(), "digital_signature");
    }

    if (EVP_DigestSignUpdate(ctx.get(), mac_data.data(), mac_data.size()) != 1) {
        return kcenon::common::make_error<std::vector<std::uint8_t>>(
            5, "Failed to update sign: " + get_openssl_error(), "digital_signature");
    }

    // Get signature length
    size_t sig_len = 0;
    if (EVP_DigestSignFinal(ctx.get(), nullptr, &sig_len) != 1) {
        return kcenon::common::make_error<std::vector<std::uint8_t>>(
            6, "Failed to get signature length: " + get_openssl_error(), "digital_signature");
    }

    std::vector<std::uint8_t> signature(sig_len);
    if (EVP_DigestSignFinal(ctx.get(), signature.data(), &sig_len) != 1) {
        return kcenon::common::make_error<std::vector<std::uint8_t>>(
            7, "Failed to create signature: " + get_openssl_error(), "digital_signature");
    }

    signature.resize(sig_len);
    return signature;
}

auto digital_signature::verify_mac_signature(
    std::span<const std::uint8_t> mac_data,
    std::span<const std::uint8_t> signature,
    const certificate& cert,
    signature_algorithm algo
) -> bool {
    if (!cert.is_loaded()) {
        return false;
    }

    // Get X509 from certificate
    auto* cert_impl = cert.impl();
    if (!cert_impl) {
        return false;
    }

    // Access the X509 through the impl
    X509* x509 = reinterpret_cast<X509*>(const_cast<void*>(
        static_cast<const void*>(cert_impl)));

    EVP_PKEY* pkey = X509_get_pubkey(x509);
    if (!pkey) {
        return false;
    }

    const EVP_MD* md = get_evp_md(algo);

    evp_md_ctx_ptr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return false;
    }

    bool result = false;

    if (EVP_DigestVerifyInit(ctx.get(), nullptr, md, nullptr, pkey) == 1 &&
        EVP_DigestVerifyUpdate(ctx.get(), mac_data.data(), mac_data.size()) == 1 &&
        EVP_DigestVerifyFinal(ctx.get(), signature.data(), signature.size()) == 1) {
        result = true;
    }

    EVP_PKEY_free(pkey);
    return result;
}

} // namespace pacs::security
