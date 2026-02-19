/**
 * @file azure_blob_storage.cpp
 * @brief Implementation of Azure Blob DICOM storage backend
 *
 * This file implements the azure_blob_storage class for cloud storage support.
 * Supports two Azure client backends selected at compile time:
 * - Mock client (default / PACS_USE_MOCK_AZURE): in-memory testing
 * - Azure SDK client (PACS_WITH_AZURE_SDK): production Blob operations
 */

#include <pacs/storage/azure_blob_storage.hpp>

#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>

#if defined(PACS_WITH_AZURE_SDK) && !defined(PACS_USE_MOCK_AZURE)
#include <azure/storage/blobs.hpp>
#endif

namespace pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;

namespace {

/// Error codes for Azure storage operations
constexpr int kMissingRequiredUid = -1;
constexpr int kBlobNotFound = -2;
constexpr int kUploadError = -3;
constexpr int kDownloadError = -4;
constexpr int kConnectionError = -6;
constexpr int kIntegrityError = -7;
constexpr int kSerializationError = -8;
constexpr int kTierChangeError = -9;

/**
 * @brief Generate a simple hash for content verification
 * @param data Data to hash
 * @return Hex string representation of hash
 */
auto compute_content_hash(const std::vector<std::uint8_t> &data)
    -> std::string {
  std::size_t hash = 0;
  for (const auto &byte : data) {
    hash = hash * 31 + byte;
  }

  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(16) << hash;
  return oss.str();
}

/**
 * @brief Generate a unique block ID for block blob upload
 * @param block_index Block index
 * @return Block ID string
 */
auto generate_block_id(std::size_t block_index) -> std::string {
  std::ostringstream oss;
  oss << "block_" << std::setfill('0') << std::setw(6) << block_index;
  return oss.str();
}

} // namespace

// ============================================================================
// Azure Client Interface
// ============================================================================

/**
 * @brief Abstract interface for Azure Blob client operations
 *
 * Both mock_azure_client and azure_sdk_client implement this interface,
 * allowing compile-time selection of the backend.
 */
class azure_blob_storage::azure_client_interface {
public:
  virtual ~azure_client_interface() = default;

  [[nodiscard]] virtual auto put_blob(const std::string &blob_name,
                                      const std::vector<std::uint8_t> &data)
      -> VoidResult = 0;

  [[nodiscard]] virtual auto get_blob(const std::string &blob_name)
      -> Result<std::vector<std::uint8_t>> = 0;

  [[nodiscard]] virtual auto delete_blob(const std::string &blob_name)
      -> VoidResult = 0;

  [[nodiscard]] virtual auto head_blob(const std::string &blob_name) const
      -> bool = 0;

  [[nodiscard]] virtual auto get_blob_size(const std::string &blob_name) const
      -> std::size_t = 0;

  [[nodiscard]] virtual auto get_blob_etag(const std::string &blob_name) const
      -> std::string = 0;

  [[nodiscard]] virtual auto get_blob_md5(const std::string &blob_name) const
      -> std::string = 0;

  [[nodiscard]] virtual auto list_blobs() const
      -> std::vector<std::string> = 0;

  [[nodiscard]] virtual auto is_connected() const -> bool = 0;

  [[nodiscard]] virtual auto stage_block(
      const std::string &blob_name, const std::string &block_id,
      const std::vector<std::uint8_t> &data) -> VoidResult = 0;

  [[nodiscard]] virtual auto commit_blocks(
      const std::string &blob_name,
      const std::vector<std::string> &block_ids) -> VoidResult = 0;

  [[nodiscard]] virtual auto set_tier(const std::string &blob_name,
                                      const std::string &tier)
      -> VoidResult = 0;
};

// ============================================================================
// Mock Azure Client Implementation
// ============================================================================

/**
 * @brief Mock Azure Blob client for testing without Azure SDK dependency
 *
 * Simulates Azure Blob operations using in-memory storage.
 * Always used when PACS_USE_MOCK_AZURE is defined or PACS_WITH_AZURE_SDK is
 * not set.
 */
