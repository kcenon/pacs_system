/**
 * @file s3_storage.hpp
 * @brief S3-compatible DICOM storage backend for cloud storage support
 *
 * This file provides the s3_storage class which implements storage_interface
 * using S3-compatible object storage (AWS S3, MinIO, etc.).
 *
 * @see SRS-STOR-003, FR-4.2 (Cloud Storage Backend)
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
 * @brief Configuration for S3-compatible cloud storage
 *
 * Contains all settings needed to connect to an S3-compatible storage service.
 * Supports AWS S3, MinIO, and other S3-compatible services via endpoint_url.
 */
struct cloud_storage_config {
  /// S3 bucket name for storing DICOM files
  std::string bucket_name;

  /// AWS region (e.g., "us-east-1", "eu-west-1")
  std::string region{"us-east-1"};

  /// AWS access key ID for authentication
  std::string access_key_id;

  /// AWS secret access key for authentication
  std::string secret_access_key;

  /// Optional custom endpoint URL for MinIO or S3-compatible services
  /// If empty, uses the default AWS S3 endpoint
  std::optional<std::string> endpoint_url;

  /// Threshold for multipart upload in bytes (default: 100MB)
  /// Files larger than this will use multipart upload
  std::size_t multipart_threshold = 100 * 1024 * 1024;

  /// Part size for multipart upload in bytes (default: 10MB)
  std::size_t part_size = 10 * 1024 * 1024;

  /// Maximum number of concurrent upload connections
  std::size_t max_connections = 25;

  /// Connection timeout in milliseconds
  std::uint32_t connect_timeout_ms = 3000;

  /// Request timeout in milliseconds
  std::uint32_t request_timeout_ms = 30000;

  /// Enable server-side encryption (SSE-S3)
  bool enable_encryption = false;

  /// Storage class for S3 objects (STANDARD, INTELLIGENT_TIERING, etc.)
  std::string storage_class{"STANDARD"};
};

/**
 * @brief Information about an S3 object
 */
struct s3_object_info {
  /// S3 object key (path within bucket)
  std::string key;

  /// SOP Instance UID from DICOM metadata
  std::string sop_instance_uid;

  /// Study Instance UID
  std::string study_instance_uid;

  /// Series Instance UID
  std::string series_instance_uid;

  /// Object size in bytes
  std::size_t size_bytes{0};

  /// ETag for integrity verification
  std::string etag;

  /// Content type
  std::string content_type{"application/dicom"};
};

/**
 * @brief Callback type for upload/download progress tracking
 *
 * Parameters:
 * - bytes_transferred: Number of bytes transferred so far
 * - total_bytes: Total number of bytes to transfer
 * - Returns: true to continue, false to abort
 */
using progress_callback =
    std::function<bool(std::size_t bytes_transferred, std::size_t total_bytes)>;

/**
 * @brief S3-compatible storage backend for DICOM files
 *
 * Implements storage_interface using S3-compatible object storage.
 * Supports AWS S3 and S3-compatible services (MinIO, etc.).
 *
 * Object Key Structure:
 * @code
 * {bucket}/
 *   +-- {StudyUID}/
 *       +-- {SeriesUID}/
 *           +-- {SOPUID}.dcm
 * @endcode
 *
 * Thread Safety:
 * - All methods are thread-safe
 * - Concurrent reads are allowed (shared lock)
 * - Writes require exclusive lock for index updates
 * - S3 operations themselves are thread-safe
 *
 * @example
 * @code
 * cloud_storage_config config;
 * config.bucket_name = "my-dicom-bucket";
 * config.region = "us-east-1";
 * config.access_key_id = "AKIAIOSFODNN7EXAMPLE";
 * config.secret_access_key = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
 *
 * // For MinIO local testing
 * config.endpoint_url = "http://localhost:9000";
 *
 * s3_storage storage{config};
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
 * @note This implementation currently uses mock S3 operations for testing.
 *       Full AWS SDK integration will be added in a future update.
 */
class s3_storage : public storage_interface {
public:
  // =========================================================================
  // Construction
  // =========================================================================

  /**
   * @brief Construct S3 storage with configuration
   * @param config Cloud storage configuration
   */
  explicit s3_storage(const cloud_storage_config &config);

  /**
   * @brief Destructor
   */
  ~s3_storage() override;

  /// Non-copyable (contains mutex)
  s3_storage(const s3_storage &) = delete;
  s3_storage &operator=(const s3_storage &) = delete;

  /// Non-movable (contains mutex)
  s3_storage(s3_storage &&) = delete;
  s3_storage &operator=(s3_storage &&) = delete;

  // =========================================================================
  // storage_interface Implementation
  // =========================================================================

