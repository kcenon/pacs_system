/**
 * @file certificate.cpp
 * @brief Implementation of X.509 certificate and private key handling
 *
 * @copyright Copyright (c) 2025
 */

#include "pacs/security/certificate.hpp"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <fstream>
#include <sstream>

namespace pacs::security {

namespace {

/**
 * @brief Cross-platform timegm implementation
 *
 * timegm is a POSIX extension not available on Windows.
 * Windows uses _mkgmtime instead.
 */
inline auto portable_timegm(struct tm* tm_time) -> time_t {
#ifdef _WIN32
    return _mkgmtime(tm_time);
#else
    return timegm(tm_time);
#endif
}

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
 * @brief Read entire file contents
 */
auto read_file(std::string_view path) -> kcenon::common::Result<std::string> {
    std::ifstream file(std::string(path), std::ios::binary);
    if (!file.is_open()) {
        return kcenon::common::make_error<std::string>(
            1, "Failed to open file: " + std::string(path), "certificate");
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/**
 * @brief RAII wrapper for BIO
 */
struct bio_deleter {
    void operator()(BIO* bio) const {
        if (bio) BIO_free(bio);
    }
};
using bio_ptr = std::unique_ptr<BIO, bio_deleter>;

/**
 * @brief Convert ASN1_TIME to system_clock time_point
 */
auto asn1_time_to_time_point(const ASN1_TIME* asn1_time)
    -> std::chrono::system_clock::time_point {
    if (!asn1_time) {
        return std::chrono::system_clock::time_point{};
    }

    struct tm tm_time = {};
    int offset_sec = 0;

    // Parse the ASN1 time
    if (ASN1_TIME_to_tm(asn1_time, &tm_time) != 1) {
        return std::chrono::system_clock::time_point{};
    }

    // Convert to time_t (using portable function for cross-platform support)
    time_t time = portable_timegm(&tm_time);

    return std::chrono::system_clock::from_time_t(time);
}

} // anonymous namespace

// ============================================================================
// certificate_impl - PIMPL implementation
// ============================================================================

class certificate_impl {
public:
    certificate_impl() : x509_(nullptr) {}

    explicit certificate_impl(X509* cert) : x509_(cert) {}

    ~certificate_impl() {
        if (x509_) {
            X509_free(x509_);
        }
    }

    // Non-copyable but movable
    certificate_impl(const certificate_impl& other) {
        if (other.x509_) {
            x509_ = X509_dup(other.x509_);
        } else {
            x509_ = nullptr;
        }
    }

    certificate_impl(certificate_impl&& other) noexcept
        : x509_(other.x509_) {
        other.x509_ = nullptr;
    }

    auto operator=(const certificate_impl& other) -> certificate_impl& {
        if (this != &other) {
            if (x509_) {
                X509_free(x509_);
            }
            if (other.x509_) {
                x509_ = X509_dup(other.x509_);
            } else {
                x509_ = nullptr;
            }
        }
        return *this;
    }

    auto operator=(certificate_impl&& other) noexcept -> certificate_impl& {
        if (this != &other) {
            if (x509_) {
                X509_free(x509_);
            }
            x509_ = other.x509_;
            other.x509_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] auto x509() const noexcept -> X509* { return x509_; }
    [[nodiscard]] auto is_loaded() const noexcept -> bool { return x509_ != nullptr; }

private:
    X509* x509_;
};

// ============================================================================
// private_key_impl - PIMPL implementation
// ============================================================================

class private_key_impl {
public:
    private_key_impl() : pkey_(nullptr) {}

    explicit private_key_impl(EVP_PKEY* key) : pkey_(key) {}

    ~private_key_impl() {
        if (pkey_) {
            EVP_PKEY_free(pkey_);
        }
    }

    // Non-copyable, movable
    private_key_impl(const private_key_impl&) = delete;
    auto operator=(const private_key_impl&) -> private_key_impl& = delete;

    private_key_impl(private_key_impl&& other) noexcept
        : pkey_(other.pkey_) {
        other.pkey_ = nullptr;
    }

    auto operator=(private_key_impl&& other) noexcept -> private_key_impl& {
        if (this != &other) {
            if (pkey_) {
                EVP_PKEY_free(pkey_);
            }
            pkey_ = other.pkey_;
            other.pkey_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] auto pkey() const noexcept -> EVP_PKEY* { return pkey_; }
    [[nodiscard]] auto is_loaded() const noexcept -> bool { return pkey_ != nullptr; }

private:
    EVP_PKEY* pkey_;
};

// ============================================================================
// certificate implementation
// ============================================================================

certificate::certificate() : impl_(std::make_unique<certificate_impl>()) {}

certificate::certificate(const certificate& other)
    : impl_(std::make_unique<certificate_impl>(*other.impl_)) {}

certificate::certificate(certificate&& other) noexcept = default;

auto certificate::operator=(const certificate& other) -> certificate& {
    if (this != &other) {
        impl_ = std::make_unique<certificate_impl>(*other.impl_);
    }
    return *this;
}

auto certificate::operator=(certificate&& other) noexcept -> certificate& = default;

certificate::~certificate() = default;

auto certificate::load_from_pem(std::string_view path)
    -> kcenon::common::Result<certificate> {
    auto content_result = read_file(path);
    if (content_result.is_err()) {
        return kcenon::common::make_error<certificate>(
            content_result.error().code,
            content_result.error().message,
            "certificate");
    }

    return load_from_pem_string(content_result.value());
}

auto certificate::load_from_pem_string(std::string_view pem_data)
    -> kcenon::common::Result<certificate> {
    bio_ptr bio(BIO_new_mem_buf(pem_data.data(), static_cast<int>(pem_data.size())));
    if (!bio) {
        return kcenon::common::make_error<certificate>(
            2, "Failed to create BIO: " + get_openssl_error(), "certificate");
    }

    X509* x509 = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
    if (!x509) {
        return kcenon::common::make_error<certificate>(
            3, "Failed to parse PEM certificate: " + get_openssl_error(), "certificate");
    }

    certificate cert;
    cert.impl_ = std::make_unique<certificate_impl>(x509);
    return cert;
}

auto certificate::load_from_der(std::span<const std::uint8_t> der_data)
    -> kcenon::common::Result<certificate> {
    const unsigned char* data = der_data.data();
    X509* x509 = d2i_X509(nullptr, &data, static_cast<long>(der_data.size()));
    if (!x509) {
        return kcenon::common::make_error<certificate>(
            4, "Failed to parse DER certificate: " + get_openssl_error(), "certificate");
    }

    certificate cert;
    cert.impl_ = std::make_unique<certificate_impl>(x509);
    return cert;
}

auto certificate::subject_name() const -> std::string {
    if (!impl_->is_loaded()) {
        return "";
    }

    X509_NAME* name = X509_get_subject_name(impl_->x509());
    if (!name) {
        return "";
    }

    bio_ptr bio(BIO_new(BIO_s_mem()));
    X509_NAME_print_ex(bio.get(), name, 0, XN_FLAG_RFC2253);

    char* data = nullptr;
    long len = BIO_get_mem_data(bio.get(), &data);
    return std::string(data, static_cast<size_t>(len));
}

auto certificate::subject_common_name() const -> std::string {
    if (!impl_->is_loaded()) {
        return "";
    }

    X509_NAME* name = X509_get_subject_name(impl_->x509());
    if (!name) {
        return "";
    }

    int idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
    if (idx < 0) {
        return "";
    }

    X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
    if (!entry) {
        return "";
    }

    ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
    if (!data) {
        return "";
    }

    unsigned char* utf8 = nullptr;
    int len = ASN1_STRING_to_UTF8(&utf8, data);
    if (len < 0) {
        return "";
    }

    std::string result(reinterpret_cast<char*>(utf8), static_cast<size_t>(len));
    OPENSSL_free(utf8);
    return result;
}

auto certificate::subject_organization() const -> std::string {
    if (!impl_->is_loaded()) {
        return "";
    }

    X509_NAME* name = X509_get_subject_name(impl_->x509());
    if (!name) {
        return "";
    }

    int idx = X509_NAME_get_index_by_NID(name, NID_organizationName, -1);
    if (idx < 0) {
        return "";
    }

    X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
    if (!entry) {
        return "";
    }

    ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
    if (!data) {
        return "";
    }

    unsigned char* utf8 = nullptr;
    int len = ASN1_STRING_to_UTF8(&utf8, data);
    if (len < 0) {
        return "";
    }

    std::string result(reinterpret_cast<char*>(utf8), static_cast<size_t>(len));
    OPENSSL_free(utf8);
    return result;
}

auto certificate::issuer_name() const -> std::string {
    if (!impl_->is_loaded()) {
        return "";
    }

    X509_NAME* name = X509_get_issuer_name(impl_->x509());
    if (!name) {
        return "";
    }

    bio_ptr bio(BIO_new(BIO_s_mem()));
    X509_NAME_print_ex(bio.get(), name, 0, XN_FLAG_RFC2253);

    char* data = nullptr;
    long len = BIO_get_mem_data(bio.get(), &data);
    return std::string(data, static_cast<size_t>(len));
}

auto certificate::serial_number() const -> std::string {
    if (!impl_->is_loaded()) {
        return "";
    }

    const ASN1_INTEGER* serial = X509_get_serialNumber(impl_->x509());
    if (!serial) {
        return "";
    }

    BIGNUM* bn = ASN1_INTEGER_to_BN(serial, nullptr);
    if (!bn) {
        return "";
    }

    char* hex = BN_bn2hex(bn);
    BN_free(bn);

    if (!hex) {
        return "";
    }

    std::string result(hex);
    OPENSSL_free(hex);
    return result;
}

auto certificate::thumbprint() const -> std::string {
    if (!impl_->is_loaded()) {
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    if (X509_digest(impl_->x509(), EVP_sha256(), hash, &hash_len) != 1) {
        return "";
    }

    std::string result;
    result.reserve(hash_len * 2);

    static const char hex_chars[] = "0123456789ABCDEF";
    for (unsigned int i = 0; i < hash_len; ++i) {
        result += hex_chars[(hash[i] >> 4) & 0x0F];
        result += hex_chars[hash[i] & 0x0F];
    }

    return result;
}

auto certificate::not_before() const -> std::chrono::system_clock::time_point {
    if (!impl_->is_loaded()) {
        return std::chrono::system_clock::time_point{};
    }

    const ASN1_TIME* time = X509_get0_notBefore(impl_->x509());
    return asn1_time_to_time_point(time);
}

auto certificate::not_after() const -> std::chrono::system_clock::time_point {
    if (!impl_->is_loaded()) {
        return std::chrono::system_clock::time_point{};
    }

    const ASN1_TIME* time = X509_get0_notAfter(impl_->x509());
    return asn1_time_to_time_point(time);
}

auto certificate::is_valid() const -> bool {
    if (!impl_->is_loaded()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    return now >= not_before() && now <= not_after();
}

auto certificate::is_expired() const -> bool {
    if (!impl_->is_loaded()) {
        return true;
    }

    auto now = std::chrono::system_clock::now();
    return now > not_after();
}

auto certificate::to_pem() const -> std::string {
    if (!impl_->is_loaded()) {
        return "";
    }

    bio_ptr bio(BIO_new(BIO_s_mem()));
    if (PEM_write_bio_X509(bio.get(), impl_->x509()) != 1) {
        return "";
    }

    char* data = nullptr;
    long len = BIO_get_mem_data(bio.get(), &data);
    return std::string(data, static_cast<size_t>(len));
}

auto certificate::to_der() const -> std::vector<std::uint8_t> {
    if (!impl_->is_loaded()) {
        return {};
    }

    unsigned char* data = nullptr;
    int len = i2d_X509(impl_->x509(), &data);
    if (len <= 0) {
        return {};
    }

    std::vector<std::uint8_t> result(data, data + len);
    OPENSSL_free(data);
    return result;
}

auto certificate::is_loaded() const noexcept -> bool {
    return impl_ && impl_->is_loaded();
}

auto certificate::impl() const noexcept -> const certificate_impl* {
    return impl_.get();
}

auto certificate::impl() noexcept -> certificate_impl* {
    return impl_.get();
}

// ============================================================================
// private_key implementation
// ============================================================================

private_key::private_key() : impl_(std::make_unique<private_key_impl>()) {}

private_key::private_key(private_key&& other) noexcept = default;

auto private_key::operator=(private_key&& other) noexcept -> private_key& = default;

private_key::~private_key() = default;

auto private_key::load_from_pem(std::string_view path, std::string_view password)
    -> kcenon::common::Result<private_key> {
    auto content_result = read_file(path);
    if (content_result.is_err()) {
        return kcenon::common::make_error<private_key>(
            content_result.error().code,
            content_result.error().message,
            "private_key");
    }

    return load_from_pem_string(content_result.value(), password);
}

auto private_key::load_from_pem_string(std::string_view pem_data, std::string_view password)
    -> kcenon::common::Result<private_key> {
    bio_ptr bio(BIO_new_mem_buf(pem_data.data(), static_cast<int>(pem_data.size())));
    if (!bio) {
        return kcenon::common::make_error<private_key>(
            2, "Failed to create BIO: " + get_openssl_error(), "private_key");
    }

    EVP_PKEY* pkey = nullptr;
    if (password.empty()) {
        pkey = PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);
    } else {
        pkey = PEM_read_bio_PrivateKey(
            bio.get(), nullptr, nullptr,
            const_cast<char*>(std::string(password).c_str()));
    }

    if (!pkey) {
        return kcenon::common::make_error<private_key>(
            3, "Failed to parse PEM private key: " + get_openssl_error(), "private_key");
    }

    private_key key;
    key.impl_ = std::make_unique<private_key_impl>(pkey);
    return key;
}

auto private_key::algorithm_name() const -> std::string {
    if (!impl_->is_loaded()) {
        return "";
    }

    int type = EVP_PKEY_base_id(impl_->pkey());
    switch (type) {
        case EVP_PKEY_RSA:
        case EVP_PKEY_RSA2:
            return "RSA";
        case EVP_PKEY_EC:
            return "EC";
        case EVP_PKEY_DSA:
            return "DSA";
        case EVP_PKEY_ED25519:
            return "ED25519";
        case EVP_PKEY_ED448:
            return "ED448";
        default:
            return "Unknown";
    }
}

auto private_key::key_size() const -> int {
    if (!impl_->is_loaded()) {
        return 0;
    }

    return EVP_PKEY_bits(impl_->pkey());
}

auto private_key::is_loaded() const noexcept -> bool {
    return impl_ && impl_->is_loaded();
}

auto private_key::impl() const noexcept -> const private_key_impl* {
    return impl_.get();
}

auto private_key::impl() noexcept -> private_key_impl* {
    return impl_.get();
}

// ============================================================================
// certificate_chain implementation
// ============================================================================

void certificate_chain::add(certificate cert) {
    certs_.push_back(std::move(cert));
}

auto certificate_chain::end_entity() const -> const certificate* {
    if (certs_.empty()) {
        return nullptr;
    }
    return &certs_.front();
}

auto certificate_chain::certificates() const -> const std::vector<certificate>& {
    return certs_;
}

auto certificate_chain::empty() const noexcept -> bool {
    return certs_.empty();
}

auto certificate_chain::size() const noexcept -> size_t {
    return certs_.size();
}

auto certificate_chain::load_from_pem(std::string_view path)
    -> kcenon::common::Result<certificate_chain> {
    auto content_result = read_file(path);
    if (content_result.is_err()) {
        return kcenon::common::make_error<certificate_chain>(
            content_result.error().code,
            content_result.error().message,
            "certificate_chain");
    }

    const std::string& content = content_result.value();
    certificate_chain chain;

    // Parse multiple certificates from PEM
    bio_ptr bio(BIO_new_mem_buf(content.data(), static_cast<int>(content.size())));
    if (!bio) {
        return kcenon::common::make_error<certificate_chain>(
            2, "Failed to create BIO: " + get_openssl_error(), "certificate_chain");
    }

    while (true) {
        X509* x509 = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
        if (!x509) {
            // Check if it's EOF or an error
            unsigned long err = ERR_peek_last_error();
            if (ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
                ERR_clear_error();
                break; // End of file
            }
            break; // Error or EOF
        }

        certificate cert;
        cert.impl_ = std::make_unique<certificate_impl>(x509);
        chain.add(std::move(cert));
    }

    if (chain.empty()) {
        return kcenon::common::make_error<certificate_chain>(
            3, "No certificates found in PEM file", "certificate_chain");
    }

    return chain;
}

} // namespace pacs::security
