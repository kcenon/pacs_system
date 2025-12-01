/**
 * @file us_storage_test.cpp
 * @brief Unit tests for Ultrasound Storage SOP Classes and IOD Validator
 */

#include <pacs/services/sop_classes/us_storage.hpp>
#include <pacs/services/validation/us_iod_validator.hpp>
#include <pacs/services/sop_class_registry.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services::sop_classes;
using namespace pacs::services::validation;
using namespace pacs::services;
using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// US Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("US Storage SOP Class UIDs are correct", "[services][us][sop_class]") {
    CHECK(us_image_storage_uid == "1.2.840.10008.5.1.4.1.1.6.1");
    CHECK(us_multiframe_image_storage_uid == "1.2.840.10008.5.1.4.1.1.6.2");
    CHECK(us_image_storage_retired_uid == "1.2.840.10008.5.1.4.1.1.6");
    CHECK(us_multiframe_image_storage_retired_uid == "1.2.840.10008.5.1.4.1.1.3.1");
}

TEST_CASE("is_us_storage_sop_class identifies US classes", "[services][us][sop_class]") {
    SECTION("recognizes primary US classes") {
        CHECK(is_us_storage_sop_class(us_image_storage_uid));
        CHECK(is_us_storage_sop_class(us_multiframe_image_storage_uid));
    }

    SECTION("recognizes retired US classes") {
        CHECK(is_us_storage_sop_class(us_image_storage_retired_uid));
        CHECK(is_us_storage_sop_class(us_multiframe_image_storage_retired_uid));
    }

    SECTION("rejects non-US classes") {
        CHECK_FALSE(is_us_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));  // CT
        CHECK_FALSE(is_us_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));  // MR
        CHECK_FALSE(is_us_storage_sop_class("1.2.840.10008.1.1"));          // Verification
        CHECK_FALSE(is_us_storage_sop_class(""));
        CHECK_FALSE(is_us_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_us_multiframe_sop_class identifies multiframe classes", "[services][us][sop_class]") {
    CHECK(is_us_multiframe_sop_class(us_multiframe_image_storage_uid));
    CHECK(is_us_multiframe_sop_class(us_multiframe_image_storage_retired_uid));
    CHECK_FALSE(is_us_multiframe_sop_class(us_image_storage_uid));
    CHECK_FALSE(is_us_multiframe_sop_class(us_image_storage_retired_uid));
}

// ============================================================================
// US SOP Class Information Tests
// ============================================================================

TEST_CASE("get_us_sop_class_info returns correct information", "[services][us][sop_class]") {
    SECTION("US Image Storage info") {
        const auto* info = get_us_sop_class_info(us_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == us_image_storage_uid);
        CHECK(info->name == "US Image Storage");
        CHECK_FALSE(info->is_retired);
        CHECK_FALSE(info->supports_multiframe);
    }

    SECTION("US Multi-frame Image Storage info") {
        const auto* info = get_us_sop_class_info(us_multiframe_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == us_multiframe_image_storage_uid);
        CHECK_FALSE(info->is_retired);
        CHECK(info->supports_multiframe);
    }

    SECTION("Retired class info") {
        const auto* info = get_us_sop_class_info(us_image_storage_retired_uid);
        REQUIRE(info != nullptr);
        CHECK(info->is_retired);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_us_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_us_storage_sop_classes returns correct list", "[services][us][sop_class]") {
    SECTION("with retired classes") {
        auto classes = get_us_storage_sop_classes(true);
        CHECK(classes.size() == 4);
    }

    SECTION("without retired classes") {
        auto classes = get_us_storage_sop_classes(false);
        CHECK(classes.size() == 2);
        // Verify only current classes are returned
        for (const auto& uid : classes) {
            const auto* info = get_us_sop_class_info(uid);
            REQUIRE(info != nullptr);
            CHECK_FALSE(info->is_retired);
        }
    }
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_us_transfer_syntaxes returns valid syntaxes", "[services][us][transfer]") {
    auto syntaxes = get_us_transfer_syntaxes();

    CHECK(syntaxes.size() > 0);

    // Should include Explicit VR Little Endian (most preferred)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1") != syntaxes.end());

    // Should include Implicit VR Little Endian (universal baseline)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2") != syntaxes.end());
}

// ============================================================================
// Photometric Interpretation Tests
// ============================================================================

TEST_CASE("us_photometric_interpretation conversions", "[services][us][photometric]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(us_photometric_interpretation::monochrome1) == "MONOCHROME1");
        CHECK(to_string(us_photometric_interpretation::monochrome2) == "MONOCHROME2");
        CHECK(to_string(us_photometric_interpretation::palette_color) == "PALETTE COLOR");
        CHECK(to_string(us_photometric_interpretation::rgb) == "RGB");
        CHECK(to_string(us_photometric_interpretation::ybr_full) == "YBR_FULL");
        CHECK(to_string(us_photometric_interpretation::ybr_full_422) == "YBR_FULL_422");
    }

    SECTION("parse_photometric_interpretation parses correctly") {
        CHECK(parse_photometric_interpretation("MONOCHROME1") ==
              us_photometric_interpretation::monochrome1);
        CHECK(parse_photometric_interpretation("RGB") ==
              us_photometric_interpretation::rgb);
        CHECK(parse_photometric_interpretation("YBR_FULL_422") ==
              us_photometric_interpretation::ybr_full_422);
    }

    SECTION("parse returns monochrome2 for unknown values") {
        CHECK(parse_photometric_interpretation("UNKNOWN") ==
              us_photometric_interpretation::monochrome2);
        CHECK(parse_photometric_interpretation("") ==
              us_photometric_interpretation::monochrome2);
    }
}

