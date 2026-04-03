// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_commitment_types.hpp
 * @brief Data types for DICOM Storage Commitment Push Model Service
 *
 * Defines the shared data structures used by both the Storage Commitment
 * SCP and SCU implementations, as specified in DICOM PS3.4 Annex J.
 *
 * @see DICOM PS3.4 Annex J - Storage Commitment Push Model Service Class
 * @see DICOM PS3.4 Table J.3-1 - Storage Commitment Request
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_STORAGE_COMMITMENT_TYPES_HPP
#define PACS_SERVICES_STORAGE_COMMITMENT_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kcenon::pacs::services {

// =============================================================================
// Storage Commitment SOP Class UIDs
// =============================================================================

/// Storage Commitment Push Model SOP Class UID (PS3.4 Table J.3-1)
inline constexpr std::string_view storage_commitment_push_model_sop_class_uid =
    "1.2.840.10008.1.20.1";

/// Storage Commitment Push Model SOP Instance UID (Well-Known)
inline constexpr std::string_view storage_commitment_push_model_sop_instance_uid =
    "1.2.840.10008.1.20.1.1";

// =============================================================================
// Action and Event Type IDs
// =============================================================================

/// N-ACTION: Request Storage Commitment (Action Type ID = 1)
inline constexpr uint16_t storage_commitment_action_type_request = 1;

/// N-EVENT-REPORT: Storage Commitment Request Successful (Event Type ID = 1)
inline constexpr uint16_t storage_commitment_event_type_success = 1;

/// N-EVENT-REPORT: Storage Commitment Request Complete - Failures Exist (Event Type ID = 2)
inline constexpr uint16_t storage_commitment_event_type_failure = 2;

// =============================================================================
// SOP Reference
// =============================================================================

/**
 * @brief Reference to a SOP Instance in a commitment request
 *
 * Corresponds to items in the Referenced SOP Sequence (0008,1199)
 * or Failed SOP Sequence (0008,1198).
 */
struct sop_reference {
    std::string sop_class_uid;      ///< Referenced SOP Class UID (0008,1150)
    std::string sop_instance_uid;   ///< Referenced SOP Instance UID (0008,1155)
};

// =============================================================================
// Commitment Failure Reasons
// =============================================================================

/**
 * @brief Failure reason codes for Storage Commitment
 *
 * As defined in DICOM PS3.4 Table J.3-2 (Failure Reason values).
 * These codes appear in the Failure Reason (0008,1197) attribute
 * within the Failed SOP Sequence.
 */
enum class commitment_failure_reason : uint16_t {
    /// General processing failure
    processing_failure = 0x0110,

    /// Referenced SOP Instance not found in storage
    no_such_object_instance = 0x0112,

    /// Storage resource limitation (e.g., disk full)
    resource_limitation = 0x0213,

    /// Referenced SOP Class not supported by SCP
    referenced_sop_class_not_supported = 0x0122,

    /// SOP Class / Instance UID mismatch
    class_instance_conflict = 0x0119,

    /// Duplicate Transaction UID
    duplicate_transaction_uid = 0xA770
};

/**
 * @brief Convert failure reason to human-readable string
 * @param reason The failure reason code
 * @return Description string
 */
[[nodiscard]] inline std::string_view
to_string(commitment_failure_reason reason) noexcept {
    switch (reason) {
        case commitment_failure_reason::processing_failure:
            return "Processing failure";
        case commitment_failure_reason::no_such_object_instance:
            return "No such object instance";
        case commitment_failure_reason::resource_limitation:
            return "Resource limitation";
        case commitment_failure_reason::referenced_sop_class_not_supported:
            return "Referenced SOP Class not supported";
        case commitment_failure_reason::class_instance_conflict:
            return "Class/Instance conflict";
        case commitment_failure_reason::duplicate_transaction_uid:
            return "Duplicate Transaction UID";
    }
    return "Unknown failure reason";
}

// =============================================================================
// Commitment Result
// =============================================================================

/**
 * @brief Result of a Storage Commitment verification
 *
 * Contains the per-instance success/failure status after the SCP
 * has verified storage of the requested SOP Instances.
 */
struct commitment_result {
    /// Transaction UID identifying this commitment request
    std::string transaction_uid;

    /// Successfully committed SOP Instance references
    std::vector<sop_reference> success_references;

    /// Failed SOP Instance references with failure reasons
    std::vector<std::pair<sop_reference, commitment_failure_reason>> failed_references;

    /// Timestamp when verification was completed
    std::chrono::system_clock::time_point timestamp;

    /// Whether all instances were successfully committed
    [[nodiscard]] bool all_successful() const noexcept {
        return failed_references.empty() && !success_references.empty();
    }

    /// Total number of instances in this result
    [[nodiscard]] std::size_t total_instances() const noexcept {
        return success_references.size() + failed_references.size();
    }
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_STORAGE_COMMITMENT_TYPES_HPP
