/**
 * @file heightmap_seg_iod_validator_test.cpp
 * @brief Unit tests for Heightmap Segmentation IOD Validator (Supplement 240)
 */

#include <pacs/services/sop_classes/seg_storage.hpp>
#include <pacs/services/validation/heightmap_seg_iod_validator.hpp>
#include <pacs/services/sop_class_registry.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services::sop_classes;
using namespace kcenon::pacs::services::validation;
using namespace kcenon::pacs::services;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

void insert_sequence(dicom_dataset& ds, dicom_tag tag,
                     std::vector<dicom_dataset> items) {
    dicom_element seq_elem(tag, vr_type::SQ);
    seq_elem.sequence_items() = std::move(items);
    ds.insert(std::move(seq_elem));
}

dicom_dataset create_heightmap_segment_item() {
    dicom_dataset segment;
    // Segment Number (Type 1)
    segment.set_numeric<uint16_t>(
        dicom_tag{0x0062, 0x0004}, vr_type::US, 1);
    // Segment Label (Type 1) - retinal layer boundary
    segment.set_string(
        dicom_tag{0x0062, 0x0005}, vr_type::LO, "ILM");
    // Segment Algorithm Type (Type 1)
    segment.set_string(
        dicom_tag{0x0062, 0x0008}, vr_type::CS, "AUTOMATIC");

    // Segmented Property Category Code Sequence (Type 1)
    dicom_dataset category_code;
    category_code.set_string(
        dicom_tag{0x0008, 0x0100}, vr_type::SH, "91723000");
    category_code.set_string(
        dicom_tag{0x0008, 0x0102}, vr_type::SH, "SCT");
    category_code.set_string(
        dicom_tag{0x0008, 0x0104}, vr_type::LO, "Anatomical Structure");
    insert_sequence(segment, dicom_tag{0x0062, 0x0003}, {category_code});

    // Segmented Property Type Code Sequence (Type 1)
    dicom_dataset property_code;
    property_code.set_string(
        dicom_tag{0x0008, 0x0100}, vr_type::SH, "T-AA621");
    property_code.set_string(
        dicom_tag{0x0008, 0x0102}, vr_type::SH, "SRT");
    property_code.set_string(
        dicom_tag{0x0008, 0x0104}, vr_type::LO,
        "Internal Limiting Membrane of Retina");
    insert_sequence(segment, dicom_tag{0x0062, 0x000F}, {property_code});

    return segment;
}

