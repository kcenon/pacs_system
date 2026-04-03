// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file aira_assessment_manager.h
 * @brief IHE AIRA Assessment Manager Actor
 *
 * Provides storage, retrieval, and query capabilities for AI result
 * assessment documents. Acts as the Assessment Manager actor in the
 * IHE AIRA profile.
 *
 * @see IHE RAD AIRA Supplement (2025-06-12)
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_AI_AIRA_ASSESSMENT_MANAGER_HPP
#define PACS_AI_AIRA_ASSESSMENT_MANAGER_HPP

#include "kcenon/pacs/ai/aira_assessment.h"
#include "kcenon/pacs/core/dicom_dataset.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::pacs::ai {

// =============================================================================
// Assessment Manager
// =============================================================================

/**
 * @brief IHE AIRA Assessment Manager Actor
 *
 * Stores and manages assessment documents, providing query and
 * retrieval capabilities. Assessments are stored in-memory and
 * can be queried by AI result UID, assessor, or assessment type.
 *
 * @example
 * @code
 * assessment_manager manager;
 *
 * // Store an assessment
 * auto store_result = manager.store_assessment(sr_dataset);
 *
 * // Find assessments for an AI result
 * auto assessments = manager.find_by_ai_result("1.2.3.4.5");
 * @endcode
 */
class assessment_manager {
public:
    assessment_manager() = default;

    /**
     * @brief Store an assessment SR document.
     *
     * Parses the SR dataset to extract assessment metadata and
     * stores both the dataset and metadata for later retrieval.
     *
     * @param assessment_sr The assessment SR dataset
     * @return true if stored successfully
     */
    [[nodiscard]] bool store_assessment(
        const core::dicom_dataset& assessment_sr);

    /**
     * @brief Retrieve an assessment SR dataset by its UID.
     *
     * @param assessment_uid SOP Instance UID of the assessment
     * @return The dataset if found
     */
    [[nodiscard]] std::optional<core::dicom_dataset> retrieve_assessment(
        const std::string& assessment_uid) const;

    /**
     * @brief Find all assessments for a given AI result.
     *
     * @param ai_result_uid SOP Instance UID of the AI result
     * @return List of assessment info records
     */
    [[nodiscard]] std::vector<assessment_info> find_by_ai_result(
        const std::string& ai_result_uid) const;

    /**
     * @brief Find all assessments by a specific assessor.
     *
     * @param assessor_name Name of the assessor
     * @return List of assessment info records
     */
    [[nodiscard]] std::vector<assessment_info> find_by_assessor(
        const std::string& assessor_name) const;

    /**
     * @brief Find all assessments of a specific type.
     *
     * @param type Assessment type to filter by
     * @return List of assessment info records
     */
    [[nodiscard]] std::vector<assessment_info> find_by_type(
        assessment_type type) const;

    /**
     * @brief Get metadata for a specific assessment.
     *
     * @param assessment_uid SOP Instance UID
     * @return Assessment info if found
     */
    [[nodiscard]] std::optional<assessment_info> get_info(
        const std::string& assessment_uid) const;

    /**
     * @brief Check if an assessment exists.
     *
     * @param assessment_uid SOP Instance UID
     * @return true if the assessment exists
     */
    [[nodiscard]] bool exists(const std::string& assessment_uid) const;

    /**
     * @brief Remove an assessment.
     *
     * @param assessment_uid SOP Instance UID
     * @return true if removed
     */
    bool remove(const std::string& assessment_uid);

    /**
     * @brief Get total number of stored assessments.
     *
     * @return Count of assessments
     */
    [[nodiscard]] size_t count() const noexcept;

    /**
     * @brief Get assessments grouped by type with counts.
     *
     * @return Map of assessment_type to count
     */
    [[nodiscard]] std::map<assessment_type, size_t> get_statistics() const;

private:
    assessment_info parse_assessment_info(
        const core::dicom_dataset& sr) const;

    std::map<std::string, core::dicom_dataset> datasets_;
    std::map<std::string, assessment_info> metadata_;
};

}  // namespace kcenon::pacs::ai

#endif  // PACS_AI_AIRA_ASSESSMENT_MANAGER_HPP
