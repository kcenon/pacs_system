// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "pacs/security/tls_policy.hpp"

namespace kcenon::pacs::security {

// ============================================================================
// tls_profile string conversion
// ============================================================================

std::string_view to_string(tls_profile profile) noexcept {
    switch (profile) {
        case tls_profile::bcp195_basic:
            return "BCP195-Basic";
        case tls_profile::bcp195_non_downgrading:
            return "BCP195-NonDowngrading";
        case tls_profile::bcp195_extended:
            return "BCP195-Extended";
    }
    return "Unknown";
}

std::optional<tls_profile> parse_tls_profile(std::string_view str) noexcept {
    if (str == "BCP195-Basic" || str == "basic")
        return tls_profile::bcp195_basic;
    if (str == "BCP195-NonDowngrading" || str == "non-downgrading")
        return tls_profile::bcp195_non_downgrading;
    if (str == "BCP195-Extended" || str == "extended")
        return tls_profile::bcp195_extended;
    return std::nullopt;
}

// ============================================================================
// tls_policy private constructor
// ============================================================================

tls_policy::tls_policy(tls_profile prof, uint16_t min_ver, uint16_t max_ver,
                       bool non_downgrade, cipher_suite_spec ciphers,
                       certificate_constraints certs)
    : profile_(prof),
      min_version_(min_ver),
      max_version_(max_ver),
      non_downgrading_(non_downgrade),
      ciphers_(std::move(ciphers)),
      cert_constraints_(std::move(certs)) {}

// ============================================================================
// Factory methods
// ============================================================================

tls_policy tls_policy::bcp195_basic_profile() {
    return {tls_profile::bcp195_basic,
            kTls12Version,
            kTls13Version,
            true,
            {std::string(kTls13Required), std::string(kTls12Recommended)},
            {2048, 256, 5, true}};
}

tls_policy tls_policy::bcp195_non_downgrading_profile() {
    return {tls_profile::bcp195_non_downgrading,
            kTls12Version,
            kTls13Version,
            true,
            {std::string(kTls13Required), std::string(kTls12Recommended)},
            {2048, 256, 5, true}};
}

tls_policy tls_policy::bcp195_extended_profile() {
    return {tls_profile::bcp195_extended,
            kTls13Version,
            kTls13Version,
            true,
            {std::string(kTls13Strict), ""},
            {3072, 256, 4, true}};
}

tls_policy tls_policy::from_profile(tls_profile profile) {
    switch (profile) {
        case tls_profile::bcp195_basic:
            return bcp195_basic_profile();
        case tls_profile::bcp195_non_downgrading:
            return bcp195_non_downgrading_profile();
        case tls_profile::bcp195_extended:
            return bcp195_extended_profile();
    }
    return bcp195_non_downgrading_profile();
}

// ============================================================================
// Property accessors
// ============================================================================

tls_profile tls_policy::profile() const noexcept {
    return profile_;
}

std::string_view tls_policy::profile_name() const noexcept {
    return to_string(profile_);
}

uint16_t tls_policy::min_protocol_version() const noexcept {
    return min_version_;
}

uint16_t tls_policy::max_protocol_version() const noexcept {
    return max_version_;
}

bool tls_policy::non_downgrading() const noexcept {
    return non_downgrading_;
}

const cipher_suite_spec& tls_policy::cipher_suites() const noexcept {
    return ciphers_;
}

const certificate_constraints& tls_policy::cert_constraints() const noexcept {
    return cert_constraints_;
}

// ============================================================================
// Validation
// ============================================================================

bool tls_policy::is_version_allowed(uint16_t version) const noexcept {
    return version >= min_version_ && version <= max_version_;
}

bool tls_policy::is_rsa_key_acceptable(uint16_t bits) const noexcept {
    return bits >= cert_constraints_.min_rsa_key_bits;
}

bool tls_policy::is_ecdsa_key_acceptable(uint16_t bits) const noexcept {
    return bits >= cert_constraints_.min_ecdsa_curve_bits;
}

std::string_view tls_policy::tls13_ciphersuites() const noexcept {
    return ciphers_.tls13_ciphers;
}

std::string_view tls_policy::tls12_ciphersuites() const noexcept {
    return ciphers_.tls12_ciphers;
}

// ============================================================================
// Utility functions
// ============================================================================

std::vector<tls_profile> available_tls_profiles() {
    return {tls_profile::bcp195_basic,
            tls_profile::bcp195_non_downgrading,
            tls_profile::bcp195_extended};
}

}  // namespace kcenon::pacs::security
