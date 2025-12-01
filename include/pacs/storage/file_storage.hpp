/**
 * @file file_storage.hpp
 * @brief Filesystem-based DICOM storage with hierarchical organization
 *
 * This file provides the file_storage class which implements storage_interface
 * using the local filesystem. DICOM files are organized in a hierarchical
 * directory structure based on Study/Series/Instance UIDs.
 *
 * @see SRS-STOR-002, FR-4.1 (Hierarchical File Storage)
 */

#pragma once

#include "storage_interface.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_file.hpp>

#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace pacs::storage {

/**
 * @brief Naming scheme for DICOM file organization
 *
 * Determines how files are organized in the storage directory.
 */
enum class naming_scheme {
    /// {StudyUID}/{SeriesUID}/{SOPUID}.dcm
    uid_hierarchical,

    /// YYYY/MM/DD/{StudyUID}/{SOPUID}.dcm
    date_hierarchical,

    /// {SOPUID}.dcm (flat structure)
    flat
};

/**
 * @brief Policy for handling duplicate SOP Instance UIDs
 */
enum class duplicate_policy {
    /// Return error if instance already exists
    reject,

    /// Overwrite existing instance
    replace,

    /// Skip silently if instance exists
    ignore
};

/**
 * @brief Configuration for file_storage
 */
struct file_storage_config {
    /// Root directory for storage
    std::filesystem::path root_path;

    /// File organization scheme
    naming_scheme naming = naming_scheme::uid_hierarchical;

    /// How to handle duplicate instances
    duplicate_policy duplicate = duplicate_policy::reject;

    /// Create directories automatically if they don't exist
    bool create_directories = true;

    /// File extension for DICOM files
    std::string file_extension = ".dcm";
};

/**
 * @brief Filesystem-based DICOM storage implementation
 *
 * Stores DICOM datasets as Part 10 files in a hierarchical directory structure.
 * The organization follows the pattern:
 *
 * @code
 * {root}/
 * +-- {StudyUID}/
 *     +-- {SeriesUID}/
 *         +-- {SOPUID}.dcm
 * @endcode
 *
 * Thread Safety:
 * - All methods are thread-safe
 * - Concurrent reads are allowed (shared lock)
 * - Writes require exclusive lock
 * - File operations use atomic write pattern (write to temp, then rename)
 *
 * @example
 * @code
 * file_storage_config config;
 * config.root_path = "/data/dicom";
 * config.naming = naming_scheme::uid_hierarchical;
 * config.duplicate = duplicate_policy::replace;
 *
 * file_storage storage{config};
 *
 * // Store a dataset
 * core::dicom_dataset ds;
 * ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3");
 * ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4");
 * ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
 * auto result = storage.store(ds);
 *
 * // Retrieve by SOP Instance UID
 * auto retrieved = storage.retrieve("1.2.3.4.5");
 * @endcode
 */
class file_storage : public storage_interface {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct file storage with configuration
     * @param config Storage configuration
     * @throws std::runtime_error if root_path is invalid and create_directories is false
     */
    explicit file_storage(const file_storage_config& config);

    /**
     * @brief Destructor
     */
    ~file_storage() override = default;

    /// Non-copyable (contains mutex)
    file_storage(const file_storage&) = delete;
    file_storage& operator=(const file_storage&) = delete;

    /// Non-movable (contains mutex)
    file_storage(file_storage&&) = delete;
    file_storage& operator=(file_storage&&) = delete;

    // =========================================================================
    // storage_interface Implementation
    // =========================================================================

    /**
     * @brief Store a DICOM dataset to filesystem
     *
     * Extracts UIDs from the dataset and stores as a Part 10 file.
     * Uses atomic write pattern: writes to temp file, then renames.
     *
     * @param dataset The DICOM dataset to store
     * @return VoidResult Success or error information
     *
     * @note Requires Study, Series, and SOP Instance UIDs in the dataset
     */
    [[nodiscard]] auto store(const core::dicom_dataset& dataset)
        -> VoidResult override;

    /**
     * @brief Retrieve a DICOM dataset by SOP Instance UID
     *
     * @param sop_instance_uid The unique identifier for the instance
     * @return Result containing the dataset or error information
     */
    [[nodiscard]] auto retrieve(std::string_view sop_instance_uid)
        -> Result<core::dicom_dataset> override;

