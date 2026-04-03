// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file s3_storage.cpp
 * @brief Implementation of S3-compatible DICOM storage backend
 *
 * This file implements the s3_storage class for cloud storage support.
 * Supports two S3 client backends selected at compile time:
 * - Mock client (default / PACS_USE_MOCK_S3): in-memory testing
 * - AWS SDK client (PACS_WITH_AWS_SDK): production S3 operations
 */

#include <pacs/storage/s3_storage.hpp>

#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <algorithm>
#include <set>
#include <sstream>

#if defined(PACS_WITH_AWS_SDK) && !defined(PACS_USE_MOCK_S3)
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/PutObjectRequest.h>

#include <aws/s3/model/AbortMultipartUploadRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/UploadPartRequest.h>

#include <mutex>
#endif

namespace kcenon::pacs::storage {

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
// S3 Client Interface
// ============================================================================

/**
 * @brief Abstract interface for S3 client operations
 *
 * Both mock_s3_client and aws_s3_client implement this interface,
 * allowing compile-time selection of the backend.
 */
class s3_storage::s3_client_interface {
public:
  virtual ~s3_client_interface() = default;

  [[nodiscard]] virtual auto put_object(const std::string &key,
                                        const std::vector<std::uint8_t> &data)
      -> VoidResult = 0;

  [[nodiscard]] virtual auto get_object(const std::string &key)
      -> Result<std::vector<std::uint8_t>> = 0;

  [[nodiscard]] virtual auto delete_object(const std::string &key)
      -> VoidResult = 0;

  [[nodiscard]] virtual auto head_object(const std::string &key) const
      -> bool = 0;

  [[nodiscard]] virtual auto get_object_size(const std::string &key) const
      -> std::size_t = 0;

  [[nodiscard]] virtual auto list_objects() const
      -> std::vector<std::string> = 0;

  [[nodiscard]] virtual auto is_connected() const -> bool = 0;

  [[nodiscard]] virtual auto multipart_upload(
      const std::string &key, const std::vector<std::uint8_t> &data,
      std::size_t part_size, progress_callback callback) -> VoidResult = 0;
};

// ============================================================================
// Mock S3 Client Implementation
// ============================================================================

/**
 * @brief Mock S3 client for testing without AWS SDK dependency
 *
 * Simulates S3 operations using in-memory storage.
 * Always used when PACS_USE_MOCK_S3 is defined or PACS_WITH_AWS_SDK is not set.
 */
class mock_s3_client : public s3_storage::s3_client_interface {
public:
  explicit mock_s3_client(const cloud_storage_config & /*config*/)
      : connected_(true) {}

  [[nodiscard]] auto put_object(const std::string &key,
                                const std::vector<std::uint8_t> &data)
      -> VoidResult override {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "S3 client not connected", "s3_storage");
    }
    objects_[key] = data;
    return ok();
  }

  [[nodiscard]] auto get_object(const std::string &key)
      -> Result<std::vector<std::uint8_t>> override {
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

  [[nodiscard]] auto delete_object(const std::string &key)
      -> VoidResult override {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "S3 client not connected", "s3_storage");
    }
    objects_.erase(key);
    return ok();
  }

  [[nodiscard]] auto head_object(const std::string &key) const
      -> bool override {
    if (!connected_) {
      return false;
    }
    return objects_.contains(key);
  }

  [[nodiscard]] auto get_object_size(const std::string &key) const
      -> std::size_t override {
    auto it = objects_.find(key);
    if (it != objects_.end()) {
      return it->second.size();
    }
    return 0;
  }

  [[nodiscard]] auto list_objects() const
      -> std::vector<std::string> override {
    std::vector<std::string> keys;
    keys.reserve(objects_.size());
    for (const auto &[key, data] : objects_) {
      keys.push_back(key);
    }
    return keys;
  }

  [[nodiscard]] auto is_connected() const -> bool override {
    return connected_;
  }

  [[nodiscard]] auto multipart_upload(const std::string &key,
                                      const std::vector<std::uint8_t> &data,
                                      std::size_t part_size,
                                      progress_callback callback)
      -> VoidResult override {
    std::size_t total_bytes = data.size();
    std::size_t bytes_uploaded = 0;

    while (bytes_uploaded < total_bytes) {
      std::size_t chunk = (std::min)(part_size, total_bytes - bytes_uploaded);
      bytes_uploaded += chunk;

      if (callback && !callback(bytes_uploaded, total_bytes)) {
        return make_error<std::monostate>(
            kUploadError, "Upload cancelled by user", "s3_storage");
      }
    }

    return put_object(key, data);
  }