class mock_azure_client
    : public azure_blob_storage::azure_client_interface {
public:
  explicit mock_azure_client(const azure_storage_config & /*config*/)
      : connected_(true) {}

  [[nodiscard]] auto put_blob(const std::string &blob_name,
                              const std::vector<std::uint8_t> &data)
      -> VoidResult override {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "Azure client not connected",
          "azure_blob_storage");
    }
    blob_data blob;
    blob.data = data;
    blob.etag = "\"" + compute_content_hash(data) + "\"";
    blob.content_md5 = compute_content_hash(data);
    blob.tier = "Hot";
    blobs_[blob_name] = std::move(blob);
    return ok();
  }

  [[nodiscard]] auto get_blob(const std::string &blob_name)
      -> Result<std::vector<std::uint8_t>> override {
    if (!connected_) {
      return make_error<std::vector<std::uint8_t>>(
          kConnectionError, "Azure client not connected",
          "azure_blob_storage");
    }
    auto it = blobs_.find(blob_name);
    if (it == blobs_.end()) {
      return make_error<std::vector<std::uint8_t>>(
          kBlobNotFound, "Blob not found: " + blob_name,
          "azure_blob_storage");
    }
    return it->second.data;
  }

  [[nodiscard]] auto delete_blob(const std::string &blob_name)
      -> VoidResult override {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "Azure client not connected",
          "azure_blob_storage");
    }
    blobs_.erase(blob_name);
    return ok();
  }

  [[nodiscard]] auto head_blob(const std::string &blob_name) const
      -> bool override {
    if (!connected_) {
      return false;
    }
    return blobs_.contains(blob_name);
  }

  [[nodiscard]] auto get_blob_size(const std::string &blob_name) const
      -> std::size_t override {
    auto it = blobs_.find(blob_name);
    if (it != blobs_.end()) {
      return it->second.data.size();
    }
    return 0;
  }

  [[nodiscard]] auto get_blob_etag(const std::string &blob_name) const
      -> std::string override {
    auto it = blobs_.find(blob_name);
    if (it != blobs_.end()) {
      return it->second.etag;
    }
    return {};
  }

  [[nodiscard]] auto get_blob_md5(const std::string &blob_name) const
      -> std::string override {
    auto it = blobs_.find(blob_name);
    if (it != blobs_.end()) {
      return it->second.content_md5;
    }
    return {};
  }

  [[nodiscard]] auto list_blobs() const
      -> std::vector<std::string> override {
    std::vector<std::string> names;
    names.reserve(blobs_.size());
    for (const auto &[name, data] : blobs_) {
      names.push_back(name);
    }
    return names;
  }

  [[nodiscard]] auto is_connected() const -> bool override {
    return connected_;
  }

  [[nodiscard]] auto stage_block(const std::string &blob_name,
                                 const std::string &block_id,
                                 const std::vector<std::uint8_t> &data)
      -> VoidResult override {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "Azure client not connected",
          "azure_blob_storage");
    }
    staged_blocks_[blob_name][block_id] = data;
    return ok();
  }

  [[nodiscard]] auto commit_blocks(const std::string &blob_name,
                                   const std::vector<std::string> &block_ids)
      -> VoidResult override {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "Azure client not connected",
          "azure_blob_storage");
    }

    auto it = staged_blocks_.find(blob_name);
    if (it == staged_blocks_.end()) {
      return make_error<std::monostate>(
          kUploadError, "No staged blocks found for: " + blob_name,
          "azure_blob_storage");
    }

    std::vector<std::uint8_t> combined_data;
    for (const auto &block_id : block_ids) {
      auto block_it = it->second.find(block_id);
      if (block_it == it->second.end()) {
        return make_error<std::monostate>(
            kUploadError, "Block not found: " + block_id,
            "azure_blob_storage");
      }
      combined_data.insert(combined_data.end(), block_it->second.begin(),
                           block_it->second.end());
    }

    blob_data blob;
    blob.data = std::move(combined_data);
    blob.etag = "\"" + compute_content_hash(blob.data) + "\"";
    blob.content_md5 = compute_content_hash(blob.data);
    blob.tier = "Hot";
    blobs_[blob_name] = std::move(blob);

    staged_blocks_.erase(blob_name);
    return ok();
  }

  [[nodiscard]] auto set_tier(const std::string &blob_name,
                              const std::string &tier)
      -> VoidResult override {
    if (!connected_) {
      return make_error<std::monostate>(
          kConnectionError, "Azure client not connected",
          "azure_blob_storage");
    }
    auto it = blobs_.find(blob_name);
    if (it == blobs_.end()) {
      return make_error<std::monostate>(
          kBlobNotFound, "Blob not found: " + blob_name,
          "azure_blob_storage");
    }
    it->second.tier = tier;
    return ok();
  }

