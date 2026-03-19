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
 * @file aira_assessment.hpp
 * @brief IHE AIRA (AI Result Assessment) - Assessment Creator Actor
 *
 * Implements the IHE AIRA profile for creating structured assessments
 * of AI-generated results. Clinicians can Accept, Modify, or Reject
 * AI results, and this assessment is encoded as a DICOM Structured
 * Report linked to the original AI result.
 *
 * @see IHE RAD AIRA Supplement (2025-06-12)
 * @see DICOM PS3.3 - Enhanced SR IOD
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_AI_AIRA_ASSESSMENT_HPP
#define PACS_AI_AIRA_ASSESSMENT_HPP

#include "pacs/core/dicom_dataset.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::pacs::ai {

// =============================================================================
// Assessment Types
// =============================================================================

/**
 * @brief Assessment decision made by a clinician on an AI result
 */
enum class assessment_type {
    accept,   ///< Clinician accepts AI result as-is
    modify,   ///< Clinician modifies AI result (e.g., edits segmentation)
    reject    ///< Clinician rejects AI result with reason
};

/**
 * @brief Assessment status tracking
 */
enum class assessment_status {
    draft,      ///< Assessment in progress, not yet finalized
    final_,     ///< Assessment finalized and signed off
    amended     ///< Previously finalized assessment has been amended
};

// =============================================================================
// Data Structures
// =============================================================================

/**
 * @brief Reference to the AI result being assessed
 */
struct assessed_result_reference {
    /// SOP Class UID of the AI result
    std::string sop_class_uid;

    /// SOP Instance UID of the AI result
    std::string sop_instance_uid;

    /// Study Instance UID containing the AI result
    std::string study_instance_uid;

    /// Series Instance UID of the AI result
    std::string series_instance_uid;
};

/**
 * @brief Modification details when assessment_type is modify
 */
struct assessment_modification {
    /// Description of what was modified
    std::string description;

    /// SOP Instance UID of the modified result (if a new object was created)
    std::optional<std::string> modified_result_uid;

    /// SOP Class UID of the modified result
    std::optional<std::string> modified_result_class_uid;
};

/**
 * @brief Rejection reason when assessment_type is reject
 */
struct assessment_rejection {
    /// Coded reason for rejection
    std::string reason_code;

    /// Code scheme designator for the reason
    std::string reason_scheme{"DCM"};

    /// Human-readable reason description
    std::string reason_description;
};

/**
 * @brief Complete assessment of an AI result
 */
struct ai_assessment {
    /// Assessment type (accept/modify/reject)
    assessment_type type;

    /// Current status of the assessment
    assessment_status status{assessment_status::draft};

    /// Reference to the AI result being assessed
    assessed_result_reference ai_result;

    /// Clinician who performed the assessment
    std::string assessor_name;

    /// Clinician identifier
    std::optional<std::string> assessor_id;

    /// Institution name
    std::optional<std::string> institution_name;

    /// Free-text comment about the assessment
    std::optional<std::string> comment;

    /// Modification details (only for assessment_type::modify)
    std::optional<assessment_modification> modification;

    /// Rejection details (only for assessment_type::reject)
    std::optional<assessment_rejection> rejection;

    /// Timestamp of the assessment
    std::chrono::system_clock::time_point assessment_time{
        std::chrono::system_clock::now()};
};

/**
 * @brief Result of creating an assessment SR document
 */
struct assessment_creation_result {
    /// Whether creation succeeded
    bool success{false};

    /// The created assessment SR dataset
    std::optional<core::dicom_dataset> sr_dataset;

    /// SOP Instance UID of the created assessment
    std::string assessment_uid;

    /// Error message (if failed)
    std::string error_message;
};

/**
 * @brief Information about a stored assessment
 */
struct assessment_info {
    /// SOP Instance UID of the assessment SR
    std::string assessment_uid;

    /// Assessment type
    assessment_type type;

    /// Assessment status
    assessment_status status;

    /// Reference to the assessed AI result
    std::string ai_result_uid;

    /// Assessor name
    std::string assessor_name;

    /// Assessment timestamp
    std::chrono::system_clock::time_point assessment_time;
};

// =============================================================================
// Assessment Creator
// =============================================================================

/**
 * @brief IHE AIRA Assessment Creator Actor
 *
 * Creates structured assessment documents for AI results encoded
 * as DICOM Enhanced SR, following the IHE AIRA profile.
 *
 * ## Workflow
 *
 * 1. Create an ai_assessment structure with the assessment decision
 * 2. Call create_assessment() to generate the SR document
 * 3. Store the resulting dataset using a storage backend
 *
 * @example
 * @code
 * assessment_creator creator;
 *
 * ai_assessment assessment;
 * assessment.type = assessment_type::accept;
 * assessment.ai_result.sop_instance_uid = "1.2.3.4.5";
 * assessment.assessor_name = "DR^SMITH";
 *
 * auto result = creator.create_assessment(assessment);
 * if (result.success) {
 *     // Store result.sr_dataset
 * }
 * @endcode
 */
class assessment_creator {
public:
    assessment_creator() = default;

    /**
     * @brief Create an assessment SR document for an AI result.
     *
     * @param assessment The assessment data
     * @return Creation result with the SR dataset
     */
    [[nodiscard]] assessment_creation_result create_assessment(
        const ai_assessment& assessment) const;

    /**
     * @brief Convert assessment_type to DICOM coded value.
     *
     * @param type Assessment type
     * @return Coded string representation
     */
    [[nodiscard]] static std::string assessment_type_to_code(
        assessment_type type);

    /**
     * @brief Convert assessment_type to human-readable meaning.
     *
     * @param type Assessment type
     * @return Display string
     */
    [[nodiscard]] static std::string assessment_type_to_meaning(
        assessment_type type);

    /**
     * @brief Convert assessment_status to DICOM completion flag value.
     *
     * @param status Assessment status
     * @return Completion flag string (COMPLETE or PARTIAL)
     */
    [[nodiscard]] static std::string status_to_completion_flag(
        assessment_status status);

private:
    void build_patient_module(core::dicom_dataset& sr,
                              const ai_assessment& assessment) const;
    void build_sr_content(core::dicom_dataset& sr,
                          const ai_assessment& assessment) const;
    void build_referenced_sop_sequence(core::dicom_dataset& sr,
                                       const ai_assessment& assessment) const;

    [[nodiscard]] std::string generate_uid() const;
};

/**
 * @brief Convert assessment_type enum to string
 */
[[nodiscard]] std::string to_string(assessment_type type);

/**
 * @brief Convert assessment_status enum to string
 */
[[nodiscard]] std::string to_string(assessment_status status);

}  // namespace kcenon::pacs::ai

#endif  // PACS_AI_AIRA_ASSESSMENT_HPP
