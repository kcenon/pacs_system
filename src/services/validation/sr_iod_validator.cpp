/**
 * @file sr_iod_validator.cpp
 * @brief Implementation of Structured Report IOD Validator
 */

#include "pacs/services/validation/sr_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/sr_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// SR-specific DICOM Tags
// =============================================================================

namespace sr_tags {

// SR Document Series Module
[[maybe_unused]] constexpr dicom_tag modality{0x0008, 0x0060};
[[maybe_unused]] constexpr dicom_tag series_instance_uid{0x0020, 0x000E};

// SR Document General Module
constexpr dicom_tag instance_number{0x0020, 0x0013};
constexpr dicom_tag completion_flag{0x0040, 0xA491};
constexpr dicom_tag verification_flag{0x0040, 0xA493};
constexpr dicom_tag content_date{0x0008, 0x0023};
constexpr dicom_tag content_time{0x0008, 0x0033};
constexpr dicom_tag verifying_observer_sequence{0x0040, 0xA073};
[[maybe_unused]] constexpr dicom_tag predecessor_documents_sequence{0x0040, 0xA360};
[[maybe_unused]] constexpr dicom_tag identical_documents_sequence{0x0040, 0xA525};

// SR Document Content Module
constexpr dicom_tag value_type{0x0040, 0xA040};
constexpr dicom_tag concept_name_code_sequence{0x0040, 0xA043};
constexpr dicom_tag content_sequence{0x0040, 0xA730};
[[maybe_unused]] constexpr dicom_tag continuity_of_content{0x0040, 0xA050};

// Content Item attributes
constexpr dicom_tag relationship_type{0x0040, 0xA010};
constexpr dicom_tag text_value{0x0040, 0xA160};
constexpr dicom_tag measured_value_sequence{0x0040, 0xA300};
constexpr dicom_tag numeric_value{0x0040, 0xA30A};
constexpr dicom_tag measurement_units_code_sequence{0x0040, 0x08EA};

// Code Sequence attributes
constexpr dicom_tag code_value{0x0008, 0x0100};
constexpr dicom_tag coding_scheme_designator{0x0008, 0x0102};
constexpr dicom_tag code_meaning{0x0008, 0x0104};

// Reference attributes
constexpr dicom_tag referenced_sop_class_uid{0x0008, 0x1150};
constexpr dicom_tag referenced_sop_instance_uid{0x0008, 0x1155};
constexpr dicom_tag referenced_sop_sequence{0x0008, 0x1199};

// Evidence sequences
constexpr dicom_tag current_requested_procedure_evidence_sequence{0x0040, 0xA375};
constexpr dicom_tag pertinent_other_evidence_sequence{0x0040, 0xA385};

// Spatial coordinates
constexpr dicom_tag graphic_data{0x0070, 0x0022};
constexpr dicom_tag graphic_type{0x0070, 0x0023};
constexpr dicom_tag referenced_frame_of_reference_uid{0x3006, 0x0024};

// Temporal coordinates
[[maybe_unused]] constexpr dicom_tag temporal_range_type{0x0040, 0xA130};
[[maybe_unused]] constexpr dicom_tag referenced_sample_positions{0x0040, 0xA132};

// Template identification
[[maybe_unused]] constexpr dicom_tag template_identifier{0x0040, 0xDB00};
[[maybe_unused]] constexpr dicom_tag mapping_resource{0x0008, 0x0105};

// Key Object Selection specific
[[maybe_unused]] constexpr dicom_tag referenced_series_sequence{0x0008, 0x1115};

}  // namespace sr_tags

// =============================================================================
// sr_iod_validator Implementation
// =============================================================================

sr_iod_validator::sr_iod_validator(const sr_validation_options& options)
    : options_(options) {}

