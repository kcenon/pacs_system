// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file commitment_repository.h
 * @brief Repository for Storage Commitment transaction tracking
 *
 * Provides database persistence for Storage Commitment Push Model
 * transactions, including per-instance success/failure tracking.
 *
 * @see DICOM PS3.4 Annex J - Storage Commitment Push Model
 * @see Issue #711 - Storage Commitment database tracking
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "kcenon/pacs/services/storage_commitment_types.h"

#include <kcenon/common/patterns/result.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "kcenon/pacs/storage/base_repository.h"

namespace kcenon::pacs::storage {

// =============================================================================
// Entity Types
// =============================================================================

/**
 * @brief Status of a Storage Commitment transaction
 */
enum class commitment_status {
    pending,    ///< Commitment requested, awaiting verification
    success,    ///< All instances committed successfully
    partial,    ///< Some instances failed verification
    failed      ///< All instances failed or transaction error
};

[[nodiscard]] inline std::string_view to_string(commitment_status s) noexcept {
    switch (s) {
        case commitment_status::pending: return "pending";
        case commitment_status::success: return "success";
        case commitment_status::partial: return "partial";
        case commitment_status::failed: return "failed";
    }
    return "unknown";
}

[[nodiscard]] inline commitment_status commitment_status_from_string(
    std::string_view str) noexcept {
    if (str == "success") return commitment_status::success;
    if (str == "partial") return commitment_status::partial;
    if (str == "failed") return commitment_status::failed;
    return commitment_status::pending;
}

/**
 * @brief Database record for a Storage Commitment transaction
 */
struct commitment_record {
    std::string transaction_uid;    ///< PRIMARY KEY
    std::string requesting_ae;      ///< AE title of the requester
    std::string request_time;       ///< ISO 8601 timestamp
    std::string completion_time;    ///< ISO 8601 timestamp (empty if pending)
    commitment_status status{commitment_status::pending};
    int total_instances{0};
    int success_count{0};
    int failure_count{0};
};

/**
 * @brief Database record for a per-instance commitment reference
 */
struct commitment_reference_record {
    std::string transaction_uid;    ///< FK → storage_commitment
    std::string sop_class_uid;
    std::string sop_instance_uid;
    std::string status{"pending"};  ///< "pending", "success", "failed"
    std::optional<int> failure_reason;
};

// =============================================================================
// Repository
// =============================================================================

/**
 * @brief Repository for Storage Commitment transaction persistence
 *
 * Extends base_repository for the storage_commitment table and provides
 * additional methods for managing commitment_references.
 */
class commitment_repository
    : public base_repository<commitment_record, std::string> {
public:
    explicit commitment_repository(std::shared_ptr<pacs_database_adapter> db);
    ~commitment_repository() override = default;

    commitment_repository(const commitment_repository&) = delete;
    auto operator=(const commitment_repository&)
        -> commitment_repository& = delete;
    commitment_repository(commitment_repository&&) noexcept = default;
    auto operator=(commitment_repository&&) noexcept
        -> commitment_repository& = default;

    // =========================================================================
    // Domain-Specific Operations
    // =========================================================================

    /**
     * @brief Record a new commitment request with references
     *
     * Inserts a commitment_record and all associated reference records
     * within a single transaction.
     *
     * @param transaction_uid Unique transaction identifier
     * @param requesting_ae AE title of the requesting SCU
     * @param references SOP Instance references to commit
     * @return Success or error
     */
    [[nodiscard]] auto record_request(
        const std::string& transaction_uid,
        const std::string& requesting_ae,
        const std::vector<services::sop_reference>& references) -> VoidResult;

    /**
     * @brief Update commitment result after SCP verification
     *
     * Updates the commitment record status and per-instance reference
     * statuses based on the verification result.
     *
     * @param transaction_uid Transaction to update
     * @param result The commitment verification result
     * @return Success or error
     */
    [[nodiscard]] auto update_result(
        const std::string& transaction_uid,
        const services::commitment_result& result) -> VoidResult;

    /**
     * @brief Get the status of a commitment transaction
     *
     * @param transaction_uid Transaction to query
     * @return The commitment status or error if not found
     */
    [[nodiscard]] auto get_status(const std::string& transaction_uid)
        -> Result<commitment_status>;

    /**
     * @brief Check if a transaction UID already exists
     *
     * @param transaction_uid Transaction UID to check
     * @return true if the transaction exists
     */
    [[nodiscard]] auto is_duplicate_transaction(
        const std::string& transaction_uid) -> bool;

    /**
     * @brief Get all reference records for a transaction
     *
     * @param transaction_uid Transaction to query
     * @return Vector of reference records
     */
    [[nodiscard]] auto get_references(const std::string& transaction_uid)
        -> Result<std::vector<commitment_reference_record>>;

    /**
     * @brief Find transactions by status
     *
     * @param status Status to filter by
     * @param limit Maximum results
     * @return Vector of matching commitment records
     */
    [[nodiscard]] auto find_by_status(commitment_status status,
                                       size_t limit = 100)
        -> list_result_type;

    /**
     * @brief Delete old completed/failed transactions
     *
     * @param max_age Maximum age to keep
     * @return Number of deleted records
     */
    [[nodiscard]] auto cleanup_old_transactions(std::chrono::hours max_age)
        -> Result<size_t>;

protected:
    // =========================================================================
    // base_repository Overrides
    // =========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> commitment_record override;

    [[nodiscard]] auto entity_to_row(const commitment_record& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const commitment_record& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const commitment_record& entity) const
        -> bool override;

private:
    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;

    [[nodiscard]] auto insert_references(
        const std::string& transaction_uid,
        const std::vector<services::sop_reference>& references) -> VoidResult;
};

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
