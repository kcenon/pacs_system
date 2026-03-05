/**
 * @file waveform_storage_test.cpp
 * @brief Unit tests for Waveform Storage SOP Classes and IOD Validator
 */

#include <pacs/services/sop_classes/waveform_storage.hpp>
#include <pacs/services/validation/waveform_ps_iod_validator.hpp>
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
// Waveform Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("Waveform Storage SOP Class UIDs are correct", "[services][waveform][sop_class]") {
    CHECK(twelve_lead_ecg_storage_uid == "1.2.840.10008.5.1.4.1.1.9.1.1");
    CHECK(general_ecg_storage_uid == "1.2.840.10008.5.1.4.1.1.9.1.2");
    CHECK(ambulatory_ecg_storage_uid == "1.2.840.10008.5.1.4.1.1.9.1.3");
    CHECK(hemodynamic_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.2.1");
    CHECK(cardiac_ep_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.3.1");
    CHECK(basic_voice_audio_storage_uid == "1.2.840.10008.5.1.4.1.1.9.4.1");
    CHECK(general_audio_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.4.2");
    CHECK(arterial_pulse_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.5.1");
    CHECK(respiratory_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.6.1");
    CHECK(multichannel_respiratory_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.6.2");
    CHECK(routine_scalp_eeg_storage_uid == "1.2.840.10008.5.1.4.1.1.9.7.1");
    CHECK(emg_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.7.2");
    CHECK(eog_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.7.3");
    CHECK(sleep_eeg_storage_uid == "1.2.840.10008.5.1.4.1.1.9.7.4");
    CHECK(body_position_waveform_storage_uid == "1.2.840.10008.5.1.4.1.1.9.8.1");
    CHECK(waveform_presentation_state_storage_uid == "1.2.840.10008.5.1.4.1.1.11.11");
    CHECK(waveform_annotation_storage_uid == "1.2.840.10008.5.1.4.1.1.11.12");
}

// ============================================================================
// Waveform Type Classification Tests
// ============================================================================

TEST_CASE("get_waveform_type classifies correctly", "[services][waveform][type]") {
    CHECK(get_waveform_type(twelve_lead_ecg_storage_uid) == waveform_type::ecg_12lead);
    CHECK(get_waveform_type(general_ecg_storage_uid) == waveform_type::ecg_general);
    CHECK(get_waveform_type(ambulatory_ecg_storage_uid) == waveform_type::ecg_ambulatory);
    CHECK(get_waveform_type(hemodynamic_waveform_storage_uid) == waveform_type::hemodynamic);
    CHECK(get_waveform_type(cardiac_ep_waveform_storage_uid) == waveform_type::cardiac_ep);
    CHECK(get_waveform_type(basic_voice_audio_storage_uid) == waveform_type::audio_basic);
    CHECK(get_waveform_type(general_audio_waveform_storage_uid) == waveform_type::audio_general);
    CHECK(get_waveform_type(arterial_pulse_waveform_storage_uid) == waveform_type::arterial_pulse);
    CHECK(get_waveform_type(respiratory_waveform_storage_uid) == waveform_type::respiratory);
    CHECK(get_waveform_type(multichannel_respiratory_waveform_storage_uid) == waveform_type::respiratory_multi);
    CHECK(get_waveform_type(routine_scalp_eeg_storage_uid) == waveform_type::eeg_routine);
    CHECK(get_waveform_type(emg_waveform_storage_uid) == waveform_type::emg);
    CHECK(get_waveform_type(eog_waveform_storage_uid) == waveform_type::eog);
    CHECK(get_waveform_type(sleep_eeg_storage_uid) == waveform_type::eeg_sleep);
    CHECK(get_waveform_type(body_position_waveform_storage_uid) == waveform_type::body_position);
    CHECK(get_waveform_type(waveform_presentation_state_storage_uid) == waveform_type::presentation_state);
    CHECK(get_waveform_type(waveform_annotation_storage_uid) == waveform_type::annotation);
    CHECK(get_waveform_type("1.2.3.4.5.invalid") == waveform_type::unknown);
}

TEST_CASE("waveform_type to_string produces correct names", "[services][waveform][type]") {
    CHECK(to_string(waveform_type::ecg_12lead) == "12-Lead ECG");
    CHECK(to_string(waveform_type::ecg_general) == "General ECG");
    CHECK(to_string(waveform_type::hemodynamic) == "Hemodynamic");
    CHECK(to_string(waveform_type::presentation_state) == "Waveform Presentation State");
    CHECK(to_string(waveform_type::annotation) == "Waveform Annotation");
    CHECK(to_string(waveform_type::unknown) == "Unknown");
}

// ============================================================================
// SOP Class Identification Tests
// ============================================================================

TEST_CASE("is_waveform_storage_sop_class identifies waveform classes", "[services][waveform][sop_class]") {
    SECTION("recognizes all waveform classes") {
        CHECK(is_waveform_storage_sop_class(twelve_lead_ecg_storage_uid));
        CHECK(is_waveform_storage_sop_class(general_ecg_storage_uid));
        CHECK(is_waveform_storage_sop_class(hemodynamic_waveform_storage_uid));
        CHECK(is_waveform_storage_sop_class(waveform_presentation_state_storage_uid));
        CHECK(is_waveform_storage_sop_class(waveform_annotation_storage_uid));
    }

    SECTION("rejects non-waveform classes") {
        CHECK_FALSE(is_waveform_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));    // CT
        CHECK_FALSE(is_waveform_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));  // US
        CHECK_FALSE(is_waveform_storage_sop_class(""));
        CHECK_FALSE(is_waveform_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_waveform_presentation_state_sop_class identifies PS class", "[services][waveform][sop_class]") {
    CHECK(is_waveform_presentation_state_sop_class(waveform_presentation_state_storage_uid));
    CHECK_FALSE(is_waveform_presentation_state_sop_class(waveform_annotation_storage_uid));
    CHECK_FALSE(is_waveform_presentation_state_sop_class(twelve_lead_ecg_storage_uid));
}

TEST_CASE("is_waveform_annotation_sop_class identifies annotation class", "[services][waveform][sop_class]") {
    CHECK(is_waveform_annotation_sop_class(waveform_annotation_storage_uid));
    CHECK_FALSE(is_waveform_annotation_sop_class(waveform_presentation_state_storage_uid));
    CHECK_FALSE(is_waveform_annotation_sop_class(twelve_lead_ecg_storage_uid));
}

// ============================================================================
// SOP Class Information Tests
// ============================================================================

TEST_CASE("get_waveform_sop_class_info returns correct information", "[services][waveform][sop_class]") {
    SECTION("12-Lead ECG info") {
        const auto* info = get_waveform_sop_class_info(twelve_lead_ecg_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == twelve_lead_ecg_storage_uid);
        CHECK(info->name == "12-lead ECG Waveform Storage");
        CHECK(info->type == waveform_type::ecg_12lead);
        CHECK_FALSE(info->is_retired);
    }

    SECTION("Waveform Presentation State info") {
        const auto* info = get_waveform_sop_class_info(waveform_presentation_state_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == waveform_presentation_state_storage_uid);
        CHECK(info->type == waveform_type::presentation_state);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_waveform_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_waveform_storage_sop_classes returns correct list", "[services][waveform][sop_class]") {
    SECTION("all classes") {
        auto classes = get_waveform_storage_sop_classes(true, true);
        CHECK(classes.size() == 17);
    }

    SECTION("without presentation state") {
        auto classes = get_waveform_storage_sop_classes(false, true);
        CHECK(classes.size() == 16);
    }

    SECTION("without annotation") {
        auto classes = get_waveform_storage_sop_classes(true, false);
        CHECK(classes.size() == 16);
    }

    SECTION("without presentation state and annotation") {
        auto classes = get_waveform_storage_sop_classes(false, false);
        CHECK(classes.size() == 15);
    }
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_waveform_transfer_syntaxes returns valid syntaxes", "[services][waveform][transfer]") {
    auto syntaxes = get_waveform_transfer_syntaxes();

    CHECK(syntaxes.size() > 0);

    // Should include Explicit VR Little Endian (most preferred)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1") != syntaxes.end());

    // Should include Implicit VR Little Endian (universal baseline)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2") != syntaxes.end());
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("Waveform SOP classes are registered in central registry", "[services][waveform][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("Waveform PS Storage is registered") {
        CHECK(registry.is_supported(waveform_presentation_state_storage_uid));
        const auto* info = registry.get_info(waveform_presentation_state_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
    }

    SECTION("Waveform Annotation Storage is registered") {
        CHECK(registry.is_supported(waveform_annotation_storage_uid));
        const auto* info = registry.get_info(waveform_annotation_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
    }

    SECTION("12-Lead ECG is registered") {
        CHECK(registry.is_supported(twelve_lead_ecg_storage_uid));
        const auto* info = registry.get_info(twelve_lead_ecg_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
    }
}

// ============================================================================
// Waveform PS IOD Validator Tests
// ============================================================================

namespace {

dicom_dataset create_minimal_waveform_ps_dataset() {
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
    ds.set_string(tags::modality, vr_type::CS, "ECG");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.124");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // General Equipment Module (Type 2)
    ds.set_string(dicom_tag{0x0008, 0x0070}, vr_type::LO, "ACME Medical");  // Manufacturer

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                 std::string(waveform_presentation_state_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.125");

    return ds;
}

dicom_dataset create_minimal_waveform_annotation_dataset() {
    auto ds = create_minimal_waveform_ps_dataset();
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                 std::string(waveform_annotation_storage_uid));
    return ds;
}

}  // namespace

TEST_CASE("waveform_ps_iod_validator validates complete PS dataset", "[services][waveform][validation]") {
    waveform_ps_iod_validator validator;
    auto dataset = create_minimal_waveform_ps_dataset();

    auto result = validator.validate(dataset);
    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("waveform_ps_iod_validator validates annotation dataset", "[services][waveform][validation]") {
    waveform_ps_iod_validator validator;
    auto dataset = create_minimal_waveform_annotation_dataset();

    auto result = validator.validate(dataset);
    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("waveform_ps_iod_validator detects missing Type 1 attributes", "[services][waveform][validation]") {
    waveform_ps_iod_validator validator;
    auto dataset = create_minimal_waveform_ps_dataset();

    SECTION("missing StudyInstanceUID") {
        dataset.remove(tags::study_instance_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
        CHECK(result.has_errors());
    }

    SECTION("missing SOPClassUID") {
        dataset.remove(tags::sop_class_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SeriesInstanceUID") {
        dataset.remove(tags::series_instance_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("waveform_ps_iod_validator detects invalid SOP Class", "[services][waveform][validation]") {
    waveform_ps_iod_validator validator;
    auto dataset = create_minimal_waveform_ps_dataset();

    // Set to CT SOP Class
    dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
    CHECK(result.has_errors());
}

TEST_CASE("waveform_ps_iod_validator quick_check works correctly", "[services][waveform][validation]") {
    waveform_ps_iod_validator validator;

    SECTION("valid PS dataset passes quick check") {
        auto dataset = create_minimal_waveform_ps_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("valid annotation dataset passes quick check") {
        auto dataset = create_minimal_waveform_annotation_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("empty dataset fails quick check") {
        dicom_dataset empty_dataset;
        CHECK_FALSE(validator.quick_check(empty_dataset));
    }

    SECTION("wrong SOP class fails quick check") {
        auto dataset = create_minimal_waveform_ps_dataset();
        dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

TEST_CASE("waveform_ps_iod_validator options work correctly", "[services][waveform][validation]") {
    SECTION("can disable Type 2 checking") {
        waveform_ps_validation_options options;
        options.check_type1 = true;
        options.check_type2 = false;

        waveform_ps_iod_validator validator{options};
        auto dataset = create_minimal_waveform_ps_dataset();
        dataset.remove(tags::patient_name);  // Type 2

        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("strict mode treats warnings as errors") {
        waveform_ps_validation_options options;
        options.strict_mode = true;

        waveform_ps_iod_validator validator{options};
        auto dataset = create_minimal_waveform_ps_dataset();

        // Remove a Type 2 attribute to generate a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("waveform_ps_iod_validator validate_references works", "[services][waveform][validation]") {
    waveform_ps_iod_validator validator;
    auto dataset = create_minimal_waveform_ps_dataset();

    auto result = validator.validate_references(dataset);
    // Without referenced series sequence, should produce a warning but still be valid
    CHECK(result.is_valid);
    CHECK(result.findings.size() > 0);
}
