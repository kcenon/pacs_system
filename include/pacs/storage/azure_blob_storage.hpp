/**
 * @file azure_blob_storage.hpp
 * @brief Azure Blob storage backend for DICOM cloud storage support
 *
 * This file provides the azure_blob_storage class which implements
 * storage_interface using Azure Blob Storage service.
 *
 * @see SRS-STOR-004, FR-4.2 (Cloud Storage Backend)
 */

#pragma once

#include "storage_interface.hpp"

#include <pacs/core/dicom_dataset.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace pacs::storage {

/**
 * @brief Configuration for Azure Blob storage
 *
 * Contains all settings needed to connect to Azure Blob Storage service.
 * Supports both connection string and SAS token authentication.
 */
struct azure_storage_config {
  /// Azure Blob container name for storing DICOM files
  std::string container_name;

  /// Connection string for Azure Storage account
  /// Format: DefaultEndpointsProtocol=https;AccountName=...;AccountKey=...
  std::string connection_string;

  /// Optional endpoint suffix for sovereign clouds (e.g., "core.chinacloudapi.cn")
  /// If empty, uses the default "core.windows.net"
  std::optional<std::string> endpoint_suffix;

  /// Optional custom endpoint URL for Azurite emulator
  /// If set, this takes precedence over connection_string endpoint
  std::optional<std::string> endpoint_url;

  /// Threshold for block blob upload in bytes (default: 100MB)
  /// Files larger than this will use block blob upload with multiple blocks
  std::size_t block_upload_threshold = 100 * 1024 * 1024;

  /// Block size for block blob upload in bytes (default: 4MB)
  /// Azure allows up to 4000MB per block (API version 2019-12-12+)
  std::size_t block_size = 4 * 1024 * 1024;

  /// Maximum number of concurrent upload threads
  std::size_t max_concurrency = 8;

  /// Connection timeout in milliseconds
  std::uint32_t connect_timeout_ms = 3000;

  /// Request timeout in milliseconds
  std::uint32_t request_timeout_ms = 60000;

  /// Enable HTTPS (default: true)
  bool use_https = true;

  /// Blob tier (Hot, Cool, Archive)
  std::string access_tier{"Hot"};

  /// Retry count for transient failures
  std::uint32_t max_retries = 3;

  /// Initial retry delay in milliseconds
  std::uint32_t retry_delay_ms = 1000;
};

/**
 * @brief Information about an Azure Blob object
 */
struct azure_blob_info {
  /// Blob name (path within container)
  std::string blob_name;

  /// SOP Instance UID from DICOM metadata
  std::string sop_instance_uid;

  /// Study Instance UID
  std::string study_instance_uid;

  /// Series Instance UID
  std::string series_instance_uid;

  /// Blob size in bytes
  std::size_t size_bytes{0};

  /// ETag for integrity verification
  std::string etag;

  /// Content type
  std::string content_type{"application/dicom"};

  /// Content MD5 hash
  std::string content_md5;
};

/**
 * @brief Callback type for upload/download progress tracking
 *
 * Parameters:
 * - bytes_transferred: Number of bytes transferred so far
 * - total_bytes: Total number of bytes to transfer
 * - Returns: true to continue, false to abort
 */
using azure_progress_callback =
    std::function<bool(std::size_t bytes_transferred, std::size_t total_bytes)>;

