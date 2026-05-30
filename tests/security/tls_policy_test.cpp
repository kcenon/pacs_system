#include <catch2/catch_test_macros.hpp>

#include "kcenon/pacs/security/tls_policy.h"

using namespace kcenon::pacs::security;

// ============================================================================
// tls_profile enum tests
// ============================================================================

TEST_CASE("tls_profile to_string", "[security][tls]") {
    CHECK(to_string(tls_profile::bcp195_basic) == "BCP195-Basic");
    CHECK(to_string(tls_profile::bcp195_non_downgrading) == "BCP195-NonDowngrading");
    CHECK(to_string(tls_profile::bcp195_extended) == "BCP195-Extended");
}

TEST_CASE("tls_profile parse_tls_profile", "[security][tls]") {
    SECTION("Full names") {
        auto basic = parse_tls_profile("BCP195-Basic");
        REQUIRE(basic.has_value());
        CHECK(*basic == tls_profile::bcp195_basic);

        auto nd = parse_tls_profile("BCP195-NonDowngrading");
        REQUIRE(nd.has_value());
        CHECK(*nd == tls_profile::bcp195_non_downgrading);

        auto ext = parse_tls_profile("BCP195-Extended");
        REQUIRE(ext.has_value());
        CHECK(*ext == tls_profile::bcp195_extended);
    }

    SECTION("Short names") {
        auto basic = parse_tls_profile("basic");
        REQUIRE(basic.has_value());
        CHECK(*basic == tls_profile::bcp195_basic);

        auto nd = parse_tls_profile("non-downgrading");
        REQUIRE(nd.has_value());
        CHECK(*nd == tls_profile::bcp195_non_downgrading);

        auto ext = parse_tls_profile("extended");
        REQUIRE(ext.has_value());
        CHECK(*ext == tls_profile::bcp195_extended);
    }

    SECTION("Invalid names") {
        CHECK_FALSE(parse_tls_profile("invalid").has_value());
        CHECK_FALSE(parse_tls_profile("").has_value());
        CHECK_FALSE(parse_tls_profile("BASIC").has_value());
    }
}

// ============================================================================
// BCP 195 Basic Profile tests
// ============================================================================

TEST_CASE("tls_policy BCP 195 basic profile", "[security][tls]") {
    auto policy = tls_policy::bcp195_basic_profile();

    SECTION("Profile properties") {
        CHECK(policy.profile() == tls_profile::bcp195_basic);
        CHECK(policy.profile_name() == "BCP195-Basic");
        CHECK(policy.non_downgrading());
    }

    SECTION("Protocol version requirements") {
        CHECK(policy.min_protocol_version() == tls_policy::kTls12Version);
        CHECK(policy.max_protocol_version() == tls_policy::kTls13Version);

        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls10Version));
        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls11Version));
        CHECK(policy.is_version_allowed(tls_policy::kTls12Version));
        CHECK(policy.is_version_allowed(tls_policy::kTls13Version));
    }

    SECTION("Cipher suites are configured") {
        CHECK_FALSE(policy.tls13_ciphersuites().empty());
        CHECK_FALSE(policy.tls12_ciphersuites().empty());

        // Must contain required TLS 1.3 ciphers
        auto tls13 = policy.tls13_ciphersuites();
        CHECK(tls13.find("TLS_AES_256_GCM_SHA384") != std::string_view::npos);
        CHECK(tls13.find("TLS_AES_128_GCM_SHA256") != std::string_view::npos);

        // Must contain ECDHE-based TLS 1.2 ciphers
        auto tls12 = policy.tls12_ciphersuites();
        CHECK(tls12.find("ECDHE") != std::string_view::npos);
        CHECK(tls12.find("GCM") != std::string_view::npos);
    }

    SECTION("Certificate constraints") {
        CHECK(policy.cert_constraints().min_rsa_key_bits == 2048);
        CHECK(policy.cert_constraints().min_ecdsa_curve_bits == 256);
        CHECK(policy.cert_constraints().require_peer_verification);

        CHECK(policy.is_rsa_key_acceptable(2048));
        CHECK(policy.is_rsa_key_acceptable(4096));
        CHECK_FALSE(policy.is_rsa_key_acceptable(1024));

        CHECK(policy.is_ecdsa_key_acceptable(256));
        CHECK(policy.is_ecdsa_key_acceptable(384));
        CHECK_FALSE(policy.is_ecdsa_key_acceptable(128));
    }
}

