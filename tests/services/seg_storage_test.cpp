/**
 * @file seg_storage_test.cpp
 * @brief Unit tests for Segmentation (SEG) Storage SOP Classes and IOD Validator
 */

#include <pacs/services/sop_classes/seg_storage.hpp>
#include <pacs/services/validation/seg_iod_validator.hpp>
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
// SEG Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("SEG Storage SOP Class UIDs are correct", "[services][seg][sop_class]") {
    CHECK(segmentation_storage_uid == "1.2.840.10008.5.1.4.1.1.66.4");
    CHECK(surface_segmentation_storage_uid == "1.2.840.10008.5.1.4.1.1.66.5");
}

TEST_CASE("is_seg_storage_sop_class identifies SEG classes", "[services][seg][sop_class]") {
    SECTION("recognizes standard Segmentation") {
        CHECK(is_seg_storage_sop_class(segmentation_storage_uid));
    }

    SECTION("recognizes Surface Segmentation") {
        CHECK(is_seg_storage_sop_class(surface_segmentation_storage_uid));
    }

    SECTION("rejects non-SEG classes") {
        CHECK_FALSE(is_seg_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));    // CT
        CHECK_FALSE(is_seg_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));    // MR
        CHECK_FALSE(is_seg_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));  // US
        CHECK_FALSE(is_seg_storage_sop_class("1.2.840.10008.1.1"));            // Verification
        CHECK_FALSE(is_seg_storage_sop_class(""));
        CHECK_FALSE(is_seg_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_surface_segmentation_sop_class identifies surface classes", "[services][seg][sop_class]") {
    CHECK(is_surface_segmentation_sop_class(surface_segmentation_storage_uid));
    CHECK_FALSE(is_surface_segmentation_sop_class(segmentation_storage_uid));
    CHECK_FALSE(is_surface_segmentation_sop_class("1.2.840.10008.5.1.4.1.1.2"));
}

// ============================================================================
// SEG SOP Class Information Tests
// ============================================================================

TEST_CASE("get_seg_sop_class_info returns correct information", "[services][seg][sop_class]") {
    SECTION("Segmentation Storage info") {
        const auto* info = get_seg_sop_class_info(segmentation_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == segmentation_storage_uid);
        CHECK(info->name == "Segmentation Storage");
        CHECK_FALSE(info->is_retired);
        CHECK_FALSE(info->is_surface);
    }

    SECTION("Surface Segmentation Storage info") {
        const auto* info = get_seg_sop_class_info(surface_segmentation_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == surface_segmentation_storage_uid);
        CHECK(info->name == "Surface Segmentation Storage");
        CHECK_FALSE(info->is_retired);
        CHECK(info->is_surface);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_seg_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_seg_storage_sop_classes returns correct list", "[services][seg][sop_class]") {
    SECTION("with surface classes") {
        auto classes = get_seg_storage_sop_classes(true);
        CHECK(classes.size() == 2);
    }

    SECTION("without surface classes") {
        auto classes = get_seg_storage_sop_classes(false);
        CHECK(classes.size() == 1);
        CHECK(classes[0] == segmentation_storage_uid);
    }
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_seg_transfer_syntaxes returns valid syntaxes", "[services][seg][transfer]") {
    auto syntaxes = get_seg_transfer_syntaxes();

    CHECK(syntaxes.size() > 0);

    // Should include Explicit VR Little Endian (most preferred)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1") != syntaxes.end());

    // Should include Implicit VR Little Endian (universal baseline)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2") != syntaxes.end());
}

// ============================================================================
// Segmentation Type Tests
// ============================================================================

TEST_CASE("segmentation_type conversions", "[services][seg][segmentation_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(segmentation_type::binary) == "BINARY");
        CHECK(to_string(segmentation_type::fractional) == "FRACTIONAL");
    }

    SECTION("parse_segmentation_type parses correctly") {
        CHECK(parse_segmentation_type("BINARY") == segmentation_type::binary);
        CHECK(parse_segmentation_type("FRACTIONAL") == segmentation_type::fractional);
        CHECK(parse_segmentation_type("UNKNOWN") == segmentation_type::binary);  // Default
    }
}

TEST_CASE("is_valid_segmentation_type validates correctly", "[services][seg][segmentation_type]") {
    CHECK(is_valid_segmentation_type("BINARY"));
    CHECK(is_valid_segmentation_type("FRACTIONAL"));

    CHECK_FALSE(is_valid_segmentation_type("INVALID"));
    CHECK_FALSE(is_valid_segmentation_type(""));
}

// ============================================================================
// Segmentation Fractional Type Tests
// ============================================================================

