/**
 * @file dx_storage_test.cpp
 * @brief Unit tests for Digital X-Ray Storage SOP Classes and IOD Validator
 */

#include <pacs/services/sop_classes/dx_storage.hpp>
#include <pacs/services/validation/dx_iod_validator.hpp>
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
// DX Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("DX Storage SOP Class UIDs are correct", "[services][dx][sop_class]") {
    CHECK(dx_image_storage_for_presentation_uid == "1.2.840.10008.5.1.4.1.1.1.1");
    CHECK(dx_image_storage_for_processing_uid == "1.2.840.10008.5.1.4.1.1.1.1.1");
    CHECK(mammography_image_storage_for_presentation_uid == "1.2.840.10008.5.1.4.1.1.1.2");
    CHECK(mammography_image_storage_for_processing_uid == "1.2.840.10008.5.1.4.1.1.1.2.1");
    CHECK(intraoral_image_storage_for_presentation_uid == "1.2.840.10008.5.1.4.1.1.1.3");
    CHECK(intraoral_image_storage_for_processing_uid == "1.2.840.10008.5.1.4.1.1.1.3.1");
}

TEST_CASE("is_dx_storage_sop_class identifies DX classes", "[services][dx][sop_class]") {
    SECTION("recognizes general DX classes") {
        CHECK(is_dx_storage_sop_class(dx_image_storage_for_presentation_uid));
        CHECK(is_dx_storage_sop_class(dx_image_storage_for_processing_uid));
    }

    SECTION("recognizes mammography classes") {
        CHECK(is_dx_storage_sop_class(mammography_image_storage_for_presentation_uid));
        CHECK(is_dx_storage_sop_class(mammography_image_storage_for_processing_uid));
    }

    SECTION("recognizes intra-oral classes") {
        CHECK(is_dx_storage_sop_class(intraoral_image_storage_for_presentation_uid));
        CHECK(is_dx_storage_sop_class(intraoral_image_storage_for_processing_uid));
    }

    SECTION("rejects non-DX classes") {
        CHECK_FALSE(is_dx_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));  // CT
        CHECK_FALSE(is_dx_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));  // MR
        CHECK_FALSE(is_dx_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));  // US
        CHECK_FALSE(is_dx_storage_sop_class("1.2.840.10008.1.1"));  // Verification
        CHECK_FALSE(is_dx_storage_sop_class(""));
        CHECK_FALSE(is_dx_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_dx_for_processing_sop_class identifies For Processing classes", "[services][dx][sop_class]") {
    CHECK(is_dx_for_processing_sop_class(dx_image_storage_for_processing_uid));
    CHECK(is_dx_for_processing_sop_class(mammography_image_storage_for_processing_uid));
    CHECK(is_dx_for_processing_sop_class(intraoral_image_storage_for_processing_uid));

    CHECK_FALSE(is_dx_for_processing_sop_class(dx_image_storage_for_presentation_uid));
    CHECK_FALSE(is_dx_for_processing_sop_class(mammography_image_storage_for_presentation_uid));
    CHECK_FALSE(is_dx_for_processing_sop_class(intraoral_image_storage_for_presentation_uid));
}

TEST_CASE("is_dx_for_presentation_sop_class identifies For Presentation classes", "[services][dx][sop_class]") {
    CHECK(is_dx_for_presentation_sop_class(dx_image_storage_for_presentation_uid));
    CHECK(is_dx_for_presentation_sop_class(mammography_image_storage_for_presentation_uid));
    CHECK(is_dx_for_presentation_sop_class(intraoral_image_storage_for_presentation_uid));

    CHECK_FALSE(is_dx_for_presentation_sop_class(dx_image_storage_for_processing_uid));
    CHECK_FALSE(is_dx_for_presentation_sop_class(mammography_image_storage_for_processing_uid));
    CHECK_FALSE(is_dx_for_presentation_sop_class(intraoral_image_storage_for_processing_uid));
}