// ============================================================================
// BCP 195 Non-Downgrading Profile tests
// ============================================================================

TEST_CASE("tls_policy BCP 195 non-downgrading profile", "[security][tls]") {
    auto policy = tls_policy::bcp195_non_downgrading_profile();

    SECTION("Profile properties") {
        CHECK(policy.profile() == tls_profile::bcp195_non_downgrading);
        CHECK(policy.profile_name() == "BCP195-NonDowngrading");
        CHECK(policy.non_downgrading());
    }

    SECTION("Protocol version requirements") {
        CHECK(policy.min_protocol_version() == tls_policy::kTls12Version);
        CHECK(policy.max_protocol_version() == tls_policy::kTls13Version);

        // TLS 1.0 and 1.1 must be rejected
        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls10Version));
        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls11Version));

        // TLS 1.2 and 1.3 must be accepted
        CHECK(policy.is_version_allowed(tls_policy::kTls12Version));
        CHECK(policy.is_version_allowed(tls_policy::kTls13Version));
    }

    SECTION("Non-downgrading flag is set") {
        CHECK(policy.non_downgrading());
    }

    SECTION("Cipher suites match BCP 195 requirements") {
        auto tls13 = policy.tls13_ciphersuites();
        CHECK(tls13.find("TLS_AES_256_GCM_SHA384") != std::string_view::npos);
        CHECK(tls13.find("TLS_AES_128_GCM_SHA256") != std::string_view::npos);
        CHECK(tls13.find("TLS_CHACHA20_POLY1305_SHA256") != std::string_view::npos);

        auto tls12 = policy.tls12_ciphersuites();
        CHECK(tls12.find("ECDHE-ECDSA-AES256-GCM-SHA384") != std::string_view::npos);
        CHECK(tls12.find("ECDHE-RSA-AES256-GCM-SHA384") != std::string_view::npos);
        CHECK(tls12.find("ECDHE-RSA-AES128-GCM-SHA256") != std::string_view::npos);
    }

    SECTION("Certificate constraints match DICOM PS3.15") {
        CHECK(policy.cert_constraints().min_rsa_key_bits == 2048);
        CHECK(policy.cert_constraints().min_ecdsa_curve_bits == 256);
        CHECK(policy.cert_constraints().max_chain_depth == 5);
    }
}

// ============================================================================
// BCP 195 Extended Profile tests
// ============================================================================

TEST_CASE("tls_policy BCP 195 extended profile", "[security][tls]") {
    auto policy = tls_policy::bcp195_extended_profile();

    SECTION("Profile properties") {
        CHECK(policy.profile() == tls_profile::bcp195_extended);
        CHECK(policy.profile_name() == "BCP195-Extended");
        CHECK(policy.non_downgrading());
    }

    SECTION("TLS 1.3 only") {
        CHECK(policy.min_protocol_version() == tls_policy::kTls13Version);
        CHECK(policy.max_protocol_version() == tls_policy::kTls13Version);

        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls10Version));
        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls11Version));
        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls12Version));
        CHECK(policy.is_version_allowed(tls_policy::kTls13Version));
    }

    SECTION("Strict cipher suites") {
        auto tls13 = policy.tls13_ciphersuites();
        CHECK(tls13.find("TLS_AES_256_GCM_SHA384") != std::string_view::npos);
        CHECK(tls13.find("TLS_CHACHA20_POLY1305_SHA256") != std::string_view::npos);

        // No TLS 1.2 ciphers in extended profile
        CHECK(policy.tls12_ciphersuites().empty());
    }

    SECTION("Stricter certificate requirements") {
        CHECK(policy.cert_constraints().min_rsa_key_bits == 3072);
        CHECK(policy.cert_constraints().min_ecdsa_curve_bits == 256);
        CHECK(policy.cert_constraints().max_chain_depth == 4);

        CHECK_FALSE(policy.is_rsa_key_acceptable(2048));
        CHECK(policy.is_rsa_key_acceptable(3072));
        CHECK(policy.is_rsa_key_acceptable(4096));
    }
}

// ============================================================================
// from_profile factory method
// ============================================================================

