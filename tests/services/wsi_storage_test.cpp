/**
 * @file wsi_storage_test.cpp
 * @brief Unit tests for VL Whole Slide Microscopy Image Storage SOP Class
 *
 * @see Issue #825 - Add WSI SOP Class registration and storage definition
 */

#include <kcenon/pacs/services/sop_classes/wsi_storage.h>
#include <kcenon/pacs/services/sop_class_registry.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::services::sop_classes;

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("wsi_image_storage_uid is correct", "[services][wsi][storage]") {
    CHECK(wsi_image_storage_uid == "1.2.840.10008.5.1.4.1.1.77.1.6");
}

// ============================================================================
// is_wsi_storage_sop_class Tests
// ============================================================================

TEST_CASE("is_wsi_storage_sop_class identifies WSI SOP Classes",
          "[services][wsi][storage]") {
    SECTION("VL Whole Slide Microscopy Image Storage") {
        CHECK(is_wsi_storage_sop_class("1.2.840.10008.5.1.4.1.1.77.1.6"));
    }

    SECTION("non-WSI SOP Class - CT") {
        CHECK_FALSE(is_wsi_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));
    }

    SECTION("non-WSI SOP Class - US") {
        CHECK_FALSE(is_wsi_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));
    }

    SECTION("empty string") {
        CHECK_FALSE(is_wsi_storage_sop_class(""));
    }
}

// ============================================================================
// get_wsi_storage_sop_classes Tests
// ============================================================================

TEST_CASE("get_wsi_storage_sop_classes returns all WSI UIDs",
          "[services][wsi][storage]") {
    auto uids = get_wsi_storage_sop_classes();
    CHECK(uids.size() == 1);
    CHECK(uids[0] == "1.2.840.10008.5.1.4.1.1.77.1.6");
}

// ============================================================================
// is_valid_wsi_photometric Tests
// ============================================================================

TEST_CASE("is_valid_wsi_photometric checks supported color models",
          "[services][wsi][storage]") {
    SECTION("RGB is valid (brightfield)") {
        CHECK(is_valid_wsi_photometric("RGB"));
    }

    SECTION("YBR_FULL_422 is valid (JPEG compressed)") {
        CHECK(is_valid_wsi_photometric("YBR_FULL_422"));
    }

    SECTION("YBR_ICT is valid (JPEG 2000 lossy)") {
        CHECK(is_valid_wsi_photometric("YBR_ICT"));
    }

    SECTION("YBR_RCT is valid (JPEG 2000 lossless)") {
        CHECK(is_valid_wsi_photometric("YBR_RCT"));
    }

    SECTION("MONOCHROME2 is valid (fluorescence single-channel)") {
        CHECK(is_valid_wsi_photometric("MONOCHROME2"));
    }

    SECTION("MONOCHROME1 is invalid for WSI") {
        CHECK_FALSE(is_valid_wsi_photometric("MONOCHROME1"));
    }

    SECTION("PALETTE COLOR is invalid for WSI") {
        CHECK_FALSE(is_valid_wsi_photometric("PALETTE COLOR"));
    }

    SECTION("empty string is invalid") {
        CHECK_FALSE(is_valid_wsi_photometric(""));
    }
}

// ============================================================================
// WSI DICOM Tag Constant Tests
// ============================================================================

TEST_CASE("WSI DICOM tag constants are correct",
          "[services][wsi][storage]") {
    CHECK(wsi_tags::total_pixel_matrix_columns == 0x00480006);
    CHECK(wsi_tags::total_pixel_matrix_rows == 0x00480007);
    CHECK(wsi_tags::imaged_volume_width == 0x00480001);
    CHECK(wsi_tags::imaged_volume_height == 0x00480002);
    CHECK(wsi_tags::optical_path_identifier == 0x00480105);
    CHECK(wsi_tags::specimen_identifier == 0x00400551);
    CHECK(wsi_tags::container_identifier == 0x00400512);
    CHECK(wsi_tags::dimension_organization_type == 0x00209311);
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("WSI SOP Class is registered in sop_class_registry",
          "[services][wsi][storage][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("WSI UID is supported") {
        CHECK(registry.is_supported(wsi_image_storage_uid));
    }

    SECTION("WSI info is correct") {
        const auto* info = registry.get_info(wsi_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == wsi_image_storage_uid);
        CHECK(info->name == "VL Whole Slide Microscopy Image Storage");
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::sm);
        CHECK(info->is_retired == false);
        CHECK(info->supports_multiframe == true);
    }

    SECTION("WSI appears in storage class list") {
        auto storage_classes = registry.get_all_storage_classes();
        auto it = std::find(storage_classes.begin(), storage_classes.end(),
                            std::string(wsi_image_storage_uid));
        CHECK(it != storage_classes.end());
    }

    SECTION("WSI appears in SM modality query") {
        auto sm_classes = registry.get_by_modality(modality_type::sm);
        CHECK(sm_classes.size() >= 1);
        auto it = std::find(sm_classes.begin(), sm_classes.end(),
                            std::string(wsi_image_storage_uid));
        CHECK(it != sm_classes.end());
    }
}

// ============================================================================
// Modality Conversion Tests
// ============================================================================

TEST_CASE("SM modality string conversion",
          "[services][wsi][storage][registry]") {
    SECTION("modality_to_string returns SM") {
        CHECK(sop_class_registry::modality_to_string(modality_type::sm)
              == "SM");
    }

    SECTION("parse_modality recognizes SM") {
        CHECK(sop_class_registry::parse_modality("SM") == modality_type::sm);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("convenience functions work for WSI",
          "[services][wsi][storage][registry]") {
    SECTION("is_storage_sop_class") {
        CHECK(is_storage_sop_class(wsi_image_storage_uid));
    }

    SECTION("get_storage_modality") {
        CHECK(get_storage_modality(wsi_image_storage_uid) == modality_type::sm);
    }

    SECTION("get_sop_class_name") {
        CHECK(get_sop_class_name(wsi_image_storage_uid)
              == "VL Whole Slide Microscopy Image Storage");
    }
}