TEST_CASE("is_mammography_sop_class identifies mammography classes", "[services][dx][sop_class]") {
    CHECK(is_mammography_sop_class(mammography_image_storage_for_presentation_uid));
    CHECK(is_mammography_sop_class(mammography_image_storage_for_processing_uid));

    CHECK_FALSE(is_mammography_sop_class(dx_image_storage_for_presentation_uid));
    CHECK_FALSE(is_mammography_sop_class(dx_image_storage_for_processing_uid));
    CHECK_FALSE(is_mammography_sop_class(intraoral_image_storage_for_presentation_uid));
}

// ============================================================================
// DX SOP Class Information Tests
// ============================================================================

TEST_CASE("get_dx_sop_class_info returns correct information", "[services][dx][sop_class]") {
    SECTION("DX For Presentation info") {
        const auto* info = get_dx_sop_class_info(dx_image_storage_for_presentation_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == dx_image_storage_for_presentation_uid);
        CHECK(info->name == "Digital X-Ray Image Storage - For Presentation");
        CHECK(info->image_type == dx_image_type::for_presentation);
        CHECK_FALSE(info->is_mammography);
        CHECK_FALSE(info->is_intraoral);
    }

    SECTION("DX For Processing info") {
        const auto* info = get_dx_sop_class_info(dx_image_storage_for_processing_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == dx_image_storage_for_processing_uid);
        CHECK(info->image_type == dx_image_type::for_processing);
        CHECK_FALSE(info->is_mammography);
    }

    SECTION("Mammography info") {
        const auto* info = get_dx_sop_class_info(mammography_image_storage_for_presentation_uid);
        REQUIRE(info != nullptr);
        CHECK(info->is_mammography);
        CHECK_FALSE(info->is_intraoral);
    }

    SECTION("Intra-oral info") {
        const auto* info = get_dx_sop_class_info(intraoral_image_storage_for_presentation_uid);
        REQUIRE(info != nullptr);
        CHECK_FALSE(info->is_mammography);
        CHECK(info->is_intraoral);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_dx_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_dx_storage_sop_classes returns correct list", "[services][dx][sop_class]") {
    SECTION("all DX classes") {
        auto classes = get_dx_storage_sop_classes(true, true);
        CHECK(classes.size() == 6);
    }

    SECTION("without mammography") {
        auto classes = get_dx_storage_sop_classes(false, true);
        CHECK(classes.size() == 4);
        for (const auto& uid : classes) {
            CHECK_FALSE(is_mammography_sop_class(uid));
        }
    }

    SECTION("without intra-oral") {
        auto classes = get_dx_storage_sop_classes(true, false);
        CHECK(classes.size() == 4);
    }

    SECTION("only general DX") {
        auto classes = get_dx_storage_sop_classes(false, false);
        CHECK(classes.size() == 2);
    }
}

// ============================================================================
// DX Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_dx_transfer_syntaxes returns valid syntaxes", "[services][dx][transfer_syntax]") {
    auto syntaxes = get_dx_transfer_syntaxes();

    CHECK_FALSE(syntaxes.empty());

    // Should include Explicit VR Little Endian
    auto it = std::find(syntaxes.begin(), syntaxes.end(), "1.2.840.10008.1.2.1");
    CHECK(it != syntaxes.end());

    // Should include Implicit VR Little Endian
    it = std::find(syntaxes.begin(), syntaxes.end(), "1.2.840.10008.1.2");
    CHECK(it != syntaxes.end());

    // Should include JPEG Lossless for diagnostic quality
    it = std::find(syntaxes.begin(), syntaxes.end(), "1.2.840.10008.1.2.4.70");
    CHECK(it != syntaxes.end());
}

// ============================================================================
// DX Photometric Interpretation Tests
// ============================================================================