private:
  struct blob_data {
    std::vector<std::uint8_t> data;
    std::string etag;
    std::string content_md5;
    std::string tier;
  };

  std::unordered_map<std::string, blob_data> blobs_;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::vector<std::uint8_t>>>
      staged_blocks_;
  bool connected_;
};

// ============================================================================
// Azure SDK Client Implementation
// ============================================================================

#if defined(PACS_WITH_AZURE_SDK) && !defined(PACS_USE_MOCK_AZURE)

/**
 * @brief Production Azure Blob client using Azure SDK for C++
 *
 * Wraps Azure::Storage::Blobs::BlobContainerClient for real Azure operations.
 * Enabled when PACS_WITH_AZURE_SDK is defined and PACS_USE_MOCK_AZURE is not.
 */
class azure_sdk_client
    : public azure_blob_storage::azure_client_interface {
public:
  explicit azure_sdk_client(const azure_storage_config &config) {
    if (config.endpoint_url.has_value()) {
      // Azurite or custom endpoint
      container_client_ =
          std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(
              config.endpoint_url.value() + "/" + config.container_name);
    } else {
      container_client_ =
          std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(
              Azure::Storage::Blobs::BlobContainerClient::
                  CreateFromConnectionString(config.connection_string,
                                             config.container_name));
    }
  }

  [[nodiscard]] auto put_blob(const std::string &blob_name,
                              const std::vector<std::uint8_t> &data)
      -> VoidResult override {
    try {
      auto blob_client = container_client_->GetBlockBlobClient(blob_name);
      Azure::Core::IO::MemoryBodyStream stream(data);
      blob_client.Upload(stream);
      return ok();
    } catch (const Azure::Storage::StorageException &e) {
      return make_error<std::monostate>(
          kUploadError,
          "Azure PutBlob failed: " + std::string(e.what()),
          "azure_blob_storage");
    }
  }

  [[nodiscard]] auto get_blob(const std::string &blob_name)
      -> Result<std::vector<std::uint8_t>> override {
    try {
      auto blob_client = container_client_->GetBlobClient(blob_name);
      auto response = blob_client.Download();
      auto &body = response.Value.BodyStream;

      std::vector<std::uint8_t> result(
          static_cast<std::size_t>(body->Length()));
      body->ReadToCount(result.data(), result.size());
      return result;
    } catch (const Azure::Storage::StorageException &e) {
      if (e.StatusCode == Azure::Core::Http::HttpStatusCode::NotFound) {
        return make_error<std::vector<std::uint8_t>>(
            kBlobNotFound, "Blob not found: " + blob_name,
            "azure_blob_storage");
      }
      return make_error<std::vector<std::uint8_t>>(
          kDownloadError,
          "Azure GetBlob failed: " + std::string(e.what()),
          "azure_blob_storage");
    }
  }

  [[nodiscard]] auto delete_blob(const std::string &blob_name)
      -> VoidResult override {
    try {
      auto blob_client = container_client_->GetBlobClient(blob_name);
      blob_client.Delete();
      return ok();
    } catch (const Azure::Storage::StorageException &e) {
      return make_error<std::monostate>(
          kUploadError,
          "Azure DeleteBlob failed: " + std::string(e.what()),
          "azure_blob_storage");
    }
  }

  [[nodiscard]] auto head_blob(const std::string &blob_name) const
      -> bool override {
    try {
      auto blob_client = container_client_->GetBlobClient(blob_name);
      blob_client.GetProperties();
      return true;
    } catch (const Azure::Storage::StorageException &) {
      return false;
    }
  }

  [[nodiscard]] auto get_blob_size(const std::string &blob_name) const
      -> std::size_t override {
    try {
      auto blob_client = container_client_->GetBlobClient(blob_name);
      auto props = blob_client.GetProperties();
      return static_cast<std::size_t>(props.Value.BlobSize);
    } catch (const Azure::Storage::StorageException &) {
      return 0;
    }
  }

  [[nodiscard]] auto get_blob_etag(const std::string &blob_name) const
      -> std::string override {
    try {
      auto blob_client = container_client_->GetBlobClient(blob_name);
      auto props = blob_client.GetProperties();
      return props.Value.ETag.ToString();
    } catch (const Azure::Storage::StorageException &) {
      return {};
    }
  }

  [[nodiscard]] auto get_blob_md5(const std::string &blob_name) const
      -> std::string override {
    try {
      auto blob_client = container_client_->GetBlobClient(blob_name);
      auto props = blob_client.GetProperties();
      if (props.Value.HttpHeaders.ContentHash.Value.empty()) {
        return {};
      }
      // Convert hash bytes to hex string
      std::ostringstream oss;
      for (auto byte : props.Value.HttpHeaders.ContentHash.Value) {
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(byte);
      }
      return oss.str();
    } catch (const Azure::Storage::StorageException &) {
      return {};
    }
  }

  [[nodiscard]] auto list_blobs() const
      -> std::vector<std::string> override {
    std::vector<std::string> names;
    try {
      for (auto page = container_client_->ListBlobs(); page.HasPage();
           page.MoveToNextPage()) {
        for (const auto &blob : page.Blobs) {
          names.push_back(blob.Name);
        }
      }
    } catch (const Azure::Storage::StorageException &) {
      // Return whatever was collected
    }
    return names;
  }

  [[nodiscard]] auto is_connected() const -> bool override {
    return container_client_ != nullptr;
  }

  [[nodiscard]] auto stage_block(const std::string &blob_name,
                                 const std::string &block_id,
                                 const std::vector<std::uint8_t> &data)
      -> VoidResult override {
    try {
      auto blob_client = container_client_->GetBlockBlobClient(blob_name);
      Azure::Core::IO::MemoryBodyStream stream(data);
      blob_client.StageBlock(block_id, stream);
      return ok();
    } catch (const Azure::Storage::StorageException &e) {
      return make_error<std::monostate>(
          kUploadError,
          "Azure StageBlock failed: " + std::string(e.what()),
          "azure_blob_storage");
    }
  }

  [[nodiscard]] auto commit_blocks(const std::string &blob_name,
                                   const std::vector<std::string> &block_ids)
      -> VoidResult override {
    try {
      auto blob_client = container_client_->GetBlockBlobClient(blob_name);
      blob_client.CommitBlockList(block_ids);
      return ok();
    } catch (const Azure::Storage::StorageException &e) {
      return make_error<std::monostate>(
          kUploadError,
          "Azure CommitBlockList failed: " + std::string(e.what()),
          "azure_blob_storage");
    }
  }

  [[nodiscard]] auto set_tier(const std::string &blob_name,
                              const std::string &tier)
      -> VoidResult override {
    try {
      auto blob_client = container_client_->GetBlobClient(blob_name);
      Azure::Storage::Blobs::Models::AccessTier access_tier;
      if (tier == "Hot") {
        access_tier = Azure::Storage::Blobs::Models::AccessTier::Hot;
      } else if (tier == "Cool") {
        access_tier = Azure::Storage::Blobs::Models::AccessTier::Cool;
      } else if (tier == "Archive") {
        access_tier = Azure::Storage::Blobs::Models::AccessTier::Archive;
      } else {
        access_tier = Azure::Storage::Blobs::Models::AccessTier::Hot;
      }
      blob_client.SetAccessTier(access_tier);
      return ok();
    } catch (const Azure::Storage::StorageException &e) {
      return make_error<std::monostate>(
          kTierChangeError,
          "Azure SetAccessTier failed: " + std::string(e.what()),
          "azure_blob_storage");
    }
  }

private:
  std::unique_ptr<Azure::Storage::Blobs::BlobContainerClient>
      container_client_;
};

