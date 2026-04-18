// BSD 3-Clause License
// Copyright (c) 2021-2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file audit_log_cipher.h
 * @brief At-rest encryption for ATNA audit log records (ISO 27799 compliance)
 *
 * Provides AES-256-GCM authenticated encryption with HMAC-SHA256 integrity
 * signing for audit log records. Supports key rotation by embedding a key
 * identifier in every record so that historical records remain decryptable
 * after rotation.
 *
 * Record format (ASCII, single line, terminated by '\n'):
 *   PACSAUDIT|v1|<key_id>|<iv_b64>|<ciphertext_b64>|<tag_b64>|<hmac_b64>
 *
 * Where:
 *   - v1 is the format version tag
 *   - key_id is the identifier of the key used to encrypt the record
 *   - iv_b64 is the 96-bit AES-GCM initialisation vector (base64)
 *   - ciphertext_b64 is the AES-256-GCM ciphertext (base64)
 *   - tag_b64 is the 128-bit AES-GCM authentication tag (base64)
 *   - hmac_b64 is an HMAC-SHA256 over the preceding fields (base64), bound
 *     to a separate MAC key for defence-in-depth per ISO 27799 §7.9
 *
 * @see ISO 27799:2016 §7.9 — Audit logging (integrity & confidentiality)
 * @see HIPAA §164.312(b) — Audit controls
 * @see Issue #1102
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SECURITY_AUDIT_LOG_CIPHER_HPP
#define PACS_SECURITY_AUDIT_LOG_CIPHER_HPP

#include "kcenon/pacs/core/result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kcenon::pacs::security {

// =============================================================================
// Constants (ISO 27799 §7.9 mapped values)
// =============================================================================

/// Format version string embedded in every encrypted record
inline constexpr const char* audit_cipher_format_tag = "PACSAUDIT";
inline constexpr const char* audit_cipher_format_version = "v1";

/// AES-256 key length in bytes
inline constexpr std::size_t audit_cipher_key_bytes = 32;

/// HMAC-SHA256 key length in bytes (equal to block size for full strength)
inline constexpr std::size_t audit_cipher_mac_key_bytes = 32;

/// GCM IV length in bytes (NIST SP 800-38D recommended 96 bits)
inline constexpr std::size_t audit_cipher_iv_bytes = 12;

/// GCM authentication tag length in bytes
inline constexpr std::size_t audit_cipher_tag_bytes = 16;

// =============================================================================
// Key Material
// =============================================================================

/**
 * @brief A versioned symmetric key pair (data encryption + MAC)
 *
 * The encryption key is used for AES-256-GCM confidentiality; the separate
 * MAC key provides an independent integrity check so that a compromise of
 * the GCM tag alone does not imply forgery of records in storage.
 */
struct audit_cipher_key {
    /// Stable identifier (ASCII, non-empty, no '|' character)
    std::string key_id;

    /// 256-bit AES data encryption key
    std::array<std::uint8_t, audit_cipher_key_bytes> data_key{};

    /// 256-bit HMAC-SHA256 key
    std::array<std::uint8_t, audit_cipher_mac_key_bytes> mac_key{};

    /// Secure zero helper (wipes key material on destruction)
    void wipe() noexcept;

    ~audit_cipher_key();
    audit_cipher_key() = default;
    audit_cipher_key(const audit_cipher_key&) = default;
    audit_cipher_key(audit_cipher_key&&) noexcept = default;
    audit_cipher_key& operator=(const audit_cipher_key&) = default;
    audit_cipher_key& operator=(audit_cipher_key&&) noexcept = default;
};

// =============================================================================
// Key Manager
// =============================================================================

/**
 * @brief Holds the active encryption key and any retired keys for
 *        decryption of historical records.
 *
 * A single "active" key is used for all new writes. Any number of
 * additional keys (including the former active key) may be registered so
 * that previously-written records remain verifiable after rotation.
 *
 * Thread-safety: all public operations are internally synchronised.
 *
 * Key sourcing: in production, populate via `load_from_env()` which reads
 * `PACS_AUDIT_LOG_ACTIVE_KEY_ID` plus `PACS_AUDIT_LOG_KEY_<id>` and
 * `PACS_AUDIT_LOG_MAC_<id>` (both hex- or base64-encoded). NEVER hard-code
 * keys in source. Rotation procedure: provision a new `<id>`, update
 * `PACS_AUDIT_LOG_ACTIVE_KEY_ID`, keep the previous key registered for as
 * long as historical records are retained.
 */
class audit_key_manager {
public:
    audit_key_manager() = default;

    /// Register a key (idempotent by key_id; overwrites existing entry)
    void register_key(audit_cipher_key key);

    /// Mark a previously-registered key as the active write key
    [[nodiscard]] VoidResult set_active(const std::string& key_id);

