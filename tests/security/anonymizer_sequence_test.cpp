// BSD 3-Clause License. See LICENSE.
#include <catch2/catch_test_macros.hpp>
#include "kcenon/pacs/security/anonymizer.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "../../tools/dcm_modify/anonymizer.h"
#include <array>

using namespace kcenon::pacs::core;
using namespace kcenon::pacs::security;
using kcenon::pacs::encoding::vr_type;

namespace {
constexpr dicom_tag study_ref{0x0008, 0x1110};
constexpr dicom_tag series_ref{0x0008, 0x1115};
constexpr dicom_tag image_ref{0x0008, 0x1140};
constexpr dicom_tag referenced_instance{0x0008, 0x1155};
constexpr dicom_tag referenced_class{0x0008, 0x1150};
constexpr dicom_tag anatomy{0x0008, 0x2218};
constexpr dicom_tag unknown_sequence{0x7776, 0x0010};
constexpr dicom_tag private_creator{0x0011, 0x0010};
constexpr dicom_tag private_value{0x0011, 0x1010};
constexpr dicom_tag code_value{0x0008, 0x0100};
constexpr dicom_tag request_attributes{0x0040, 0x0275};
constexpr dicom_tag procedure_id{0x0040, 0x1001};

dicom_dataset identifiers() {
    dicom_dataset ds;
    ds.set_string(tags::patient_name, vr_type::PN, "SOURCE^PATIENT");
    ds.set_string(tags::patient_id, vr_type::LO, "SOURCE-ID");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::accession_number, vr_type::SH, "SOURCE-ACCESSION");
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.123001");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.123002");
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.123003");
    ds.set_string(private_creator, vr_type::LO, "SOURCE-CREATOR");
    ds.set_string(private_value, vr_type::LO, "SOURCE-PRIVATE");
    return ds;
}

const dicom_dataset& item(const dicom_dataset& ds, dicom_tag tag) {
    const auto* items = ds.get_sequence(tag);
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    return items->front();
}
}

TEST_CASE("Anonymizer: recursive K and U share root mappings at depth three",
          "[security][anonymization][sequence]") {
    auto ds = identifiers();
    auto study = identifiers();
    auto series = identifiers();
    auto image = identifiers();
    image.set_string(referenced_instance, vr_type::UI, "1.2.3.123003");
    image.set_string(referenced_class, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    image.set_string(code_value, vr_type::SH, "T-D3000");
    series.get_or_create_sequence(image_ref).push_back(std::move(image));
    study.get_or_create_sequence(series_ref).push_back(std::move(series));
    ds.get_or_create_sequence(study_ref).push_back(std::move(study));
    anonymizer anon;
    // Basic's X/Z defaults to Z. Explicit K tests supported recursive retention.
    anon.add_tag_action(study_ref, tag_action_config::make_keep());
    anon.set_private_tag_action(private_tag_action::remove_all);
    anon.set_detailed_reporting(true);
    uid_mapping mapping;
    auto result = anon.anonymize_with_mapping(ds, mapping);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_successful());
    const auto& first = item(ds, study_ref);
    const auto& second = item(first, series_ref);
    const auto& third = item(second, image_ref);
    for (const auto* level : std::array<const dicom_dataset*, 4>{&ds, &first, &second, &third}) {
        CHECK(level->get_string(tags::patient_name) == "ANONYMOUS");
        CHECK(level->get_string(tags::patient_id) == "ANON_ID");
        CHECK(level->get_string(tags::accession_number).empty());
        CHECK(level->get_string(tags::study_instance_uid) == ds.get_string(tags::study_instance_uid));
        CHECK(level->get_string(tags::series_instance_uid) == ds.get_string(tags::series_instance_uid));
        CHECK_FALSE(level->contains(private_creator));
        CHECK_FALSE(level->contains(private_value));
    }
    CHECK(third.get_string(referenced_instance) == ds.get_string(tags::sop_instance_uid));
    CHECK(third.get_string(referenced_class) == "1.2.840.10008.5.1.4.1.1.2");
    CHECK(third.get_string(code_value) == "T-D3000");
    CHECK(mapping.size() == 3);
    CHECK(result.value().private_tags_removed == 8);
    CHECK(result.value().uids_replaced == 13); // Sequence containers are not UIDs.
    CHECK(result.value().action_records.size() == result.value().total_tags_processed);
}