TEST_CASE("DX photometric interpretation conversion", "[services][dx][photometric]") {
    SECTION("enum to string") {
        CHECK(to_string(dx_photometric_interpretation::monochrome1) == "MONOCHROME1");
        CHECK(to_string(dx_photometric_interpretation::monochrome2) == "MONOCHROME2");
    }

    SECTION("string to enum") {
        CHECK(parse_dx_photometric_interpretation("MONOCHROME1") ==
              dx_photometric_interpretation::monochrome1);
        CHECK(parse_dx_photometric_interpretation("MONOCHROME2") ==
              dx_photometric_interpretation::monochrome2);
        // Unknown defaults to MONOCHROME2
        CHECK(parse_dx_photometric_interpretation("RGB") ==
              dx_photometric_interpretation::monochrome2);
    }

    SECTION("validation") {
        CHECK(is_valid_dx_photometric("MONOCHROME1"));
        CHECK(is_valid_dx_photometric("MONOCHROME2"));
        CHECK_FALSE(is_valid_dx_photometric("RGB"));
        CHECK_FALSE(is_valid_dx_photometric("PALETTE COLOR"));
        CHECK_FALSE(is_valid_dx_photometric(""));
    }
}

// ============================================================================
// DX Image Type Tests
// ============================================================================

TEST_CASE("DX image type conversion", "[services][dx][image_type]") {
    CHECK(to_string(dx_image_type::for_presentation) == "FOR PRESENTATION");
    CHECK(to_string(dx_image_type::for_processing) == "FOR PROCESSING");
}

// ============================================================================
// DX View Position Tests
// ============================================================================

TEST_CASE("DX view position conversion", "[services][dx][view_position]") {
    SECTION("enum to string") {
        CHECK(to_string(dx_view_position::ap) == "AP");
        CHECK(to_string(dx_view_position::pa) == "PA");
        CHECK(to_string(dx_view_position::lateral) == "LATERAL");
        CHECK(to_string(dx_view_position::oblique) == "OBLIQUE");
    }

    SECTION("string to enum") {
        CHECK(parse_view_position("AP") == dx_view_position::ap);
        CHECK(parse_view_position("PA") == dx_view_position::pa);
        CHECK(parse_view_position("LATERAL") == dx_view_position::lateral);
        CHECK(parse_view_position("LAT") == dx_view_position::lateral);
        CHECK(parse_view_position("LL") == dx_view_position::lateral);
        CHECK(parse_view_position("OBLIQUE") == dx_view_position::oblique);
        CHECK(parse_view_position("LAO") == dx_view_position::oblique);
        CHECK(parse_view_position("UNKNOWN") == dx_view_position::other);
    }
}

// ============================================================================
// DX Detector Type Tests
// ============================================================================

TEST_CASE("DX detector type conversion", "[services][dx][detector]") {
    SECTION("enum to string") {
        CHECK(to_string(dx_detector_type::direct) == "DIRECT");
        CHECK(to_string(dx_detector_type::indirect) == "INDIRECT");
        CHECK(to_string(dx_detector_type::storage) == "STORAGE");
        CHECK(to_string(dx_detector_type::film) == "FILM");
    }

    SECTION("string to enum") {
        CHECK(parse_detector_type("DIRECT") == dx_detector_type::direct);
        CHECK(parse_detector_type("INDIRECT") == dx_detector_type::indirect);
        CHECK(parse_detector_type("STORAGE") == dx_detector_type::storage);
        CHECK(parse_detector_type("FILM") == dx_detector_type::film);
        // Unknown defaults to direct
        CHECK(parse_detector_type("UNKNOWN") == dx_detector_type::direct);
    }
}

// ============================================================================
// DX Body Part Tests
// ============================================================================

