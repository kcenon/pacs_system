/**
 * @file s3_storage.cpp
 * @brief Implementation of S3-compatible DICOM storage backend
 *
 * This file implements the s3_storage class for cloud storage support.
 * Currently uses a mock S3 client for testing and API validation.
 * Full AWS SDK integration will be added in a future update.
 */

#include <pacs/storage/s3_storage.hpp>

#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <algorithm>
#include <set>
#include <sstream>

namespace pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;

namespace {

/// Error codes for S3 storage operations
constexpr int kMissingRequiredUid = -1;
constexpr int kObjectNotFound = -2;
constexpr int kUploadError = -3;
constexpr int kDownloadError = -4;
constexpr int kConnectionError = -6;
constexpr int kIntegrityError = -7;
constexpr int kSerializationError = -8;

} // namespace

// ============================================================================
// Mock S3 Client Implementation
// ============================================================================

/**
 * @brief Mock S3 client for testing without AWS SDK dependency
 *
 * This class simulates S3 operations using an in-memory storage.
 * It will be replaced with AWS SDK S3Client when integrated.
 */
class s3_storage::mock_s3_client {
public:
  explicit mock_s3_client(const cloud_storage_config & /*config*/)
      : connected_(true) {}

  /**
   * @brief Simulate S3 PutObject operation
   */
  [[nodiscard]] auto put_object(const std::string &key,
                                const std::vector<std::uint8_t> &data)
      -> VoidResult {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "S3 client not connected", "s3_storage");
    }

    objects_[key] = data;
    return ok();
  }

  /**
   * @brief Simulate S3 GetObject operation
   */
  [[nodiscard]] auto get_object(const std::string &key)
      -> Result<std::vector<std::uint8_t>> {
    if (!connected_) {
      return make_error<std::vector<std::uint8_t>>(
          kConnectionError, "S3 client not connected", "s3_storage");
    }

    auto it = objects_.find(key);
    if (it == objects_.end()) {
      return make_error<std::vector<std::uint8_t>>(
          kObjectNotFound, "Object not found: " + key, "s3_storage");
    }

    return it->second;
  }

  /**
   * @brief Simulate S3 DeleteObject operation
   */
  [[nodiscard]] auto delete_object(const std::string &key) -> VoidResult {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "S3 client not connected", "s3_storage");
    }

    objects_.erase(key);
    return ok();
  }

  /**
   * @brief Simulate S3 HeadObject operation
   */
  [[nodiscard]] auto head_object(const std::string &key) const -> bool {
    if (!connected_) {
      return false;
    }
    return objects_.contains(key);
  }

  /**
   * @brief Get object size
   */
  [[nodiscard]] auto get_object_size(const std::string &key) const
      -> std::size_t {
    auto it = objects_.find(key);
    if (it != objects_.end()) {
      return it->second.size();
    }
    return 0;
  }

  /**
   * @brief List all object keys
   */
  [[nodiscard]] auto list_objects() const -> std::vector<std::string> {
    std::vector<std::string> keys;
    keys.reserve(objects_.size());
    for (const auto &[key, data] : objects_) {
      keys.push_back(key);
    }
    return keys;
  }

  /**
   * @brief Check connection status
   */
  [[nodiscard]] auto is_connected() const -> bool { return connected_; }

  /**
   * @brief Set connection status (for testing)
   */
  void set_connected(bool connected) { connected_ = connected; }

private:
  std::unordered_map<std::string, std::vector<std::uint8_t>> objects_;
  bool connected_;
};

// ============================================================================
// Construction
// ============================================================================

s3_storage::s3_storage(const cloud_storage_config &config)
    : config_(config), client_(std::make_unique<mock_s3_client>(config)) {}

s3_storage::~s3_storage() = default;

// ============================================================================
// storage_interface Implementation
// ============================================================================

auto s3_storage::store(const core::dicom_dataset &dataset) -> VoidResult {
  return store_with_progress(dataset, nullptr);
}

auto s3_storage::store_with_progress(const core::dicom_dataset &dataset,
                                     progress_callback callback) -> VoidResult {
  // Extract required UIDs
  auto study_uid = dataset.get_string(core::tags::study_instance_uid);
  auto series_uid = dataset.get_string(core::tags::series_instance_uid);
  auto sop_uid = dataset.get_string(core::tags::sop_instance_uid);

  if (study_uid.empty() || series_uid.empty() || sop_uid.empty()) {
    return make_error<std::monostate>(
        kMissingRequiredUid,
        "Missing required UID (Study, Series, or SOP Instance UID)",
        "s3_storage");
  }

  // Build S3 object key
  auto object_key = build_object_key(study_uid, series_uid, sop_uid);

  // Create DICOM file and serialize to bytes
  auto dicom_file = core::dicom_file::create(
      dataset, encoding::transfer_syntax::explicit_vr_little_endian);

  auto data = dicom_file.to_bytes();
  if (data.empty()) {
    return make_error<std::monostate>(
        kSerializationError, "Failed to serialize DICOM dataset", "s3_storage");
  }

  // Report initial progress
  if (callback && !callback(0, data.size())) {
    return make_error<std::monostate>(kUploadError, "Upload cancelled by user",
                                      "s3_storage");
  }

  // Upload to S3 (use multipart for large files)
  VoidResult upload_result = ok();
  if (data.size() > config_.multipart_threshold) {
    upload_result = upload_multipart(object_key, data, callback);
  } else {
    upload_result = client_->put_object(object_key, data);

    // Report completion progress
    if (callback) {
      callback(data.size(), data.size());
    }
  }

  if (upload_result.is_err()) {
    return upload_result;
  }

  // Update local index
  {
    std::unique_lock lock(mutex_);
    s3_object_info info;
    info.key = object_key;
    info.sop_instance_uid = sop_uid;
    info.study_instance_uid = study_uid;
    info.series_instance_uid = series_uid;
    info.size_bytes = data.size();
    index_[sop_uid] = std::move(info);
  }

  return ok();
}