private:
  std::unordered_map<std::string, std::vector<std::uint8_t>> objects_;
  bool connected_;
};

// ============================================================================
// AWS SDK S3 Client Implementation
// ============================================================================

#if defined(PACS_WITH_AWS_SDK) && !defined(PACS_USE_MOCK_S3)

namespace {

/// RAII guard for AWS SDK initialization
class aws_sdk_guard {
public:
  static void ensure_initialized() {
    std::call_once(init_flag_, [] {
      Aws::InitAPI(options_);
      std::atexit([] { Aws::ShutdownAPI(options_); });
    });
  }

private:
  static inline std::once_flag init_flag_;
  static inline Aws::SDKOptions options_;
};

} // namespace

/**
 * @brief Production S3 client using AWS SDK for C++
 *
 * Wraps Aws::S3::S3Client for real S3/MinIO operations.
 * Enabled when PACS_WITH_AWS_SDK is defined and PACS_USE_MOCK_S3 is not.
 */
class aws_s3_client : public s3_storage::s3_client_interface {
public:
  explicit aws_s3_client(const cloud_storage_config &config)
      : bucket_(config.bucket_name) {
    aws_sdk_guard::ensure_initialized();

    Aws::Client::ClientConfiguration client_config;
    client_config.region = config.region;
    client_config.connectTimeoutMs = config.connect_timeout_ms;
    client_config.requestTimeoutMs = config.request_timeout_ms;
    client_config.maxConnections = static_cast<unsigned>(config.max_connections);

    if (config.endpoint_url.has_value()) {
      client_config.endpointOverride = config.endpoint_url.value();
      // For S3-compatible services (MinIO), use path-style addressing
      use_path_style_ = true;
    }

    Aws::Auth::AWSCredentials credentials(config.access_key_id,
                                          config.secret_access_key);

    Aws::S3::S3ClientConfiguration s3_config(client_config);
    s3_config.useVirtualAddressing = !use_path_style_;
    client_ = std::make_unique<Aws::S3::S3Client>(credentials, nullptr,
                                                   s3_config);
  }

  [[nodiscard]] auto put_object(const std::string &key,
                                const std::vector<std::uint8_t> &data)
      -> VoidResult override {
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket_);
    request.SetKey(key);
    request.SetContentType("application/dicom");

    auto stream = Aws::MakeShared<Aws::StringStream>("PutObjectStream");
    stream->write(reinterpret_cast<const char *>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    request.SetBody(stream);

    auto outcome = client_->PutObject(request);
    if (!outcome.IsSuccess()) {
      return make_error<std::monostate>(
          kUploadError,
          "S3 PutObject failed: " +
              std::string(outcome.GetError().GetMessage()),
          "s3_storage");
    }

    return ok();
  }

  [[nodiscard]] auto get_object(const std::string &key)
      -> Result<std::vector<std::uint8_t>> override {
    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket_);
    request.SetKey(key);

