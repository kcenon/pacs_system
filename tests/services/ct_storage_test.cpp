/**
 * @file ct_storage_test.cpp
 * @brief Unit tests for CT Image Storage SOP Class utilities
 *
 * @see Issue #717 - Add CT Image IOD Validator
 */

#include <pacs/services/sop_classes/ct_storage.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services::sop_classes;

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("ct_image_storage_uid is correct", "[services][ct][storage]") {
    CHECK(ct_image_storage_uid == "1.2.840.10008.5.1.4.1.1.2");
}

TEST_CASE("enhanced_ct_image_storage_uid is correct", "[services][ct][storage]") {
    CHECK(enhanced_ct_image_storage_uid == "1.2.840.10008.5.1.4.1.1.2.1");
}

TEST_CASE("ct_for_processing_image_storage_uid is correct",
          "[services][ct][storage]") {
    CHECK(ct_for_processing_image_storage_uid == "1.2.840.10008.5.1.4.1.1.2.2");
}

// ============================================================================
// is_ct_storage_sop_class Tests
// ============================================================================

TEST_CASE("is_ct_storage_sop_class identifies CT SOP Classes",
          "[services][ct][storage]") {
    SECTION("standard CT Image Storage") {
        CHECK(is_ct_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));
    }

    SECTION("Enhanced CT Image Storage") {
        CHECK(is_ct_storage_sop_class("1.2.840.10008.5.1.4.1.1.2.1"));
    }

    SECTION("CT For Processing Image Storage") {
        CHECK(is_ct_storage_sop_class("1.2.840.10008.5.1.4.1.1.2.2"));
    }

    SECTION("non-CT SOP Class - US") {
        CHECK_FALSE(is_ct_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));
    }

    SECTION("non-CT SOP Class - MR") {
        CHECK_FALSE(is_ct_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));
    }

    SECTION("empty string") {
        CHECK_FALSE(is_ct_storage_sop_class(""));
    }
}

// ============================================================================
// get_ct_storage_sop_classes Tests
// ============================================================================

TEST_CASE("get_ct_storage_sop_classes returns all CT UIDs",
          "[services][ct][storage]") {
    auto uids = get_ct_storage_sop_classes();
    CHECK(uids.size() == 3);
    CHECK(uids[0] == "1.2.840.10008.5.1.4.1.1.2");
    CHECK(uids[1] == "1.2.840.10008.5.1.4.1.1.2.1");
    CHECK(uids[2] == "1.2.840.10008.5.1.4.1.1.2.2");
}

// ============================================================================
// is_valid_ct_photometric Tests
// ============================================================================

TEST_CASE("is_valid_ct_photometric checks grayscale values",
          "[services][ct][storage]") {
    SECTION("MONOCHROME1 is valid") {
        CHECK(is_valid_ct_photometric("MONOCHROME1"));
    }

    SECTION("MONOCHROME2 is valid") {
        CHECK(is_valid_ct_photometric("MONOCHROME2"));
    }

    SECTION("RGB is invalid for CT") {
        CHECK_FALSE(is_valid_ct_photometric("RGB"));
    }

    SECTION("PALETTE COLOR is invalid for CT") {
        CHECK_FALSE(is_valid_ct_photometric("PALETTE COLOR"));
    }

    SECTION("empty string is invalid") {
        CHECK_FALSE(is_valid_ct_photometric(""));
    }
}