TEST_CASE("Anonymizer: explicit X Z D and conservative unknown sequence actions",
          "[security][anonymization][sequence]") {
    anonymizer anon;
    dicom_dataset ds;
    constexpr dicom_tag other_ids{0x0010, 0x1002}; // X
    constexpr dicom_tag issuer{0x0040, 0x0513}; // Z
    constexpr dicom_tag content{0x0040, 0xA730}; // D in current E.1-1
    for (auto tag : {other_ids, request_attributes, issuer, content, study_ref, unknown_sequence}) {
        ds.get_or_create_sequence(tag).push_back(identifiers());
    }
    auto nested = identifiers();
    nested.get_or_create_sequence(other_ids).push_back(identifiers());
    nested.get_or_create_sequence(unknown_sequence).push_back(identifiers());
    ds.get_or_create_sequence(anatomy).push_back(std::move(nested));
    auto result = anon.anonymize(ds);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_successful());
    CHECK_FALSE(ds.contains(other_ids));
    CHECK_FALSE(ds.contains(request_attributes));
    for (auto tag : {issuer, study_ref, unknown_sequence}) {
        REQUIRE(ds.get_sequence(tag) != nullptr);
        CHECK(ds.get_sequence(tag)->empty());
        CHECK(ds.get(tag)->raw_data().empty());
    }
    CHECK(item(ds, content).empty());
    const auto& cleaned = item(ds, anatomy);
    CHECK_FALSE(cleaned.contains(other_ids));
    REQUIRE(cleaned.get_sequence(unknown_sequence) != nullptr);
    CHECK(cleaned.get_sequence(unknown_sequence)->empty());
    CHECK(cleaned.get_string(tags::patient_name) == "ANONYMOUS");
}

TEST_CASE("Anonymizer: C retains cleaned request items and clinical codes",
          "[security][anonymization][sequence]") {
    anonymizer anon(anonymization_profile::clean_descriptions);
    dicom_dataset ds;
    auto request = identifiers();
    request.set_string(procedure_id, vr_type::SH, "SOURCE-PROCEDURE");
    request.set_string(tags::study_description, vr_type::LO, "SOURCE-TEXT");
    request.set_string(code_value, vr_type::SH, "CHEST");
    ds.get_or_create_sequence(request_attributes).push_back(std::move(request));
    auto result = anon.anonymize(ds);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_successful());
    CHECK(anon.get_tag_action(request_attributes).action == tag_action::clean);
    const auto& cleaned = item(ds, request_attributes);
    CHECK(cleaned.get_string(procedure_id).empty());
    CHECK(cleaned.get_string(tags::study_description).empty());
    CHECK(cleaned.get_string(tags::patient_id) == "ANON_ID");
    CHECK(cleaned.get_string(code_value) == "CHEST");
}

TEST_CASE("Anonymizer: UID multiplicity and cross-file references are consistent",
          "[security][anonymization][sequence]") {
    anonymizer anon;
    uid_mapping mapping;
    auto first = identifiers();
    constexpr dicom_tag uid_list{0x0008, 0x0058};
    first.set_string(uid_list, vr_type::UI, "1.2.3.123003\\1.2.3.123004\\1.2.3.123003");
    REQUIRE(anon.anonymize_with_mapping(first, mapping).value().is_successful());
    auto second = identifiers();
    second.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.123004");
    dicom_dataset reference;
    reference.set_string(referenced_instance, vr_type::UI, "1.2.3.123003");
    second.get_or_create_sequence(image_ref).push_back(std::move(reference));
    REQUIRE(anon.anonymize_with_mapping(second, mapping).value().is_successful());
    const auto first_uid = first.get_string(tags::sop_instance_uid);
    const auto second_uid = second.get_string(tags::sop_instance_uid);
    CHECK(first_uid != second_uid);
    CHECK(first.get_string(uid_list) == first_uid + "\\" + second_uid + "\\" + first_uid);
    CHECK(item(second, image_ref).get_string(referenced_instance) == first_uid);
}

TEST_CASE("Anonymizer: private policy and custom scalar actions apply inside K sequences",
          "[security][anonymization][sequence]") {
    anonymizer anon;
    dicom_dataset ds;
    auto outer = identifiers();
    outer.get_or_create_sequence(anatomy).push_back(identifiers());
    ds.get_or_create_sequence(anatomy).push_back(std::move(outer));
    bool creator = true;
    bool data = true;
    SECTION("default keeps private scalar elements") {}
    SECTION("remove_data keeps creators at every depth") {
        anon.set_private_tag_action(private_tag_action::remove_data);
        data = false;
    }
    SECTION("remove_all removes creators at every depth") {
        anon.set_private_tag_action(private_tag_action::remove_all);
        creator = data = false;
    }
    anon.add_tag_action(tags::patient_name, tag_action_config::make_replace("CUSTOM"));
    auto result = anon.anonymize(ds);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_successful());
    const auto& first = item(ds, anatomy);
    for (const auto* level : {&first, &item(first, anatomy)}) {
        CHECK(level->contains(private_creator) == creator);
        CHECK(level->contains(private_value) == data);
        CHECK(level->get_string(tags::patient_name) == "CUSTOM");
    }
}