validation_result sr_iod_validator::validate(const dicom_dataset& dataset) const {
    // Detect SR type and delegate to appropriate validation
    if (!dataset.contains(tags::sop_class_uid)) {
        validation_result result;
        result.is_valid = false;
        result.findings.push_back({
            validation_severity::error,
            tags::sop_class_uid,
            "SOPClassUID is required to determine SR type",
            "SR-ERR-000"
        });
        return result;
    }

    auto sop_class = dataset.get_string(tags::sop_class_uid);
    auto doc_type = sop_classes::get_sr_document_type(sop_class);

    switch (doc_type) {
        case sop_classes::sr_document_type::basic_text:
            return validate_basic_text_sr(dataset);
        case sop_classes::sr_document_type::enhanced:
            return validate_enhanced_sr(dataset);
        case sop_classes::sr_document_type::comprehensive:
        case sop_classes::sr_document_type::comprehensive_3d:
            return validate_comprehensive_sr(dataset);
        case sop_classes::sr_document_type::key_object_selection:
            return validate_key_object_selection(dataset);
        default:
            // For other types, use comprehensive validation
            return validate_comprehensive_sr(dataset);
    }
}

validation_result
sr_iod_validator::validate_basic_text_sr(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Common modules
    validate_patient_module(dataset, result.findings);
    validate_general_study_module(dataset, result.findings);
    validate_sr_document_series_module(dataset, result.findings);
    validate_general_equipment_module(dataset, result.findings);
    validate_sr_document_general_module(dataset, result.findings);
    validate_sr_document_content_module(dataset, result.findings);
    validate_sop_common_module(dataset, result.findings);

    // Basic Text SR does not support SCOORD, IMAGE, WAVEFORM, etc.
    // Content validation will handle value type restrictions

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
sr_iod_validator::validate_enhanced_sr(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Common modules
    validate_patient_module(dataset, result.findings);
    validate_general_study_module(dataset, result.findings);
    validate_sr_document_series_module(dataset, result.findings);
    validate_general_equipment_module(dataset, result.findings);
    validate_sr_document_general_module(dataset, result.findings);
    validate_sr_document_content_module(dataset, result.findings);
    validate_sop_common_module(dataset, result.findings);

    // Enhanced SR specific - evidence sequences
    if (options_.validate_references) {
        validate_evidence_sequences(dataset, result.findings);
    }

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
sr_iod_validator::validate_comprehensive_sr(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Common modules
    validate_patient_module(dataset, result.findings);
    validate_general_study_module(dataset, result.findings);
    validate_sr_document_series_module(dataset, result.findings);
    validate_general_equipment_module(dataset, result.findings);
    validate_sr_document_general_module(dataset, result.findings);
    validate_sr_document_content_module(dataset, result.findings);
    validate_sop_common_module(dataset, result.findings);

    // Comprehensive SR specific - evidence sequences
    if (options_.validate_references) {
        validate_evidence_sequences(dataset, result.findings);
    }

    // Content tree validation (includes SCOORD support)
    if (options_.validate_content_sequence) {
        validate_content_sequence(dataset, result.findings);
    }

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
sr_iod_validator::validate_key_object_selection(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    if (!options_.validate_key_object_selection) {
        return result;
    }

    // Common modules
    validate_patient_module(dataset, result.findings);
    validate_general_study_module(dataset, result.findings);
    validate_sr_document_series_module(dataset, result.findings);
    validate_general_equipment_module(dataset, result.findings);
    validate_sr_document_general_module(dataset, result.findings);
    validate_sr_document_content_module(dataset, result.findings);
    validate_sop_common_module(dataset, result.findings);

    // KOS must have Current Requested Procedure Evidence Sequence
    if (options_.check_type1) {
        if (!dataset.contains(sr_tags::current_requested_procedure_evidence_sequence)) {
            result.findings.push_back({
                validation_severity::error,
                sr_tags::current_requested_procedure_evidence_sequence,
                "CurrentRequestedProcedureEvidenceSequence is required for Key Object Selection",
                "SR-KOS-ERR-001"
            });
        }
    }

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
sr_iod_validator::validate_content_tree(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    validate_content_sequence(dataset, result.findings);

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
sr_iod_validator::validate_references(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    validate_evidence_sequences(dataset, result.findings);

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool sr_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check essential Type 1 attributes

    // General Study Module
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is SR
    auto modality = dataset.get_string(tags::modality);
    if (modality != "SR") return false;

    // SR Document General Module
    if (!dataset.contains(sr_tags::completion_flag)) return false;
    if (!dataset.contains(sr_tags::verification_flag)) return false;

    // SR Document Content Module - root content item
    if (!dataset.contains(sr_tags::value_type)) return false;
    if (!dataset.contains(sr_tags::concept_name_code_sequence)) return false;

    // SOP Common Module
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    // Verify SOP Class is SR
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    if (!sop_classes::is_sr_storage_sop_class(sop_class)) return false;

    return true;
}

const sr_validation_options& sr_iod_validator::options() const noexcept {
    return options_;
}

void sr_iod_validator::set_options(const sr_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void sr_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName", findings);
        check_type2_attribute(dataset, tags::patient_id, "PatientID", findings);
        check_type2_attribute(dataset, tags::patient_birth_date, "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex", findings);
    }
}

void sr_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid, "StudyInstanceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate", findings);
        check_type2_attribute(dataset, tags::study_time, "StudyTime", findings);
        check_type2_attribute(dataset, tags::referring_physician_name, "ReferringPhysicianName", findings);
        check_type2_attribute(dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number, "AccessionNumber", findings);
    }
}

void sr_iod_validator::validate_sr_document_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);
        check_modality(dataset, findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void sr_iod_validator::validate_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    constexpr dicom_tag manufacturer{0x0008, 0x0070};
    if (options_.check_type2) {
        check_type2_attribute(dataset, manufacturer, "Manufacturer", findings);
    }
}

