/**
 * @file anonymizer_test.cpp
 * @brief Unit tests for DICOM anonymization functionality
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/security/anonymizer.hpp"
#include "pacs/security/anonymization_profile.hpp"
#include "pacs/security/tag_action.hpp"
#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"

using namespace pacs::security;
using namespace pacs::core;
using namespace pacs::encoding;

namespace {

dicom_dataset create_test_dataset() {
    dicom_dataset ds;

    // Patient information
    ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19800115");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");
    ds.set_string(tags::patient_age, vr_type::AS, "044Y");
    ds.set_string(tags::patient_address, vr_type::LO, "123 Main St");

    // Institution information
    ds.set_string(tags::institution_name, vr_type::LO, "General Hospital");
    ds.set_string(tags::institution_address, vr_type::ST, "456 Hospital Ave");
    ds.set_string(tags::station_name, vr_type::SH, "CT-001");

    // Personnel
    ds.set_string(tags::referring_physician_name, vr_type::PN, "SMITH^JANE");
    ds.set_string(tags::performing_physician_name, vr_type::PN, "JONES^BOB");
    ds.set_string(tags::operators_name, vr_type::PN, "WILSON^TOM");

    // UIDs
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.10");
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.11");

    // Study information
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::study_date, vr_type::DA, "20240115");
    ds.set_string(tags::study_description, vr_type::LO, "CT Chest");

    // Series information
    ds.set_string(tags::series_date, vr_type::DA, "20240115");
    ds.set_string(tags::series_description, vr_type::LO, "Axial 5mm");

    return ds;
}

} // namespace

TEST_CASE("Anonymizer: Basic Profile", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::basic);
    auto dataset = create_test_dataset();

    auto result = anon.anonymize(dataset);
    REQUIRE(result.is_ok());

    auto report = result.value();

    SECTION("Report contains profile name") {
        REQUIRE(report.profile_name == "basic");
    }

    SECTION("Patient name is replaced") {
        REQUIRE(dataset.get_string(tags::patient_name) == "ANONYMOUS");
    }

    SECTION("Patient ID is replaced") {
        REQUIRE(dataset.get_string(tags::patient_id) == "ANON_ID");
    }

    SECTION("Patient birth date is emptied") {
        REQUIRE(dataset.get_string(tags::patient_birth_date).empty());
    }

    SECTION("Patient sex is kept") {
        REQUIRE(dataset.get_string(tags::patient_sex) == "M");
    }

    SECTION("Institution name is emptied") {
        REQUIRE(dataset.get_string(tags::institution_name).empty());
    }

    SECTION("UIDs are replaced") {
        REQUIRE(dataset.get_string(tags::study_instance_uid) != "1.2.3.4.5.6.7.8.9");
        REQUIRE(dataset.get_string(tags::series_instance_uid) != "1.2.3.4.5.6.7.8.10");
        REQUIRE(dataset.get_string(tags::sop_instance_uid) != "1.2.3.4.5.6.7.8.11");
    }

    SECTION("Report statistics are accurate") {
        REQUIRE(report.total_tags_processed > 0);
        REQUIRE(report.is_successful());
    }
}

TEST_CASE("Anonymizer: HIPAA Safe Harbor Profile", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::hipaa_safe_harbor);
    auto dataset = create_test_dataset();

    auto result = anon.anonymize(dataset);
    REQUIRE(result.is_ok());

    SECTION("Direct identifiers removed") {
        REQUIRE(dataset.get_string(tags::patient_name) != "DOE^JOHN");
        REQUIRE(dataset.get_string(tags::patient_id) != "12345");
    }

    SECTION("Birth date is removed") {
        // HIPAA requires birth date removal (or just year)
        auto birth_date = dataset.get_string(tags::patient_birth_date);
        REQUIRE(birth_date.empty());
    }

    SECTION("Accession number is emptied") {
        REQUIRE(dataset.get_string(tags::accession_number).empty());
    }
}

TEST_CASE("Anonymizer: Retain Longitudinal Profile", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::retain_longitudinal);
    anon.set_date_offset(std::chrono::days{-100});

    auto dataset = create_test_dataset();

    auto result = anon.anonymize(dataset);
    REQUIRE(result.is_ok());

    SECTION("Dates are shifted, not removed") {
        auto study_date = dataset.get_string(tags::study_date);
        REQUIRE_FALSE(study_date.empty());
        REQUIRE(study_date != "20240115");  // Should be shifted
    }

    SECTION("Report shows date offset") {
        auto report = result.value();
        REQUIRE(report.date_offset.has_value());
        REQUIRE(report.date_offset.value() == std::chrono::days{-100});
    }
}

TEST_CASE("Anonymizer: Clean Descriptions Profile", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::clean_descriptions);
    auto dataset = create_test_dataset();

    auto result = anon.anonymize(dataset);
    REQUIRE(result.is_ok());

    SECTION("Study description is emptied") {
        REQUIRE(dataset.get_string(tags::study_description).empty());
    }

    SECTION("Series description is emptied") {
        REQUIRE(dataset.get_string(tags::series_description).empty());
    }
}

TEST_CASE("Anonymizer: Retain Patient Characteristics Profile", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::retain_patient_characteristics);
    auto dataset = create_test_dataset();

    auto result = anon.anonymize(dataset);
    REQUIRE(result.is_ok());

    SECTION("Patient sex is kept") {
        REQUIRE(dataset.get_string(tags::patient_sex) == "M");
    }

    SECTION("Patient age is kept") {
        REQUIRE(dataset.get_string(tags::patient_age) == "044Y");
    }

    SECTION("Patient name is still anonymized") {
        REQUIRE(dataset.get_string(tags::patient_name) != "DOE^JOHN");
    }
}

TEST_CASE("Anonymizer: GDPR Compliant Profile", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::gdpr_compliant);
    auto dataset = create_test_dataset();

    auto result = anon.anonymize(dataset);
    REQUIRE(result.is_ok());

    SECTION("Patient ID is hashed for pseudonymization") {
        auto patient_id = dataset.get_string(tags::patient_id);
        REQUIRE_FALSE(patient_id.empty());
        REQUIRE(patient_id != "12345");
        // Hash should be hex string
        REQUIRE(patient_id.find_first_not_of("0123456789abcdef") == std::string::npos);
    }

    SECTION("Patient name is hashed") {
        auto name = dataset.get_string(tags::patient_name);
        REQUIRE_FALSE(name.empty());
        REQUIRE(name != "DOE^JOHN");
    }
}

TEST_CASE("Anonymizer: UID Mapping Consistency", "[security][anonymization]") {
    uid_mapping mapping;
    anonymizer anon(anonymization_profile::basic);

    auto dataset1 = create_test_dataset();
    auto dataset2 = create_test_dataset();

    anon.anonymize_with_mapping(dataset1, mapping);
    anon.anonymize_with_mapping(dataset2, mapping);

    SECTION("Same UIDs map to same anonymized values") {
        REQUIRE(dataset1.get_string(tags::study_instance_uid) ==
                dataset2.get_string(tags::study_instance_uid));
        REQUIRE(dataset1.get_string(tags::series_instance_uid) ==
                dataset2.get_string(tags::series_instance_uid));
    }

    SECTION("Mapping contains all UIDs") {
        REQUIRE(mapping.size() >= 3);  // At least study, series, sop UIDs
    }
}

TEST_CASE("Anonymizer: Custom Tag Actions", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::basic);

    SECTION("Keep specific tag") {
        anon.add_tag_action(tags::institution_name, tag_action_config::make_keep());

        auto dataset = create_test_dataset();
        anon.anonymize(dataset);

        REQUIRE(dataset.get_string(tags::institution_name) == "General Hospital");
    }

    SECTION("Replace with custom value") {
        anon.add_tag_action(tags::patient_name,
                           tag_action_config::make_replace("REDACTED^PATIENT"));

        auto dataset = create_test_dataset();
        anon.anonymize(dataset);

        REQUIRE(dataset.get_string(tags::patient_name) == "REDACTED^PATIENT");
    }

    SECTION("Hash specific tag") {
        anon.add_tag_action(tags::patient_id, tag_action_config::make_hash());

        auto dataset = create_test_dataset();
        anon.anonymize(dataset);

        auto id = dataset.get_string(tags::patient_id);
        REQUIRE_FALSE(id.empty());
        REQUIRE(id != "12345");
    }

    SECTION("Remove custom action reverts to profile default") {
        anon.add_tag_action(tags::institution_name, tag_action_config::make_keep());
        anon.remove_tag_action(tags::institution_name);

        auto dataset = create_test_dataset();
        anon.anonymize(dataset);

        // Should be emptied per basic profile
        REQUIRE(dataset.get_string(tags::institution_name).empty());
    }

    SECTION("Clear all custom actions") {
        anon.add_tag_action(tags::institution_name, tag_action_config::make_keep());
        anon.add_tag_action(tags::patient_name, tag_action_config::make_keep());
        anon.clear_custom_actions();

        auto dataset = create_test_dataset();
        anon.anonymize(dataset);

        REQUIRE(dataset.get_string(tags::patient_name) != "DOE^JOHN");
    }
}

TEST_CASE("Anonymizer: Date Shifting", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::retain_longitudinal);

    SECTION("Positive date offset shifts dates forward") {
        anon.set_date_offset(std::chrono::days{30});

        auto dataset = create_test_dataset();
        // Study date: 20240115
        anon.anonymize(dataset);

        auto shifted = dataset.get_string(tags::study_date);
        REQUIRE(shifted == "20240214");  // +30 days
    }

    SECTION("Negative date offset shifts dates backward") {
        anon.set_date_offset(std::chrono::days{-15});

        auto dataset = create_test_dataset();
        anon.anonymize(dataset);

        auto shifted = dataset.get_string(tags::study_date);
        REQUIRE(shifted == "20231231");  // -15 days from 2024-01-15
    }

    SECTION("Clear date offset results in empty dates") {
        anon.set_date_offset(std::chrono::days{30});
        anon.clear_date_offset();

        auto dataset = create_test_dataset();
        anon.anonymize(dataset);

        REQUIRE(dataset.get_string(tags::study_date).empty());
    }

    SECTION("Random date offset is within range") {
        auto offset = anonymizer::generate_random_date_offset(
            std::chrono::days{-100},
            std::chrono::days{100}
        );

        REQUIRE(offset.count() >= -100);
        REQUIRE(offset.count() <= 100);
    }
}

TEST_CASE("Anonymizer: Profile Management", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::basic);

    SECTION("Get initial profile") {
        REQUIRE(anon.get_profile() == anonymization_profile::basic);
    }

    SECTION("Change profile") {
        anon.set_profile(anonymization_profile::hipaa_safe_harbor);
        REQUIRE(anon.get_profile() == anonymization_profile::hipaa_safe_harbor);
    }

    SECTION("Custom actions preserved after profile change") {
        anon.add_tag_action(tags::manufacturer, tag_action_config::make_keep());
        anon.set_profile(anonymization_profile::hipaa_safe_harbor);

        // Custom action should still apply
        auto config = anon.get_tag_action(tags::manufacturer);
        REQUIRE(config.action == tag_action::keep);
    }
}

TEST_CASE("Anonymizer: Hash Configuration", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::gdpr_compliant);

    SECTION("Hash salt affects output") {
        anon.set_hash_salt("secret_salt_1");
        auto dataset1 = create_test_dataset();
        anon.anonymize(dataset1);
        auto hash1 = dataset1.get_string(tags::patient_id);

        anon.set_hash_salt("secret_salt_2");
        auto dataset2 = create_test_dataset();
        anon.anonymize(dataset2);
        auto hash2 = dataset2.get_string(tags::patient_id);

        REQUIRE(hash1 != hash2);
    }

    SECTION("Same salt produces consistent hashes") {
        anon.set_hash_salt("consistent_salt");

        auto dataset1 = create_test_dataset();
        auto dataset2 = create_test_dataset();

        anon.anonymize(dataset1);
        anon.anonymize(dataset2);

        REQUIRE(dataset1.get_string(tags::patient_id) ==
                dataset2.get_string(tags::patient_id));
    }
}

TEST_CASE("Anonymizer: Detailed Reporting", "[security][anonymization]") {
    anonymizer anon(anonymization_profile::basic);

    SECTION("Detailed reporting disabled by default") {
        REQUIRE_FALSE(anon.is_detailed_reporting());
    }

    SECTION("Enable detailed reporting") {
        anon.set_detailed_reporting(true);
        REQUIRE(anon.is_detailed_reporting());

        auto dataset = create_test_dataset();
        auto result = anon.anonymize(dataset);
        REQUIRE(result.is_ok());

        auto report = result.value();
        REQUIRE_FALSE(report.action_records.empty());
    }

    SECTION("Detailed records contain original values") {
        anon.set_detailed_reporting(true);

        auto dataset = create_test_dataset();
        auto result = anon.anonymize(dataset);
        auto report = result.value();

        bool found_patient_name = false;
        for (const auto& record : report.action_records) {
            if (record.tag == tags::patient_name) {
                found_patient_name = true;
                REQUIRE(record.original_value == "DOE^JOHN");
                REQUIRE(record.success);
                break;
            }
        }
        REQUIRE(found_patient_name);
    }
}

TEST_CASE("Anonymizer: Static Helpers", "[security][anonymization]") {
    SECTION("Get profile actions returns non-empty map") {
        auto actions = anonymizer::get_profile_actions(anonymization_profile::basic);
        REQUIRE_FALSE(actions.empty());
    }

    SECTION("HIPAA identifier tags list") {
        auto tags = anonymizer::get_hipaa_identifier_tags();
        REQUIRE_FALSE(tags.empty());
        // Should contain patient name
        bool has_patient_name = std::find(tags.begin(), tags.end(),
                                          tags::patient_name) != tags.end();
        REQUIRE(has_patient_name);
    }

    SECTION("GDPR personal data tags list") {
        auto tags = anonymizer::get_gdpr_personal_data_tags();
        REQUIRE_FALSE(tags.empty());
    }
}

TEST_CASE("Anonymization Profile: String Conversion", "[security][anonymization]") {
    SECTION("to_string conversion") {
        REQUIRE(to_string(anonymization_profile::basic) == "basic");
        REQUIRE(to_string(anonymization_profile::clean_pixel) == "clean_pixel");
        REQUIRE(to_string(anonymization_profile::hipaa_safe_harbor) == "hipaa_safe_harbor");
        REQUIRE(to_string(anonymization_profile::gdpr_compliant) == "gdpr_compliant");
    }

    SECTION("from_string conversion") {
        REQUIRE(profile_from_string("basic").value() == anonymization_profile::basic);
        REQUIRE(profile_from_string("hipaa_safe_harbor").value() ==
                anonymization_profile::hipaa_safe_harbor);
        REQUIRE_FALSE(profile_from_string("invalid_profile").has_value());
    }
}

TEST_CASE("Tag Action: String Conversion", "[security][anonymization]") {
    REQUIRE(to_string(tag_action::remove) == "remove");
    REQUIRE(to_string(tag_action::empty) == "empty");
    REQUIRE(to_string(tag_action::keep) == "keep");
    REQUIRE(to_string(tag_action::replace) == "replace");
    REQUIRE(to_string(tag_action::hash) == "hash");
    REQUIRE(to_string(tag_action::shift_date) == "shift_date");
}

TEST_CASE("Tag Action Config: Factory Methods", "[security][anonymization]") {
    SECTION("make_remove") {
        auto config = tag_action_config::make_remove();
        REQUIRE(config.action == tag_action::remove);
    }

    SECTION("make_empty") {
        auto config = tag_action_config::make_empty();
        REQUIRE(config.action == tag_action::empty);
    }

    SECTION("make_keep") {
        auto config = tag_action_config::make_keep();
        REQUIRE(config.action == tag_action::keep);
    }

    SECTION("make_replace") {
        auto config = tag_action_config::make_replace("custom_value");
        REQUIRE(config.action == tag_action::replace);
        REQUIRE(config.replacement_value == "custom_value");
    }

    SECTION("make_hash") {
        auto config = tag_action_config::make_hash("SHA512", false);
        REQUIRE(config.action == tag_action::hash);
        REQUIRE(config.hash_algorithm == "SHA512");
        REQUIRE_FALSE(config.use_salt);
    }
}

TEST_CASE("Anonymization Report", "[security][anonymization]") {
    anonymization_report report;

    SECTION("Initial state is successful") {
        REQUIRE(report.is_successful());
        REQUIRE(report.total_modifications() == 0);
    }

    SECTION("Errors make report unsuccessful") {
        report.errors.push_back("Test error");
        REQUIRE_FALSE(report.is_successful());
    }

    SECTION("Total modifications calculation") {
        report.tags_removed = 5;
        report.tags_emptied = 3;
        report.tags_replaced = 2;
        report.uids_replaced = 4;
        report.dates_shifted = 1;
        report.values_hashed = 2;

        REQUIRE(report.total_modifications() == 17);
    }
}