TEST_CASE("DX body part conversion", "[services][dx][body_part]") {
    SECTION("enum to string") {
        CHECK(to_string(dx_body_part::chest) == "CHEST");
        CHECK(to_string(dx_body_part::abdomen) == "ABDOMEN");
        CHECK(to_string(dx_body_part::spine) == "SPINE");
        CHECK(to_string(dx_body_part::hand) == "HAND");
        CHECK(to_string(dx_body_part::knee) == "KNEE");
        CHECK(to_string(dx_body_part::breast) == "BREAST");
    }

    SECTION("string to enum") {
        CHECK(parse_body_part("CHEST") == dx_body_part::chest);
        CHECK(parse_body_part("ABDOMEN") == dx_body_part::abdomen);
        CHECK(parse_body_part("SPINE") == dx_body_part::spine);
        CHECK(parse_body_part("CSPINE") == dx_body_part::spine);
        CHECK(parse_body_part("LSPINE") == dx_body_part::spine);
        CHECK(parse_body_part("HAND") == dx_body_part::hand);
        CHECK(parse_body_part("FINGER") == dx_body_part::hand);
        CHECK(parse_body_part("KNEE") == dx_body_part::knee);
        CHECK(parse_body_part("BREAST") == dx_body_part::breast);
        CHECK(parse_body_part("UNKNOWN") == dx_body_part::other);
    }
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("DX classes are registered in SOP Class Registry", "[services][dx][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("DX For Presentation is registered") {
        CHECK(registry.is_supported(dx_image_storage_for_presentation_uid));
        const auto* info = registry.get_info(dx_image_storage_for_presentation_uid);
        REQUIRE(info != nullptr);
        CHECK(info->modality == modality_type::dx);
        CHECK(info->category == sop_class_category::storage);
    }

    SECTION("DX For Processing is registered") {
        CHECK(registry.is_supported(dx_image_storage_for_processing_uid));
        const auto* info = registry.get_info(dx_image_storage_for_processing_uid);
        REQUIRE(info != nullptr);
        CHECK(info->modality == modality_type::dx);
    }

    SECTION("Mammography classes are registered") {
        CHECK(registry.is_supported(mammography_image_storage_for_presentation_uid));
        CHECK(registry.is_supported(mammography_image_storage_for_processing_uid));
        const auto* info = registry.get_info(mammography_image_storage_for_presentation_uid);
        REQUIRE(info != nullptr);
        CHECK(info->modality == modality_type::mg);
    }

    SECTION("Intra-oral classes are registered") {
        CHECK(registry.is_supported(intraoral_image_storage_for_presentation_uid));
        CHECK(registry.is_supported(intraoral_image_storage_for_processing_uid));
    }

    SECTION("get_by_modality returns DX classes") {
        auto dx_classes = registry.get_by_modality(modality_type::dx);
        CHECK(dx_classes.size() >= 2);

        // Should contain the main DX classes
        auto it = std::find(dx_classes.begin(), dx_classes.end(),
                           std::string(dx_image_storage_for_presentation_uid));
        CHECK(it != dx_classes.end());
    }
}

// ============================================================================
// DX IOD Validator Tests
// ============================================================================

