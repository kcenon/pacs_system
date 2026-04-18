// BSD 3-Clause License
// Copyright (c) 2021-2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file audit_log_cipher.cpp
 * @brief AES-256-GCM + HMAC-SHA256 envelope cipher for ATNA audit logs
 *
 * @see Issue #1102 (ISO 27799 §7.9 — Audit log confidentiality/integrity)
 */

#include "kcenon/pacs/security/audit_log_cipher.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifdef PACS_WITH_DIGITAL_SIGNATURES
    #include <openssl/evp.h>
    #include <openssl/hmac.h>
    #include <openssl/rand.h>
#endif

namespace kcenon::pacs::security {

// =============================================================================
// Small Encoding Helpers
// =============================================================================

namespace detail {

namespace {

constexpr char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Decoded value 0-63, or -1 on invalid.  '=' is handled separately.
int base64_value(char c) noexcept {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

std::string base64_encode(const std::uint8_t* data, std::size_t size) {
    std::string out;
    out.reserve(((size + 2) / 3) * 4);

    std::size_t i = 0;
    for (; i + 3 <= size; i += 3) {
        std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                          (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                           static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 6) & 0x3F]);
        out.push_back(kBase64Alphabet[v & 0x3F]);
    }

    const std::size_t rem = size - i;
    if (rem == 1) {
        std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                          (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

bool base64_decode(const std::string& in, std::vector<std::uint8_t>& out) {
    if (in.size() % 4 != 0) return false;
    out.clear();
    out.reserve((in.size() / 4) * 3);

    for (std::size_t i = 0; i < in.size(); i += 4) {
        const char c0 = in[i];
        const char c1 = in[i + 1];
        const char c2 = in[i + 2];
        const char c3 = in[i + 3];

        const int v0 = base64_value(c0);
        const int v1 = base64_value(c1);
        if (v0 < 0 || v1 < 0) return false;

        const std::uint32_t b0 = static_cast<std::uint32_t>(v0);
        const std::uint32_t b1 = static_cast<std::uint32_t>(v1);

        out.push_back(static_cast<std::uint8_t>((b0 << 2) | (b1 >> 4)));

        if (c2 == '=') {
            if (c3 != '=') return false;
            if (i + 4 != in.size()) return false;
            continue;
        }
        const int v2 = base64_value(c2);
        if (v2 < 0) return false;
        const std::uint32_t b2 = static_cast<std::uint32_t>(v2);

        out.push_back(static_cast<std::uint8_t>(((b1 & 0xF) << 4) | (b2 >> 2)));

        if (c3 == '=') {
            if (i + 4 != in.size()) return false;
            continue;
        }
        const int v3 = base64_value(c3);
        if (v3 < 0) return false;
        const std::uint32_t b3 = static_cast<std::uint32_t>(v3);
        out.push_back(static_cast<std::uint8_t>(((b2 & 0x3) << 6) | b3));
    }
    return true;
}

bool hex_decode(const std::string& in,
                std::uint8_t* out,
                std::size_t out_size) {
    if (in.size() != out_size * 2) return false;
    for (std::size_t i = 0; i < out_size; ++i) {
        const int hi = hex_value(in[2 * i]);
        const int lo = hex_value(in[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

bool constant_time_equal(const std::uint8_t* a,
                         const std::uint8_t* b,
                         std::size_t n) noexcept {
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < n; ++i) {
        diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

}  // namespace detail

// =============================================================================
// audit_cipher_key
// =============================================================================

void audit_cipher_key::wipe() noexcept {
    // volatile write to discourage the compiler from optimising it out
    volatile std::uint8_t* p = data_key.data();
    for (std::size_t i = 0; i < data_key.size(); ++i) p[i] = 0;
    volatile std::uint8_t* q = mac_key.data();
    for (std::size_t i = 0; i < mac_key.size(); ++i) q[i] = 0;
}

audit_cipher_key::~audit_cipher_key() {
    wipe();
}

// =============================================================================
// audit_key_manager
// =============================================================================

void audit_key_manager::register_key(audit_cipher_key key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string id = key.key_id;
    keys_[id] = std::move(key);
    if (active_id_.empty()) active_id_ = id;
}

VoidResult audit_key_manager::set_active(const std::string& key_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (keys_.find(key_id) == keys_.end()) {
        return pacs_void_error(
            kcenon::common::error::codes::common_errors::not_found,
            "audit key not registered: " + key_id);
    }
    active_id_ = key_id;
    return kcenon::common::ok();
}

const audit_cipher_key* audit_key_manager::find(
    const std::string& key_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = keys_.find(key_id);
    return it == keys_.end() ? nullptr : &it->second;
}

Result<audit_cipher_key> audit_key_manager::active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_id_.empty()) {
        return pacs_error<audit_cipher_key>(
            kcenon::common::error::codes::common_errors::not_found,
            "no active audit encryption key configured");
    }
    auto it = keys_.find(active_id_);
    if (it == keys_.end()) {
        return pacs_error<audit_cipher_key>(
            kcenon::common::error::codes::common_errors::internal_error,
            "active audit key missing from key store: " + active_id_);
    }
    return kcenon::common::Result<audit_cipher_key>(it->second);
}

void audit_key_manager::forget(const std::string& key_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    keys_.erase(key_id);
    if (active_id_ == key_id) active_id_.clear();
}

std::size_t audit_key_manager::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return keys_.size();
}

VoidResult audit_key_manager::decode_key(const std::string& encoded,
                                         std::uint8_t* out,
                                         std::size_t out_size) {
    if (encoded.empty() || out == nullptr || out_size == 0) {
        return pacs_void_error(
            kcenon::common::error::codes::common_errors::invalid_argument,
            "empty key encoding or invalid buffer");
    }

    // Hex first: encoded length must be exactly 2 * out_size
    if (encoded.size() == out_size * 2) {
        if (detail::hex_decode(encoded, out, out_size)) {
            return kcenon::common::ok();
        }
        // fall through — may still be base64 of the same length
    }

    std::vector<std::uint8_t> decoded;
    if (!detail::base64_decode(encoded, decoded)) {
        return pacs_void_error(
            kcenon::common::error::codes::common_errors::invalid_argument,
            "audit key is neither valid hex nor base64");
    }
    if (decoded.size() != out_size) {
        return pacs_void_error(
            kcenon::common::error::codes::common_errors::invalid_argument,
            "audit key has wrong length after decoding");
    }
    std::memcpy(out, decoded.data(), out_size);
    // wipe temporary copy
    volatile std::uint8_t* p = decoded.data();
    for (std::size_t i = 0; i < decoded.size(); ++i) p[i] = 0;
    return kcenon::common::ok();
}

namespace {

std::string getenv_str(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::string tok;
    for (char c : s) {
        if (c == ',' || c == ' ') {
            if (!tok.empty()) {
                out.push_back(tok);
                tok.clear();
            }
        } else {
            tok.push_back(c);
        }
    }
    if (!tok.empty()) out.push_back(tok);
    return out;
}

}  // namespace

VoidResult audit_key_manager::load_from_env() {
    const std::string active = getenv_str("PACS_AUDIT_LOG_ACTIVE_KEY_ID");
    if (active.empty()) {
        return pacs_void_error(
            kcenon::common::error::codes::common_errors::invalid_argument,
            "PACS_AUDIT_LOG_ACTIVE_KEY_ID is not set");
    }

    std::vector<std::string> ids;
    const std::string list = getenv_str("PACS_AUDIT_LOG_KEYS");
    if (list.empty()) {
        ids.push_back(active);
    } else {
        ids = split_csv(list);
        if (std::find(ids.begin(), ids.end(), active) == ids.end()) {
            ids.push_back(active);
        }
    }

    for (const auto& id : ids) {
        if (id.empty() || id.find('|') != std::string::npos) {
            return pacs_void_error(
                kcenon::common::error::codes::common_errors::invalid_argument,
                "invalid audit key id: '" + id + "'");
        }
        const std::string key_env = "PACS_AUDIT_LOG_KEY_" + id;
        const std::string mac_env = "PACS_AUDIT_LOG_MAC_" + id;
        const std::string key_val = getenv_str(key_env.c_str());
        const std::string mac_val = getenv_str(mac_env.c_str());
        if (key_val.empty() || mac_val.empty()) {
            return pacs_void_error(
                kcenon::common::error::codes::common_errors::invalid_argument,
                "missing env var for audit key '" + id + "'");
        }

        audit_cipher_key k;
        k.key_id = id;
        auto r1 = decode_key(key_val, k.data_key.data(), k.data_key.size());
        if (!r1) {
            return pacs_void_error(
                kcenon::common::error::codes::common_errors::invalid_argument,
                "failed to decode " + key_env);
        }
        auto r2 = decode_key(mac_val, k.mac_key.data(), k.mac_key.size());
        if (!r2) {
            return pacs_void_error(
                kcenon::common::error::codes::common_errors::invalid_argument,
                "failed to decode " + mac_env);
        }
        register_key(std::move(k));
    }

    return set_active(active);
}

// =============================================================================
// audit_log_cipher
// =============================================================================

audit_log_cipher::audit_log_cipher(audit_key_manager& keys) : keys_(keys) {}

bool audit_log_cipher::looks_encrypted(const std::string& line) noexcept {
    const std::string prefix = std::string(audit_cipher_format_tag) + "|" +
                               audit_cipher_format_version + "|";
    return line.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), line.begin());
}

#ifdef PACS_WITH_DIGITAL_SIGNATURES

namespace {

// RAII helper around EVP_CIPHER_CTX
struct evp_cipher_ctx_guard {
    EVP_CIPHER_CTX* ctx{nullptr};
    evp_cipher_ctx_guard() : ctx(EVP_CIPHER_CTX_new()) {}
    ~evp_cipher_ctx_guard() {
        if (ctx) EVP_CIPHER_CTX_free(ctx);
    }
    evp_cipher_ctx_guard(const evp_cipher_ctx_guard&) = delete;
    evp_cipher_ctx_guard& operator=(const evp_cipher_ctx_guard&) = delete;
};

bool hmac_sha256(const std::uint8_t* key,
                 std::size_t key_size,
                 const std::uint8_t* data,
                 std::size_t data_size,
                 std::uint8_t* out32) {
    unsigned int out_len = 0;
    const unsigned char* result = ::HMAC(
        EVP_sha256(),
        key, static_cast<int>(key_size),
        data, data_size,
        out32, &out_len);
    return result != nullptr && out_len == 32;
}

}  // namespace

Result<std::string> audit_log_cipher::encrypt_record(
    const std::string& plaintext) const {
    auto key_r = keys_.active();
    if (!key_r) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::not_found,
            "no active audit encryption key");
    }
    const audit_cipher_key& key = key_r.value();