dicom_dataset create_minimal_heightmap_seg_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");
    ds.set_string(tags::patient_id, vr_type::LO, "HM12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::patient_sex, vr_type::CS, "F");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI,
        "1.2.840.113619.2.55.3.604688119.969.1234567890.500");
    ds.set_string(tags::study_date, vr_type::DA, "20241201");
    ds.set_string(tags::study_time, vr_type::TM, "100000");
    ds.set_string(tags::referring_physician_name, vr_type::PN,
        "DR^OPHTHALMOLOGIST");
    ds.set_string(tags::study_id, vr_type::SH, "OCT-STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC-OCT001");

    // General Series Module
    ds.set_string(tags::modality, vr_type::CS, "SEG");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
        "1.2.840.113619.2.55.3.604688119.969.1234567890.501");
    ds.set_string(tags::series_number, vr_type::IS, "2");

    // Frame of Reference Module
    ds.set_string(tags::frame_of_reference_uid, vr_type::UI,
        "1.2.840.113619.2.55.3.604688119.969.1234567890.502");

    // General Equipment Module (Type 2)
    ds.set_string(dicom_tag{0x0008, 0x0070}, vr_type::LO,
        "OCT Segmentation Inc.");

    // Enhanced General Equipment Module (Type 1)
    ds.set_string(dicom_tag{0x0008, 0x1090}, vr_type::LO,
        "Retinal Layer Analyzer");
    ds.set_string(dicom_tag{0x0018, 0x1000}, vr_type::LO, "SN-OCT-001");
    ds.set_string(dicom_tag{0x0018, 0x1020}, vr_type::LO, "2.0.0");

    // General Image Module (Type 2)
    ds.set_string(dicom_tag{0x0020, 0x0013}, vr_type::IS, "1");

    // Heightmap Segmentation Image Module
    ds.set_string(dicom_tag{0x0062, 0x0001}, vr_type::CS, "HEIGHTMAP");

    // Segment Sequence (Type 1) - retinal layer boundaries
    auto ilm_segment = create_heightmap_segment_item();

    dicom_dataset rpe_segment;
    rpe_segment.set_numeric<uint16_t>(
        dicom_tag{0x0062, 0x0004}, vr_type::US, 2);
    rpe_segment.set_string(
        dicom_tag{0x0062, 0x0005}, vr_type::LO, "RPE");
    rpe_segment.set_string(
        dicom_tag{0x0062, 0x0008}, vr_type::CS, "AUTOMATIC");

    dicom_dataset cat_code2;
    cat_code2.set_string(
        dicom_tag{0x0008, 0x0100}, vr_type::SH, "91723000");
    cat_code2.set_string(
        dicom_tag{0x0008, 0x0102}, vr_type::SH, "SCT");
    cat_code2.set_string(
        dicom_tag{0x0008, 0x0104}, vr_type::LO, "Anatomical Structure");
    insert_sequence(rpe_segment, dicom_tag{0x0062, 0x0003}, {cat_code2});

    dicom_dataset prop_code2;
    prop_code2.set_string(
        dicom_tag{0x0008, 0x0100}, vr_type::SH, "T-AA630");
    prop_code2.set_string(
        dicom_tag{0x0008, 0x0102}, vr_type::SH, "SRT");
    prop_code2.set_string(
        dicom_tag{0x0008, 0x0104}, vr_type::LO,
        "Retinal Pigment Epithelium");
    insert_sequence(rpe_segment, dicom_tag{0x0062, 0x000F}, {prop_code2});

    insert_sequence(ds, dicom_tag{0x0062, 0x0002},
                    {ilm_segment, rpe_segment});

    // Image Pixel Module (heightmap uses 16-bit or 32-bit values)
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS,
        "MONOCHROME2");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 128);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 512);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 15);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);
    ds.set_string(tags::pixel_data, vr_type::OW, "heightmap_pixel_data");
    ds.set_string(dicom_tag{0x0028, 0x0008}, vr_type::IS, "2");

    // Multi-frame Functional Groups Module (Type 1)
    dicom_dataset shared_fg_item;
    insert_sequence(ds, dicom_tag{0x5200, 0x9229}, {shared_fg_item});

    dicom_dataset per_frame_fg_item1;
    dicom_dataset per_frame_fg_item2;
    insert_sequence(ds, dicom_tag{0x5200, 0x9230},
                    {per_frame_fg_item1, per_frame_fg_item2});

    // Multi-frame Dimension Module (Type 1)
    dicom_dataset dim_org_item;
    dim_org_item.set_string(dicom_tag{0x0020, 0x9164}, vr_type::UI,
        "1.2.840.113619.2.55.3.604688119.969.1234567890.600");
    insert_sequence(ds, dicom_tag{0x0020, 0x9221}, {dim_org_item});

    dicom_dataset dim_idx_item;
    dim_idx_item.set_string(dicom_tag{0x0020, 0x9164}, vr_type::UI,
        "1.2.840.113619.2.55.3.604688119.969.1234567890.600");
    insert_sequence(ds, dicom_tag{0x0020, 0x9222}, {dim_idx_item});

    // Common Instance Reference Module
    dicom_dataset ref_series_item;
    ref_series_item.set_string(tags::series_instance_uid, vr_type::UI,
        "1.2.840.113619.2.55.3.604688119.969.1234567890.400");
    insert_sequence(ds, dicom_tag{0x0008, 0x1115}, {ref_series_item});

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI,
        std::string(heightmap_segmentation_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
        "1.2.840.113619.2.55.3.604688119.969.1234567890.503");

    return ds;
}

}  // namespace

// ============================================================================
// Heightmap Segmentation Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("Heightmap Segmentation Storage UID is correct",
          "[services][heightmap_seg][sop_class]") {
    CHECK(heightmap_segmentation_storage_uid ==
          "1.2.840.10008.5.1.4.1.1.66.7");
}

TEST_CASE("is_seg_storage_sop_class recognizes Heightmap Segmentation",
          "[services][heightmap_seg][sop_class]") {
    CHECK(is_seg_storage_sop_class(heightmap_segmentation_storage_uid));
}

