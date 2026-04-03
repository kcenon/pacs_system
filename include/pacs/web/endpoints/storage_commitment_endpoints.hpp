// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_commitment_endpoints.hpp
 * @brief DICOMweb Storage Commitment REST endpoints (Supplement 234)
 *
 * Provides RESTful API for requesting and polling storage commitment
 * status, complementing the existing DIMSE-based Storage Commitment
 * Push Model (N-ACTION/N-EVENT-REPORT).
 *
 * @see DICOM Supplement 234 -- DICOMweb Storage Commitment
 * @see DICOM PS3.18 -- Web Services
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pacs/services/storage_commitment_types.hpp"

namespace kcenon::pacs::web {

struct rest_server_context;

namespace storage_commitment {

/**
 * @brief Transaction state for DICOMweb Storage Commitment
 */
enum class transaction_state {
    pending,    ///< Commitment request received, verification in progress
    success,    ///< All referenced instances verified present and intact
    partial,    ///< Some instances verified, others failed
    failure     ///< Commitment verification failed
};

/**
 * @brief Convert transaction state to string
 */
[[nodiscard]] std::string_view to_string(transaction_state state) noexcept;

/**
 * @brief Parse transaction state from string
 */
[[nodiscard]] std::optional<transaction_state> parse_state(
    std::string_view str) noexcept;

/**
 * @brief A storage commitment transaction record
 */
struct commitment_transaction {
    /// Unique transaction identifier (DICOM UID format)
    std::string transaction_uid;

    /// Current state of the transaction
    transaction_state state{transaction_state::pending};

    /// SOP Instance references requested for commitment
    std::vector<services::sop_reference> requested_references;

    /// Successfully committed references (populated when verified)
    std::vector<services::sop_reference> success_references;

    /// Failed references with reasons (populated when verified)
    std::vector<std::pair<services::sop_reference,
                          services::commitment_failure_reason>> failed_references;

    /// Study Instance UID (optional, for study-level commitment)
    std::string study_instance_uid;

    /// Timestamp when the commitment was requested
    std::chrono::system_clock::time_point created_at;

    /// Timestamp when verification completed
    std::optional<std::chrono::system_clock::time_point> completed_at;
};

/**
 * @brief In-memory store for commitment transactions
 *
 * Thread-safe storage for tracking DICOMweb commitment requests.
 * In production, this would be backed by a persistent store.
 */
class transaction_store {
public:
    /**
     * @brief Add a new transaction
     * @param txn The transaction to store
     */
    void add(commitment_transaction txn);

    /**
     * @brief Find a transaction by its UID
     * @param transaction_uid The transaction UID
     * @return The transaction if found
     */
    [[nodiscard]] std::optional<commitment_transaction> find(
        std::string_view transaction_uid) const;

    /**
     * @brief Update an existing transaction
     * @param txn The updated transaction
     * @return true if the transaction was found and updated
     */
    bool update(const commitment_transaction& txn);

    /**
     * @brief List all transactions (newest first)
     * @param limit Maximum number to return (0 = unlimited)
     * @return Vector of transactions
     */
    [[nodiscard]] std::vector<commitment_transaction> list(
        size_t limit = 0) const;

    /**
     * @brief Get the number of stored transactions
     */
    [[nodiscard]] size_t size() const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, commitment_transaction, std::less<>> transactions_;
};

/**
 * @brief Serialize a commitment transaction to DICOM JSON
 * @param txn The transaction to serialize
 * @return JSON string
 */
[[nodiscard]] std::string transaction_to_json(const commitment_transaction& txn);

/**
 * @brief Serialize a list of transactions to DICOM JSON array
 * @param transactions The transactions to serialize
 * @return JSON array string
 */
[[nodiscard]] std::string transactions_to_json(
    const std::vector<commitment_transaction>& transactions);

/**
 * @brief Parse a commitment request from JSON body
 * @param json_body The request body
 * @param study_uid Optional study UID from URL path
 * @return Parsed references, or empty on error
 */
struct parse_result {
    std::vector<services::sop_reference> references;
    std::string error_message;
    bool valid{false};
};

[[nodiscard]] parse_result parse_commitment_request(
    std::string_view json_body,
    std::string_view study_uid = "");

}  // namespace storage_commitment

namespace endpoints {

// Internal function - implementation in cpp file
// Registers storage commitment endpoints with the Crow app
// Called from rest_server.cpp

}  // namespace endpoints

}  // namespace kcenon::pacs::web