auto s3_storage::retrieve(std::string_view sop_instance_uid)
    -> Result<core::dicom_dataset> {
  return retrieve_with_progress(sop_instance_uid, nullptr);
}

auto s3_storage::retrieve_with_progress(std::string_view sop_instance_uid,
                                        progress_callback callback)
    -> Result<core::dicom_dataset> {
  std::string object_key;

  {
    std::shared_lock lock(mutex_);
    auto it = index_.find(std::string{sop_instance_uid});
    if (it == index_.end()) {
      return make_error<core::dicom_dataset>(
          kObjectNotFound,
          "Instance not found: " + std::string{sop_instance_uid}, "s3_storage");
    }
    object_key = it->second.key;
  }

  // Download from S3
  auto download_result = client_->get_object(object_key);
  if (download_result.is_err()) {
    return make_error<core::dicom_dataset>(
        kDownloadError, "Failed to download from S3", "s3_storage");
  }

  const auto &data = download_result.value();

  // Report progress (download complete)
  if (callback) {
    callback(data.size(), data.size());
  }

  // Deserialize DICOM data
  auto parse_result = core::dicom_file::from_bytes(data);
  if (!parse_result.has_value()) {
    return make_error<core::dicom_dataset>(
        kSerializationError,
        "Failed to parse DICOM data: " + core::to_string(parse_result.error()),
        "s3_storage");
  }

  return parse_result->dataset();
}

auto s3_storage::remove(std::string_view sop_instance_uid) -> VoidResult {
  std::string object_key;

  {
    std::unique_lock lock(mutex_);
    auto it = index_.find(std::string{sop_instance_uid});
    if (it == index_.end()) {
      // Not found is not an error for remove
      return ok();
    }
    object_key = it->second.key;
    index_.erase(it);
  }

  // Delete from S3
  auto delete_result = client_->delete_object(object_key);
  // Ignore delete errors - object might have been deleted externally

  return ok();
}

auto s3_storage::exists(std::string_view sop_instance_uid) const -> bool {
  std::shared_lock lock(mutex_);
  return index_.contains(std::string{sop_instance_uid});
}

auto s3_storage::find(const core::dicom_dataset &query)
    -> Result<std::vector<core::dicom_dataset>> {
  std::vector<core::dicom_dataset> results;

  std::vector<std::string> keys_to_retrieve;
  {
    std::shared_lock lock(mutex_);
    keys_to_retrieve.reserve(index_.size());
    for (const auto &[uid, info] : index_) {
      keys_to_retrieve.push_back(info.key);
    }
  }

  for (const auto &key : keys_to_retrieve) {
    auto download_result = client_->get_object(key);
    if (download_result.is_err()) {
      continue; // Skip objects that can't be downloaded
    }

    auto parse_result = core::dicom_file::from_bytes(download_result.value());
    if (!parse_result.has_value()) {
      continue; // Skip invalid DICOM files
    }

    const auto &dataset = parse_result->dataset();
    if (matches_query(dataset, query)) {
      results.push_back(dataset);
    }
  }

  return results;
}

auto s3_storage::get_statistics() const -> storage_statistics {
  storage_statistics stats;

  std::set<std::string> studies;
  std::set<std::string> series;
  std::set<std::string> patients;

  {
    std::shared_lock lock(mutex_);
    stats.total_instances = index_.size();

    for (const auto &[uid, info] : index_) {
      stats.total_bytes += info.size_bytes;

      if (!info.study_instance_uid.empty()) {
        studies.insert(info.study_instance_uid);
      }
      if (!info.series_instance_uid.empty()) {
        series.insert(info.series_instance_uid);
      }
    }
  }

  stats.studies_count = studies.size();
  stats.series_count = series.size();
  // Note: patient_count requires downloading datasets to extract PatientID

  return stats;
}

