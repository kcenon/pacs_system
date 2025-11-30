/**
 * @file storage_interface.hpp
 * @brief Abstract storage interface for DICOM persistent storage backends
 *
 * This file defines the abstract storage_interface class which provides
 * a unified API for DICOM data persistence. Concrete implementations
 * (file_storage, cloud_storage, etc.) must inherit from this interface.
 *
 * @see SRS-STOR-001, FR-4.1
 */

#pragma once

#include <pacs/core/dicom_dataset.hpp>

#include <kcenon/common/patterns/result.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::storage {

/// Result type alias for operations returning a value
template <typename T>
using Result = kcenon::common::Result<T>;

/// Result type alias for void operations
using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Storage statistics structure
 *
 * Contains aggregated metrics about the storage backend's state.
 */
struct storage_statistics {
    /// Total number of DICOM instances stored
    std::size_t total_instances{0};

    /// Total storage size in bytes
    std::size_t total_bytes{0};

    /// Number of unique studies
    std::size_t studies_count{0};

    /// Number of unique series
    std::size_t series_count{0};

    /// Number of unique patients
    std::size_t patients_count{0};
};

/**
 * @brief Abstract storage interface for DICOM persistence
 *
 * Provides a unified API for storing, retrieving, and querying DICOM data.
 * All concrete storage implementations must inherit from this interface.
 *
 * Thread Safety:
 * - All methods must be thread-safe in concrete implementations
 * - Concurrent reads are allowed
 * - Writes must be atomic
 *
 * @example
 * @code
 * // Using a concrete implementation (e.g., file_storage)
 * std::unique_ptr<storage_interface> storage =
 *     std::make_unique<file_storage>("/path/to/storage");
 *
 * // Store a DICOM dataset
 * core::dicom_dataset dataset;
 * // ... populate dataset ...
 * auto result = storage->store(dataset);
 *
 * // Retrieve by SOP Instance UID
 * auto retrieved = storage->retrieve("1.2.3.4.5.6.7.8.9");
 *
 * // Query for studies
 * core::dicom_dataset query;
 * query.set_string(tags::patient_id, vr_type::LO, "12345");
 * auto matches = storage->find(query);
 * @endcode
 */
class storage_interface {
public:
    /// Virtual destructor for proper polymorphic destruction
    virtual ~storage_interface() = default;

    // =========================================================================
    // CRUD Operations
    // =========================================================================

    /**
     * @brief Store a DICOM dataset
     *
     * Stores the dataset using its SOP Instance UID as the key.
     * If a dataset with the same UID already exists, it will be replaced.
     *
     * @param dataset The DICOM dataset to store
     * @return VoidResult Success or error information
     *
     * @note The dataset must contain a valid SOP Instance UID (0008,0018)
     */
    [[nodiscard]] virtual auto store(const core::dicom_dataset& dataset)
        -> VoidResult = 0;

    /**
     * @brief Retrieve a DICOM dataset by SOP Instance UID
     *
     * @param sop_instance_uid The unique identifier for the instance
     * @return Result containing the dataset or error information
     */
    [[nodiscard]] virtual auto retrieve(std::string_view sop_instance_uid)
        -> Result<core::dicom_dataset> = 0;

    /**
     * @brief Remove a DICOM dataset by SOP Instance UID
     *
     * @param sop_instance_uid The unique identifier for the instance to remove
     * @return VoidResult Success or error information
     *
     * @note Removing a non-existent instance is not considered an error
     */
    [[nodiscard]] virtual auto remove(std::string_view sop_instance_uid)
        -> VoidResult = 0;

    /**
     * @brief Check if a DICOM instance exists
     *
     * @param sop_instance_uid The unique identifier to check
     * @return true if the instance exists, false otherwise
     */
    [[nodiscard]] virtual auto exists(std::string_view sop_instance_uid) const
        -> bool = 0;

    // =========================================================================
    // Query Operations
    // =========================================================================

    /**
     * @brief Find DICOM datasets matching query criteria
     *
     * Performs a search using DICOM C-FIND semantics. The query dataset
     * contains the search criteria where empty values act as wildcards.
     *
     * @param query The query dataset containing search criteria
     * @return Result containing matching datasets or error information
     *
     * @note Supports standard DICOM wildcard matching (* and ?)
     */
    [[nodiscard]] virtual auto find(const core::dicom_dataset& query)
        -> Result<std::vector<core::dicom_dataset>> = 0;

    // =========================================================================
    // Batch Operations
    // =========================================================================

    /**
     * @brief Store multiple DICOM datasets in a single operation
     *
     * Default implementation calls store() for each dataset.
     * Concrete implementations may override for better performance.
     *
     * @param datasets The collection of datasets to store
     * @return VoidResult Success or error information
     *
     * @note On error, some datasets may have been stored
     */
    [[nodiscard]] virtual auto store_batch(
        const std::vector<core::dicom_dataset>& datasets) -> VoidResult;

    /**
     * @brief Retrieve multiple DICOM datasets by their SOP Instance UIDs
     *
     * Default implementation calls retrieve() for each UID.
     * Concrete implementations may override for better performance.
     *
     * @param sop_instance_uids The collection of UIDs to retrieve
     * @return Result containing found datasets or error information
     *
     * @note Missing instances are silently skipped in the result
     */
    [[nodiscard]] virtual auto retrieve_batch(
        const std::vector<std::string>& sop_instance_uids)
        -> Result<std::vector<core::dicom_dataset>>;

    // =========================================================================
    // Maintenance Operations
    // =========================================================================

    /**
     * @brief Get storage statistics
     *
     * Returns aggregated metrics about the current state of storage.
     *
     * @return Storage statistics structure
     */
    [[nodiscard]] virtual auto get_statistics() const
        -> storage_statistics = 0;

    /**
     * @brief Verify storage integrity
     *
     * Performs a consistency check on the storage backend.
     * The specific checks depend on the implementation.
     *
     * @return VoidResult Success if integrity is verified, error otherwise
     */
    [[nodiscard]] virtual auto verify_integrity() -> VoidResult = 0;

protected:
    /// Protected default constructor for derived classes
    storage_interface() = default;

    /// Non-copyable
    storage_interface(const storage_interface&) = delete;
    storage_interface& operator=(const storage_interface&) = delete;

    /// Movable
    storage_interface(storage_interface&&) = default;
    storage_interface& operator=(storage_interface&&) = default;
};

}  // namespace pacs::storage