void sr_iod_validator::validate_sr_document_general_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, sr_tags::instance_number, "InstanceNumber", findings);
        check_type1_attribute(dataset, sr_tags::completion_flag, "CompletionFlag", findings);
        check_type1_attribute(dataset, sr_tags::verification_flag, "VerificationFlag", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, sr_tags::content_date, "ContentDate", findings);
        check_type2_attribute(dataset, sr_tags::content_time, "ContentTime", findings);
    }

    // Validate flag values
    if (options_.validate_document_status) {
        check_completion_flag(dataset, findings);
        check_verification_flag(dataset, findings);
    }

    // VerifyingObserverSequence is Type 1C - required if VerificationFlag is VERIFIED
    if (options_.check_conditional && dataset.contains(sr_tags::verification_flag)) {
        auto flag = dataset.get_string(sr_tags::verification_flag);
        if (flag == "VERIFIED" && !dataset.contains(sr_tags::verifying_observer_sequence)) {
            findings.push_back({
                validation_severity::error,
                sr_tags::verifying_observer_sequence,
                "VerifyingObserverSequence is required when VerificationFlag is VERIFIED",
                "SR-DOC-ERR-001"
            });
        }
    }
}

void sr_iod_validator::validate_sr_document_content_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Root content item attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, sr_tags::value_type, "ValueType", findings);
        check_type1_attribute(dataset, sr_tags::concept_name_code_sequence, "ConceptNameCodeSequence", findings);
    }

    // Validate root value type
    if (options_.validate_value_types && dataset.contains(sr_tags::value_type)) {
        auto value_type = dataset.get_string(sr_tags::value_type);
        if (value_type != "CONTAINER") {
            findings.push_back({
                validation_severity::error,
                sr_tags::value_type,
                "Root content item ValueType must be CONTAINER, found: " + value_type,
                "SR-CONTENT-ERR-001"
            });
        }
    }

    // Validate Concept Name Code Sequence
    if (options_.validate_coded_entries && dataset.contains(sr_tags::concept_name_code_sequence)) {
        const auto* concept_elem = dataset.get(sr_tags::concept_name_code_sequence);
        if (concept_elem && concept_elem->is_sequence() && !concept_elem->sequence_items().empty()) {
            validate_coded_entry(concept_elem->sequence_items()[0], "Root Concept Name", findings);
        }
    }

    // Validate content sequence
    if (options_.validate_content_sequence) {
        validate_content_sequence(dataset, findings);
    }
}

void sr_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is an SR storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_sr_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized SR Storage SOP Class: " + sop_class,
                "SR-ERR-001"
            });
        }
    }
}

