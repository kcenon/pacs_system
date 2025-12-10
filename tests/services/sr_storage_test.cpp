/**
 * @file sr_storage_test.cpp
 * @brief Unit tests for Structured Report (SR) Storage SOP Classes and IOD Validator
 */

#include <pacs/services/sop_classes/sr_storage.hpp>
#include <pacs/services/validation/sr_iod_validator.hpp>
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
// SR Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("SR Storage SOP Class UIDs are correct", "[services][sr][sop_class]") {
    CHECK(basic_text_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.11");
    CHECK(enhanced_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.22");
    CHECK(comprehensive_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.33");
    CHECK(comprehensive_3d_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.34");
    CHECK(extensible_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.35");
    CHECK(key_object_selection_document_storage_uid == "1.2.840.10008.5.1.4.1.1.88.59");
    CHECK(mammography_cad_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.50");
    CHECK(chest_cad_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.65");
    CHECK(colon_cad_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.69");
    CHECK(xray_radiation_dose_sr_storage_uid == "1.2.840.10008.5.1.4.1.1.88.67");
}

TEST_CASE("is_sr_storage_sop_class identifies SR classes", "[services][sr][sop_class]") {
    SECTION("recognizes Basic Text SR") {
        CHECK(is_sr_storage_sop_class(basic_text_sr_storage_uid));
    }

    SECTION("recognizes Enhanced SR") {
        CHECK(is_sr_storage_sop_class(enhanced_sr_storage_uid));
    }

    SECTION("recognizes Comprehensive SR") {
        CHECK(is_sr_storage_sop_class(comprehensive_sr_storage_uid));
    }

    SECTION("recognizes Comprehensive 3D SR") {
        CHECK(is_sr_storage_sop_class(comprehensive_3d_sr_storage_uid));
    }

    SECTION("recognizes Key Object Selection") {
        CHECK(is_sr_storage_sop_class(key_object_selection_document_storage_uid));
    }

    SECTION("recognizes CAD SR classes") {
        CHECK(is_sr_storage_sop_class(mammography_cad_sr_storage_uid));
        CHECK(is_sr_storage_sop_class(chest_cad_sr_storage_uid));
        CHECK(is_sr_storage_sop_class(colon_cad_sr_storage_uid));
    }

    SECTION("recognizes Dose Report SR classes") {
        CHECK(is_sr_storage_sop_class(xray_radiation_dose_sr_storage_uid));
        CHECK(is_sr_storage_sop_class(radiopharmaceutical_radiation_dose_sr_storage_uid));
        CHECK(is_sr_storage_sop_class(patient_radiation_dose_sr_storage_uid));
    }

    SECTION("rejects non-SR classes") {
        CHECK_FALSE(is_sr_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));     // CT
        CHECK_FALSE(is_sr_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));     // MR
        CHECK_FALSE(is_sr_storage_sop_class("1.2.840.10008.5.1.4.1.1.66.4")); // SEG
        CHECK_FALSE(is_sr_storage_sop_class("1.2.840.10008.1.1"));             // Verification
        CHECK_FALSE(is_sr_storage_sop_class(""));
        CHECK_FALSE(is_sr_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_cad_sr_storage_sop_class identifies CAD SR classes", "[services][sr][sop_class]") {
    CHECK(is_cad_sr_storage_sop_class(mammography_cad_sr_storage_uid));
    CHECK(is_cad_sr_storage_sop_class(chest_cad_sr_storage_uid));
    CHECK(is_cad_sr_storage_sop_class(colon_cad_sr_storage_uid));

    CHECK_FALSE(is_cad_sr_storage_sop_class(basic_text_sr_storage_uid));
    CHECK_FALSE(is_cad_sr_storage_sop_class(comprehensive_sr_storage_uid));
    CHECK_FALSE(is_cad_sr_storage_sop_class(xray_radiation_dose_sr_storage_uid));
}

TEST_CASE("is_dose_sr_storage_sop_class identifies Dose SR classes", "[services][sr][sop_class]") {
    CHECK(is_dose_sr_storage_sop_class(xray_radiation_dose_sr_storage_uid));
    CHECK(is_dose_sr_storage_sop_class(radiopharmaceutical_radiation_dose_sr_storage_uid));
    CHECK(is_dose_sr_storage_sop_class(patient_radiation_dose_sr_storage_uid));
    CHECK(is_dose_sr_storage_sop_class(enhanced_xray_radiation_dose_sr_storage_uid));

    CHECK_FALSE(is_dose_sr_storage_sop_class(basic_text_sr_storage_uid));
    CHECK_FALSE(is_dose_sr_storage_sop_class(mammography_cad_sr_storage_uid));
}

TEST_CASE("sr_supports_spatial_coords identifies spatial coord support", "[services][sr][sop_class]") {
    CHECK(sr_supports_spatial_coords(comprehensive_sr_storage_uid));
    CHECK(sr_supports_spatial_coords(comprehensive_3d_sr_storage_uid));
    CHECK(sr_supports_spatial_coords(extensible_sr_storage_uid));
    CHECK(sr_supports_spatial_coords(mammography_cad_sr_storage_uid));

    CHECK_FALSE(sr_supports_spatial_coords(basic_text_sr_storage_uid));
    CHECK_FALSE(sr_supports_spatial_coords(key_object_selection_document_storage_uid));
}

// ============================================================================
// SR SOP Class Information Tests
// ============================================================================

TEST_CASE("get_sr_sop_class_info returns correct information", "[services][sr][sop_class]") {
    SECTION("Basic Text SR Storage info") {
        const auto* info = get_sr_sop_class_info(basic_text_sr_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == basic_text_sr_storage_uid);
        CHECK(info->name == "Basic Text SR Storage");
        CHECK_FALSE(info->is_retired);
        CHECK_FALSE(info->supports_spatial_coords);
        CHECK(info->document_type == sr_document_type::basic_text);
    }

    SECTION("Comprehensive SR Storage info") {
        const auto* info = get_sr_sop_class_info(comprehensive_sr_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == comprehensive_sr_storage_uid);
        CHECK(info->name == "Comprehensive SR Storage");
        CHECK_FALSE(info->is_retired);
        CHECK(info->supports_spatial_coords);
        CHECK(info->document_type == sr_document_type::comprehensive);
    }

    SECTION("Mammography CAD SR Storage info") {
        const auto* info = get_sr_sop_class_info(mammography_cad_sr_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == mammography_cad_sr_storage_uid);
        CHECK_FALSE(info->is_retired);
        CHECK(info->document_type == sr_document_type::cad);
    }

    SECTION("Key Object Selection info") {
        const auto* info = get_sr_sop_class_info(key_object_selection_document_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->document_type == sr_document_type::key_object_selection);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_sr_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_sr_storage_sop_classes returns correct list", "[services][sr][sop_class]") {
    SECTION("all SR classes") {
        auto classes = get_sr_storage_sop_classes(true, true);
        CHECK(classes.size() == 17);  // All 17 SR SOP classes
    }

    SECTION("without CAD classes") {
        auto classes = get_sr_storage_sop_classes(false, true);
        // Should exclude 3 CAD classes
        for (const auto& uid : classes) {
            CHECK_FALSE(is_cad_sr_storage_sop_class(uid));
        }
    }

    SECTION("without Dose classes") {
        auto classes = get_sr_storage_sop_classes(true, false);
        // Should exclude dose report classes
        for (const auto& uid : classes) {
            CHECK_FALSE(is_dose_sr_storage_sop_class(uid));
        }
    }
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_sr_transfer_syntaxes returns valid syntaxes", "[services][sr][transfer]") {
    auto syntaxes = get_sr_transfer_syntaxes();

    CHECK(syntaxes.size() > 0);

    // Should include Explicit VR Little Endian (most preferred)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1") != syntaxes.end());

    // Should include Implicit VR Little Endian (universal baseline)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2") != syntaxes.end());

    // Should include deflated for large SRs
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1.99") != syntaxes.end());
}

// ============================================================================
// SR Document Type Tests
// ============================================================================

TEST_CASE("get_sr_document_type returns correct types", "[services][sr][document_type]") {
    CHECK(get_sr_document_type(basic_text_sr_storage_uid) == sr_document_type::basic_text);
    CHECK(get_sr_document_type(enhanced_sr_storage_uid) == sr_document_type::enhanced);
    CHECK(get_sr_document_type(comprehensive_sr_storage_uid) == sr_document_type::comprehensive);
    CHECK(get_sr_document_type(comprehensive_3d_sr_storage_uid) == sr_document_type::comprehensive_3d);
    CHECK(get_sr_document_type(extensible_sr_storage_uid) == sr_document_type::extensible);
    CHECK(get_sr_document_type(key_object_selection_document_storage_uid) == sr_document_type::key_object_selection);
    CHECK(get_sr_document_type(mammography_cad_sr_storage_uid) == sr_document_type::cad);
    CHECK(get_sr_document_type(xray_radiation_dose_sr_storage_uid) == sr_document_type::dose_report);
}

TEST_CASE("sr_document_type to_string conversions", "[services][sr][document_type]") {
    CHECK(to_string(sr_document_type::basic_text) == "Basic Text SR");
    CHECK(to_string(sr_document_type::enhanced) == "Enhanced SR");
    CHECK(to_string(sr_document_type::comprehensive) == "Comprehensive SR");
    CHECK(to_string(sr_document_type::comprehensive_3d) == "Comprehensive 3D SR");
    CHECK(to_string(sr_document_type::extensible) == "Extensible SR");
    CHECK(to_string(sr_document_type::key_object_selection) == "Key Object Selection");
    CHECK(to_string(sr_document_type::cad) == "CAD SR");
    CHECK(to_string(sr_document_type::dose_report) == "Dose Report SR");
}

// ============================================================================
// SR Value Type Tests
// ============================================================================

TEST_CASE("sr_value_type conversions", "[services][sr][value_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(sr_value_type::text) == "TEXT");
        CHECK(to_string(sr_value_type::code) == "CODE");
        CHECK(to_string(sr_value_type::num) == "NUM");
        CHECK(to_string(sr_value_type::datetime) == "DATETIME");
        CHECK(to_string(sr_value_type::date) == "DATE");
        CHECK(to_string(sr_value_type::time) == "TIME");
        CHECK(to_string(sr_value_type::uidref) == "UIDREF");
        CHECK(to_string(sr_value_type::pname) == "PNAME");
        CHECK(to_string(sr_value_type::composite) == "COMPOSITE");
        CHECK(to_string(sr_value_type::image) == "IMAGE");
        CHECK(to_string(sr_value_type::waveform) == "WAVEFORM");
        CHECK(to_string(sr_value_type::scoord) == "SCOORD");
        CHECK(to_string(sr_value_type::scoord3d) == "SCOORD3D");
        CHECK(to_string(sr_value_type::tcoord) == "TCOORD");
        CHECK(to_string(sr_value_type::container) == "CONTAINER");
        CHECK(to_string(sr_value_type::table) == "TABLE");
    }

    SECTION("parse_sr_value_type parses correctly") {
        CHECK(parse_sr_value_type("TEXT") == sr_value_type::text);
        CHECK(parse_sr_value_type("CODE") == sr_value_type::code);
        CHECK(parse_sr_value_type("NUM") == sr_value_type::num);
        CHECK(parse_sr_value_type("IMAGE") == sr_value_type::image);
        CHECK(parse_sr_value_type("SCOORD") == sr_value_type::scoord);
        CHECK(parse_sr_value_type("SCOORD3D") == sr_value_type::scoord3d);
        CHECK(parse_sr_value_type("CONTAINER") == sr_value_type::container);
        CHECK(parse_sr_value_type("UNKNOWN_TYPE") == sr_value_type::unknown);
    }
}

TEST_CASE("is_valid_sr_value_type validates correctly", "[services][sr][value_type]") {
    CHECK(is_valid_sr_value_type("TEXT"));
    CHECK(is_valid_sr_value_type("CODE"));
    CHECK(is_valid_sr_value_type("NUM"));
    CHECK(is_valid_sr_value_type("IMAGE"));
    CHECK(is_valid_sr_value_type("SCOORD"));
    CHECK(is_valid_sr_value_type("CONTAINER"));
    CHECK(is_valid_sr_value_type("TABLE"));

    CHECK_FALSE(is_valid_sr_value_type("INVALID"));
    CHECK_FALSE(is_valid_sr_value_type(""));
}

// ============================================================================
// SR Relationship Type Tests
// ============================================================================

TEST_CASE("sr_relationship_type conversions", "[services][sr][relationship_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(sr_relationship_type::contains) == "CONTAINS");
        CHECK(to_string(sr_relationship_type::has_obs_context) == "HAS OBS CONTEXT");
        CHECK(to_string(sr_relationship_type::has_acq_context) == "HAS ACQ CONTEXT");
        CHECK(to_string(sr_relationship_type::has_concept_mod) == "HAS CONCEPT MOD");
        CHECK(to_string(sr_relationship_type::has_properties) == "HAS PROPERTIES");
        CHECK(to_string(sr_relationship_type::inferred_from) == "INFERRED FROM");
        CHECK(to_string(sr_relationship_type::selected_from) == "SELECTED FROM");
    }

    SECTION("parse_sr_relationship_type parses correctly") {
        CHECK(parse_sr_relationship_type("CONTAINS") == sr_relationship_type::contains);
        CHECK(parse_sr_relationship_type("HAS OBS CONTEXT") == sr_relationship_type::has_obs_context);
        CHECK(parse_sr_relationship_type("HAS ACQ CONTEXT") == sr_relationship_type::has_acq_context);
        CHECK(parse_sr_relationship_type("INFERRED FROM") == sr_relationship_type::inferred_from);
        CHECK(parse_sr_relationship_type("UNKNOWN") == sr_relationship_type::unknown);
    }
}

// ============================================================================
// SR Completion and Verification Flag Tests
// ============================================================================

TEST_CASE("sr_completion_flag conversions", "[services][sr][completion_flag]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(sr_completion_flag::partial) == "PARTIAL");
        CHECK(to_string(sr_completion_flag::complete) == "COMPLETE");
    }

    SECTION("parse_sr_completion_flag parses correctly") {
        CHECK(parse_sr_completion_flag("PARTIAL") == sr_completion_flag::partial);
        CHECK(parse_sr_completion_flag("COMPLETE") == sr_completion_flag::complete);
        CHECK(parse_sr_completion_flag("UNKNOWN") == sr_completion_flag::partial);  // Default
    }
}

TEST_CASE("sr_verification_flag conversions", "[services][sr][verification_flag]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(sr_verification_flag::unverified) == "UNVERIFIED");
        CHECK(to_string(sr_verification_flag::verified) == "VERIFIED");
    }

    SECTION("parse_sr_verification_flag parses correctly") {
        CHECK(parse_sr_verification_flag("UNVERIFIED") == sr_verification_flag::unverified);
        CHECK(parse_sr_verification_flag("VERIFIED") == sr_verification_flag::verified);
        CHECK(parse_sr_verification_flag("UNKNOWN") == sr_verification_flag::unverified);  // Default
    }
}

// ============================================================================
// SR Template Tests
// ============================================================================

TEST_CASE("SR template constants are defined", "[services][sr][template]") {
    CHECK(sr_template::basic_diagnostic_imaging_report == "2000");
    CHECK(sr_template::mammography_cad_report == "4000");
    CHECK(sr_template::chest_cad_report == "4100");
    CHECK(sr_template::colon_cad_report == "4200");
    CHECK(sr_template::xray_radiation_dose_report == "10001");
    CHECK(sr_template::key_object_selection == "2010");
    CHECK(sr_template::measurement_report == "1500");
}

TEST_CASE("get_recommended_sr_template returns correct templates", "[services][sr][template]") {
    CHECK(get_recommended_sr_template(mammography_cad_sr_storage_uid) ==
          sr_template::mammography_cad_report);
    CHECK(get_recommended_sr_template(chest_cad_sr_storage_uid) ==
          sr_template::chest_cad_report);
    CHECK(get_recommended_sr_template(colon_cad_sr_storage_uid) ==
          sr_template::colon_cad_report);
    CHECK(get_recommended_sr_template(xray_radiation_dose_sr_storage_uid) ==
          sr_template::xray_radiation_dose_report);
    CHECK(get_recommended_sr_template(key_object_selection_document_storage_uid) ==
          sr_template::key_object_selection);
    CHECK(get_recommended_sr_template(basic_text_sr_storage_uid) ==
          sr_template::basic_diagnostic_imaging_report);
}

// ============================================================================
// SR IOD Validator Tests
// ============================================================================

namespace {

// Helper function to insert a sequence element into a dataset
void insert_sequence(dicom_dataset& ds, dicom_tag tag, std::vector<dicom_dataset> items) {
    dicom_element seq_elem(tag, vr_type::SQ);
    seq_elem.sequence_items() = std::move(items);
    ds.insert(std::move(seq_elem));
}

// Helper function to create Concept Name Code Sequence item
dicom_dataset create_concept_name_code() {
    dicom_dataset code;
    code.set_string(dicom_tag{0x0008, 0x0100}, vr_type::SH, "11528-7");  // Code Value (LOINC for Radiology Report)
    code.set_string(dicom_tag{0x0008, 0x0102}, vr_type::SH, "LN");  // Coding Scheme Designator
    code.set_string(dicom_tag{0x0008, 0x0104}, vr_type::LO, "Radiology Report");  // Code Meaning
    return code;
}

// Helper function to create Verifying Observer Sequence item
dicom_dataset create_verifying_observer() {
    dicom_dataset observer;
    observer.set_string(dicom_tag{0x0040, 0xA075}, vr_type::PN, "SMITH^JOHN^DR");  // Verifying Observer Name
    observer.set_string(dicom_tag{0x0040, 0xA030}, vr_type::DT, "20231201120000");  // Verification DateTime
    observer.set_string(dicom_tag{0x0040, 0xA027}, vr_type::LO, "ACME Hospital");  // Verifying Organization
    return observer;
}

dicom_dataset create_minimal_sr_dataset() {
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
    ds.set_string(tags::modality, vr_type::CS, "SR");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.124");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // General Equipment Module (Type 2)
    ds.set_string(dicom_tag{0x0008, 0x0070}, vr_type::LO, "ACME Medical");  // Manufacturer

    // SR Document General Module
    ds.set_string(tags::instance_number, vr_type::IS, "1");
    ds.set_string(dicom_tag{0x0040, 0xA491}, vr_type::CS, "COMPLETE");  // Completion Flag
    ds.set_string(dicom_tag{0x0040, 0xA493}, vr_type::CS, "VERIFIED");  // Verification Flag
    ds.set_string(tags::content_date, vr_type::DA, "20231201");
    ds.set_string(tags::content_time, vr_type::TM, "120000");

    // Verifying Observer Sequence (Type 1C - required when Verification Flag is VERIFIED)
    insert_sequence(ds, dicom_tag{0x0040, 0xA073}, {create_verifying_observer()});

    // SR Document Content Module - Root content item
    ds.set_string(dicom_tag{0x0040, 0xA040}, vr_type::CS, "CONTAINER");  // Value Type (Type 1)

    // Concept Name Code Sequence (Document Title) - must be actual sequence with items
    insert_sequence(ds, dicom_tag{0x0040, 0xA043}, {create_concept_name_code()});

    // Content Sequence (children of root container) - can be empty for minimal test
    insert_sequence(ds, dicom_tag{0x0040, 0xA730}, {});

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(basic_text_sr_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.125");

    return ds;
}

}  // namespace

TEST_CASE("sr_iod_validator validates complete dataset", "[services][sr][validation]") {
    sr_iod_validator validator;
    auto dataset = create_minimal_sr_dataset();

    auto result = validator.validate(dataset);
    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("sr_iod_validator detects missing Type 1 attributes", "[services][sr][validation]") {
    sr_iod_validator validator;
    auto dataset = create_minimal_sr_dataset();

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
}

TEST_CASE("sr_iod_validator detects wrong modality", "[services][sr][validation]") {
    sr_iod_validator validator;
    auto dataset = create_minimal_sr_dataset();

    dataset.set_string(tags::modality, vr_type::CS, "CT");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
    CHECK(result.has_errors());
}

TEST_CASE("sr_iod_validator detects invalid SOP Class", "[services][sr][validation]") {
    sr_iod_validator validator;
    auto dataset = create_minimal_sr_dataset();

    // Set to CT SOP Class
    dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
}

TEST_CASE("sr_iod_validator quick_check works correctly", "[services][sr][validation]") {
    sr_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_sr_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid dataset fails quick check") {
        auto dataset = create_minimal_sr_dataset();
        dataset.remove(tags::modality);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("wrong modality fails quick check") {
        auto dataset = create_minimal_sr_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

TEST_CASE("sr_iod_validator options work correctly", "[services][sr][validation]") {
    SECTION("can disable Type 2 checking") {
        sr_validation_options options;
        options.check_type1 = true;
        options.check_type2 = false;

        sr_iod_validator validator{options};
        auto dataset = create_minimal_sr_dataset();
        dataset.remove(tags::patient_name);  // Type 2

        auto result = validator.validate(dataset);
        CHECK(result.is_valid);  // Should pass without Type 2 errors
    }

    SECTION("strict mode treats warnings as errors") {
        sr_validation_options options;
        options.strict_mode = true;

        sr_iod_validator validator{options};
        auto dataset = create_minimal_sr_dataset();

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

TEST_CASE("SR SOP classes are registered in central registry", "[services][sr][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("Basic Text SR is registered") {
        CHECK(registry.is_supported(basic_text_sr_storage_uid));
        const auto* info = registry.get_info(basic_text_sr_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::sr);
    }

    SECTION("Enhanced SR is registered") {
        CHECK(registry.is_supported(enhanced_sr_storage_uid));
    }

    SECTION("Comprehensive SR is registered") {
        CHECK(registry.is_supported(comprehensive_sr_storage_uid));
    }

    SECTION("Key Object Selection is registered") {
        CHECK(registry.is_supported(key_object_selection_document_storage_uid));
    }

    SECTION("CAD SR classes are registered") {
        CHECK(registry.is_supported(mammography_cad_sr_storage_uid));
        CHECK(registry.is_supported(chest_cad_sr_storage_uid));
        CHECK(registry.is_supported(colon_cad_sr_storage_uid));
    }

    SECTION("SR classes are returned by modality query") {
        auto sr_classes = registry.get_by_modality(modality_type::sr, true);
        CHECK(sr_classes.size() >= 10);

        for (const auto& uid : sr_classes) {
            const auto* info = registry.get_info(uid);
            REQUIRE(info != nullptr);
            CHECK(info->modality == modality_type::sr);
        }
    }
}

TEST_CASE("SR modality parsing works correctly", "[services][sr][registry]") {
    CHECK(sop_class_registry::parse_modality("SR") == modality_type::sr);
    CHECK(sop_class_registry::modality_to_string(modality_type::sr) == "SR");
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_sr_iod convenience function", "[services][sr][validation]") {
    auto dataset = create_minimal_sr_dataset();
    auto result = validate_sr_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_sr_dataset convenience function", "[services][sr][validation]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_sr_dataset();
        CHECK(is_valid_sr_dataset(dataset));
    }

    SECTION("invalid dataset") {
        dicom_dataset empty_dataset;
        CHECK_FALSE(is_valid_sr_dataset(empty_dataset));
    }
}
