/**
 * @file ophthalmic_storage_test.cpp
 * @brief Unit tests for Ophthalmic Photography/Tomography Storage SOP Classes
 *
 * @see Issue #829 - Add Ophthalmic SOP Class registration and storage definition
 */

#include <pacs/services/sop_classes/ophthalmic_storage.hpp>
#include <pacs/services/sop_class_registry.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::services::sop_classes;

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("ophthalmic SOP Class UIDs are correct",
          "[services][ophthalmic][storage]") {
    CHECK(ophthalmic_photo_8bit_storage_uid
          == "1.2.840.10008.5.1.4.1.1.77.1.5.1");
    CHECK(ophthalmic_photo_16bit_storage_uid
          == "1.2.840.10008.5.1.4.1.1.77.1.5.2");
    CHECK(ophthalmic_tomography_storage_uid
          == "1.2.840.10008.5.1.4.1.1.77.1.5.4");
    CHECK(wide_field_ophthalmic_photo_storage_uid
          == "1.2.840.10008.5.1.4.1.1.77.1.5.7");
    CHECK(ophthalmic_oct_bscan_analysis_storage_uid
          == "1.2.840.10008.5.1.4.1.1.77.1.5.8");
}

// ============================================================================
// is_ophthalmic_storage_sop_class Tests
// ============================================================================