    /// Look up a key by identifier (returns nullptr if unknown)
    [[nodiscard]] const audit_cipher_key* find(const std::string& key_id) const;

    /// Retrieve the active write key (error if none configured)
    [[nodiscard]] Result<audit_cipher_key> active() const;

    /// Drop a key by id (does nothing if it was not registered)
    void forget(const std::string& key_id);

    /// Number of registered keys
    [[nodiscard]] std::size_t size() const;

    /**
     * @brief Load keys from environment variables
     *
     * Reads:
     *   - `PACS_AUDIT_LOG_ACTIVE_KEY_ID`  — the active write key id
     *   - `PACS_AUDIT_LOG_KEYS`           — comma-separated list of ids
     *                                       to load (optional; defaults
     *                                       to the active id only)
     *   - `PACS_AUDIT_LOG_KEY_<id>`       — 32-byte data key, hex or base64
     *   - `PACS_AUDIT_LOG_MAC_<id>`       — 32-byte MAC key, hex or base64
     *
     * @return VoidResult reporting the first configuration error, if any
     */
    [[nodiscard]] VoidResult load_from_env();

    /**
     * @brief Decode a hex or base64 key string into a fixed-size buffer
     *
     * Accepts:
     *   - lowercase/uppercase hexadecimal of exactly 2*N characters, or
     *   - base64 (standard alphabet, padding required) of N bytes
     *
     * @param encoded the encoded string
     * @param out destination buffer; must be exactly `N` bytes
     * @return VoidResult success or decode error
     */
    [[nodiscard]] static VoidResult decode_key(
        const std::string& encoded,
        std::uint8_t* out,
        std::size_t out_size);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, audit_cipher_key> keys_;
    std::string active_id_;
};

// =============================================================================
// Cipher
// =============================================================================

/**
 * @brief Encrypts and decrypts single audit log records.
 *
 * Instances are stateless with respect to plaintext content; they hold a
 * reference to a key manager and generate a fresh random IV per record.
 *
 * Thread-safety: all public methods are safe to call from multiple threads
 * provided the referenced `audit_key_manager` outlives the cipher.
 */
class audit_log_cipher {
public:
    /**
     * @brief Construct a cipher backed by the given key manager
     * @param keys key store (must outlive this cipher)
     */
    explicit audit_log_cipher(audit_key_manager& keys);

    /**
     * @brief Encrypt a plaintext audit record
     *
     * Uses the currently-active key. A fresh 96-bit IV is drawn for every
     * call from the OS CSPRNG. The returned string is the single-line
     * serialised record (without trailing newline).
     *
     * @param plaintext arbitrary byte string (typically an RFC 3881 XML
     *                  audit message)
     * @return encoded record or an error if no active key is configured or
     *         the underlying crypto provider fails
     */
    [[nodiscard]] Result<std::string> encrypt_record(
        const std::string& plaintext) const;

    /**
     * @brief Decrypt a previously encoded record
     *
     * Looks up the key by the embedded `key_id`. If the key is unknown,
     * returns an error. If either the HMAC or the GCM tag fails to verify,
     * returns a distinct integrity error (never a partial plaintext).
     *
     * @param encoded single-line record produced by `encrypt_record`
     *                (trailing '\n' is tolerated)
     * @return plaintext on success
     */
    [[nodiscard]] Result<std::string> decrypt_record(
        const std::string& encoded) const;

    /**
     * @brief Verify a record's authenticity without returning plaintext
     * @return VoidResult::ok if both HMAC and GCM tag verify, error otherwise
     */
    [[nodiscard]] VoidResult verify_record(const std::string& encoded) const;

    /// Check whether a line appears to be an encrypted audit record
    [[nodiscard]] static bool looks_encrypted(const std::string& line) noexcept;

private:
    audit_key_manager& keys_;
};

// =============================================================================
// Small Encoding Helpers (exposed for tests)
// =============================================================================

namespace detail {

/// Standard base64 encode (no line breaks)
[[nodiscard]] std::string base64_encode(const std::uint8_t* data,
                                        std::size_t size);

/// Standard base64 decode; returns false on malformed input
[[nodiscard]] bool base64_decode(const std::string& in,
                                 std::vector<std::uint8_t>& out);

/// Hex decode (lowercase or uppercase); returns false on malformed input
[[nodiscard]] bool hex_decode(const std::string& in,
                              std::uint8_t* out,
                              std::size_t out_size);

/// Constant-time equality on byte buffers of equal length
[[nodiscard]] bool constant_time_equal(const std::uint8_t* a,
                                       const std::uint8_t* b,
                                       std::size_t n) noexcept;

}  // namespace detail

}  // namespace kcenon::pacs::security

#endif  // PACS_SECURITY_AUDIT_LOG_CIPHER_HPP