TEST_CASE("segmentation_fractional_type conversions", "[services][seg][fractional_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(segmentation_fractional_type::probability) == "PROBABILITY");
        CHECK(to_string(segmentation_fractional_type::occupancy) == "OCCUPANCY");
    }

    SECTION("parse_segmentation_fractional_type parses correctly") {
        CHECK(parse_segmentation_fractional_type("PROBABILITY") ==
              segmentation_fractional_type::probability);
        CHECK(parse_segmentation_fractional_type("OCCUPANCY") ==
              segmentation_fractional_type::occupancy);
        CHECK(parse_segmentation_fractional_type("UNKNOWN") ==
              segmentation_fractional_type::probability);  // Default
    }
}

// ============================================================================
// Segment Algorithm Type Tests
// ============================================================================

TEST_CASE("segment_algorithm_type conversions", "[services][seg][algorithm_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(segment_algorithm_type::automatic) == "AUTOMATIC");
        CHECK(to_string(segment_algorithm_type::semiautomatic) == "SEMIAUTOMATIC");
        CHECK(to_string(segment_algorithm_type::manual) == "MANUAL");
    }

    SECTION("parse_segment_algorithm_type parses correctly") {
        CHECK(parse_segment_algorithm_type("AUTOMATIC") == segment_algorithm_type::automatic);
        CHECK(parse_segment_algorithm_type("SEMIAUTOMATIC") == segment_algorithm_type::semiautomatic);
        CHECK(parse_segment_algorithm_type("MANUAL") == segment_algorithm_type::manual);
        CHECK(parse_segment_algorithm_type("UNKNOWN") == segment_algorithm_type::manual);  // Default
    }
}

TEST_CASE("is_valid_segment_algorithm_type validates correctly", "[services][seg][algorithm_type]") {
    CHECK(is_valid_segment_algorithm_type("AUTOMATIC"));
    CHECK(is_valid_segment_algorithm_type("SEMIAUTOMATIC"));
    CHECK(is_valid_segment_algorithm_type("MANUAL"));

    CHECK_FALSE(is_valid_segment_algorithm_type("INVALID"));
    CHECK_FALSE(is_valid_segment_algorithm_type(""));
}

// ============================================================================
// Segment Color Tests
// ============================================================================

TEST_CASE("get_recommended_segment_color returns valid colors", "[services][seg][color]") {
    SECTION("known anatomical structures have specific colors") {
        auto liver_color = get_recommended_segment_color("Liver");
        // Liver should have a distinct color (brownish red in CIELab)
        CHECK(liver_color.l > 0);
        CHECK(liver_color.a > 0);  // Positive a* indicates red
    }

    SECTION("tumors have specific colors") {
        auto tumor_color = get_recommended_segment_color("Tumor");
        // Tumors are typically shown in red/yellow
        CHECK(tumor_color.l > 0);
    }

    SECTION("unknown structures get default gray") {
        auto unknown_color = get_recommended_segment_color("UnknownStructure123");
        // Default should be neutral gray
        CHECK(unknown_color.l > 0);
    }
}

// ============================================================================
// Segment Category Tests
// ============================================================================

TEST_CASE("segment_category codes are correct", "[services][seg][category]") {
    CHECK(get_segment_category_code(segment_category::tissue).size() > 0);
    CHECK(get_segment_category_code(segment_category::anatomical_structure).size() > 0);
    CHECK(get_segment_category_code(segment_category::morphologically_abnormal).size() > 0);
}

TEST_CASE("segment_category meanings are correct", "[services][seg][category]") {
    CHECK(get_segment_category_meaning(segment_category::tissue) == "Tissue");
    CHECK(get_segment_category_meaning(segment_category::anatomical_structure) == "Anatomical Structure");
    CHECK(get_segment_category_meaning(segment_category::morphologically_abnormal) == "Morphologically Abnormal Structure");
}

// ============================================================================
// SEG IOD Validator Tests
// ============================================================================