    /**
     * @brief Remove a DICOM file by SOP Instance UID
     *
     * @param sop_instance_uid The unique identifier for the instance to remove
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto remove(std::string_view sop_instance_uid)
        -> VoidResult override;

    /**
     * @brief Check if a DICOM instance exists
     *
     * @param sop_instance_uid The unique identifier to check
     * @return true if the file exists, false otherwise
     */
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const
        -> bool override;

    /**
     * @brief Find DICOM datasets matching query criteria
     *
     * Scans the storage directory and matches datasets against query.
     *
     * @param query The query dataset containing search criteria
     * @return Result containing matching datasets or error information
     *
     * @note This operation can be slow for large storage directories
     */
    [[nodiscard]] auto find(const core::dicom_dataset& query)
        -> Result<std::vector<core::dicom_dataset>> override;

    /**
     * @brief Get storage statistics
     *
     * @return Storage statistics structure
     */
    [[nodiscard]] auto get_statistics() const -> storage_statistics override;

    /**
     * @brief Verify storage integrity
     *
     * Checks that all indexed files exist and are valid DICOM files.
     *
     * @return VoidResult Success if integrity is verified, error otherwise
     */
    [[nodiscard]] auto verify_integrity() -> VoidResult override;

    // =========================================================================
    // File-specific Operations
    // =========================================================================

    /**
     * @brief Get the filesystem path for a SOP Instance UID
     *
     * @param sop_instance_uid The SOP Instance UID
     * @return The filesystem path (may not exist)
     */
    [[nodiscard]] auto get_file_path(std::string_view sop_instance_uid) const
        -> std::filesystem::path;

    /**
     * @brief Import DICOM files from a directory
     *
     * Recursively scans the source directory for DICOM files and imports them.
     *
     * @param source Source directory to import from
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto import_directory(const std::filesystem::path& source)
        -> VoidResult;

    /**
     * @brief Get the root storage path
     *
     * @return The configured root path
     */
    [[nodiscard]] auto root_path() const -> const std::filesystem::path&;

    /**
     * @brief Rebuild the internal index from filesystem
     *
     * Scans the storage directory and rebuilds the SOP UID to path mapping.
     *
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto rebuild_index() -> VoidResult;

private:
    // =========================================================================
    // Internal Helper Methods
    // =========================================================================

    /**
     * @brief Build filesystem path for a dataset
     * @param study_uid Study Instance UID
     * @param series_uid Series Instance UID
     * @param sop_uid SOP Instance UID
     * @return The constructed path
     */
    [[nodiscard]] auto build_path(std::string_view study_uid,
                                  std::string_view series_uid,
                                  std::string_view sop_uid) const
        -> std::filesystem::path;

    /**
     * @brief Build filesystem path using date-based hierarchy
     * @param study_date Study date (YYYYMMDD format)
     * @param study_uid Study Instance UID
     * @param sop_uid SOP Instance UID
     * @return The constructed path
     */
    [[nodiscard]] auto build_date_path(std::string_view study_date,
                                       std::string_view study_uid,
                                       std::string_view sop_uid) const
        -> std::filesystem::path;

    /**
     * @brief Update internal index with new mapping
     * @param sop_uid SOP Instance UID
     * @param path File path
     */
    void update_index(const std::string& sop_uid,
                      const std::filesystem::path& path);

    /**
     * @brief Remove entry from internal index
     * @param sop_uid SOP Instance UID
     */
    void remove_from_index(const std::string& sop_uid);

    /**
     * @brief Check if dataset matches query criteria
     * @param dataset The dataset to check
     * @param query The query criteria
     * @return true if dataset matches query
     */
    [[nodiscard]] static auto matches_query(const core::dicom_dataset& dataset,
                                            const core::dicom_dataset& query)
        -> bool;

    /**
     * @brief Sanitize UID for use in filesystem path
     * @param uid The UID to sanitize
     * @return Sanitized string safe for filesystem use
     */
    [[nodiscard]] static auto sanitize_uid(std::string_view uid) -> std::string;

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Storage configuration
    file_storage_config config_;

    /// Mapping from SOP Instance UID to file path
    std::unordered_map<std::string, std::filesystem::path> index_;

    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;
};

}  // namespace pacs::storage