    if (key.key_id.empty() ||
        key.key_id.find('|') != std::string::npos) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::invalid_argument,
            "active audit key has invalid id");
    }

    // 1. Fresh random IV
    std::array<std::uint8_t, audit_cipher_iv_bytes> iv{};
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "RAND_bytes failed for audit log IV");
    }

    // 2. AES-256-GCM encrypt
    evp_cipher_ctx_guard guard;
    if (!guard.ctx) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "EVP_CIPHER_CTX_new failed");
    }
    if (EVP_EncryptInit_ex(guard.ctx, EVP_aes_256_gcm(), nullptr,
                            key.data_key.data(), iv.data()) != 1) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "EVP_EncryptInit_ex failed");
    }

    std::vector<std::uint8_t> ct(plaintext.size() + 16);  // GCM no padding
    int out_len = 0;
    if (!plaintext.empty()) {
        if (EVP_EncryptUpdate(
                guard.ctx, ct.data(), &out_len,
                reinterpret_cast<const unsigned char*>(plaintext.data()),
                static_cast<int>(plaintext.size())) != 1) {
            return pacs_error<std::string>(
                kcenon::common::error::codes::common_errors::internal_error,
                "EVP_EncryptUpdate failed");
        }
    }
    int final_len = 0;
    if (EVP_EncryptFinal_ex(guard.ctx, ct.data() + out_len, &final_len) != 1) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "EVP_EncryptFinal_ex failed");
    }
    ct.resize(static_cast<std::size_t>(out_len + final_len));

    std::array<std::uint8_t, audit_cipher_tag_bytes> tag{};
    if (EVP_CIPHER_CTX_ctrl(guard.ctx, EVP_CTRL_GCM_GET_TAG,
                             static_cast<int>(tag.size()),
                             tag.data()) != 1) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "EVP_CTRL_GCM_GET_TAG failed");
    }

    // 3. Serialise prefix and inner fields
    const std::string iv_b64 = detail::base64_encode(iv.data(), iv.size());
    const std::string ct_b64 = detail::base64_encode(ct.data(), ct.size());
    const std::string tag_b64 = detail::base64_encode(tag.data(), tag.size());

    std::string prefix;
    prefix.reserve(64 + key.key_id.size() + iv_b64.size() +
                   ct_b64.size() + tag_b64.size());
    prefix.append(audit_cipher_format_tag);
    prefix.push_back('|');
    prefix.append(audit_cipher_format_version);
    prefix.push_back('|');
    prefix.append(key.key_id);
    prefix.push_back('|');
    prefix.append(iv_b64);
    prefix.push_back('|');
    prefix.append(ct_b64);
    prefix.push_back('|');
    prefix.append(tag_b64);

    // 4. HMAC-SHA256 over prefix, using the independent MAC key
    std::array<std::uint8_t, 32> mac{};
    if (!hmac_sha256(key.mac_key.data(), key.mac_key.size(),
                     reinterpret_cast<const std::uint8_t*>(prefix.data()),
                     prefix.size(),
                     mac.data())) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "HMAC-SHA256 failed");
    }
    const std::string mac_b64 = detail::base64_encode(mac.data(), mac.size());

    prefix.push_back('|');
    prefix.append(mac_b64);
    return kcenon::common::Result<std::string>(std::move(prefix));
}