TEST_CASE("Anonymizer: unsafe sequence payloads report errors without exposing identifiers",
          "[security][anonymization][sequence]") {
    anonymizer anon;
    dicom_dataset ds;
    SECTION("unparsed data in a retained sequence") {
        const std::array<uint8_t, 6> bytes{'S', 'O', 'U', 'R', 'C', 'E'};
        ds.insert(dicom_element{anatomy, vr_type::SQ, bytes});
    }
    SECTION("opaque UN cannot bypass an explicit K action") {
        const std::array<uint8_t, 6> bytes{'S', 'O', 'U', 'R', 'C', 'E'};
        ds.insert(dicom_element{anatomy, vr_type::UN, bytes});
    }
    SECTION("unsupported scalar transform on a sequence") {
        ds.get_or_create_sequence(anatomy).push_back(identifiers());
        anon.add_tag_action(anatomy, tag_action_config::make_hash());
    }
    auto result = anon.anonymize(ds);
    REQUIRE(result.is_ok());
    CHECK_FALSE(result.value().is_successful());
    REQUIRE(result.value().errors.size() == 1);
    CHECK(result.value().errors.front().find("SOURCE") == std::string::npos);
    REQUIRE(ds.get(anatomy) != nullptr);
    CHECK(ds.get(anatomy)->sequence_items().empty());
    CHECK(ds.get(anatomy)->raw_data().empty());
}

TEST_CASE("Anonymizer: nesting limit accepts 64 levels and rejects 65",
          "[security][anonymization][sequence]") {
    for (std::size_t levels : {64U, 65U}) {
        auto ds = identifiers();
        for (std::size_t i = 0; i < levels; ++i) {
            dicom_dataset parent;
            parent.get_or_create_sequence(anatomy).push_back(std::move(ds));
            ds = std::move(parent);
        }
        anonymizer anon;
        auto result = anon.anonymize(ds);
        REQUIRE(result.is_ok());
        CHECK(result.value().is_successful() == (levels == 64));
    }
}

TEST_CASE("DCM modify compatibility anonymizer delegates nested processing and UID retention",
          "[security][anonymization][sequence]") {
    kcenon::pacs::dcm_modify::anonymize_options options;
    SECTION("remap instance references") {}
    SECTION("retain primary and referenced UIDs") { options.replace_uids = false; }
    kcenon::pacs::dcm_modify::anonymizer anon(options);
    auto ds = identifiers();
    auto reference = identifiers();
    reference.set_string(referenced_instance, vr_type::UI, "1.2.3.123003");
    ds.get_or_create_sequence(image_ref).push_back(std::move(reference));
    const auto mapped = anon.get_uid_mapper().map("1.2.3.123003");
    auto result = anon.anonymize(ds);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_successful());
    const auto& cleaned = item(ds, image_ref);
    CHECK(cleaned.get_string(tags::patient_name) == "ANONYMOUS");
    CHECK(cleaned.get_string(tags::patient_id) == ds.get_string(tags::patient_id));
    CHECK_FALSE(cleaned.contains(tags::patient_birth_date));
    CHECK_FALSE(cleaned.contains(private_creator));
    CHECK_FALSE(cleaned.contains(private_value));
    CHECK(cleaned.get_string(referenced_instance) == ds.get_string(tags::sop_instance_uid));
    CHECK(ds.get_string(tags::sop_instance_uid) == (options.replace_uids ? mapped : "1.2.3.123003"));
}

TEST_CASE("Anonymizer: unknown opaque values cannot bypass the sequence guard",
          "[security][anonymization][sequence]") {
    anonymizer anon;
    dicom_dataset ds;
    const std::array<uint8_t, 6> bytes{'S', 'O', 'U', 'R', 'C', 'E'};
    ds.insert(dicom_element{unknown_sequence, vr_type::UN, bytes});
    auto result = anon.anonymize(ds);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_successful());
    REQUIRE(ds.get(unknown_sequence) != nullptr);
    CHECK(ds.get(unknown_sequence)->raw_data().empty());
}

TEST_CASE("Anonymizer: every preset applies its scalar policy at depth two",
          "[security][anonymization][sequence]") {
    for (auto profile : {anonymization_profile::basic, anonymization_profile::clean_pixel,
                         anonymization_profile::clean_descriptions, anonymization_profile::retain_longitudinal,
                         anonymization_profile::retain_patient_characteristics,
                         anonymization_profile::hipaa_safe_harbor, anonymization_profile::gdpr_compliant}) {
        CAPTURE(to_string(profile));
        anonymizer anon(profile);
        anon.set_date_offset(std::chrono::days{30});
        auto ds = identifiers();
        auto first = identifiers();
        first.get_or_create_sequence(anatomy).push_back(identifiers());
        ds.get_or_create_sequence(anatomy).push_back(std::move(first));
        auto result = anon.anonymize(ds);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().is_successful());
        const auto& nested = item(item(ds, anatomy), anatomy);
        for (auto tag : {tags::patient_name, tags::patient_id, tags::patient_birth_date,
                         tags::accession_number, tags::study_instance_uid}) {
            CHECK(nested.get_string(tag) == ds.get_string(tag));
        }
        CHECK(nested.get_string(tags::patient_name) != "SOURCE^PATIENT");
        CHECK(nested.get_string(tags::patient_id) != "SOURCE-ID");
        CHECK(nested.get_string(tags::patient_birth_date) != "19700101");
    }
}