#endif // PACS_WITH_AZURE_SDK && !PACS_USE_MOCK_AZURE

// ============================================================================
// Construction
// ============================================================================

azure_blob_storage::azure_blob_storage(const azure_storage_config &config)
    : config_(config),
#if defined(PACS_WITH_AZURE_SDK) && !defined(PACS_USE_MOCK_AZURE)
      client_(std::make_unique<azure_sdk_client>(config))
#else
      client_(std::make_unique<mock_azure_client>(config))
#endif
{
}

azure_blob_storage::~azure_blob_storage() = default;

// ============================================================================
// storage_interface Implementation
// ============================================================================

auto azure_blob_storage::store(const core::dicom_dataset &dataset)
    -> VoidResult {
  return store_with_progress(dataset, nullptr);
}

auto azure_blob_storage::store_with_progress(
    const core::dicom_dataset &dataset, azure_progress_callback callback)
    -> VoidResult {
  // Extract required UIDs
  auto study_uid = dataset.get_string(core::tags::study_instance_uid);
  auto series_uid = dataset.get_string(core::tags::series_instance_uid);
  auto sop_uid = dataset.get_string(core::tags::sop_instance_uid);

  if (study_uid.empty() || series_uid.empty() || sop_uid.empty()) {
    return make_error<std::monostate>(
        kMissingRequiredUid,
        "Missing required UID (Study, Series, or SOP Instance UID)",
        "azure_blob_storage");
  }

  // Build blob name
  auto blob_name = build_blob_name(study_uid, series_uid, sop_uid);

  // Create DICOM file and serialize to bytes
  auto dicom_file = core::dicom_file::create(
      dataset, encoding::transfer_syntax::explicit_vr_little_endian);

  auto data = dicom_file.to_bytes();
  if (data.empty()) {
    return make_error<std::monostate>(kSerializationError,
                                      "Failed to serialize DICOM dataset",
                                      "azure_blob_storage");
  }

  // Report initial progress
  if (callback && !callback(0, data.size())) {
    return make_error<std::monostate>(kUploadError, "Upload cancelled by user",
                                      "azure_blob_storage");
  }

  // Upload to Azure (use block blob for large files)
  VoidResult upload_result = ok();
  if (data.size() > config_.block_upload_threshold) {
    upload_result = upload_block_blob(blob_name, data, callback);
  } else {
    upload_result = client_->put_blob(blob_name, data);

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
    azure_blob_info info;
    info.blob_name = blob_name;
    info.sop_instance_uid = sop_uid;
    info.study_instance_uid = study_uid;
    info.series_instance_uid = series_uid;
    info.size_bytes = data.size();
    info.etag = client_->get_blob_etag(blob_name);
    info.content_md5 = client_->get_blob_md5(blob_name);
    index_[sop_uid] = std::move(info);
  }

  return ok();
}

