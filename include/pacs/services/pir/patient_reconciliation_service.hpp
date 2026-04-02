// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file patient_reconciliation_service.hpp
 * @brief IHE PIR (Patient Information Reconciliation) Service
 *
 * Handles patient demographic corrections and patient merge operations
 * across stored DICOM instances. When patient identity information is
 * updated, PIR ensures all stored instances are updated consistently.
 *
 * @see IHE RAD PIR Profile
 * @see HL7 v2.x ADT messages (A08, A40, A24)
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_PIR_PATIENT_RECONCILIATION_SERVICE_HPP
#define PACS_SERVICES_PIR_PATIENT_RECONCILIATION_SERVICE_HPP

#include "pacs/core/dicom_dataset.hpp"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::pacs::services::pir {

// =============================================================================
// PIR Operation Types
// =============================================================================

/**
 * @brief Type of patient reconciliation operation
 */
enum class reconciliation_type {
    demographics_update,  ///< ADT^A08: update patient demographics
    patient_merge,        ///< ADT^A40: merge two patients
    patient_link          ///< ADT^A24: link related patients
};

// =============================================================================
// Data Structures
// =============================================================================

/**
 * @brief Updated patient demographics for a reconciliation operation
 */
struct patient_demographics {
    /// Patient Name (0010,0010)
    std::optional<std::string> patient_name;

    /// Patient ID (0010,0020)
    std::optional<std::string> patient_id;

    /// Patient Birth Date (0010,0030)
    std::optional<std::string> patient_birth_date;

    /// Patient Sex (0010,0040)
    std::optional<std::string> patient_sex;

    /// Issuer of Patient ID (0010,0021)
    std::optional<std::string> issuer_of_patient_id;

    /// Other Patient IDs (0010,1000)
    std::optional<std::string> other_patient_ids;
};

/**
 * @brief Request to update patient demographics
 */
struct demographics_update_request {
    /// Patient ID to update
    std::string target_patient_id;

    /// Updated demographics
    patient_demographics updated_demographics;

    /// Reason for update
    std::optional<std::string> reason;

    /// Operator performing the update
    std::optional<std::string> operator_name;
};

/**
 * @brief Request to merge two patients
 */
struct patient_merge_request {
    /// Patient ID to merge from (source - will be removed)
    std::string source_patient_id;

    /// Patient ID to merge into (target - will retain)
    std::string target_patient_id;

    /// Optional updated demographics for the target
    std::optional<patient_demographics> target_demographics;

    /// Reason for merge
    std::optional<std::string> reason;

    /// Operator performing the merge
    std::optional<std::string> operator_name;
};

/**
 * @brief Result of a reconciliation operation
 */
struct reconciliation_result {
    /// Whether the operation succeeded
    bool success{false};

    /// Number of instances affected
    size_t instances_updated{0};

    /// Number of studies affected
    size_t studies_affected{0};

    /// Error message (if failed)
    std::string error_message;

    /// Type of operation performed
    reconciliation_type type;

    /// Timestamp of the operation
    std::chrono::system_clock::time_point timestamp{
        std::chrono::system_clock::now()};
};

/**
 * @brief Audit record of a reconciliation operation
 */
struct reconciliation_audit_record {
    /// Unique identifier for this audit record
    std::string record_id;

    /// Type of operation
    reconciliation_type type;

    /// Patient ID(s) involved
    std::string primary_patient_id;
    std::optional<std::string> secondary_patient_id;

    /// Operator who performed the action
    std::string operator_name;

    /// Number of instances affected
    size_t instances_updated{0};

    /// Timestamp
    std::chrono::system_clock::time_point timestamp;

    /// Whether operation succeeded
    bool success{false};
};

// =============================================================================
// Patient Reconciliation Service
// =============================================================================

/**
 * @brief IHE PIR Patient Information Reconciliation Service
 *
 * Provides patient demographic update and patient merge operations
 * across DICOM instances stored in the system. Uses an in-memory
 * store of DICOM datasets for unit testing and demonstration.
 *
 * ## Supported Operations
 *
 * - **Demographics Update** (ADT^A08): Update patient name, ID, DOB, sex
 *   across all instances for a given patient
 * - **Patient Merge** (ADT^A40): Reassign all instances from source patient
 *   to target patient
 *
 * @example
 * @code
 * patient_reconciliation_service service;
 *
 * // Add some instances
 * service.add_instance(dataset1);
 * service.add_instance(dataset2);
 *
 * // Update demographics
 * demographics_update_request req;
 * req.target_patient_id = "12345";
 * req.updated_demographics.patient_name = "CORRECTED^NAME";
 *
 * auto result = service.update_demographics(req);
 * @endcode
 */
class patient_reconciliation_service {
public:
    patient_reconciliation_service() = default;

    /**
     * @brief Add a DICOM instance to the managed store.
     *
     * @param dataset The DICOM dataset to add
     * @return true if added successfully
     */
    bool add_instance(const core::dicom_dataset& dataset);

    /**
     * @brief Update patient demographics across all matching instances.
     *
     * Finds all instances with the specified patient ID and updates
     * the patient-level attributes with the new values.
     *
     * @param request Demographics update request
     * @return Result of the operation
     */
    [[nodiscard]] reconciliation_result update_demographics(
        const demographics_update_request& request);

    /**
     * @brief Merge instances from source patient to target patient.
     *
     * All instances belonging to the source patient are reassigned
     * to the target patient. The source patient effectively ceases
     * to exist after the merge.
     *
     * @param request Patient merge request
     * @return Result of the operation
     */
    [[nodiscard]] reconciliation_result merge_patients(
        const patient_merge_request& request);

    /**
     * @brief Find all instances for a given patient ID.
     *
     * @param patient_id Patient ID to search for
     * @return List of matching DICOM datasets
     */
    [[nodiscard]] std::vector<core::dicom_dataset> find_instances(
        const std::string& patient_id) const;

    /**
     * @brief Get the total number of managed instances.
     *
     * @return Instance count
     */
    [[nodiscard]] size_t instance_count() const noexcept;

    /**
     * @brief Get distinct patient IDs in the store.
     *
     * @return List of unique patient IDs
     */
    [[nodiscard]] std::vector<std::string> get_patient_ids() const;

    /**
     * @brief Get the audit trail of reconciliation operations.
     *
     * @return List of audit records
     */
    [[nodiscard]] const std::vector<reconciliation_audit_record>&
    audit_trail() const noexcept;

    /**
     * @brief Get audit records for a specific patient.
     *
     * @param patient_id Patient ID to filter by
     * @return Filtered audit records
     */
    [[nodiscard]] std::vector<reconciliation_audit_record>
    audit_trail_for_patient(const std::string& patient_id) const;

private:
    void apply_demographics(core::dicom_dataset& dataset,
                            const patient_demographics& demographics) const;
    std::string generate_record_id() const;

    std::vector<core::dicom_dataset> instances_;
    std::vector<reconciliation_audit_record> audit_records_;
};

/**
 * @brief Convert reconciliation_type to string
 */
[[nodiscard]] std::string to_string(reconciliation_type type);

}  // namespace kcenon::pacs::services::pir

#endif  // PACS_SERVICES_PIR_PATIENT_RECONCILIATION_SERVICE_HPP