namespace {

// Helper to create a minimal valid DX dataset
dicom_dataset create_minimal_dx_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, "Test^Patient", vr_type::PN);
    ds.set_string(tags::patient_id, "12345", vr_type::LO);
    ds.set_string(tags::patient_birth_date, "19800101", vr_type::DA);
    ds.set_string(tags::patient_sex, "M", vr_type::CS);

    // General Study Module
    ds.set_string(tags::study_instance_uid, "1.2.3.4.5.6.7.8.9", vr_type::UI);
    ds.set_string(tags::study_date, "20240101", vr_type::DA);
    ds.set_string(tags::study_time, "120000", vr_type::TM);
    ds.set_string(tags::referring_physician_name, "Dr^Referring", vr_type::PN);
    ds.set_string(tags::study_id, "STUDY001", vr_type::SH);
    ds.set_string(tags::accession_number, "ACC001", vr_type::SH);

    // General Series Module
    ds.set_string(tags::modality, "DX", vr_type::CS);
    ds.set_string(tags::series_instance_uid, "1.2.3.4.5.6.7.8.9.1", vr_type::UI);
    ds.set_numeric(tags::series_number, uint32_t{1}, vr_type::IS);

    // Image Pixel Module
    ds.set_numeric(tags::samples_per_pixel, uint16_t{1}, vr_type::US);
    ds.set_string(tags::photometric_interpretation, "MONOCHROME2", vr_type::CS);
    ds.set_numeric(tags::rows, uint16_t{2048}, vr_type::US);
    ds.set_numeric(tags::columns, uint16_t{2048}, vr_type::US);
    ds.set_numeric(tags::bits_allocated, uint16_t{16}, vr_type::US);
    ds.set_numeric(tags::bits_stored, uint16_t{12}, vr_type::US);
    ds.set_numeric(tags::high_bit, uint16_t{11}, vr_type::US);
    ds.set_numeric(tags::pixel_representation, uint16_t{0}, vr_type::US);

    // Pixel Data (minimal placeholder)
    std::vector<uint8_t> pixel_data(2048 * 2048 * 2, 0);
    ds.set_bytes(tags::pixel_data, pixel_data, vr_type::OW);

    // DX Image Module
    ds.set_string(dicom_tag{0x0008, 0x0008}, "ORIGINAL\\PRIMARY", vr_type::CS);  // Image Type
    ds.set_string(dicom_tag{0x0028, 0x1040}, "LIN", vr_type::CS);  // Pixel Intensity Relationship
    ds.set_numeric(dicom_tag{0x0028, 0x1041}, int16_t{1}, vr_type::SS);  // Pixel Intensity Relationship Sign

    // DX Detector Module
    ds.set_string(dicom_tag{0x0018, 0x7004}, "DIRECT", vr_type::CS);  // Detector Type
    ds.set_string(dicom_tag{0x0018, 0x1164}, "0.15\\0.15", vr_type::DS);  // Imager Pixel Spacing

    // DX Anatomy Imaged Module
    ds.set_string(dicom_tag{0x0018, 0x0015}, "CHEST", vr_type::CS);  // Body Part Examined

    // DX Positioning Module
    ds.set_string(dicom_tag{0x0018, 0x5101}, "PA", vr_type::CS);  // View Position

    // SOP Common Module
    ds.set_string(tags::sop_class_uid,
                  std::string(dx_image_storage_for_presentation_uid), vr_type::UI);
    ds.set_string(tags::sop_instance_uid, "1.2.3.4.5.6.7.8.9.2", vr_type::UI);

    return ds;
}

}  // namespace