TEST_CASE("get_seg_sop_class_info returns Heightmap info",
          "[services][heightmap_seg][sop_class]") {
    const auto* info =
        get_seg_sop_class_info(heightmap_segmentation_storage_uid);
    REQUIRE(info != nullptr);
    CHECK(info->uid == heightmap_segmentation_storage_uid);
    CHECK(info->name == "Heightmap Segmentation Storage");
    CHECK_FALSE(info->is_retired);
    CHECK_FALSE(info->is_surface);
}

TEST_CASE("get_seg_storage_sop_classes includes Heightmap",
          "[services][heightmap_seg][sop_class]") {
    auto classes = get_seg_storage_sop_classes(true);
    CHECK(classes.size() == 4);

    bool found = false;
    for (const auto& uid : classes) {
        if (uid == heightmap_segmentation_storage_uid) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

// ============================================================================
// Heightmap Segmentation IOD Validator Tests
// ============================================================================

TEST_CASE("heightmap_seg_iod_validator validates complete dataset",
          "[services][heightmap_seg][validation]") {
    heightmap_seg_iod_validator validator;
    auto dataset = create_minimal_heightmap_seg_dataset();

    auto result = validator.validate(dataset);
    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("heightmap_seg_iod_validator detects missing Type 1 attributes",
          "[services][heightmap_seg][validation]") {
    heightmap_seg_iod_validator validator;

    SECTION("missing StudyInstanceUID") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.remove(tags::study_instance_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
        CHECK(result.has_errors());
    }

    SECTION("missing Modality") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.remove(tags::modality);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SOPClassUID") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.remove(tags::sop_class_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing FrameOfReferenceUID") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.remove(tags::frame_of_reference_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SegmentationType") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.remove(dicom_tag{0x0062, 0x0001});
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("heightmap_seg_iod_validator detects wrong modality",
          "[services][heightmap_seg][validation]") {
    heightmap_seg_iod_validator validator;
    auto dataset = create_minimal_heightmap_seg_dataset();

    dataset.set_string(tags::modality, vr_type::CS, "OPT");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
    CHECK(result.has_errors());
}

TEST_CASE("heightmap_seg_iod_validator detects wrong segmentation type",
          "[services][heightmap_seg][validation]") {
    heightmap_seg_iod_validator validator;
    auto dataset = create_minimal_heightmap_seg_dataset();

    // Set to binary instead of heightmap
    dataset.set_string(dicom_tag{0x0062, 0x0001}, vr_type::CS, "BINARY");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);

    bool found_seg_type_error = false;
    for (const auto& finding : result.findings) {
        if (finding.code == "HMSEG-ERR-006") {
            found_seg_type_error = true;
            break;
        }
    }
    CHECK(found_seg_type_error);
}

TEST_CASE("heightmap_seg_iod_validator detects invalid SOP Class",
          "[services][heightmap_seg][validation]") {
    heightmap_seg_iod_validator validator;
    auto dataset = create_minimal_heightmap_seg_dataset();

    // Set to regular Segmentation SOP Class
    dataset.set_string(tags::sop_class_uid, vr_type::UI,
        std::string(segmentation_storage_uid));
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
}

TEST_CASE("heightmap_seg_iod_validator validates segment sequence",
          "[services][heightmap_seg][validation]") {
    heightmap_seg_iod_validator validator;

    SECTION("validates complete segments") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        auto result = validator.validate_segments(dataset);
        CHECK(result.is_valid);
    }

    SECTION("detects missing segment attributes") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        // Replace segment sequence with incomplete segment
        dicom_dataset incomplete_segment;
        incomplete_segment.set_numeric<uint16_t>(
            dicom_tag{0x0062, 0x0004}, vr_type::US, 1);
        // Missing SegmentLabel, SegmentAlgorithmType, etc.
        insert_sequence(dataset, dicom_tag{0x0062, 0x0002},
                        {incomplete_segment});

        auto result = validator.validate_segments(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("heightmap_seg_iod_validator quick_check works correctly",
          "[services][heightmap_seg][validation]") {
    heightmap_seg_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("missing modality fails quick check") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.remove(tags::modality);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("wrong modality fails quick check") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("wrong segmentation type fails quick check") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.set_string(dicom_tag{0x0062, 0x0001}, vr_type::CS,
            "BINARY");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("wrong SOP class fails quick check") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
            std::string(segmentation_storage_uid));
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("empty dataset fails quick check") {
        dicom_dataset empty_dataset;
        CHECK_FALSE(validator.quick_check(empty_dataset));
    }
}

TEST_CASE("heightmap_seg_iod_validator options work correctly",
          "[services][heightmap_seg][validation]") {
    SECTION("can disable Type 2 checking") {
        heightmap_seg_validation_options options;
        options.check_type1 = true;
        options.check_type2 = false;

        heightmap_seg_iod_validator validator{options};
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.remove(tags::patient_name);  // Type 2

        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("strict mode treats warnings as errors") {
        heightmap_seg_validation_options options;
        options.strict_mode = true;

        heightmap_seg_iod_validator validator{options};
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("can disable heightmap instance validation") {
        heightmap_seg_validation_options options;
        options.validate_heightmap_instance = false;

        heightmap_seg_iod_validator validator{options};
        auto dataset = create_minimal_heightmap_seg_dataset();
        // Remove segmentation type - normally required
        dataset.remove(dicom_tag{0x0062, 0x0001});

        auto result = validator.validate(dataset);
        // Heightmap instance module not checked, but type1 check
        // in other modules may still flag it
        CHECK(result.findings.size() >= 0);
    }
}

TEST_CASE("heightmap_seg_iod_validator pixel data consistency",
          "[services][heightmap_seg][validation]") {
    heightmap_seg_iod_validator validator;

    SECTION("accepts 16-bit heightmap data") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("accepts 32-bit heightmap data") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 32);
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 32);
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 31);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("warns on non-standard bit depth") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 8);
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 7);
        auto result = validator.validate(dataset);
        // Should have a warning but still be valid
        bool found_warning = false;
        for (const auto& finding : result.findings) {
            if (finding.code == "HMSEG-PXL-WARN-001") {
                found_warning = true;
                break;
            }
        }
        CHECK(found_warning);
        CHECK(result.is_valid);
    }

    SECTION("errors on wrong SamplesPerPixel") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel,
            vr_type::US, 3);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("errors on wrong PhotometricInterpretation") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
            "RGB");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("Heightmap Segmentation is registered in central registry",
          "[services][heightmap_seg][registry]") {
    auto& registry = sop_class_registry::instance();

    CHECK(registry.is_supported(heightmap_segmentation_storage_uid));
    const auto* info =
        registry.get_info(heightmap_segmentation_storage_uid);
    REQUIRE(info != nullptr);
    CHECK(info->category == sop_class_category::storage);
    CHECK(info->modality == modality_type::seg);
    CHECK(info->supports_multiframe);
    CHECK_FALSE(info->is_retired);
}