    auto outcome = client_->GetObject(request);
    if (!outcome.IsSuccess()) {
      const auto &error = outcome.GetError();
      if (error.GetErrorType() ==
          Aws::S3::S3Errors::NO_SUCH_KEY) {
        return make_error<std::vector<std::uint8_t>>(
            kObjectNotFound, "Object not found: " + key, "s3_storage");
      }
      return make_error<std::vector<std::uint8_t>>(
          kDownloadError,
          "S3 GetObject failed: " + std::string(error.GetMessage()),
          "s3_storage");
    }

    auto &body = outcome.GetResult().GetBody();
    std::vector<std::uint8_t> result(
        (std::istreambuf_iterator<char>(body)),
        std::istreambuf_iterator<char>());
    return result;
  }

  [[nodiscard]] auto delete_object(const std::string &key)
      -> VoidResult override {
    Aws::S3::Model::DeleteObjectRequest request;
    request.SetBucket(bucket_);
    request.SetKey(key);

    auto outcome = client_->DeleteObject(request);
    if (!outcome.IsSuccess()) {
      return make_error<std::monostate>(
          kUploadError,
          "S3 DeleteObject failed: " +
              std::string(outcome.GetError().GetMessage()),
          "s3_storage");
    }

    return ok();
  }

  [[nodiscard]] auto head_object(const std::string &key) const
      -> bool override {
    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(bucket_);
    request.SetKey(key);

    auto outcome = client_->HeadObject(request);
    return outcome.IsSuccess();
  }

  [[nodiscard]] auto get_object_size(const std::string &key) const
      -> std::size_t override {
    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(bucket_);
    request.SetKey(key);

    auto outcome = client_->HeadObject(request);
    if (outcome.IsSuccess()) {
      return static_cast<std::size_t>(
          outcome.GetResult().GetContentLength());
    }
    return 0;
  }

  [[nodiscard]] auto list_objects() const
      -> std::vector<std::string> override {
    std::vector<std::string> keys;

    Aws::S3::Model::ListObjectsV2Request request;
    request.SetBucket(bucket_);

    bool has_more = true;
    while (has_more) {
      auto outcome = client_->ListObjectsV2(request);
      if (!outcome.IsSuccess()) {
        break;
      }

      const auto &result = outcome.GetResult();
      for (const auto &object : result.GetContents()) {
        keys.push_back(object.GetKey());
      }

      has_more = result.GetIsTruncated();
      if (has_more) {
        request.SetContinuationToken(result.GetNextContinuationToken());
      }
    }

    return keys;
  }

  [[nodiscard]] auto is_connected() const -> bool override {
    return client_ != nullptr;
  }

  [[nodiscard]] auto multipart_upload(const std::string &key,
                                      const std::vector<std::uint8_t> &data,
                                      std::size_t part_size,
                                      progress_callback callback)
      -> VoidResult override {
    // Initiate multipart upload
    Aws::S3::Model::CreateMultipartUploadRequest create_request;
    create_request.SetBucket(bucket_);
    create_request.SetKey(key);
    create_request.SetContentType("application/dicom");

    auto create_outcome = client_->CreateMultipartUpload(create_request);
    if (!create_outcome.IsSuccess()) {
      return make_error<std::monostate>(
          kUploadError,
          "Failed to initiate multipart upload: " +
              std::string(create_outcome.GetError().GetMessage()),
          "s3_storage");
    }

    auto upload_id = create_outcome.GetResult().GetUploadId();
    Aws::S3::Model::CompletedMultipartUpload completed_upload;

    std::size_t total_bytes = data.size();
    std::size_t bytes_uploaded = 0;
    int part_number = 1;

    while (bytes_uploaded < total_bytes) {
      std::size_t chunk =
          (std::min)(part_size, total_bytes - bytes_uploaded);

      auto stream = Aws::MakeShared<Aws::StringStream>("UploadPartStream");
      stream->write(
          reinterpret_cast<const char *>(data.data() + bytes_uploaded),
          static_cast<std::streamsize>(chunk));

      Aws::S3::Model::UploadPartRequest part_request;
      part_request.SetBucket(bucket_);
      part_request.SetKey(key);
      part_request.SetUploadId(upload_id);
      part_request.SetPartNumber(part_number);
      part_request.SetBody(stream);
      part_request.SetContentLength(static_cast<long long>(chunk));

      auto part_outcome = client_->UploadPart(part_request);
      if (!part_outcome.IsSuccess()) {
        // Abort the multipart upload on failure
        Aws::S3::Model::AbortMultipartUploadRequest abort_request;
        abort_request.SetBucket(bucket_);
        abort_request.SetKey(key);
        abort_request.SetUploadId(upload_id);
        client_->AbortMultipartUpload(abort_request);

        return make_error<std::monostate>(
            kUploadError,
            "Failed to upload part " + std::to_string(part_number) + ": " +
                std::string(part_outcome.GetError().GetMessage()),
            "s3_storage");
      }

      Aws::S3::Model::CompletedPart completed_part;
      completed_part.SetPartNumber(part_number);
      completed_part.SetETag(part_outcome.GetResult().GetETag());
      completed_upload.AddParts(std::move(completed_part));

      bytes_uploaded += chunk;
      part_number++;

      if (callback && !callback(bytes_uploaded, total_bytes)) {
        // Abort on user cancellation
        Aws::S3::Model::AbortMultipartUploadRequest abort_request;
        abort_request.SetBucket(bucket_);
        abort_request.SetKey(key);
        abort_request.SetUploadId(upload_id);
        client_->AbortMultipartUpload(abort_request);

        return make_error<std::monostate>(
            kUploadError, "Upload cancelled by user", "s3_storage");
      }
    }

    // Complete multipart upload
    Aws::S3::Model::CompleteMultipartUploadRequest complete_request;
    complete_request.SetBucket(bucket_);
    complete_request.SetKey(key);
    complete_request.SetUploadId(upload_id);
    complete_request.SetMultipartUpload(completed_upload);

    auto complete_outcome = client_->CompleteMultipartUpload(complete_request);
    if (!complete_outcome.IsSuccess()) {
      return make_error<std::monostate>(
          kUploadError,
          "Failed to complete multipart upload: " +
              std::string(complete_outcome.GetError().GetMessage()),
          "s3_storage");
    }

    return ok();
  }