/**
 * @brief Azure Blob storage backend for DICOM files
 *
 * Implements storage_interface using Azure Blob Storage.
 * Supports Azure Storage and Azurite emulator for local testing.
 *
 * Blob Naming Structure:
 * @code
 * {container}/
 *   +-- {StudyUID}/
 *       +-- {SeriesUID}/
 *           +-- {SOPUID}.dcm
 * @endcode
 *
 * Thread Safety:
 * - All methods are thread-safe
 * - Concurrent reads are allowed (shared lock)
 * - Writes require exclusive lock for index updates
 * - Azure SDK operations themselves are thread-safe
 *
 * @example
 * @code
 * azure_storage_config config;
 * config.container_name = "dicom-container";
 * config.connection_string = "DefaultEndpointsProtocol=https;AccountName=...";
 *
 * // For Azurite local testing
 * config.endpoint_url = "http://127.0.0.1:10000/devstoreaccount1";
 *
 * azure_blob_storage storage{config};
 *
 * // Store a DICOM dataset
 * core::dicom_dataset ds;
 * // ... populate dataset ...
 * auto result = storage.store(ds);
 *
 * // Retrieve by SOP Instance UID
 * auto retrieved = storage.retrieve("1.2.3.4.5.6.7.8.9");
 * @endcode
 *
 * @note This implementation currently uses mock Azure operations for testing.
 *       Full Azure SDK integration will be added in a future update.
 */
class azure_blob_storage : public storage_interface {
public:
  // =========================================================================
  // Construction
  // =========================================================================

  /**
   * @brief Construct Azure Blob storage with configuration
   * @param config Azure storage configuration
   */
  explicit azure_blob_storage(const azure_storage_config &config);

  /**
   * @brief Destructor
   */
  ~azure_blob_storage() override;

  /// Non-copyable (contains mutex)
  azure_blob_storage(const azure_blob_storage &) = delete;
  azure_blob_storage &operator=(const azure_blob_storage &) = delete;

  /// Non-movable (contains mutex)
  azure_blob_storage(azure_blob_storage &&) = delete;
  azure_blob_storage &operator=(azure_blob_storage &&) = delete;

  // =========================================================================
  // storage_interface Implementation
  // =========================================================================

  /**
   * @brief Store a DICOM dataset to Azure Blob Storage
   *
   * Serializes the dataset to DICOM Part 10 format and uploads to Azure.
   * Uses block blob upload for files exceeding block_upload_threshold.
   *
   * @param dataset The DICOM dataset to store
   * @return VoidResult Success or error information
   *
   * @note Requires Study, Series, and SOP Instance UIDs in the dataset
   */
  [[nodiscard]] auto store(const core::dicom_dataset &dataset)
      -> VoidResult override;

  /**
   * @brief Retrieve a DICOM dataset by SOP Instance UID
   *
   * Downloads the blob from Azure and deserializes to dicom_dataset.
   *
   * @param sop_instance_uid The unique identifier for the instance
   * @return Result containing the dataset or error information
   */
  [[nodiscard]] auto retrieve(std::string_view sop_instance_uid)
      -> Result<core::dicom_dataset> override;

  /**
   * @brief Remove a DICOM blob from Azure Storage
   *
   * @param sop_instance_uid The unique identifier for the instance to remove
   * @return VoidResult Success or error information
   */
  [[nodiscard]] auto remove(std::string_view sop_instance_uid)
      -> VoidResult override;

  /**
   * @brief Check if a DICOM instance exists in Azure Storage
   *
   * Uses Azure GetBlobProperties to check existence without downloading.
   *
   * @param sop_instance_uid The unique identifier to check
   * @return true if the blob exists, false otherwise
   */
  [[nodiscard]] auto exists(std::string_view sop_instance_uid) const
      -> bool override;

  /**
   * @brief Find DICOM datasets matching query criteria
   *
   * This operation requires scanning the local index. For large containers,
   * consider using the index_database instead.
   *
   * @param query The query dataset containing search criteria
   * @return Result containing matching datasets or error information
   *
   * @note This operation can be slow for large storage
   */
  [[nodiscard]] auto find(const core::dicom_dataset &query)
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
   * Checks that all indexed blobs exist in Azure and have valid ETags.
   *
   * @return VoidResult Success if integrity is verified, error otherwise
   */
  [[nodiscard]] auto verify_integrity() -> VoidResult override;

  // =========================================================================
  // Azure-specific Operations
  // =========================================================================