auto azure_blob_storage::retrieve(std::string_view sop_instance_uid)
    -> Result<core::dicom_dataset> {
  return retrieve_with_progress(sop_instance_uid, nullptr);
}

auto azure_blob_storage::retrieve_with_progress(
    std::string_view sop_instance_uid, azure_progress_callback callback)
    -> Result<core::dicom_dataset> {
  std::string blob_name;

  {
    std::shared_lock lock(mutex_);
    auto it = index_.find(std::string{sop_instance_uid});
    if (it == index_.end()) {
      return make_error<core::dicom_dataset>(
          kBlobNotFound,
          "Instance not found: " + std::string{sop_instance_uid},
          "azure_blob_storage");
    }
    blob_name = it->second.blob_name;
  }

  // Download from Azure
  auto download_result = client_->get_blob(blob_name);
  if (download_result.is_err()) {
    return make_error<core::dicom_dataset>(kDownloadError,
                                           "Failed to download from Azure",
                                           "azure_blob_storage");
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
        "azure_blob_storage");
  }

  return parse_result.value().dataset();
}

auto azure_blob_storage::remove(std::string_view sop_instance_uid)
    -> VoidResult {
  std::string blob_name;

  {
    std::unique_lock lock(mutex_);
    auto it = index_.find(std::string{sop_instance_uid});
    if (it == index_.end()) {
      // Not found is not an error for remove
      return ok();
    }
    blob_name = it->second.blob_name;
    index_.erase(it);
  }

  // Delete from Azure
  auto delete_result = client_->delete_blob(blob_name);
  // Ignore delete errors - blob might have been deleted externally

  return ok();
}