void sr_iod_validator::validate_evidence_sequences(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // For Enhanced/Comprehensive SR, at least one evidence sequence should be present
    bool has_evidence = dataset.contains(sr_tags::current_requested_procedure_evidence_sequence) ||
                       dataset.contains(sr_tags::pertinent_other_evidence_sequence);

    if (!has_evidence) {
        findings.push_back({
            validation_severity::info,
            sr_tags::current_requested_procedure_evidence_sequence,
            "No evidence sequences present - SR may not properly link to source images",
            "SR-EVIDENCE-INFO-001"
        });
    }
}

void sr_iod_validator::validate_content_sequence(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(sr_tags::content_sequence)) {
        findings.push_back({
            validation_severity::info,
            sr_tags::content_sequence,
            "ContentSequence not present - SR document has no content items",
            "SR-CONTENT-INFO-001"
        });
        return;
    }

    const auto* content_elem = dataset.get(sr_tags::content_sequence);
    if (!content_elem || !content_elem->is_sequence() || content_elem->sequence_items().empty()) {
        findings.push_back({
            validation_severity::info,
            sr_tags::content_sequence,
            "ContentSequence is empty",
            "SR-CONTENT-INFO-002"
        });
        return;
    }

    // Get root value type for context
    auto root_value_type = dataset.get_string(sr_tags::value_type);

    // Validate each content item
    const auto& content_seq = content_elem->sequence_items();
    for (size_t i = 0; i < content_seq.size(); ++i) {
        validate_content_item(content_seq[i], 1, root_value_type, findings);
    }
}

void sr_iod_validator::validate_content_item(
    const dicom_dataset& item,
    size_t depth,
    [[maybe_unused]] std::string_view parent_value_type,
    std::vector<validation_finding>& findings) const {

    // Prevent infinite recursion
    if (depth > 100) {
        findings.push_back({
            validation_severity::warning,
            sr_tags::content_sequence,
            "Content tree depth exceeds 100 - possible circular reference",
            "SR-CONTENT-WARN-001"
        });
        return;
    }

    // Check required attributes
    if (!item.contains(sr_tags::relationship_type)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::relationship_type,
            "RelationshipType missing in content item at depth " + std::to_string(depth),
            "SR-ITEM-ERR-001"
        });
    }

    if (!item.contains(sr_tags::value_type)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::value_type,
            "ValueType missing in content item at depth " + std::to_string(depth),
            "SR-ITEM-ERR-002"
        });
        return;  // Can't validate further without value type
    }

    auto value_type = item.get_string(sr_tags::value_type);
    auto sr_vt = sop_classes::parse_sr_value_type(value_type);

    if (sr_vt == sop_classes::sr_value_type::unknown) {
        findings.push_back({
            validation_severity::error,
            sr_tags::value_type,
            "Invalid ValueType: " + value_type,
            "SR-ITEM-ERR-003"
        });
        return;
    }

    // Value type specific validation
    switch (sr_vt) {
        case sop_classes::sr_value_type::text:
            validate_text_content_item(item, findings);
            break;
        case sop_classes::sr_value_type::code:
            validate_code_content_item(item, findings);
            break;
        case sop_classes::sr_value_type::num:
            validate_num_content_item(item, findings);
            break;
        case sop_classes::sr_value_type::image:
            validate_image_content_item(item, findings);
            break;
        case sop_classes::sr_value_type::scoord:
            validate_scoord_content_item(item, findings);
            break;
        case sop_classes::sr_value_type::scoord3d:
            validate_scoord3d_content_item(item, findings);
            break;
        default:
            break;
    }

    // Validate nested content items
    if (item.contains(sr_tags::content_sequence)) {
        const auto* nested_elem = item.get(sr_tags::content_sequence);
        if (nested_elem && nested_elem->is_sequence()) {
            const auto& nested_seq = nested_elem->sequence_items();
            for (size_t i = 0; i < nested_seq.size(); ++i) {
                validate_content_item(nested_seq[i], depth + 1, value_type, findings);
            }
        }
    }
}