TEST_CASE("dx_iod_validator validates minimal valid dataset", "[services][dx][validator]") {
    dx_iod_validator validator;
    auto dataset = create_minimal_dx_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("dx_iod_validator detects missing Type 1 attributes", "[services][dx][validator]") {
    dx_iod_validator validator;
    auto dataset = create_minimal_dx_dataset();

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

    SECTION("missing SeriesInstanceUID") {
        dataset.remove(tags::series_instance_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SOPClassUID") {
        dataset.remove(tags::sop_class_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing Rows") {
        dataset.remove(tags::rows);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("dx_iod_validator checks modality value", "[services][dx][validator]") {
    dx_iod_validator validator;
    auto dataset = create_minimal_dx_dataset();

    SECTION("wrong modality") {
        dataset.set_string(tags::modality, "CT", vr_type::CS);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        // Should have an error about modality
        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "DX-ERR-002") {
                found_modality_error = true;
                break;
            }
        }
        CHECK(found_modality_error);
    }
}

TEST_CASE("dx_iod_validator checks photometric interpretation", "[services][dx][validator]") {
    dx_iod_validator validator;
    auto dataset = create_minimal_dx_dataset();

    SECTION("invalid photometric for DX") {
        dataset.set_string(tags::photometric_interpretation, "RGB", vr_type::CS);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "DX-ERR-007") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK(found_photometric_error);
    }

    SECTION("MONOCHROME1 is valid") {
        dataset.set_string(tags::photometric_interpretation, "MONOCHROME1", vr_type::CS);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }
}

TEST_CASE("dx_iod_validator checks SOP Class UID", "[services][dx][validator]") {
    dx_iod_validator validator;
    auto dataset = create_minimal_dx_dataset();

    SECTION("non-DX SOP Class") {
        dataset.set_string(tags::sop_class_uid, "1.2.840.10008.5.1.4.1.1.2", vr_type::UI);  // CT
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("For Processing SOP Class") {
        dataset.set_string(tags::sop_class_uid,
                          std::string(dx_image_storage_for_processing_uid), vr_type::UI);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }
}

TEST_CASE("dx_iod_validator checks pixel data consistency", "[services][dx][validator]") {
    dx_iod_validator validator;
    auto dataset = create_minimal_dx_dataset();

    SECTION("BitsStored exceeds BitsAllocated") {
        dataset.set_numeric(tags::bits_stored, uint16_t{20}, vr_type::US);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("wrong HighBit") {
        dataset.set_numeric(tags::high_bit, uint16_t{15}, vr_type::US);  // Should be 11
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("non-grayscale SamplesPerPixel") {
        dataset.set_numeric(tags::samples_per_pixel, uint16_t{3}, vr_type::US);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("dx_iod_validator quick_check works correctly", "[services][dx][validator]") {
    dx_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_dx_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_dx_dataset();
        dataset.set_string(tags::modality, "CT", vr_type::CS);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_dx_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

TEST_CASE("dx_iod_validator validates For Presentation images", "[services][dx][validator]") {
    dx_iod_validator validator;
    auto dataset = create_minimal_dx_dataset();

    // Add Window Center/Width for presentation
    dataset.set_string(tags::window_center, "2048", vr_type::DS);
    dataset.set_string(tags::window_width, "4096", vr_type::DS);

    auto result = validator.validate_for_presentation(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("dx_iod_validator validates For Processing images", "[services][dx][validator]") {
    dx_iod_validator validator;
    auto dataset = create_minimal_dx_dataset();

    // Change to For Processing SOP Class
    dataset.set_string(tags::sop_class_uid,
                      std::string(dx_image_storage_for_processing_uid), vr_type::UI);

    auto result = validator.validate_for_processing(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("dx_iod_validator with custom options", "[services][dx][validator]") {
    SECTION("strict mode treats warnings as errors") {
        dx_validation_options options;
        options.strict_mode = true;

        dx_iod_validator validator(options);
        auto dataset = create_minimal_dx_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);  // Strict mode makes warnings into errors
    }

    SECTION("can disable pixel data validation") {
        dx_validation_options options;
        options.validate_pixel_data = false;

        dx_iod_validator validator(options);
        auto dataset = create_minimal_dx_dataset();
        dataset.set_numeric(tags::bits_stored, uint16_t{20}, vr_type::US);  // Invalid normally

        auto result = validator.validate(dataset);
        // Should not have pixel data errors when validation is disabled
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "DX-ERR-005") {
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_dx_iod convenience function", "[services][dx][validator]") {
    auto dataset = create_minimal_dx_dataset();
    auto result = validate_dx_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_dx_dataset convenience function", "[services][dx][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_dx_dataset();
        CHECK(is_valid_dx_dataset(dataset));
    }

    SECTION("invalid dataset") {
        auto dataset = create_minimal_dx_dataset();
        dataset.set_string(tags::modality, "CT", vr_type::CS);
        CHECK_FALSE(is_valid_dx_dataset(dataset));
    }
}

TEST_CASE("is_for_presentation_dx detects presentation images", "[services][dx][validator]") {
    auto dataset = create_minimal_dx_dataset();
    CHECK(is_for_presentation_dx(dataset));

    dataset.set_string(tags::sop_class_uid,
                      std::string(dx_image_storage_for_processing_uid), vr_type::UI);
    CHECK_FALSE(is_for_presentation_dx(dataset));
}

TEST_CASE("is_for_processing_dx detects processing images", "[services][dx][validator]") {
    auto dataset = create_minimal_dx_dataset();
    CHECK_FALSE(is_for_processing_dx(dataset));

    dataset.set_string(tags::sop_class_uid,
                      std::string(dx_image_storage_for_processing_uid), vr_type::UI);
    CHECK(is_for_processing_dx(dataset));
}
