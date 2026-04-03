/**
 * @file parametric_map_storage_test.cpp
 * @brief Unit tests for Parametric Map Storage SOP Class
 *
 * @see Issue #833 - Add Parametric Map Storage SOP Class registration
 */

#include <kcenon/pacs/services/sop_classes/parametric_map_storage.h>
#include <kcenon/pacs/services/sop_class_registry.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::services::sop_classes;

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("parametric map SOP Class UID is correct",
          "[services][parametric_map][storage]") {
    CHECK(parametric_map_storage_uid == "1.2.840.10008.5.1.4.1.1.30");
}

// ============================================================================
// is_parametric_map_storage_sop_class Tests
// ============================================================================

TEST_CASE("is_parametric_map_storage_sop_class identifies parametric map SOP Class",
          "[services][parametric_map][storage]") {
    SECTION("Parametric Map Storage") {
        CHECK(is_parametric_map_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.30"));
    }

    SECTION("non-parametric-map SOP Class - CT") {
        CHECK_FALSE(is_parametric_map_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.2"));
    }

    SECTION("non-parametric-map SOP Class - SEG") {
        CHECK_FALSE(is_parametric_map_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.66.4"));
    }

    SECTION("empty string") {
        CHECK_FALSE(is_parametric_map_storage_sop_class(""));
    }

    SECTION("invalid UID") {
        CHECK_FALSE(is_parametric_map_storage_sop_class("invalid"));
    }
}

// ============================================================================
// get_parametric_map_storage_sop_classes Tests
// ============================================================================

TEST_CASE("get_parametric_map_storage_sop_classes returns all UIDs",
          "[services][parametric_map][storage]") {
    auto uids = get_parametric_map_storage_sop_classes();
    CHECK(uids.size() == 1);
    CHECK(uids[0] == "1.2.840.10008.5.1.4.1.1.30");
}

// ============================================================================
// SOP Class Information Tests
// ============================================================================

TEST_CASE("get_parametric_map_sop_class_info returns correct information",
          "[services][parametric_map][storage]") {
    SECTION("Parametric Map Storage info") {
        const auto* info = get_parametric_map_sop_class_info(
            parametric_map_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == parametric_map_storage_uid);
        CHECK(info->name == "Parametric Map Storage");
        CHECK_FALSE(info->is_retired);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_parametric_map_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }

    SECTION("returns nullptr for empty string") {
        CHECK(get_parametric_map_sop_class_info("") == nullptr);
    }
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_parametric_map_transfer_syntaxes returns valid syntaxes",
          "[services][parametric_map][transfer]") {
    auto syntaxes = get_parametric_map_transfer_syntaxes();

    CHECK(syntaxes.size() > 0);

    // Should include Explicit VR Little Endian (most preferred)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1") != syntaxes.end());

    // Should include Implicit VR Little Endian (universal baseline)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2") != syntaxes.end());
}

// ============================================================================
// Pixel Value Representation Tests
// ============================================================================

TEST_CASE("pixel_value_representation conversions",
          "[services][parametric_map][pixel_repr]") {
    SECTION("get_bits_allocated returns correct values") {
        CHECK(get_bits_allocated(pixel_value_representation::float32) == 32);
        CHECK(get_bits_allocated(pixel_value_representation::float64) == 64);
    }

    SECTION("parse_pixel_value_representation parses correctly") {
        CHECK(parse_pixel_value_representation(32)
              == pixel_value_representation::float32);
        CHECK(parse_pixel_value_representation(64)
              == pixel_value_representation::float64);
        CHECK(parse_pixel_value_representation(16)
              == pixel_value_representation::float32);  // Default
    }
}

TEST_CASE("is_valid_parametric_map_bits_allocated validates correctly",
          "[services][parametric_map][pixel_repr]") {
    CHECK(is_valid_parametric_map_bits_allocated(32));
    CHECK(is_valid_parametric_map_bits_allocated(64));

    CHECK_FALSE(is_valid_parametric_map_bits_allocated(8));
    CHECK_FALSE(is_valid_parametric_map_bits_allocated(16));
    CHECK_FALSE(is_valid_parametric_map_bits_allocated(0));
    CHECK_FALSE(is_valid_parametric_map_bits_allocated(128));
}

// ============================================================================
// Photometric Interpretation Tests
// ============================================================================

TEST_CASE("is_valid_parametric_map_photometric checks supported color models",
          "[services][parametric_map][storage]") {
    SECTION("MONOCHROME2 is valid") {
        CHECK(is_valid_parametric_map_photometric("MONOCHROME2"));
    }

    SECTION("MONOCHROME1 is invalid for parametric maps") {
        CHECK_FALSE(is_valid_parametric_map_photometric("MONOCHROME1"));
    }

    SECTION("RGB is invalid for parametric maps") {
        CHECK_FALSE(is_valid_parametric_map_photometric("RGB"));
    }

    SECTION("empty string is invalid") {
        CHECK_FALSE(is_valid_parametric_map_photometric(""));
    }
}

// ============================================================================
// DICOM Tag Constant Tests
// ============================================================================

TEST_CASE("parametric map DICOM tag constants are correct",
          "[services][parametric_map][storage]") {
    CHECK(parametric_map_tags::real_world_value_mapping_sequence == 0x00409096);
    CHECK(parametric_map_tags::real_world_value_intercept == 0x00409224);
    CHECK(parametric_map_tags::real_world_value_slope == 0x00409225);
    CHECK(parametric_map_tags::real_world_value_first_value_mapped == 0x00409216);
    CHECK(parametric_map_tags::real_world_value_last_value_mapped == 0x00409211);
    CHECK(parametric_map_tags::measurement_units_code_sequence == 0x004008EA);
    CHECK(parametric_map_tags::content_label == 0x00700080);
    CHECK(parametric_map_tags::content_description == 0x00700081);
    CHECK(parametric_map_tags::content_creator_name == 0x00700084);
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("parametric map SOP Class is registered in sop_class_registry",
          "[services][parametric_map][storage][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("Parametric Map Storage is supported") {
        CHECK(registry.is_supported(parametric_map_storage_uid));
    }

    SECTION("Parametric Map Storage info is correct") {
        const auto* info = registry.get_info(parametric_map_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == parametric_map_storage_uid);
        CHECK(info->name == "Parametric Map Storage");
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::pmap);
        CHECK(info->is_retired == false);
        CHECK(info->supports_multiframe == true);
    }

    SECTION("parametric map appears in storage class list") {
        auto storage_classes = registry.get_all_storage_classes();
        auto it = std::find(storage_classes.begin(), storage_classes.end(),
                            std::string(parametric_map_storage_uid));
        CHECK(it != storage_classes.end());
    }

    SECTION("parametric map appears in PMAP modality query") {
        auto pmap_classes = registry.get_by_modality(modality_type::pmap);
        CHECK(pmap_classes.size() >= 1);
        auto it = std::find(pmap_classes.begin(), pmap_classes.end(),
                            std::string(parametric_map_storage_uid));
        CHECK(it != pmap_classes.end());
    }
}

// ============================================================================
// Modality Conversion Tests
// ============================================================================

TEST_CASE("PMAP modality string conversion",
          "[services][parametric_map][storage][registry]") {
    SECTION("modality_to_string returns RWV") {
        CHECK(sop_class_registry::modality_to_string(modality_type::pmap)
              == "RWV");
    }

    SECTION("parse_modality recognizes RWV") {
        CHECK(sop_class_registry::parse_modality("RWV") == modality_type::pmap);
    }

    SECTION("parse_modality recognizes PMAP") {
        CHECK(sop_class_registry::parse_modality("PMAP") == modality_type::pmap);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("convenience functions work for parametric map",
          "[services][parametric_map][storage][registry]") {
    SECTION("is_storage_sop_class") {
        CHECK(is_storage_sop_class(parametric_map_storage_uid));
    }

    SECTION("get_storage_modality") {
        CHECK(get_storage_modality(parametric_map_storage_uid)
              == modality_type::pmap);
    }

    SECTION("get_sop_class_name") {
        CHECK(get_sop_class_name(parametric_map_storage_uid)
              == "Parametric Map Storage");
    }
}