auto azure_blob_storage::exists(std::string_view sop_instance_uid) const
    -> bool {
  std::shared_lock lock(mutex_);
  return index_.contains(std::string{sop_instance_uid});
}

auto azure_blob_storage::find(const core::dicom_dataset &query)
    -> Result<std::vector<core::dicom_dataset>> {
  std::vector<core::dicom_dataset> results;

  std::vector<std::string> blobs_to_retrieve;
  {
    std::shared_lock lock(mutex_);
    blobs_to_retrieve.reserve(index_.size());
    for (const auto &[uid, info] : index_) {
      blobs_to_retrieve.push_back(info.blob_name);
    }
  }

  for (const auto &blob_name : blobs_to_retrieve) {
    auto download_result = client_->get_blob(blob_name);
    if (download_result.is_err()) {
      continue; // Skip blobs that can't be downloaded
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

auto azure_blob_storage::get_statistics() const -> storage_statistics {
  storage_statistics stats;

  std::set<std::string> studies;
  std::set<std::string> series;

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

auto azure_blob_storage::verify_integrity() -> VoidResult {
  std::vector<std::pair<std::string, std::string>> entries;
  {
    std::shared_lock lock(mutex_);
    entries.reserve(index_.size());
    for (const auto &[uid, info] : index_) {
      entries.emplace_back(uid, info.blob_name);
    }
  }

  std::vector<std::string> invalid_entries;

  for (const auto &[uid, blob_name] : entries) {
    if (!client_->head_blob(blob_name)) {
      invalid_entries.push_back(uid + " (blob missing)");
    }
  }

  if (!invalid_entries.empty()) {
    std::string message = "Integrity check failed for " +
                          std::to_string(invalid_entries.size()) + " entries";
    return make_error<std::monostate>(kIntegrityError, message,
                                      "azure_blob_storage");
  }

  return ok();
}

// ============================================================================
// Azure-specific Operations
// ============================================================================

auto azure_blob_storage::get_blob_name(
    std::string_view sop_instance_uid) const -> std::string {
  std::shared_lock lock(mutex_);
  auto it = index_.find(std::string{sop_instance_uid});
  if (it != index_.end()) {
    return it->second.blob_name;
  }
  return {};
}

auto azure_blob_storage::container_name() const -> const std::string & {
  return config_.container_name;
}

auto azure_blob_storage::rebuild_index() -> VoidResult {
  std::unique_lock lock(mutex_);
  index_.clear();

  // List all blobs from Azure
  auto blob_names = client_->list_blobs();

  for (const auto &blob_name : blob_names) {
    // Download and parse each blob to rebuild index
    auto download_result = client_->get_blob(blob_name);
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
      azure_blob_info info;
      info.blob_name = blob_name;
      info.sop_instance_uid = sop_uid;
      info.study_instance_uid = study_uid;
      info.series_instance_uid = series_uid;
      info.size_bytes = client_->get_blob_size(blob_name);
      info.etag = client_->get_blob_etag(blob_name);
      info.content_md5 = client_->get_blob_md5(blob_name);
      index_[sop_uid] = std::move(info);
    }
  }

  return ok();
}

auto azure_blob_storage::is_connected() const -> bool {
  return client_ && client_->is_connected();
}

auto azure_blob_storage::set_access_tier(std::string_view sop_instance_uid,
                                         std::string_view tier) -> VoidResult {
  std::string blob_name;

  {
    std::shared_lock lock(mutex_);
    auto it = index_.find(std::string{sop_instance_uid});
    if (it == index_.end()) {
      return make_error<std::monostate>(
          kBlobNotFound,
          "Instance not found: " + std::string{sop_instance_uid},
          "azure_blob_storage");
    }
    blob_name = it->second.blob_name;
  }

  auto result = client_->set_tier(blob_name, std::string{tier});
  if (result.is_err()) {
    return make_error<std::monostate>(
        kTierChangeError,
        "Failed to change access tier: " + std::string{sop_instance_uid},
        "azure_blob_storage");
  }

  return ok();
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

auto azure_blob_storage::build_blob_name(std::string_view study_uid,
                                         std::string_view series_uid,
                                         std::string_view sop_uid) const
    -> std::string {
  std::ostringstream oss;
  oss << sanitize_uid(study_uid) << "/" << sanitize_uid(series_uid) << "/"
      << sanitize_uid(sop_uid) << ".dcm";
  return oss.str();
}

auto azure_blob_storage::sanitize_uid(std::string_view uid) -> std::string {
  std::string result;
  result.reserve(uid.length());

  for (char c : uid) {
    // UIDs contain digits and dots, which are safe for blob names
    // Replace any other characters with underscore
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '.') {
      result += c;
    } else {
      result += '_';
    }
  }

  return result;
}

auto azure_blob_storage::upload_block_blob(
    const std::string &blob_name, const std::vector<std::uint8_t> &data,
    azure_progress_callback callback) -> VoidResult {
  std::size_t total_bytes = data.size();
  std::size_t bytes_uploaded = 0;
  std::vector<std::string> block_ids;

  // Stage blocks
  std::size_t block_index = 0;
  while (bytes_uploaded < total_bytes) {
    std::size_t block_size =
        (std::min)(config_.block_size, total_bytes - bytes_uploaded);

    auto block_id = generate_block_id(block_index);
    block_ids.push_back(block_id);

    std::vector<std::uint8_t> block_data(data.begin() + bytes_uploaded,
                                         data.begin() + bytes_uploaded +
                                             block_size);

    auto stage_result = client_->stage_block(blob_name, block_id, block_data);
    if (stage_result.is_err()) {
      return make_error<std::monostate>(kUploadError,
                                        "Failed to stage block: " + block_id,
                                        "azure_blob_storage");
    }

    bytes_uploaded += block_size;
    block_index++;

    if (callback && !callback(bytes_uploaded, total_bytes)) {
      return make_error<std::monostate>(kUploadError,
                                        "Upload cancelled by user",
                                        "azure_blob_storage");
    }
  }

  // Commit blocks
  auto commit_result = client_->commit_blocks(blob_name, block_ids);
  if (commit_result.is_err()) {
    return make_error<std::monostate>(kUploadError, "Failed to commit blocks",
                                      "azure_blob_storage");
  }

  return ok();
}

auto azure_blob_storage::matches_query(const core::dicom_dataset &dataset,
                                       const core::dicom_dataset &query)
    -> bool {
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

} // namespace pacs::storage
