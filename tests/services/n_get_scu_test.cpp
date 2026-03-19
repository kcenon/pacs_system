/**
 * @file n_get_scu_test.cpp
 * @brief Unit tests for N-GET SCU service
 *
 * @see DICOM PS3.7 Section 10.1.2 - N-GET Service
 * @see Issue #720 - Implement N-GET SCP/SCU service
 */

#include <pacs/services/n_get_scu.hpp>
#include <pacs/services/mpps_scp.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/core/dicom_tag_constants.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;

// ============================================================================
// n_get_scu Construction Tests
// ============================================================================

TEST_CASE("n_get_scu construction", "[services][n_get][scu]") {
    SECTION("default construction succeeds") {
        n_get_scu scu;
        CHECK(scu.gets_performed() == 0);
    }

    SECTION("construction with config succeeds") {
        n_get_scu_config config;
        config.timeout = std::chrono::milliseconds{60000};

        n_get_scu scu(config);
        CHECK(scu.gets_performed() == 0);
    }

    SECTION("construction with nullptr logger succeeds") {
        n_get_scu scu(nullptr);
        CHECK(scu.gets_performed() == 0);
    }

    SECTION("construction with config and nullptr logger succeeds") {
        n_get_scu_config config;
        n_get_scu scu(config, nullptr);
        CHECK(scu.gets_performed() == 0);
    }
}

// ============================================================================
// n_get_scu_config Tests
// ============================================================================

TEST_CASE("n_get_scu_config defaults", "[services][n_get][scu]") {
    n_get_scu_config config;

    CHECK(config.timeout == std::chrono::milliseconds{30000});
}

TEST_CASE("n_get_scu_config customization", "[services][n_get][scu]") {
    n_get_scu_config config;
    config.timeout = std::chrono::milliseconds{60000};

    CHECK(config.timeout == std::chrono::milliseconds{60000});
}

// ============================================================================
// n_get_result Structure Tests
// ============================================================================

TEST_CASE("n_get_result structure", "[services][n_get][scu]") {
    n_get_result result;

    SECTION("default construction") {
        CHECK(result.status == 0);
        CHECK(result.error_comment.empty());
        CHECK(result.elapsed == std::chrono::milliseconds{0});
    }

    SECTION("is_success returns true for status 0x0000") {
        result.status = 0x0000;
        CHECK(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK_FALSE(result.is_error());
    }

    SECTION("is_warning returns true for 0xBxxx status") {
        result.status = 0xB000;
        CHECK_FALSE(result.is_success());
        CHECK(result.is_warning());
        CHECK_FALSE(result.is_error());

        result.status = 0xB123;
        CHECK(result.is_warning());

        result.status = 0xBFFF;
        CHECK(result.is_warning());
    }

    SECTION("is_error returns true for error status codes") {
        result.status = 0xC310;
        CHECK_FALSE(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK(result.is_error());

        result.status = 0xA700;  // Out of resources
        CHECK(result.is_error());

        result.status = 0x0110;  // Processing failure
        CHECK(result.is_error());
    }

    SECTION("can store elapsed time") {
        result.elapsed = std::chrono::milliseconds{150};
        CHECK(result.elapsed.count() == 150);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("n_get_scu statistics", "[services][n_get][scu]") {
    n_get_scu scu;

    SECTION("initial statistics are zero") {
        CHECK(scu.gets_performed() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scu.reset_statistics();
        CHECK(scu.gets_performed() == 0);
    }
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple n_get_scu instances are independent", "[services][n_get][scu]") {
    n_get_scu scu1;
    n_get_scu scu2;

    // Both should have zero statistics
    CHECK(scu1.gets_performed() == scu2.gets_performed());

    // Statistics should be independent
    scu1.reset_statistics();
    CHECK(scu1.gets_performed() == 0);
    CHECK(scu2.gets_performed() == 0);
}

// ============================================================================
// n_get_result Copy and Move Tests
// ============================================================================

TEST_CASE("n_get_result is copyable and movable", "[services][n_get][scu]") {
    n_get_result result;
    result.status = 0x0000;
    result.elapsed = std::chrono::milliseconds{100};
    result.error_comment = "test comment";

    SECTION("copy construction") {
        n_get_result copy = result;
        CHECK(copy.status == 0x0000);
        CHECK(copy.elapsed.count() == 100);
        CHECK(copy.error_comment == "test comment");
    }

    SECTION("move construction") {
        n_get_result moved = std::move(result);
        CHECK(moved.status == 0x0000);
        CHECK(moved.elapsed.count() == 100);
        CHECK(moved.error_comment == "test comment");
    }
}

// ============================================================================
// SOP Class UID Accessibility Test
// ============================================================================

TEST_CASE("mpps_sop_class_uid is accessible from n_get_scu", "[services][n_get][scu]") {
    // N-GET is commonly used with MPPS SOP Class for attribute retrieval
    CHECK(mpps_sop_class_uid == "1.2.840.10008.3.1.2.3.3");
}