TEST_CASE("is_valid_us_photometric validates correctly", "[services][us][photometric]") {
    CHECK(is_valid_us_photometric("MONOCHROME1"));
    CHECK(is_valid_us_photometric("MONOCHROME2"));
    CHECK(is_valid_us_photometric("PALETTE COLOR"));
    CHECK(is_valid_us_photometric("RGB"));
    CHECK(is_valid_us_photometric("YBR_FULL"));
    CHECK(is_valid_us_photometric("YBR_FULL_422"));

    CHECK_FALSE(is_valid_us_photometric("ARGB"));
    CHECK_FALSE(is_valid_us_photometric("HSV"));
    CHECK_FALSE(is_valid_us_photometric(""));
}

// ============================================================================
// US IOD Validator Tests
// ============================================================================

namespace {

dicom_dataset create_minimal_us_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19800101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.123");
    ds.set_string(tags::study_date, vr_type::DA, "20231201");
    ds.set_string(tags::study_time, vr_type::TM, "120000");
    ds.set_string(tags::referring_physician_name, vr_type::PN, "DR^REFERRER");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module
    ds.set_string(tags::modality, vr_type::CS, "US");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.124");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // Image Pixel Module
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 480);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 640);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 8);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 7);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);
    ds.set_string(tags::pixel_data, vr_type::OW, "dummy_pixel_data");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(us_image_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.125");

    return ds;
}

}  // namespace

