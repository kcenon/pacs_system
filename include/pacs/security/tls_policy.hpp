// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file tls_policy.hpp
 * @brief TLS security policy for BCP 195 compliance (DICOM PS3.15)
 *
 * Defines TLS policy profiles that enforce cipher suite restrictions,
 * protocol version requirements, and certificate constraints as
 * specified by BCP 195 (RFC 9325) and DICOM PS3.15.
 *
 * @see DICOM PS3.15 -- Security and System Management Profiles
 * @see RFC 9325 -- Recommendations for Secure Use of TLS and DTLS
 * @see RFC 8446 -- TLS 1.3
 * @see RFC 8996 -- Deprecating TLS 1.0 and TLS 1.1
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::security {

/**
 * @brief TLS policy profile levels
 *
 * Predefined security profiles aligned with DICOM PS3.15 and BCP 195.
 */
enum class tls_profile {
    /// BCP 195 basic profile: TLS 1.2 minimum, standard cipher suites
    bcp195_basic,

    /// BCP 195 non-downgrading profile: TLS 1.2+ with no downgrade
    /// This is the DICOM PS3.15 recommended profile
    bcp195_non_downgrading,

    /// Extended profile: TLS 1.3 only, strictest cipher suites
    bcp195_extended
};

/**
 * @brief Convert TLS profile to string
 */
[[nodiscard]] std::string_view to_string(tls_profile profile) noexcept;

/**
 * @brief Parse TLS profile from string
 */
[[nodiscard]] std::optional<tls_profile> parse_tls_profile(
    std::string_view str) noexcept;

/**
 * @brief TLS cipher suite specification
 */
struct cipher_suite_spec {
    /// TLS 1.3 cipher suites (OpenSSL ciphersuites string format)
    std::string tls13_ciphers;

    /// TLS 1.2 cipher suites (OpenSSL cipher string format)
    std::string tls12_ciphers;
};

/**
 * @brief Certificate validation constraints
 */
struct certificate_constraints {
    /// Minimum RSA key size in bits
    uint16_t min_rsa_key_bits{2048};

    /// Minimum ECDSA curve size (P-256 = 256, P-384 = 384)
    uint16_t min_ecdsa_curve_bits{256};

    /// Maximum certificate chain depth
    uint8_t max_chain_depth{5};

    /// Require peer certificate verification
    bool require_peer_verification{true};
};

/**
 * @brief TLS security policy configuration
 *
 * Encapsulates all TLS settings for a given security profile.
 * Use the factory methods to create standard profiles.
 */
class tls_policy {
public:
    /**
     * @brief Create a BCP 195 basic profile policy
     *
     * - Minimum TLS 1.2
     * - Standard BCP 195 cipher suites
     * - RSA >= 2048, ECDSA >= P-256
     */
    [[nodiscard]] static tls_policy bcp195_basic_profile();

    /**
     * @brief Create a BCP 195 non-downgrading profile policy
     *
     * - Minimum TLS 1.2, prefers TLS 1.3
     * - No protocol downgrade allowed
     * - Only BCP 195-compliant cipher suites
     * - RSA >= 2048, ECDSA >= P-256
     *
     * This is the recommended DICOM PS3.15 profile.
     */
    [[nodiscard]] static tls_policy bcp195_non_downgrading_profile();

    /**
     * @brief Create an extended profile (TLS 1.3 only)
     *
     * - TLS 1.3 only
     * - Strictest cipher suites
     * - RSA >= 3072, ECDSA >= P-256
     */
    [[nodiscard]] static tls_policy bcp195_extended_profile();

    /**
     * @brief Create a policy from a named profile
     */
    [[nodiscard]] static tls_policy from_profile(tls_profile profile);

    /// @name Policy Properties
    /// @{

    [[nodiscard]] tls_profile profile() const noexcept;
    [[nodiscard]] std::string_view profile_name() const noexcept;

    [[nodiscard]] uint16_t min_protocol_version() const noexcept;
    [[nodiscard]] uint16_t max_protocol_version() const noexcept;

    [[nodiscard]] bool non_downgrading() const noexcept;

    [[nodiscard]] const cipher_suite_spec& cipher_suites() const noexcept;
    [[nodiscard]] const certificate_constraints& cert_constraints() const noexcept;

    /// @}

    /// @name Validation
    /// @{

    /**
     * @brief Check if a TLS version is allowed by this policy
     * @param version OpenSSL version constant (e.g., TLS1_2_VERSION)
     */
    [[nodiscard]] bool is_version_allowed(uint16_t version) const noexcept;

    /**
     * @brief Check if an RSA key size meets minimum requirements
     * @param bits RSA key size in bits
     */
    [[nodiscard]] bool is_rsa_key_acceptable(uint16_t bits) const noexcept;

    /**
     * @brief Check if an ECDSA curve size meets minimum requirements
     * @param bits ECDSA curve size in bits
     */
    [[nodiscard]] bool is_ecdsa_key_acceptable(uint16_t bits) const noexcept;

    /**
     * @brief Get the TLS 1.3 cipher suites string for OpenSSL
     */
    [[nodiscard]] std::string_view tls13_ciphersuites() const noexcept;

    /**
     * @brief Get the TLS 1.2 cipher suites string for OpenSSL
     */
    [[nodiscard]] std::string_view tls12_ciphersuites() const noexcept;

    /// @}

    /// @name Predefined Cipher Suite Strings
    /// @{

    /// TLS 1.3 required cipher suites (BCP 195)
    static constexpr std::string_view kTls13Required =
        "TLS_AES_256_GCM_SHA384:"
        "TLS_AES_128_GCM_SHA256:"
        "TLS_CHACHA20_POLY1305_SHA256";

    /// TLS 1.3 strict cipher suites (extended profile)
    static constexpr std::string_view kTls13Strict =
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256";

    /// TLS 1.2 BCP 195 recommended cipher suites
    static constexpr std::string_view kTls12Recommended =
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256";

    /// @}

    /// @name Protocol Version Constants
    /// @{

    /// OpenSSL TLS version constants for reference
    static constexpr uint16_t kTls10Version = 0x0301;
    static constexpr uint16_t kTls11Version = 0x0302;
    static constexpr uint16_t kTls12Version = 0x0303;
    static constexpr uint16_t kTls13Version = 0x0304;

    /// @}

private:
    tls_policy(tls_profile prof, uint16_t min_ver, uint16_t max_ver,
               bool non_downgrade, cipher_suite_spec ciphers,
               certificate_constraints certs);

    tls_profile profile_;
    uint16_t min_version_;
    uint16_t max_version_;
    bool non_downgrading_;
    cipher_suite_spec ciphers_;
    certificate_constraints cert_constraints_;
};

/**
 * @brief Get a list of all available TLS profiles
 */
[[nodiscard]] std::vector<tls_profile> available_tls_profiles();

}  // namespace pacs::security
