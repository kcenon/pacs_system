// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file aira_assessment.cpp
 * @brief Implementation of IHE AIRA Assessment Creator Actor
 */

#include "pacs/ai/aira_assessment.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <chrono>
#include <random>

namespace kcenon::pacs::ai {

using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// =============================================================================
// DICOM Tags for SR Assessment
// =============================================================================

namespace aira_tags {

constexpr dicom_tag value_type{0x0040, 0xA040};
constexpr dicom_tag concept_name_code_sequence{0x0040, 0xA043};
constexpr dicom_tag text_value{0x0040, 0xA160};
constexpr dicom_tag code_value{0x0008, 0x0100};
constexpr dicom_tag coding_scheme_designator{0x0008, 0x0102};
constexpr dicom_tag code_meaning{0x0008, 0x0104};
constexpr dicom_tag completion_flag{0x0040, 0xA491};
constexpr dicom_tag verification_flag{0x0040, 0xA493};
constexpr dicom_tag content_sequence{0x0040, 0xA730};
constexpr dicom_tag content_template_sequence{0x0040, 0xA504};
constexpr dicom_tag template_identifier{0x0040, 0xDB00};
constexpr dicom_tag mapping_resource{0x0008, 0x0105};
constexpr dicom_tag referenced_sop_sequence{0x0008, 0x1199};
constexpr dicom_tag referenced_sop_class_uid{0x0008, 0x1150};
constexpr dicom_tag referenced_sop_instance_uid{0x0008, 0x1155};
constexpr dicom_tag relationship_type{0x0040, 0xA010};

}  // namespace aira_tags

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

void insert_sequence(dicom_dataset& ds, dicom_tag tag,
                     std::vector<dicom_dataset> items) {
    dicom_element seq_elem(tag, vr_type::SQ);
    seq_elem.sequence_items() = std::move(items);
    ds.insert(std::move(seq_elem));
}

std::string current_datetime() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#if defined(_WIN32)
    gmtime_s(&tm_now, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_now);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &tm_now);
    return buf;
}

std::string current_date() {
    return current_datetime().substr(0, 8);
}

std::string current_time() {
    return current_datetime().substr(8, 6);
}

dicom_dataset make_code_item(const std::string& code_value,
                             const std::string& scheme,
                             const std::string& meaning) {
    dicom_dataset item;
    item.set_string(aira_tags::code_value, vr_type::SH, code_value);
    item.set_string(aira_tags::coding_scheme_designator, vr_type::SH, scheme);
    item.set_string(aira_tags::code_meaning, vr_type::LO, meaning);
    return item;
}

}  // namespace

// =============================================================================
// assessment_creator Implementation
// =============================================================================