private:
  std::string bucket_;
  std::unique_ptr<Aws::S3::S3Client> client_;
  bool use_path_style_{false};
};

#endif // PACS_WITH_AWS_SDK && !PACS_USE_MOCK_S3

// ============================================================================
// Construction
// ============================================================================

s3_storage::s3_storage(const cloud_storage_config &config)
    : config_(config),
#if defined(PACS_WITH_AWS_SDK) && !defined(PACS_USE_MOCK_S3)
      client_(std::make_unique<aws_s3_client>(config))
#else
      client_(std::make_unique<mock_s3_client>(config))
#endif
{
}

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
    upload_result =
        client_->multipart_upload(object_key, data, config_.part_size, callback);
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
  if (parse_result.is_err()) {
    return make_error<core::dicom_dataset>(
        kSerializationError,
        "Failed to parse DICOM data: " + parse_result.error().message,
        "s3_storage");
  }

  return parse_result.value().dataset();
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
    if (parse_result.is_err()) {
      continue; // Skip invalid DICOM files
    }

    const auto &dataset = parse_result.value().dataset();
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
    if (parse_result.is_err()) {
      continue;
    }

    const auto &dataset = parse_result.value().dataset();
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
  return client_->multipart_upload(key, data, config_.part_size, callback);
}

auto s3_storage::matches_query(const core::dicom_dataset &dataset,
                               const core::dicom_dataset &query) -> bool {
  // If query is empty, match all
  if (query.empty()) {
    return true;
  }

  // Check each query element
  for (const auto &[tag, element] : query) {
    auto query_value = element.as_string().unwrap_or("");
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

} // namespace kcenon::pacs::storage
