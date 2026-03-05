// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file waveform_ps_iod_validator.cpp
 * @brief Implementation of Waveform Presentation State IOD Validator
 */

#include "pacs/services/validation/waveform_ps_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/waveform_storage.hpp"

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// Waveform PS-specific DICOM Tags
// =============================================================================

namespace waveform_tags {

// Referenced Waveform Sequence
constexpr dicom_tag referenced_series_sequence{0x0008, 0x1115};

// Waveform Presentation State Module
constexpr dicom_tag waveform_presentation_sequence{0x0070, 0x0001};

// Waveform Annotation Module
constexpr dicom_tag waveform_annotation_sequence{0x0040, 0xB020};

// General Equipment Module
constexpr dicom_tag manufacturer{0x0008, 0x0070};

}  // namespace waveform_tags

// =============================================================================
// waveform_ps_iod_validator Implementation
// =============================================================================

waveform_ps_iod_validator::waveform_ps_iod_validator(
    const waveform_ps_validation_options& options)
    : options_(options) {}

validation_result waveform_ps_iod_validator::validate(
    const core::dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result);
        validate_general_study_module(dataset, result);
        validate_general_series_module(dataset, result);
        validate_general_equipment_module(dataset, result);
        validate_sop_common_module(dataset, result);
    }

    // Determine if this is a Presentation State or Annotation
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    if (sop_classes::is_waveform_presentation_state_sop_class(sop_class)) {
        if (options_.validate_display_config) {
            validate_waveform_ps_module(dataset, result);
        }
    } else if (sop_classes::is_waveform_annotation_sop_class(sop_class)) {
        if (options_.validate_annotations) {
            validate_waveform_annotation_module(dataset, result);
        }
    }

    if (options_.validate_references) {
        auto ref_result = validate_references(dataset);
        result.findings.insert(result.findings.end(),
                               ref_result.findings.begin(),
                               ref_result.findings.end());
    }

    // Check for errors
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

validation_result waveform_ps_iod_validator::validate_references(
    const core::dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    if (!dataset.contains(waveform_tags::referenced_series_sequence)) {
        result.findings.push_back({
            validation_severity::warning,
            waveform_tags::referenced_series_sequence,
            "ReferencedSeriesSequence (0008,1115) should be present for waveform references",
            "WF-REF-WARN-001"
        });
    }

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool waveform_ps_iod_validator::quick_check(
    const core::dicom_dataset& dataset) const {
    // Check essential Type 1 attributes
    if (!dataset.contains(tags::study_instance_uid)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    // Verify SOP Class is a waveform PS or annotation class
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    if (!sop_classes::is_waveform_presentation_state_sop_class(sop_class) &&
        !sop_classes::is_waveform_annotation_sop_class(sop_class)) {
        return false;
    }

    return true;
}

const waveform_ps_validation_options&
waveform_ps_iod_validator::options() const noexcept {
    return options_;
}

void waveform_ps_iod_validator::set_options(
    const waveform_ps_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void waveform_ps_iod_validator::validate_patient_module(
    const core::dicom_dataset& dataset,
    validation_result& result) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName", result);
        check_type2_attribute(dataset, tags::patient_id, "PatientID", result);
        check_type2_attribute(dataset, tags::patient_birth_date, "PatientBirthDate", result);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex", result);
    }
}

void waveform_ps_iod_validator::validate_general_study_module(
    const core::dicom_dataset& dataset,
    validation_result& result) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid, "StudyInstanceUID", result);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate", result);
        check_type2_attribute(dataset, tags::study_time, "StudyTime", result);
        check_type2_attribute(dataset, tags::referring_physician_name, "ReferringPhysicianName", result);
        check_type2_attribute(dataset, tags::study_id, "StudyID", result);
        check_type2_attribute(dataset, tags::accession_number, "AccessionNumber", result);
    }
}

void waveform_ps_iod_validator::validate_general_series_module(
    const core::dicom_dataset& dataset,
    validation_result& result) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", result);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", result);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", result);
    }
}

void waveform_ps_iod_validator::validate_general_equipment_module(
    const core::dicom_dataset& dataset,
    validation_result& result) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, waveform_tags::manufacturer, "Manufacturer", result);
    }
}

void waveform_ps_iod_validator::validate_sop_common_module(
    const core::dicom_dataset& dataset,
    validation_result& result) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", result);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", result);
    }

    // Validate SOP Class UID is a waveform PS or annotation class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_waveform_presentation_state_sop_class(sop_class) &&
            !sop_classes::is_waveform_annotation_sop_class(sop_class)) {
            result.findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized Waveform PS/Annotation SOP Class: " + sop_class,
                "WF-ERR-001"
            });
        }
    }
}

void waveform_ps_iod_validator::validate_waveform_ps_module(
    const core::dicom_dataset& dataset,
    validation_result& result) const {

    // Waveform Presentation Sequence is expected for PS objects
    if (options_.check_conditional) {
        if (!dataset.contains(waveform_tags::waveform_presentation_sequence)) {
            result.findings.push_back({
                validation_severity::warning,
                waveform_tags::waveform_presentation_sequence,
                "WaveformPresentationSequence (0070,0001) should be present for "
                "Waveform Presentation State objects",
                "WF-PS-WARN-001"
            });
        }
    }
}

void waveform_ps_iod_validator::validate_waveform_annotation_module(
    const core::dicom_dataset& dataset,
    validation_result& result) const {

    // Waveform Annotation Sequence is expected for annotation objects
    if (options_.check_conditional) {
        if (!dataset.contains(waveform_tags::waveform_annotation_sequence)) {
            result.findings.push_back({
                validation_severity::warning,
                waveform_tags::waveform_annotation_sequence,
                "WaveformAnnotationSequence (0040,B020) should be present for "
                "Waveform Annotation objects",
                "WF-ANN-WARN-001"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void waveform_ps_iod_validator::check_type1_attribute(
    const core::dicom_dataset& dataset,
    const core::dicom_tag& tag,
    const std::string& description,
    validation_result& result) const {

    if (!dataset.contains(tag)) {
        result.findings.push_back({
            validation_severity::error,
            tag,
            "Type 1 attribute missing: " + description +
            " (" + tag.to_string() + ")",
            "WF-TYPE1-MISSING"
        });
    } else {
        const auto* element = dataset.get(tag);
        if (element != nullptr && !element->is_sequence()) {
            auto value = dataset.get_string(tag);
            if (value.empty()) {
                result.findings.push_back({
                    validation_severity::error,
                    tag,
                    "Type 1 attribute has empty value: " + description +
                    " (" + tag.to_string() + ")",
                    "WF-TYPE1-EMPTY"
                });
            }
        }
    }
}

void waveform_ps_iod_validator::check_type2_attribute(
    const core::dicom_dataset& dataset,
    const core::dicom_tag& tag,
    const std::string& description,
    validation_result& result) const {

    if (!dataset.contains(tag)) {
        result.findings.push_back({
            validation_severity::warning,
            tag,
            "Type 2 attribute missing: " + description +
            " (" + tag.to_string() + ")",
            "WF-TYPE2-MISSING"
        });
    }
}

}  // namespace pacs::services::validation