TEST_CASE("is_ophthalmic_storage_sop_class identifies ophthalmic SOP Classes",
          "[services][ophthalmic][storage]") {
    SECTION("Ophthalmic Photography 8 Bit") {
        CHECK(is_ophthalmic_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.77.1.5.1"));
    }

    SECTION("Ophthalmic Photography 16 Bit") {
        CHECK(is_ophthalmic_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.77.1.5.2"));
    }

    SECTION("Ophthalmic Tomography (OCT)") {
        CHECK(is_ophthalmic_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.77.1.5.4"));
    }

    SECTION("Wide Field Ophthalmic Photography") {
        CHECK(is_ophthalmic_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.77.1.5.7"));
    }

    SECTION("Ophthalmic OCT B-scan Volume Analysis") {
        CHECK(is_ophthalmic_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.77.1.5.8"));
    }

    SECTION("non-ophthalmic SOP Class - CT") {
        CHECK_FALSE(is_ophthalmic_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.2"));
    }

    SECTION("non-ophthalmic SOP Class - WSI") {
        CHECK_FALSE(is_ophthalmic_storage_sop_class(
            "1.2.840.10008.5.1.4.1.1.77.1.6"));
    }

    SECTION("empty string") {
        CHECK_FALSE(is_ophthalmic_storage_sop_class(""));
    }
}

// ============================================================================
// get_ophthalmic_storage_sop_classes Tests
// ============================================================================

TEST_CASE("get_ophthalmic_storage_sop_classes returns all UIDs",
          "[services][ophthalmic][storage]") {
    auto uids = get_ophthalmic_storage_sop_classes();
    CHECK(uids.size() == 5);

    CHECK(std::find(uids.begin(), uids.end(),
                    "1.2.840.10008.5.1.4.1.1.77.1.5.1") != uids.end());
    CHECK(std::find(uids.begin(), uids.end(),
                    "1.2.840.10008.5.1.4.1.1.77.1.5.2") != uids.end());
    CHECK(std::find(uids.begin(), uids.end(),
                    "1.2.840.10008.5.1.4.1.1.77.1.5.4") != uids.end());
    CHECK(std::find(uids.begin(), uids.end(),
                    "1.2.840.10008.5.1.4.1.1.77.1.5.7") != uids.end());
    CHECK(std::find(uids.begin(), uids.end(),
                    "1.2.840.10008.5.1.4.1.1.77.1.5.8") != uids.end());
}

// ============================================================================
// is_valid_ophthalmic_photometric Tests
// ============================================================================

TEST_CASE("is_valid_ophthalmic_photometric checks supported color models",
          "[services][ophthalmic][storage]") {
    SECTION("MONOCHROME1 is valid (inverse grayscale)") {
        CHECK(is_valid_ophthalmic_photometric("MONOCHROME1"));
    }

    SECTION("MONOCHROME2 is valid (OCT, fluorescein angiography)") {
        CHECK(is_valid_ophthalmic_photometric("MONOCHROME2"));
    }

    SECTION("RGB is valid (color fundus photography)") {
        CHECK(is_valid_ophthalmic_photometric("RGB"));
    }

    SECTION("YBR_FULL_422 is valid (JPEG compressed)") {
        CHECK(is_valid_ophthalmic_photometric("YBR_FULL_422"));
    }

    SECTION("PALETTE COLOR is valid (pseudo-color mapping)") {
        CHECK(is_valid_ophthalmic_photometric("PALETTE COLOR"));
    }

    SECTION("YBR_ICT is invalid for ophthalmic") {
        CHECK_FALSE(is_valid_ophthalmic_photometric("YBR_ICT"));
    }

    SECTION("YBR_RCT is invalid for ophthalmic") {
        CHECK_FALSE(is_valid_ophthalmic_photometric("YBR_RCT"));
    }

    SECTION("empty string is invalid") {
        CHECK_FALSE(is_valid_ophthalmic_photometric(""));
    }
}

// ============================================================================
// Ophthalmic DICOM Tag Constant Tests
// ============================================================================

TEST_CASE("ophthalmic DICOM tag constants are correct",
          "[services][ophthalmic][storage]") {
    CHECK(ophthalmic_tags::image_laterality == 0x00200062);
    CHECK(ophthalmic_tags::anatomic_region_sequence == 0x00082218);
    CHECK(ophthalmic_tags::acquisition_device_type_code_sequence == 0x00220015);
    CHECK(ophthalmic_tags::detector_type == 0x00187004);
    CHECK(ophthalmic_tags::pupil_dilated == 0x0022000D);
    CHECK(ophthalmic_tags::axial_length_of_eye == 0x00220030);
    CHECK(ophthalmic_tags::horizontal_field_of_view == 0x0022000C);
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("ophthalmic SOP Classes are registered in sop_class_registry",
          "[services][ophthalmic][storage][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("all ophthalmic UIDs are supported") {
        CHECK(registry.is_supported(ophthalmic_photo_8bit_storage_uid));
        CHECK(registry.is_supported(ophthalmic_photo_16bit_storage_uid));
        CHECK(registry.is_supported(ophthalmic_tomography_storage_uid));
        CHECK(registry.is_supported(wide_field_ophthalmic_photo_storage_uid));
        CHECK(registry.is_supported(ophthalmic_oct_bscan_analysis_storage_uid));
    }

    SECTION("8-bit photography info is correct") {
        const auto* info = registry.get_info(ophthalmic_photo_8bit_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == ophthalmic_photo_8bit_storage_uid);
        CHECK(info->name == "Ophthalmic Photography 8 Bit Image Storage");
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::op);
        CHECK(info->is_retired == false);
        CHECK(info->supports_multiframe == false);
    }

    SECTION("OCT info supports multiframe") {
        const auto* info = registry.get_info(ophthalmic_tomography_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->name == "Ophthalmic Tomography Image Storage");
        CHECK(info->modality == modality_type::op);
        CHECK(info->supports_multiframe == true);
    }

    SECTION("B-scan volume analysis info supports multiframe") {
        const auto* info = registry.get_info(
            ophthalmic_oct_bscan_analysis_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->name == "Ophthalmic OCT B-scan Volume Analysis Storage");
        CHECK(info->supports_multiframe == true);
    }

    SECTION("ophthalmic classes appear in storage class list") {
        auto storage_classes = registry.get_all_storage_classes();
        auto it = std::find(storage_classes.begin(), storage_classes.end(),
                            std::string(ophthalmic_photo_8bit_storage_uid));
        CHECK(it != storage_classes.end());
    }

    SECTION("ophthalmic classes appear in OP modality query") {
        auto op_classes = registry.get_by_modality(modality_type::op);
        CHECK(op_classes.size() >= 5);
        auto it = std::find(op_classes.begin(), op_classes.end(),
                            std::string(ophthalmic_tomography_storage_uid));
        CHECK(it != op_classes.end());
    }
}

// ============================================================================
// Modality Conversion Tests
// ============================================================================

TEST_CASE("OP modality string conversion",
          "[services][ophthalmic][storage][registry]") {
    SECTION("modality_to_string returns OP") {
        CHECK(sop_class_registry::modality_to_string(modality_type::op)
              == "OP");
    }

    SECTION("parse_modality recognizes OP") {
        CHECK(sop_class_registry::parse_modality("OP") == modality_type::op);
    }

    SECTION("parse_modality recognizes OPT as OP") {
        CHECK(sop_class_registry::parse_modality("OPT") == modality_type::op);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("convenience functions work for ophthalmic",
          "[services][ophthalmic][storage][registry]") {
    SECTION("is_storage_sop_class") {
        CHECK(is_storage_sop_class(ophthalmic_photo_8bit_storage_uid));
        CHECK(is_storage_sop_class(ophthalmic_tomography_storage_uid));
    }

    SECTION("get_storage_modality") {
        CHECK(get_storage_modality(ophthalmic_photo_8bit_storage_uid)
              == modality_type::op);
        CHECK(get_storage_modality(ophthalmic_tomography_storage_uid)
              == modality_type::op);
    }

    SECTION("get_sop_class_name") {
        CHECK(get_sop_class_name(ophthalmic_photo_8bit_storage_uid)
              == "Ophthalmic Photography 8 Bit Image Storage");
        CHECK(get_sop_class_name(ophthalmic_tomography_storage_uid)
              == "Ophthalmic Tomography Image Storage");
    }
}