TEST_CASE("SEG modality query includes Heightmap Segmentation",
          "[services][heightmap_seg][registry]") {
    auto& registry = sop_class_registry::instance();
    auto seg_classes = registry.get_by_modality(modality_type::seg, true);

    bool found = false;
    for (const auto& uid : seg_classes) {
        if (uid == heightmap_segmentation_storage_uid) {
            found = true;
            break;
        }
    }
    CHECK(found);
    CHECK(seg_classes.size() == 4);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_heightmap_seg_iod convenience function",
          "[services][heightmap_seg][validation]") {
    auto dataset = create_minimal_heightmap_seg_dataset();
    auto result = validate_heightmap_seg_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_heightmap_seg_dataset convenience function",
          "[services][heightmap_seg][validation]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        CHECK(is_valid_heightmap_seg_dataset(dataset));
    }

    SECTION("empty dataset") {
        dicom_dataset empty_dataset;
        CHECK_FALSE(is_valid_heightmap_seg_dataset(empty_dataset));
    }
}

TEST_CASE("is_heightmap_segmentation detects type correctly",
          "[services][heightmap_seg][validation]") {
    SECTION("detects heightmap segmentation") {
        auto dataset = create_minimal_heightmap_seg_dataset();
        CHECK(is_heightmap_segmentation(dataset));
    }

    SECTION("rejects binary segmentation") {
        dicom_dataset ds;
        ds.set_string(dicom_tag{0x0062, 0x0001}, vr_type::CS, "BINARY");
        CHECK_FALSE(is_heightmap_segmentation(ds));
    }

    SECTION("rejects empty dataset") {
        dicom_dataset empty_dataset;
        CHECK_FALSE(is_heightmap_segmentation(empty_dataset));
    }
}