assessment_creation_result assessment_creator::create_assessment(
    const ai_assessment& assessment) const {

    assessment_creation_result result;

    // Validate required fields
    if (assessment.ai_result.sop_instance_uid.empty()) {
        result.error_message = "AI result SOP Instance UID is required";
        return result;
    }

    if (assessment.assessor_name.empty()) {
        result.error_message = "Assessor name is required";
        return result;
    }

    core::dicom_dataset sr;

    // --- SOP Common Module ---
    sr.set_string(tags::sop_class_uid, vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.88.22");  // Enhanced SR
    result.assessment_uid = generate_uid();
    sr.set_string(tags::sop_instance_uid, vr_type::UI, result.assessment_uid);

    // --- Patient Module (Type 2 - empty if not provided) ---
    build_patient_module(sr, assessment);

    // --- General Study Module ---
    sr.set_string(tags::study_instance_uid, vr_type::UI,
                  assessment.ai_result.study_instance_uid);
    sr.set_string(tags::study_date, vr_type::DA, current_date());
    sr.set_string(tags::study_time, vr_type::TM, current_time());
    sr.set_string(tags::referring_physician_name, vr_type::PN, "");
    sr.set_string(tags::study_id, vr_type::SH, "");
    sr.set_string(tags::accession_number, vr_type::SH, "");

    // --- General Series Module ---
    sr.set_string(tags::modality, vr_type::CS, "SR");
    sr.set_string(tags::series_instance_uid, vr_type::UI, generate_uid());
    sr.set_string(tags::series_number, vr_type::IS, "1");

    // --- General Equipment Module ---
    sr.set_string(dicom_tag{0x0008, 0x0070}, vr_type::LO, "");  // Manufacturer

    // --- SR Document General Module ---
    sr.set_string(tags::instance_number, vr_type::IS, "1");
    sr.set_string(tags::content_date, vr_type::DA, current_date());
    sr.set_string(tags::content_time, vr_type::TM, current_time());
    sr.set_string(aira_tags::completion_flag, vr_type::CS,
                  status_to_completion_flag(assessment.status));
    sr.set_string(aira_tags::verification_flag, vr_type::CS, "UNVERIFIED");

    // --- SR Document Content Module ---
    build_sr_content(sr, assessment);

    // --- Referenced SOP Sequence ---
    build_referenced_sop_sequence(sr, assessment);

    result.success = true;
    result.sr_dataset = std::move(sr);

    return result;
}

void assessment_creator::build_patient_module(
    core::dicom_dataset& sr,
    [[maybe_unused]] const ai_assessment& assessment) const {

    sr.set_string(tags::patient_name, vr_type::PN, "");
    sr.set_string(tags::patient_id, vr_type::LO, "");
    sr.set_string(tags::patient_birth_date, vr_type::DA, "");
    sr.set_string(tags::patient_sex, vr_type::CS, "");
}

void assessment_creator::build_sr_content(
    core::dicom_dataset& sr,
    const ai_assessment& assessment) const {

    // Root content: CONTAINER
    sr.set_string(aira_tags::value_type, vr_type::CS, "CONTAINER");

    // Concept Name: AI Result Assessment (custom code)
    auto concept_name = make_code_item(
        "AIRA-001", "99PACS", "AI Result Assessment");
    insert_sequence(sr, aira_tags::concept_name_code_sequence, {concept_name});

    // Content Template Sequence (IHE AIRA assessment template)
    dicom_dataset template_item;
    template_item.set_string(aira_tags::template_identifier, vr_type::CS,
                             "AIRA");
    template_item.set_string(aira_tags::mapping_resource, vr_type::CS, "99PACS");
    insert_sequence(sr, aira_tags::content_template_sequence, {template_item});

    // Content Sequence items
    std::vector<dicom_dataset> content_items;

    // Item 1: Assessment Type (CODE)
    {
        dicom_dataset item;
        item.set_string(aira_tags::relationship_type, vr_type::CS, "CONTAINS");
        item.set_string(aira_tags::value_type, vr_type::CS, "CODE");

        auto name = make_code_item("AIRA-010", "99PACS", "Assessment Decision");
        insert_sequence(item, aira_tags::concept_name_code_sequence, {name});

        auto value = make_code_item(
            assessment_type_to_code(assessment.type),
            "99PACS",
            assessment_type_to_meaning(assessment.type));
        // Concept Code Sequence (0040,A168) for CODE value type
        constexpr dicom_tag concept_code_sequence{0x0040, 0xA168};
        insert_sequence(item, concept_code_sequence, {value});

        content_items.push_back(std::move(item));
    }

    // Item 2: Assessor (PNAME)
    {
        dicom_dataset item;
        item.set_string(aira_tags::relationship_type, vr_type::CS, "HAS OBS CONTEXT");
        item.set_string(aira_tags::value_type, vr_type::CS, "PNAME");

        auto name = make_code_item("AIRA-020", "99PACS", "Assessor");
        insert_sequence(item, aira_tags::concept_name_code_sequence, {name});

        constexpr dicom_tag person_name{0x0040, 0xA123};
        item.set_string(person_name, vr_type::PN, assessment.assessor_name);

        content_items.push_back(std::move(item));
    }

    // Item 3: Comment (TEXT, optional)
    if (assessment.comment && !assessment.comment->empty()) {
        dicom_dataset item;
        item.set_string(aira_tags::relationship_type, vr_type::CS, "CONTAINS");
        item.set_string(aira_tags::value_type, vr_type::CS, "TEXT");

        auto name = make_code_item("AIRA-030", "99PACS", "Assessment Comment");
        insert_sequence(item, aira_tags::concept_name_code_sequence, {name});

        item.set_string(aira_tags::text_value, vr_type::UT,
                        assessment.comment.value());

        content_items.push_back(std::move(item));
    }

    // Item 4: Modification description (TEXT, only for modify type)
    if (assessment.type == assessment_type::modify && assessment.modification) {
        dicom_dataset item;
        item.set_string(aira_tags::relationship_type, vr_type::CS, "CONTAINS");
        item.set_string(aira_tags::value_type, vr_type::CS, "TEXT");

        auto name = make_code_item("AIRA-040", "99PACS",
                                   "Modification Description");
        insert_sequence(item, aira_tags::concept_name_code_sequence, {name});

        item.set_string(aira_tags::text_value, vr_type::UT,
                        assessment.modification->description);

        content_items.push_back(std::move(item));
    }

    // Item 5: Rejection reason (CODE, only for reject type)
    if (assessment.type == assessment_type::reject && assessment.rejection) {
        dicom_dataset item;
        item.set_string(aira_tags::relationship_type, vr_type::CS, "CONTAINS");
        item.set_string(aira_tags::value_type, vr_type::CS, "CODE");

        auto name = make_code_item("AIRA-050", "99PACS", "Rejection Reason");
        insert_sequence(item, aira_tags::concept_name_code_sequence, {name});

        auto value = make_code_item(
            assessment.rejection->reason_code,
            assessment.rejection->reason_scheme,
            assessment.rejection->reason_description);
        constexpr dicom_tag concept_code_sequence{0x0040, 0xA168};
        insert_sequence(item, concept_code_sequence, {value});

        content_items.push_back(std::move(item));
    }

    insert_sequence(sr, aira_tags::content_sequence, std::move(content_items));
}

void assessment_creator::build_referenced_sop_sequence(
    core::dicom_dataset& sr,
    const ai_assessment& assessment) const {

    std::vector<dicom_dataset> ref_items;

    // Reference to the assessed AI result
    dicom_dataset ref_item;
    ref_item.set_string(aira_tags::referenced_sop_class_uid, vr_type::UI,
                        assessment.ai_result.sop_class_uid);
    ref_item.set_string(aira_tags::referenced_sop_instance_uid, vr_type::UI,
                        assessment.ai_result.sop_instance_uid);
    ref_items.push_back(std::move(ref_item));

    // Reference to modified result (if applicable)
    if (assessment.type == assessment_type::modify &&
        assessment.modification &&
        assessment.modification->modified_result_uid) {
        dicom_dataset mod_ref;
        mod_ref.set_string(aira_tags::referenced_sop_class_uid, vr_type::UI,
                           assessment.modification->modified_result_class_uid
                               .value_or(assessment.ai_result.sop_class_uid));
        mod_ref.set_string(aira_tags::referenced_sop_instance_uid, vr_type::UI,
                           assessment.modification->modified_result_uid.value());
        ref_items.push_back(std::move(mod_ref));
    }

    insert_sequence(sr, aira_tags::referenced_sop_sequence,
                    std::move(ref_items));
}

std::string assessment_creator::generate_uid() const {
    static constexpr const char* uid_root = "1.2.826.0.1.3680043.2.1545.1";
    static std::mt19937_64 gen{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return std::string(uid_root) + "." + std::to_string(timestamp) +
           "." + std::to_string(dist(gen) % 100000);
}

// =============================================================================
// Static Methods
// =============================================================================

std::string assessment_creator::assessment_type_to_code(assessment_type type) {
    switch (type) {
    case assessment_type::accept: return "AIRA-ACCEPT";
    case assessment_type::modify: return "AIRA-MODIFY";
    case assessment_type::reject: return "AIRA-REJECT";
    }
    return "AIRA-ACCEPT";
}

std::string assessment_creator::assessment_type_to_meaning(assessment_type type) {
    switch (type) {
    case assessment_type::accept: return "AI Result Accepted";
    case assessment_type::modify: return "AI Result Modified";
    case assessment_type::reject: return "AI Result Rejected";
    }
    return "AI Result Accepted";
}

std::string assessment_creator::status_to_completion_flag(
    assessment_status status) {
    switch (status) {
    case assessment_status::draft:   return "PARTIAL";
    case assessment_status::final_:  return "COMPLETE";
    case assessment_status::amended: return "COMPLETE";
    }
    return "PARTIAL";
}

// =============================================================================
// Free Functions
// =============================================================================

std::string to_string(assessment_type type) {
    switch (type) {
    case assessment_type::accept: return "accept";
    case assessment_type::modify: return "modify";
    case assessment_type::reject: return "reject";
    }
    return "accept";
}

std::string to_string(assessment_status status) {
    switch (status) {
    case assessment_status::draft:   return "draft";
    case assessment_status::final_:  return "final";
    case assessment_status::amended: return "amended";
    }
    return "draft";
}

}  // namespace kcenon::pacs::ai