TEST_CASE("us_iod_validator validates complete dataset", "[services][us][validation]") {
    us_iod_validator validator;
    auto dataset = create_minimal_us_dataset();

    auto result = validator.validate(dataset);
    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("us_iod_validator detects missing Type 1 attributes", "[services][us][validation]") {
    us_iod_validator validator;
    auto dataset = create_minimal_us_dataset();

    SECTION("missing StudyInstanceUID") {
        dataset.remove(tags::study_instance_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
        CHECK(result.has_errors());
    }

    SECTION("missing Modality") {
        dataset.remove(tags::modality);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing Rows") {
        dataset.remove(tags::rows);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SOPClassUID") {
        dataset.remove(tags::sop_class_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("us_iod_validator detects wrong modality", "[services][us][validation]") {
    us_iod_validator validator;
    auto dataset = create_minimal_us_dataset();

    dataset.set_string(tags::modality, vr_type::CS, "CT");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
    CHECK(result.has_errors());
}

TEST_CASE("us_iod_validator detects invalid SOP Class", "[services][us][validation]") {
    us_iod_validator validator;
    auto dataset = create_minimal_us_dataset();

    // Set to CT SOP Class
    dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
    // Should have an error about SOP Class not being US
}

TEST_CASE("us_iod_validator detects pixel data inconsistencies", "[services][us][validation]") {
    us_iod_validator validator;
    auto dataset = create_minimal_us_dataset();

    SECTION("BitsStored > BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
        auto result = validator.validate(dataset);
        CHECK(result.has_errors());
    }

    SECTION("wrong SamplesPerPixel for RGB") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "RGB");
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
        auto result = validator.validate(dataset);
        CHECK(result.has_errors());
    }
}

TEST_CASE("us_iod_validator quick_check works correctly", "[services][us][validation]") {
    us_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_us_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid dataset fails quick check") {
        auto dataset = create_minimal_us_dataset();
        dataset.remove(tags::modality);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("wrong modality fails quick check") {
        auto dataset = create_minimal_us_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

TEST_CASE("us_iod_validator options work correctly", "[services][us][validation]") {
    SECTION("can disable Type 2 checking") {
        us_validation_options options;
        options.check_type1 = true;
        options.check_type2 = false;

        us_iod_validator validator{options};
        auto dataset = create_minimal_us_dataset();
        dataset.remove(tags::patient_name);  // Type 2

        auto result = validator.validate(dataset);
        CHECK(result.is_valid);  // Should pass without Type 2 errors
    }

    SECTION("strict mode treats warnings as errors") {
        us_validation_options options;
        options.strict_mode = true;

        us_iod_validator validator{options};
        auto dataset = create_minimal_us_dataset();

        // Remove a Type 2 attribute to generate a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        // In strict mode, the warning becomes an error
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("validation_result methods work correctly", "[services][us][validation]") {
    validation_result result;
    result.is_valid = false;
    result.findings = {
        {validation_severity::error, tags::modality, "Error 1", "E001"},
        {validation_severity::error, tags::rows, "Error 2", "E002"},
        {validation_severity::warning, tags::columns, "Warning 1", "W001"},
        {validation_severity::info, tags::pixel_data, "Info 1", "I001"}
    };

    CHECK(result.has_errors());
    CHECK(result.has_warnings());
    CHECK(result.error_count() == 2);
    CHECK(result.warning_count() == 1);

    auto summary = result.summary();
    CHECK(summary.find("FAILED") != std::string::npos);
    CHECK(summary.find("2 error") != std::string::npos);
    CHECK(summary.find("1 warning") != std::string::npos);
}

// ============================================================================
// SOP Class Registry Tests
// ============================================================================

TEST_CASE("sop_class_registry contains US classes", "[services][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("supports US Image Storage") {
        CHECK(registry.is_supported(us_image_storage_uid));
        const auto* info = registry.get_info(us_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::us);
    }

    SECTION("supports US Multi-frame Image Storage") {
        CHECK(registry.is_supported(us_multiframe_image_storage_uid));
    }

    SECTION("get_by_modality returns US classes") {
        auto us_classes = registry.get_by_modality(modality_type::us, true);
        CHECK(us_classes.size() >= 4);

        // Verify all returned classes are US
        for (const auto& uid : us_classes) {
            const auto* info = registry.get_info(uid);
            REQUIRE(info != nullptr);
            CHECK(info->modality == modality_type::us);
        }
    }

    SECTION("get_by_modality filters retired classes") {
        auto current_classes = registry.get_by_modality(modality_type::us, false);
        CHECK(current_classes.size() == 2);

        for (const auto& uid : current_classes) {
            const auto* info = registry.get_info(uid);
            REQUIRE(info != nullptr);
            CHECK_FALSE(info->is_retired);
        }
    }
}

TEST_CASE("sop_class_registry modality conversion", "[services][registry]") {
    CHECK(sop_class_registry::modality_to_string(modality_type::us) == "US");
    CHECK(sop_class_registry::modality_to_string(modality_type::ct) == "CT");
    CHECK(sop_class_registry::modality_to_string(modality_type::mr) == "MR");

    CHECK(sop_class_registry::parse_modality("US") == modality_type::us);
    CHECK(sop_class_registry::parse_modality("CT") == modality_type::ct);
    CHECK(sop_class_registry::parse_modality("UNKNOWN") == modality_type::other);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_us_iod convenience function", "[services][us][validation]") {
    auto dataset = create_minimal_us_dataset();
    auto result = validate_us_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_us_dataset convenience function", "[services][us][validation]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_us_dataset();
        CHECK(is_valid_us_dataset(dataset));
    }

    SECTION("invalid dataset") {
        dicom_dataset empty_dataset;
        CHECK_FALSE(is_valid_us_dataset(empty_dataset));
    }
}

TEST_CASE("is_storage_sop_class convenience function", "[services][registry]") {
    CHECK(is_storage_sop_class(us_image_storage_uid));
    CHECK(is_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));  // CT
    CHECK_FALSE(is_storage_sop_class("1.2.840.10008.1.1"));    // Verification
}

TEST_CASE("get_storage_modality convenience function", "[services][registry]") {
    CHECK(get_storage_modality(us_image_storage_uid) == modality_type::us);
    CHECK(get_storage_modality("1.2.840.10008.5.1.4.1.1.2") == modality_type::ct);
    CHECK(get_storage_modality("1.2.840.10008.1.1") == modality_type::other);
}

TEST_CASE("get_sop_class_name convenience function", "[services][registry]") {
    CHECK(get_sop_class_name(us_image_storage_uid) == "US Image Storage");
    CHECK(get_sop_class_name("1.2.3.4.5.6") == "Unknown");
}