void sr_iod_validator::validate_coded_entry(
    const dicom_dataset& coded_entry,
    std::string_view context,
    std::vector<validation_finding>& findings) const {

    // Code Value
    if (!coded_entry.contains(sr_tags::code_value)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::code_value,
            std::string(context) + ": CodeValue is required",
            "SR-CODE-ERR-001"
        });
    }

    // Coding Scheme Designator
    if (!coded_entry.contains(sr_tags::coding_scheme_designator)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::coding_scheme_designator,
            std::string(context) + ": CodingSchemeDesignator is required",
            "SR-CODE-ERR-002"
        });
    }

    // Code Meaning
    if (!coded_entry.contains(sr_tags::code_meaning)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::code_meaning,
            std::string(context) + ": CodeMeaning is required",
            "SR-CODE-ERR-003"
        });
    }
}

void sr_iod_validator::validate_text_content_item(
    const dicom_dataset& item,
    std::vector<validation_finding>& findings) const {

    if (!item.contains(sr_tags::text_value)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::text_value,
            "TextValue is required for TEXT content item",
            "SR-TEXT-ERR-001"
        });
    }
}

void sr_iod_validator::validate_code_content_item(
    const dicom_dataset& item,
    std::vector<validation_finding>& findings) const {

    if (!item.contains(sr_tags::concept_name_code_sequence)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::concept_name_code_sequence,
            "ConceptNameCodeSequence is required for CODE content item",
            "SR-CODE-ITEM-ERR-001"
        });
    }
}

void sr_iod_validator::validate_num_content_item(
    const dicom_dataset& item,
    std::vector<validation_finding>& findings) const {

    if (!item.contains(sr_tags::measured_value_sequence)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::measured_value_sequence,
            "MeasuredValueSequence is required for NUM content item",
            "SR-NUM-ERR-001"
        });
        return;
    }

    const auto* measured_elem = item.get(sr_tags::measured_value_sequence);
    if (measured_elem && measured_elem->is_sequence() && !measured_elem->sequence_items().empty()) {
        const auto& mv_item = measured_elem->sequence_items()[0];

        if (!mv_item.contains(sr_tags::numeric_value)) {
            findings.push_back({
                validation_severity::error,
                sr_tags::numeric_value,
                "NumericValue is required in MeasuredValueSequence",
                "SR-NUM-ERR-002"
            });
        }

        if (!mv_item.contains(sr_tags::measurement_units_code_sequence)) {
            findings.push_back({
                validation_severity::error,
                sr_tags::measurement_units_code_sequence,
                "MeasurementUnitsCodeSequence is required in MeasuredValueSequence",
                "SR-NUM-ERR-003"
            });
        }
    }
}

void sr_iod_validator::validate_image_content_item(
    const dicom_dataset& item,
    std::vector<validation_finding>& findings) const {

    if (!item.contains(sr_tags::referenced_sop_sequence)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::referenced_sop_sequence,
            "ReferencedSOPSequence is required for IMAGE content item",
            "SR-IMAGE-ERR-001"
        });
        return;
    }

    const auto* ref_elem = item.get(sr_tags::referenced_sop_sequence);
    if (ref_elem && ref_elem->is_sequence() && !ref_elem->sequence_items().empty()) {
        const auto& ref_item = ref_elem->sequence_items()[0];

        if (!ref_item.contains(sr_tags::referenced_sop_class_uid)) {
            findings.push_back({
                validation_severity::error,
                sr_tags::referenced_sop_class_uid,
                "ReferencedSOPClassUID is required in IMAGE reference",
                "SR-IMAGE-ERR-002"
            });
        }

        if (!ref_item.contains(sr_tags::referenced_sop_instance_uid)) {
            findings.push_back({
                validation_severity::error,
                sr_tags::referenced_sop_instance_uid,
                "ReferencedSOPInstanceUID is required in IMAGE reference",
                "SR-IMAGE-ERR-003"
            });
        }
    }
}

void sr_iod_validator::validate_scoord_content_item(
    const dicom_dataset& item,
    std::vector<validation_finding>& findings) const {

    if (!item.contains(sr_tags::graphic_type)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::graphic_type,
            "GraphicType is required for SCOORD content item",
            "SR-SCOORD-ERR-001"
        });
    }

    if (!item.contains(sr_tags::graphic_data)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::graphic_data,
            "GraphicData is required for SCOORD content item",
            "SR-SCOORD-ERR-002"
        });
    }
}