namespace {

struct parsed_record {
    std::string key_id;
    std::vector<std::uint8_t> iv;
    std::vector<std::uint8_t> ciphertext;
    std::vector<std::uint8_t> tag;
    std::vector<std::uint8_t> mac;
    std::string mac_prefix;  // the exact bytes that were HMAC'd
};

// Parse "PACSAUDIT|v1|<id>|<iv>|<ct>|<tag>|<hmac>", trailing '\n' tolerated.
bool parse_record(std::string line, parsed_record& out) {
    while (!line.empty() &&
           (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }

    // Split on '|' into exactly 7 fields.
    std::array<std::string, 7> fields;
    std::size_t idx = 0;
    std::string tok;
    for (char c : line) {
        if (c == '|') {
            if (idx >= fields.size()) return false;
            fields[idx++] = std::move(tok);
            tok.clear();
        } else {
            tok.push_back(c);
        }
    }
    if (idx >= fields.size()) return false;
    fields[idx++] = std::move(tok);
    if (idx != fields.size()) return false;

    if (fields[0] != audit_cipher_format_tag) return false;
    if (fields[1] != audit_cipher_format_version) return false;
    if (fields[2].empty()) return false;

    if (!detail::base64_decode(fields[3], out.iv)) return false;
    if (!detail::base64_decode(fields[4], out.ciphertext)) return false;
    if (!detail::base64_decode(fields[5], out.tag)) return false;
    if (!detail::base64_decode(fields[6], out.mac)) return false;

    if (out.iv.size() != audit_cipher_iv_bytes) return false;
    if (out.tag.size() != audit_cipher_tag_bytes) return false;
    if (out.mac.size() != 32) return false;

    out.key_id = fields[2];
    // Reconstruct the MAC-covered prefix (everything before the final '|')
    out.mac_prefix.reserve(line.size());
    out.mac_prefix.append(fields[0]);
    out.mac_prefix.push_back('|');
    out.mac_prefix.append(fields[1]);
    out.mac_prefix.push_back('|');
    out.mac_prefix.append(fields[2]);
    out.mac_prefix.push_back('|');
    out.mac_prefix.append(fields[3]);
    out.mac_prefix.push_back('|');
    out.mac_prefix.append(fields[4]);
    out.mac_prefix.push_back('|');
    out.mac_prefix.append(fields[5]);
    return true;
}

}  // namespace

Result<std::string> audit_log_cipher::decrypt_record(
    const std::string& encoded) const {
    parsed_record rec;
    if (!parse_record(encoded, rec)) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::invalid_argument,
            "malformed encrypted audit record");
    }

    const audit_cipher_key* key = keys_.find(rec.key_id);
    if (key == nullptr) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::not_found,
            "audit record key id not registered: " + rec.key_id);
    }

    // 1. Verify HMAC first (fail fast before touching GCM)
    std::array<std::uint8_t, 32> expected_mac{};
    if (!hmac_sha256(key->mac_key.data(), key->mac_key.size(),
                     reinterpret_cast<const std::uint8_t*>(
                         rec.mac_prefix.data()),
                     rec.mac_prefix.size(),
                     expected_mac.data())) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "HMAC-SHA256 failed during verify");
    }
    if (!detail::constant_time_equal(expected_mac.data(),
                                     rec.mac.data(),
                                     expected_mac.size())) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::invalid_argument,
            "audit record HMAC verification failed");
    }

    // 2. AES-256-GCM decrypt
    evp_cipher_ctx_guard guard;
    if (!guard.ctx) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "EVP_CIPHER_CTX_new failed");
    }
    if (EVP_DecryptInit_ex(guard.ctx, EVP_aes_256_gcm(), nullptr,
                            key->data_key.data(), rec.iv.data()) != 1) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "EVP_DecryptInit_ex failed");
    }

    std::vector<std::uint8_t> pt(rec.ciphertext.size() + 16);
    int out_len = 0;
    if (!rec.ciphertext.empty()) {
        if (EVP_DecryptUpdate(guard.ctx, pt.data(), &out_len,
                               rec.ciphertext.data(),
                               static_cast<int>(rec.ciphertext.size())) != 1) {
            return pacs_error<std::string>(
                kcenon::common::error::codes::common_errors::invalid_argument,
                "EVP_DecryptUpdate failed");
        }
    }

    if (EVP_CIPHER_CTX_ctrl(guard.ctx, EVP_CTRL_GCM_SET_TAG,
                             static_cast<int>(rec.tag.size()),
                             rec.tag.data()) != 1) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::internal_error,
            "EVP_CTRL_GCM_SET_TAG failed");
    }

    int final_len = 0;
    if (EVP_DecryptFinal_ex(guard.ctx, pt.data() + out_len, &final_len) != 1) {
        return pacs_error<std::string>(
            kcenon::common::error::codes::common_errors::invalid_argument,
            "audit record GCM tag verification failed");
    }
    pt.resize(static_cast<std::size_t>(out_len + final_len));

    return kcenon::common::Result<std::string>(
        std::string(reinterpret_cast<const char*>(pt.data()), pt.size()));
}