TEST_CASE("tls_policy from_profile", "[security][tls]") {
    SECTION("Basic profile") {
        auto policy = tls_policy::from_profile(tls_profile::bcp195_basic);
        CHECK(policy.profile() == tls_profile::bcp195_basic);
        CHECK(policy.non_downgrading());
    }

    SECTION("Non-downgrading profile") {
        auto policy = tls_policy::from_profile(tls_profile::bcp195_non_downgrading);
        CHECK(policy.profile() == tls_profile::bcp195_non_downgrading);
        CHECK(policy.non_downgrading());
    }

    SECTION("Extended profile") {
        auto policy = tls_policy::from_profile(tls_profile::bcp195_extended);
        CHECK(policy.profile() == tls_profile::bcp195_extended);
        CHECK(policy.min_protocol_version() == tls_policy::kTls13Version);
    }
}

// ============================================================================
// Protocol version constants
// ============================================================================

TEST_CASE("tls_policy protocol version constants", "[security][tls]") {
    // Verify constants match OpenSSL TLS version encoding
    CHECK(tls_policy::kTls10Version == 0x0301);
    CHECK(tls_policy::kTls11Version == 0x0302);
    CHECK(tls_policy::kTls12Version == 0x0303);
    CHECK(tls_policy::kTls13Version == 0x0304);

    // Verify ordering
    CHECK(tls_policy::kTls10Version < tls_policy::kTls11Version);
    CHECK(tls_policy::kTls11Version < tls_policy::kTls12Version);
    CHECK(tls_policy::kTls12Version < tls_policy::kTls13Version);
}

// ============================================================================
// Cipher suite constants
// ============================================================================

TEST_CASE("tls_policy cipher suite constants", "[security][tls]") {
    SECTION("TLS 1.3 required ciphers") {
        auto ciphers = tls_policy::kTls13Required;
        CHECK(ciphers.find("TLS_AES_256_GCM_SHA384") != std::string_view::npos);
        CHECK(ciphers.find("TLS_AES_128_GCM_SHA256") != std::string_view::npos);
        CHECK(ciphers.find("TLS_CHACHA20_POLY1305_SHA256") != std::string_view::npos);
    }

    SECTION("TLS 1.3 strict ciphers") {
        auto ciphers = tls_policy::kTls13Strict;
        CHECK(ciphers.find("TLS_AES_256_GCM_SHA384") != std::string_view::npos);
        // AES-128 not in strict profile
        CHECK(ciphers.find("TLS_AES_128_GCM_SHA256") == std::string_view::npos);
    }

    SECTION("TLS 1.2 recommended ciphers use ECDHE only") {
        auto ciphers = tls_policy::kTls12Recommended;
        // All entries must use ECDHE (forward secrecy)
        CHECK(ciphers.find("ECDHE") != std::string_view::npos);
        // No DHE-only (non-ECDHE) or RSA-only key exchange
        CHECK(ciphers.find(":DHE-RSA-AES") == std::string_view::npos);
        // All must use GCM (AEAD)
        CHECK(ciphers.find("GCM") != std::string_view::npos);
        // No CBC mode
        CHECK(ciphers.find("CBC") == std::string_view::npos);
    }
}

// ============================================================================
// available_tls_profiles
// ============================================================================

TEST_CASE("available_tls_profiles", "[security][tls]") {
    auto profiles = available_tls_profiles();

    CHECK(profiles.size() == 3);
    CHECK(profiles[0] == tls_profile::bcp195_basic);
    CHECK(profiles[1] == tls_profile::bcp195_non_downgrading);
    CHECK(profiles[2] == tls_profile::bcp195_extended);
}

// ============================================================================
// Version rejection tests (BCP 195 compliance)
// ============================================================================

TEST_CASE("All profiles reject TLS 1.0 and 1.1", "[security][tls]") {
    auto profiles = available_tls_profiles();

    for (auto prof : profiles) {
        auto policy = tls_policy::from_profile(prof);

        INFO("Profile: " << to_string(prof));
        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls10Version));
        CHECK_FALSE(policy.is_version_allowed(tls_policy::kTls11Version));
    }
}

TEST_CASE("All profiles require peer verification", "[security][tls]") {
    auto profiles = available_tls_profiles();

    for (auto prof : profiles) {
        auto policy = tls_policy::from_profile(prof);

        INFO("Profile: " << to_string(prof));
        CHECK(policy.cert_constraints().require_peer_verification);
    }
}