void sr_iod_validator::validate_scoord3d_content_item(
    const dicom_dataset& item,
    std::vector<validation_finding>& findings) const {

    if (!item.contains(sr_tags::graphic_type)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::graphic_type,
            "GraphicType is required for SCOORD3D content item",
            "SR-SCOORD3D-ERR-001"
        });
    }

    if (!item.contains(sr_tags::graphic_data)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::graphic_data,
            "GraphicData is required for SCOORD3D content item",
            "SR-SCOORD3D-ERR-002"
        });
    }

    if (!item.contains(sr_tags::referenced_frame_of_reference_uid)) {
        findings.push_back({
            validation_severity::error,
            sr_tags::referenced_frame_of_reference_uid,
            "ReferencedFrameOfReferenceUID is required for SCOORD3D content item",
            "SR-SCOORD3D-ERR-003"
        });
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void sr_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::error,
            tag,
            std::string("Type 1 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "SR-TYPE1-MISSING"
        });
    } else {
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back({
                validation_severity::error,
                tag,
                std::string("Type 1 attribute has empty value: ") +
                std::string(name) + " (" + tag.to_string() + ")",
                "SR-TYPE1-EMPTY"
            });
        }
    }
}

void sr_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::warning,
            tag,
            std::string("Type 2 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "SR-TYPE2-MISSING"
        });
    }
}

void sr_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "SR") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'SR' for Structured Report objects, found: " + modality,
            "SR-ERR-002"
        });
    }
}

void sr_iod_validator::check_completion_flag(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(sr_tags::completion_flag)) {
        return;
    }

    auto flag = dataset.get_string(sr_tags::completion_flag);
    if (flag != "PARTIAL" && flag != "COMPLETE") {
        findings.push_back({
            validation_severity::error,
            sr_tags::completion_flag,
            "Invalid CompletionFlag value: " + flag + " (must be PARTIAL or COMPLETE)",
            "SR-FLAG-ERR-001"
        });
    }
}

void sr_iod_validator::check_verification_flag(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(sr_tags::verification_flag)) {
        return;
    }

    auto flag = dataset.get_string(sr_tags::verification_flag);
    if (flag != "UNVERIFIED" && flag != "VERIFIED") {
        findings.push_back({
            validation_severity::error,
            sr_tags::verification_flag,
            "Invalid VerificationFlag value: " + flag + " (must be UNVERIFIED or VERIFIED)",
            "SR-FLAG-ERR-002"
        });
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_sr_iod(const dicom_dataset& dataset) {
    sr_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_sr_dataset(const dicom_dataset& dataset) {
    sr_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_sr_complete(const dicom_dataset& dataset) {
    constexpr dicom_tag completion_flag{0x0040, 0xA491};
    if (!dataset.contains(completion_flag)) {
        return false;
    }
    return dataset.get_string(completion_flag) == "COMPLETE";
}

bool is_sr_verified(const dicom_dataset& dataset) {
    constexpr dicom_tag verification_flag{0x0040, 0xA493};
    if (!dataset.contains(verification_flag)) {
        return false;
    }
    return dataset.get_string(verification_flag) == "VERIFIED";
}

size_t get_content_item_count(const dicom_dataset& dataset) {
    constexpr dicom_tag content_sequence{0x0040, 0xA730};
    const auto* element = dataset.get(content_sequence);
    if (element && element->is_sequence()) {
        return element->sequence_items().size();
    }
    return 0;
}

std::string get_sr_document_title(const dicom_dataset& dataset) {
    constexpr dicom_tag concept_name_code_sequence{0x0040, 0xA043};
    constexpr dicom_tag code_meaning{0x0008, 0x0104};

    const auto* element = dataset.get(concept_name_code_sequence);
    if (element && element->is_sequence() && !element->sequence_items().empty()) {
        const auto& item = element->sequence_items()[0];
        if (item.contains(code_meaning)) {
            return item.get_string(code_meaning);
        }
    }
    return "";
}

}  // namespace pacs::services::validation