  /**
   * @brief Store with progress tracking
   *
   * @param dataset The DICOM dataset to store
   * @param callback Progress callback function
   * @return VoidResult Success or error information
   */
  [[nodiscard]] auto store_with_progress(const core::dicom_dataset &dataset,
                                         azure_progress_callback callback)
      -> VoidResult;

  /**
   * @brief Retrieve with progress tracking
   *
   * @param sop_instance_uid The SOP Instance UID
   * @param callback Progress callback function
   * @return Result containing the dataset or error information
   */
  [[nodiscard]] auto retrieve_with_progress(std::string_view sop_instance_uid,
                                            azure_progress_callback callback)
      -> Result<core::dicom_dataset>;

  /**
   * @brief Get the blob name for a SOP Instance UID
   *
   * @param sop_instance_uid The SOP Instance UID
   * @return The blob name (may be empty if not found)
   */
  [[nodiscard]] auto get_blob_name(std::string_view sop_instance_uid) const
      -> std::string;

  /**
   * @brief Get the container name
   *
   * @return The configured container name
   */
  [[nodiscard]] auto container_name() const -> const std::string &;

  /**
   * @brief Rebuild the local index from Azure
   *
   * Lists all blobs in the container and rebuilds the SOP UID mapping.
   *
   * @return VoidResult Success or error information
   *
   * @note This operation can be slow for containers with many blobs
   */
  [[nodiscard]] auto rebuild_index() -> VoidResult;

  /**
   * @brief Check Azure connectivity
   *
   * @return true if Azure is reachable, false otherwise
   */
  [[nodiscard]] auto is_connected() const -> bool;

  /**
   * @brief Set blob access tier
   *
   * @param sop_instance_uid The SOP Instance UID
   * @param tier The access tier (Hot, Cool, Archive)
   * @return VoidResult Success or error information
   */
  [[nodiscard]] auto set_access_tier(std::string_view sop_instance_uid,
                                     std::string_view tier) -> VoidResult;

private:
  // =========================================================================
  // Internal Types
  // =========================================================================

  /**
   * @brief Mock Azure Blob client interface for testing
   *
   * This will be replaced with Azure SDK BlobContainerClient when integrated.
   */
  class mock_azure_client;

  // =========================================================================
  // Internal Helper Methods
  // =========================================================================

  /**
   * @brief Build blob name for a dataset
   * @param study_uid Study Instance UID
   * @param series_uid Series Instance UID
   * @param sop_uid SOP Instance UID
   * @return The constructed blob name
   */
  [[nodiscard]] auto build_blob_name(std::string_view study_uid,
                                     std::string_view series_uid,
                                     std::string_view sop_uid) const
      -> std::string;

  /**
   * @brief Sanitize UID for use in blob name
   * @param uid The UID to sanitize
   * @return Sanitized string safe for blob name use
   */
  [[nodiscard]] static auto sanitize_uid(std::string_view uid) -> std::string;

  /**
   * @brief Execute block blob upload for large files
   * @param blob_name Blob name
   * @param data Data to upload
   * @param callback Optional progress callback
   * @return VoidResult Success or error information
   */
  [[nodiscard]] auto upload_block_blob(const std::string &blob_name,
                                       const std::vector<std::uint8_t> &data,
                                       azure_progress_callback callback)
      -> VoidResult;

  /**
   * @brief Check if dataset matches query criteria
   * @param dataset The dataset to check
   * @param query The query criteria
   * @return true if dataset matches query
   */
  [[nodiscard]] static auto matches_query(const core::dicom_dataset &dataset,
                                          const core::dicom_dataset &query)
      -> bool;

  // =========================================================================
  // Member Variables
  // =========================================================================

  /// Storage configuration
  azure_storage_config config_;

  /// Mock Azure client for testing (will be replaced with Azure SDK client)
  std::unique_ptr<mock_azure_client> client_;

  /// Mapping from SOP Instance UID to Azure blob info
  std::unordered_map<std::string, azure_blob_info> index_;

  /// Mutex for thread-safe access
  mutable std::shared_mutex mutex_;
};

} // namespace pacs::storage