  /**
   * @brief Store a DICOM dataset to S3
   *
   * Serializes the dataset to DICOM Part 10 format and uploads to S3.
   * Uses multipart upload for files exceeding multipart_threshold.
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
   * Downloads the object from S3 and deserializes to dicom_dataset.
   *
   * @param sop_instance_uid The unique identifier for the instance
   * @return Result containing the dataset or error information
   */
  [[nodiscard]] auto retrieve(std::string_view sop_instance_uid)
      -> Result<core::dicom_dataset> override;

  /**
   * @brief Remove a DICOM object from S3
   *
   * @param sop_instance_uid The unique identifier for the instance to remove
   * @return VoidResult Success or error information
   */
  [[nodiscard]] auto remove(std::string_view sop_instance_uid)
      -> VoidResult override;

  /**
   * @brief Check if a DICOM instance exists in S3
   *
   * Uses S3 HeadObject to check existence without downloading.
   *
   * @param sop_instance_uid The unique identifier to check
   * @return true if the object exists, false otherwise
   */
  [[nodiscard]] auto exists(std::string_view sop_instance_uid) const
      -> bool override;

  /**
   * @brief Find DICOM datasets matching query criteria
   *
   * This operation requires scanning the local index. For large buckets,
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
   * Checks that all indexed objects exist in S3 and have valid ETags.
   *
   * @return VoidResult Success if integrity is verified, error otherwise
   */
  [[nodiscard]] auto verify_integrity() -> VoidResult override;

  // =========================================================================
  // S3-specific Operations
  // =========================================================================

  /**
   * @brief Store with progress tracking
   *
   * @param dataset The DICOM dataset to store
   * @param callback Progress callback function
   * @return VoidResult Success or error information
   */
  [[nodiscard]] auto store_with_progress(const core::dicom_dataset &dataset,
                                         progress_callback callback)
      -> VoidResult;

  /**
   * @brief Retrieve with progress tracking
   *
   * @param sop_instance_uid The SOP Instance UID
   * @param callback Progress callback function
   * @return Result containing the dataset or error information
   */
  [[nodiscard]] auto retrieve_with_progress(std::string_view sop_instance_uid,
                                            progress_callback callback)
      -> Result<core::dicom_dataset>;

  /**
   * @brief Get the S3 object key for a SOP Instance UID
   *
   * @param sop_instance_uid The SOP Instance UID
   * @return The S3 object key (may be empty if not found)
   */
  [[nodiscard]] auto get_object_key(std::string_view sop_instance_uid) const
      -> std::string;

  /**
   * @brief Get the bucket name
   *
   * @return The configured bucket name
   */
  [[nodiscard]] auto bucket_name() const -> const std::string &;

  /**
   * @brief Rebuild the local index from S3
   *
   * Lists all objects in the bucket and rebuilds the SOP UID mapping.
   *
   * @return VoidResult Success or error information
   *
   * @note This operation can be slow for buckets with many objects
   */
  [[nodiscard]] auto rebuild_index() -> VoidResult;

  /**
   * @brief Check S3 connectivity
   *
   * @return true if S3 is reachable, false otherwise
   */
  [[nodiscard]] auto is_connected() const -> bool;

private:
  // =========================================================================
  // Internal Types
  // =========================================================================

  /**
   * @brief Mock S3 client interface for testing
   *
   * This will be replaced with AWS SDK S3Client when integrated.
   */
  class mock_s3_client;

  // =========================================================================
  // Internal Helper Methods
  // =========================================================================

  /**
   * @brief Build S3 object key for a dataset
   * @param study_uid Study Instance UID
   * @param series_uid Series Instance UID
   * @param sop_uid SOP Instance UID
   * @return The constructed object key
   */
  [[nodiscard]] auto build_object_key(std::string_view study_uid,
                                      std::string_view series_uid,
                                      std::string_view sop_uid) const
      -> std::string;

  /**
   * @brief Sanitize UID for use in S3 object key
   * @param uid The UID to sanitize
   * @return Sanitized string safe for S3 key use
   */
  [[nodiscard]] static auto sanitize_uid(std::string_view uid) -> std::string;

  /**
   * @brief Execute multipart upload for large files
   * @param key S3 object key
   * @param data Data to upload
   * @param callback Optional progress callback
   * @return VoidResult Success or error information
   */
  [[nodiscard]] auto upload_multipart(const std::string &key,
                                      const std::vector<std::uint8_t> &data,
                                      progress_callback callback) -> VoidResult;

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
  cloud_storage_config config_;

  /// Mock S3 client for testing (will be replaced with AWS SDK client)
  std::unique_ptr<mock_s3_client> client_;

  /// Mapping from SOP Instance UID to S3 object info
  std::unordered_map<std::string, s3_object_info> index_;

  /// Mutex for thread-safe access
  mutable std::shared_mutex mutex_;
};

} // namespace pacs::storage