namespace {

dicom_dataset create_minimal_seg_dataset() {
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
    ds.set_string(tags::modality, vr_type::CS, "SEG");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.124");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // Frame of Reference Module
    ds.set_string(tags::frame_of_reference_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.126");

    // Segmentation Image Module
    ds.set_string(tags::image_type, vr_type::CS, "DERIVED\\PRIMARY");
    ds.set_string(tags::instance_number, vr_type::IS, "1");
    ds.set_string(tags::content_date, vr_type::DA, "20231201");
    ds.set_string(tags::content_time, vr_type::TM, "120000");
    ds.set_string(dicom_tag{0x0062, 0x0001}, vr_type::CS, "BINARY");  // Segmentation Type
    ds.set_string(dicom_tag{0x0020, 0x4000}, vr_type::CS, "AI_SEG");  // Content Label
    ds.set_string(dicom_tag{0x0070, 0x0081}, vr_type::LO, "AI Segmentation Result");  // Content Description
    ds.set_string(dicom_tag{0x0070, 0x0084}, vr_type::PN, "AI^ALGORITHM");  // Content Creator Name

    // Image Pixel Module (for SEG)
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 512);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 1);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 1);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 0);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);
    ds.set_string(tags::pixel_data, vr_type::OB, "dummy_pixel_data");
    ds.set_string(dicom_tag{0x0028, 0x0008}, vr_type::IS, "1");  // Number of Frames

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(segmentation_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.125");

    return ds;
}

}  // namespace

TEST_CASE("seg_iod_validator validates complete dataset", "[services][seg][validation]") {
    seg_iod_validator validator;
    auto dataset = create_minimal_seg_dataset();

    auto result = validator.validate(dataset);
    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("seg_iod_validator detects missing Type 1 attributes", "[services][seg][validation]") {
    seg_iod_validator validator;
    auto dataset = create_minimal_seg_dataset();

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

    SECTION("missing SOPClassUID") {
        dataset.remove(tags::sop_class_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing FrameOfReferenceUID") {
        dataset.remove(tags::frame_of_reference_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("seg_iod_validator detects wrong modality", "[services][seg][validation]") {
    seg_iod_validator validator;
    auto dataset = create_minimal_seg_dataset();

    dataset.set_string(tags::modality, vr_type::CS, "CT");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
    CHECK(result.has_errors());
}

TEST_CASE("seg_iod_validator detects invalid SOP Class", "[services][seg][validation]") {
    seg_iod_validator validator;
    auto dataset = create_minimal_seg_dataset();

    // Set to CT SOP Class
    dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
}

TEST_CASE("seg_iod_validator quick_check works correctly", "[services][seg][validation]") {
    seg_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_seg_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid dataset fails quick check") {
        auto dataset = create_minimal_seg_dataset();
        dataset.remove(tags::modality);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("wrong modality fails quick check") {
        auto dataset = create_minimal_seg_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

TEST_CASE("seg_iod_validator options work correctly", "[services][seg][validation]") {
    SECTION("can disable Type 2 checking") {
        seg_validation_options options;
        options.check_type1 = true;
        options.check_type2 = false;

        seg_iod_validator validator{options};
        auto dataset = create_minimal_seg_dataset();
        dataset.remove(tags::patient_name);  // Type 2

        auto result = validator.validate(dataset);
        CHECK(result.is_valid);  // Should pass without Type 2 errors
    }

    SECTION("strict mode treats warnings as errors") {
        seg_validation_options options;
        options.strict_mode = true;

        seg_iod_validator validator{options};
        auto dataset = create_minimal_seg_dataset();

        // Remove a Type 2 attribute to generate a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        // In strict mode, the warning becomes an error
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("SEG SOP classes are registered in central registry", "[services][seg][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("Segmentation Storage is registered") {
        CHECK(registry.is_supported(segmentation_storage_uid));
        const auto* info = registry.get_info(segmentation_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::seg);
    }

    SECTION("Surface Segmentation Storage is registered") {
        CHECK(registry.is_supported(surface_segmentation_storage_uid));
        const auto* info = registry.get_info(surface_segmentation_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::seg);
    }

    SECTION("SEG classes are returned by modality query") {
        auto seg_classes = registry.get_by_modality(modality_type::seg, true);
        CHECK(seg_classes.size() == 2);

        for (const auto& uid : seg_classes) {
            const auto* info = registry.get_info(uid);
            REQUIRE(info != nullptr);
            CHECK(info->modality == modality_type::seg);
        }
    }
}

TEST_CASE("SEG modality parsing works correctly", "[services][seg][registry]") {
    CHECK(sop_class_registry::parse_modality("SEG") == modality_type::seg);
    CHECK(sop_class_registry::modality_to_string(modality_type::seg) == "SEG");
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_seg_iod convenience function", "[services][seg][validation]") {
    auto dataset = create_minimal_seg_dataset();
    auto result = validate_seg_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_seg_dataset convenience function", "[services][seg][validation]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_seg_dataset();
        CHECK(is_valid_seg_dataset(dataset));
    }

    SECTION("invalid dataset") {
        dicom_dataset empty_dataset;
        CHECK_FALSE(is_valid_seg_dataset(empty_dataset));
    }
}