VoidResult audit_log_cipher::verify_record(const std::string& encoded) const {
    auto r = decrypt_record(encoded);
    if (!r) return VoidResult(r.error());
    return kcenon::common::ok();
}

#else  // !PACS_WITH_DIGITAL_SIGNATURES

Result<std::string> audit_log_cipher::encrypt_record(
    const std::string& /*plaintext*/) const {
    return pacs_error<std::string>(
        kcenon::common::error::codes::common_errors::internal_error,
        "audit log encryption requires OpenSSL "
        "(build with PACS_WITH_OPENSSL=ON)");
}

Result<std::string> audit_log_cipher::decrypt_record(
    const std::string& /*encoded*/) const {
    return pacs_error<std::string>(
        kcenon::common::error::codes::common_errors::internal_error,
        "audit log decryption requires OpenSSL "
        "(build with PACS_WITH_OPENSSL=ON)");
}

VoidResult audit_log_cipher::verify_record(
    const std::string& /*encoded*/) const {
    return pacs_void_error(
        kcenon::common::error::codes::common_errors::internal_error,
        "audit log verification requires OpenSSL "
        "(build with PACS_WITH_OPENSSL=ON)");
}

#endif  // PACS_WITH_DIGITAL_SIGNATURES

}  // namespace kcenon::pacs::security