auto s3_storage::verify_integrity() -> VoidResult {
  std::vector<std::pair<std::string, std::string>> entries;
  {
    std::shared_lock lock(mutex_);
    entries.reserve(index_.size());
    for (const auto &[uid, info] : index_) {
      entries.emplace_back(uid, info.key);
    }
  }

  std::vector<std::string> invalid_entries;

  for (const auto &[uid, key] : entries) {
    if (!client_->head_object(key)) {
      invalid_entries.push_back(uid + " (object missing)");
    }
  }

  if (!invalid_entries.empty()) {
    std::string message = "Integrity check failed for " +
                          std::to_string(invalid_entries.size()) + " entries";
    return make_error<std::monostate>(kIntegrityError, message, "s3_storage");
  }

  return ok();
}

// ============================================================================
// S3-specific Operations
// ============================================================================

auto s3_storage::get_object_key(std::string_view sop_instance_uid) const
    -> std::string {
  std::shared_lock lock(mutex_);
  auto it = index_.find(std::string{sop_instance_uid});
  if (it != index_.end()) {
    return it->second.key;
  }
  return {};
}

auto s3_storage::bucket_name() const -> const std::string & {
  return config_.bucket_name;
}

auto s3_storage::rebuild_index() -> VoidResult {
  std::unique_lock lock(mutex_);
  index_.clear();

  // List all objects from S3
  auto keys = client_->list_objects();

  for (const auto &key : keys) {
    // Download and parse each object to rebuild index
    auto download_result = client_->get_object(key);
    if (download_result.is_err()) {
      continue;
    }

    auto parse_result = core::dicom_file::from_bytes(download_result.value());
    if (!parse_result.has_value()) {
      continue;
    }

    const auto &dataset = parse_result->dataset();
    auto sop_uid = dataset.get_string(core::tags::sop_instance_uid);
    auto study_uid = dataset.get_string(core::tags::study_instance_uid);
    auto series_uid = dataset.get_string(core::tags::series_instance_uid);

    if (!sop_uid.empty()) {
      s3_object_info info;
      info.key = key;
      info.sop_instance_uid = sop_uid;
      info.study_instance_uid = study_uid;
      info.series_instance_uid = series_uid;
      info.size_bytes = client_->get_object_size(key);
      index_[sop_uid] = std::move(info);
    }
  }

  return ok();
}

auto s3_storage::is_connected() const -> bool {
  return client_ && client_->is_connected();
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

auto s3_storage::build_object_key(std::string_view study_uid,
                                  std::string_view series_uid,
                                  std::string_view sop_uid) const
    -> std::string {
  std::ostringstream oss;
  oss << sanitize_uid(study_uid) << "/" << sanitize_uid(series_uid) << "/"
      << sanitize_uid(sop_uid) << ".dcm";
  return oss.str();
}

auto s3_storage::sanitize_uid(std::string_view uid) -> std::string {
  std::string result;
  result.reserve(uid.length());

  for (char c : uid) {
    // UIDs contain digits and dots, which are safe for S3 keys
    // Replace any other characters with underscore
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '.') {
      result += c;
    } else {
      result += '_';
    }
  }

  return result;
}

auto s3_storage::upload_multipart(const std::string &key,
                                  const std::vector<std::uint8_t> &data,
                                  progress_callback callback) -> VoidResult {
  // For mock implementation, just use regular put_object
  // Real implementation would use S3 multipart upload API

  std::size_t total_bytes = data.size();
  std::size_t bytes_uploaded = 0;

  // Simulate multipart upload with progress
  while (bytes_uploaded < total_bytes) {
    std::size_t part_size =
        std::min(config_.part_size, total_bytes - bytes_uploaded);
    bytes_uploaded += part_size;

    if (callback && !callback(bytes_uploaded, total_bytes)) {
      return make_error<std::monostate>(
          kUploadError, "Upload cancelled by user", "s3_storage");
    }
  }

  // Actually store the data
  return client_->put_object(key, data);
}

auto s3_storage::matches_query(const core::dicom_dataset &dataset,
                               const core::dicom_dataset &query) -> bool {
  // If query is empty, match all
  if (query.empty()) {
    return true;
  }

  // Check each query element
  for (const auto &[tag, element] : query) {
    auto query_value = element.as_string();
    if (query_value.empty()) {
      continue; // Empty value acts as wildcard
    }

    auto dataset_value = dataset.get_string(tag);

    // Support basic wildcard matching (* and ?)
    if (query_value.find('*') != std::string::npos ||
        query_value.find('?') != std::string::npos) {
      // Simple pattern matching
      if (query_value.front() == '*' && query_value.back() == '*') {
        // Contains
        auto inner = query_value.substr(1, query_value.length() - 2);
        if (dataset_value.find(inner) == std::string::npos) {
          return false;
        }
      } else if (query_value.front() == '*') {
        // Ends with
        auto suffix = query_value.substr(1);
        if (dataset_value.length() < suffix.length() ||
            dataset_value.substr(dataset_value.length() - suffix.length()) !=
                suffix) {
          return false;
        }
      } else if (query_value.back() == '*') {
        // Starts with
        auto prefix = query_value.substr(0, query_value.length() - 1);
        if (dataset_value.substr(0, prefix.length()) != prefix) {
          return false;
        }
      }
    } else {
      // Exact match
      if (dataset_value != query_value) {
        return false;
      }
    }
  }

  return true;
}

} // namespace pacs::storage
