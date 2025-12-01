/**
 * @file file_storage.cpp
 * @brief Implementation of filesystem-based DICOM storage
 */

#include <pacs/storage/file_storage.hpp>

#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <set>

namespace pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;

namespace {

/// Error codes for file storage operations
constexpr int kMissingRequiredUid = -1;
constexpr int kDuplicateInstance = -2;
constexpr int kFileNotFound = -3;
constexpr int kFileWriteError = -4;
constexpr int kFileReadError = -5;
constexpr int kDirectoryCreateError = -6;
constexpr int kIntegrityError = -7;

/// Generate a unique temporary filename
auto generate_temp_filename(const std::filesystem::path& base)
    -> std::filesystem::path {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    auto temp_name = base.filename().string() + ".tmp." +
                     std::to_string(dist(gen));
    return base.parent_path() / temp_name;
}

}  // namespace

// ============================================================================
// Construction
// ============================================================================

file_storage::file_storage(const file_storage_config& config)
    : config_(config) {
    // Create root directory if configured
    if (config_.create_directories && !config_.root_path.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(config_.root_path, ec);
        // Ignore error - will be caught during store operations
    }

    // Rebuild index from existing files
    if (std::filesystem::exists(config_.root_path)) {
        (void)rebuild_index();
    }
}

// ============================================================================
// storage_interface Implementation
// ============================================================================

auto file_storage::store(const core::dicom_dataset& dataset) -> VoidResult {
    // Extract required UIDs
    auto study_uid = dataset.get_string(core::tags::study_instance_uid);
    auto series_uid = dataset.get_string(core::tags::series_instance_uid);
    auto sop_uid = dataset.get_string(core::tags::sop_instance_uid);

    if (study_uid.empty() || series_uid.empty() || sop_uid.empty()) {
        return make_error<std::monostate>(
            kMissingRequiredUid,
            "Missing required UID (Study, Series, or SOP Instance UID)",
            "file_storage");
    }

    // Build file path based on naming scheme
    std::filesystem::path file_path;
    switch (config_.naming) {
        case naming_scheme::uid_hierarchical:
            file_path = build_path(study_uid, series_uid, sop_uid);
            break;
        case naming_scheme::date_hierarchical: {
            auto study_date = dataset.get_string(core::tags::study_date);
            if (study_date.empty()) {
                // Use current date if study date not available
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                std::tm tm_buf{};
#ifdef _WIN32
                localtime_s(&tm_buf, &time);
#else
                localtime_r(&time, &tm_buf);
#endif
                char date_str[9];
                std::strftime(date_str, sizeof(date_str), "%Y%m%d", &tm_buf);
                study_date = date_str;
            }
            file_path = build_date_path(study_date, study_uid, sop_uid);
            break;
        }
        case naming_scheme::flat:
            file_path = config_.root_path /
                        (sanitize_uid(sop_uid) + config_.file_extension);
            break;
    }

    // Handle duplicate checking
    {
        std::shared_lock lock(mutex_);
        if (index_.contains(sop_uid)) {
            switch (config_.duplicate) {
                case duplicate_policy::reject:
                    return make_error<std::monostate>(
                        kDuplicateInstance,
                        "Instance already exists: " + sop_uid,
                        "file_storage");
                case duplicate_policy::ignore:
                    return ok();
                case duplicate_policy::replace:
                    // Continue to overwrite
                    break;
            }
        }
    }

    // Create directories if needed
    if (config_.create_directories) {
        std::error_code ec;
        std::filesystem::create_directories(file_path.parent_path(), ec);
        if (ec) {
            return make_error<std::monostate>(
                kDirectoryCreateError,
                "Failed to create directory: " + ec.message(),
                "file_storage");
        }
    }

    // Create DICOM file and write atomically
    auto dicom_file = core::dicom_file::create(
        dataset, encoding::transfer_syntax::explicit_vr_little_endian);

    // Write to temporary file first
    auto temp_path = generate_temp_filename(file_path);
    auto save_result = dicom_file.save(temp_path);

    if (!save_result.has_value()) {
        std::filesystem::remove(temp_path);
        return make_error<std::monostate>(
            kFileWriteError,
            "Failed to write DICOM file: " +
                core::to_string(save_result.error()),
            "file_storage");
    }

    // Atomic rename
    std::error_code ec;
    std::filesystem::rename(temp_path, file_path, ec);
    if (ec) {
        std::filesystem::remove(temp_path);
        return make_error<std::monostate>(
            kFileWriteError,
            "Failed to rename temp file: " + ec.message(),
            "file_storage");
    }

    // Update index
    {
        std::unique_lock lock(mutex_);
        index_[sop_uid] = file_path;
    }

    return ok();
}

auto file_storage::retrieve(std::string_view sop_instance_uid)
    -> Result<core::dicom_dataset> {
    std::filesystem::path file_path;

    {
        std::shared_lock lock(mutex_);
        auto it = index_.find(std::string{sop_instance_uid});
        if (it == index_.end()) {
            return make_error<core::dicom_dataset>(
                kFileNotFound,
                "Instance not found: " + std::string{sop_instance_uid},
                "file_storage");
        }
        file_path = it->second;
    }

    // Read DICOM file
    auto open_result = core::dicom_file::open(file_path);
    if (!open_result.has_value()) {
        return make_error<core::dicom_dataset>(
            kFileReadError,
            "Failed to read DICOM file: " +
                core::to_string(open_result.error()),
            "file_storage");
    }

    return open_result->dataset();
}

auto file_storage::remove(std::string_view sop_instance_uid) -> VoidResult {
    std::filesystem::path file_path;

    {
        std::unique_lock lock(mutex_);
        auto it = index_.find(std::string{sop_instance_uid});
        if (it == index_.end()) {
            // Not found is not an error for remove
            return ok();
        }
        file_path = it->second;
        index_.erase(it);
    }

    // Delete the file
    std::error_code ec;
    std::filesystem::remove(file_path, ec);
    // Ignore errors - file might have been deleted externally

    // Try to clean up empty parent directories
    auto parent = file_path.parent_path();
    while (parent != config_.root_path) {
        if (std::filesystem::is_empty(parent)) {
            std::filesystem::remove(parent, ec);
            parent = parent.parent_path();
        } else {
            break;
        }
    }

    return ok();
}

auto file_storage::exists(std::string_view sop_instance_uid) const -> bool {
    std::shared_lock lock(mutex_);
    return index_.contains(std::string{sop_instance_uid});
}

auto file_storage::find(const core::dicom_dataset& query)
    -> Result<std::vector<core::dicom_dataset>> {
    std::vector<core::dicom_dataset> results;

    std::vector<std::filesystem::path> paths_to_check;
    {
        std::shared_lock lock(mutex_);
        paths_to_check.reserve(index_.size());
        for (const auto& [uid, path] : index_) {
            paths_to_check.push_back(path);
        }
    }

    for (const auto& path : paths_to_check) {
        auto open_result = core::dicom_file::open(path);
        if (!open_result.has_value()) {
            continue;  // Skip files that can't be read
        }

        const auto& dataset = open_result->dataset();
        if (matches_query(dataset, query)) {
            results.push_back(dataset);
        }
    }

    return results;
}

auto file_storage::get_statistics() const -> storage_statistics {
    storage_statistics stats;

    std::set<std::string> studies;
    std::set<std::string> series;
    std::set<std::string> patients;

    std::vector<std::filesystem::path> paths;
    {
        std::shared_lock lock(mutex_);
        stats.total_instances = index_.size();
        paths.reserve(index_.size());
        for (const auto& [uid, path] : index_) {
            paths.push_back(path);
        }
    }

    for (const auto& path : paths) {
        std::error_code ec;
        stats.total_bytes += std::filesystem::file_size(path, ec);

        // Read file to get study/series/patient info
        auto open_result = core::dicom_file::open(path);
        if (open_result.has_value()) {
            const auto& ds = open_result->dataset();
            auto study_uid = ds.get_string(core::tags::study_instance_uid);
            auto series_uid = ds.get_string(core::tags::series_instance_uid);
            auto patient_id = ds.get_string(core::tags::patient_id);

            if (!study_uid.empty()) {
                studies.insert(study_uid);
            }
            if (!series_uid.empty()) {
                series.insert(series_uid);
            }
            if (!patient_id.empty()) {
                patients.insert(patient_id);
            }
        }
    }

    stats.studies_count = studies.size();
    stats.series_count = series.size();
    stats.patients_count = patients.size();

    return stats;
}

auto file_storage::verify_integrity() -> VoidResult {
    std::vector<std::pair<std::string, std::filesystem::path>> entries;
    {
        std::shared_lock lock(mutex_);
        entries.reserve(index_.size());
        for (const auto& [uid, path] : index_) {
            entries.emplace_back(uid, path);
        }
    }

    std::vector<std::string> invalid_entries;

    for (const auto& [uid, path] : entries) {
        if (!std::filesystem::exists(path)) {
            invalid_entries.push_back(uid + " (file missing)");
            continue;
        }

        auto open_result = core::dicom_file::open(path);
        if (!open_result.has_value()) {
            invalid_entries.push_back(uid + " (invalid DICOM)");
            continue;
        }

        // Verify UID matches
        auto file_uid =
            open_result->dataset().get_string(core::tags::sop_instance_uid);
        if (file_uid != uid) {
            invalid_entries.push_back(uid + " (UID mismatch)");
        }
    }

    if (!invalid_entries.empty()) {
        std::string message = "Integrity check failed for " +
                              std::to_string(invalid_entries.size()) +
                              " entries";
        return make_error<std::monostate>(kIntegrityError, message,
                                          "file_storage");
    }

    return ok();
}

// ============================================================================
// File-specific Operations
// ============================================================================

auto file_storage::get_file_path(std::string_view sop_instance_uid) const
    -> std::filesystem::path {
    std::shared_lock lock(mutex_);
    auto it = index_.find(std::string{sop_instance_uid});
    if (it != index_.end()) {
        return it->second;
    }
    return {};
}

auto file_storage::import_directory(const std::filesystem::path& source)
    -> VoidResult {
    if (!std::filesystem::exists(source)) {
        return make_error<std::monostate>(
            kFileNotFound,
            "Source directory does not exist: " + source.string(),
            "file_storage");
    }

    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(source, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        // Try to open as DICOM file
        auto open_result = core::dicom_file::open(entry.path());
        if (!open_result.has_value()) {
            continue;  // Not a DICOM file, skip
        }

        // Store the dataset
        auto store_result = store(open_result->dataset());
        if (store_result.is_err()) {
            // Log error but continue with other files
            // In production, this should be logged
        }
    }

    return ok();
}

auto file_storage::root_path() const -> const std::filesystem::path& {
    return config_.root_path;
}

auto file_storage::rebuild_index() -> VoidResult {
    std::unique_lock lock(mutex_);
    index_.clear();

    if (!std::filesystem::exists(config_.root_path)) {
        return ok();
    }

    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(config_.root_path, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        // Check file extension
        if (!config_.file_extension.empty() &&
            entry.path().extension() != config_.file_extension) {
            continue;
        }

        // Try to read as DICOM file
        auto open_result = core::dicom_file::open(entry.path());
        if (!open_result.has_value()) {
            continue;
        }

        auto sop_uid =
            open_result->dataset().get_string(core::tags::sop_instance_uid);
        if (!sop_uid.empty()) {
            index_[sop_uid] = entry.path();
        }
    }

    return ok();
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

auto file_storage::build_path(std::string_view study_uid,
                              std::string_view series_uid,
                              std::string_view sop_uid) const
    -> std::filesystem::path {
    return config_.root_path / sanitize_uid(study_uid) /
           sanitize_uid(series_uid) /
           (sanitize_uid(sop_uid) + config_.file_extension);
}

auto file_storage::build_date_path(std::string_view study_date,
                                   std::string_view study_uid,
                                   std::string_view sop_uid) const
    -> std::filesystem::path {
    // Parse date YYYYMMDD
    std::string year = "unknown";
    std::string month = "01";
    std::string day = "01";

    if (study_date.length() >= 8) {
        year = std::string{study_date.substr(0, 4)};
        month = std::string{study_date.substr(4, 2)};
        day = std::string{study_date.substr(6, 2)};
    }

    return config_.root_path / year / month / day / sanitize_uid(study_uid) /
           (sanitize_uid(sop_uid) + config_.file_extension);
}

void file_storage::update_index(const std::string& sop_uid,
                                const std::filesystem::path& path) {
    std::unique_lock lock(mutex_);
    index_[sop_uid] = path;
}

void file_storage::remove_from_index(const std::string& sop_uid) {
    std::unique_lock lock(mutex_);
    index_.erase(sop_uid);
}

auto file_storage::matches_query(const core::dicom_dataset& dataset,
                                 const core::dicom_dataset& query) -> bool {
    // If query is empty, match all
    if (query.empty()) {
        return true;
    }

    // Check each query element
    for (const auto& [tag, element] : query) {
        auto query_value = element.as_string();
        if (query_value.empty()) {
            continue;  // Empty value acts as wildcard
        }

        auto dataset_value = dataset.get_string(tag);

        // Support basic wildcard matching (* and ?)
        if (query_value.find('*') != std::string::npos ||
            query_value.find('?') != std::string::npos) {
            // Convert to regex-like matching
            std::string pattern = query_value;
            // Escape regex special characters except * and ?
            std::string escaped;
            for (char c : pattern) {
                if (c == '*') {
                    escaped += ".*";
                } else if (c == '?') {
                    escaped += ".";
                } else if (c == '.' || c == '[' || c == ']' || c == '(' ||
                           c == ')' || c == '+' || c == '^' || c == '$' ||
                           c == '|' || c == '\\') {
                    escaped += '\\';
                    escaped += c;
                } else {
                    escaped += c;
                }
            }

            // Simple pattern matching (without full regex for performance)
            // For now, just check if pattern starts with or ends with value
            if (query_value.front() == '*' && query_value.back() == '*') {
                // Contains
                auto inner =
                    query_value.substr(1, query_value.length() - 2);
                if (dataset_value.find(inner) == std::string::npos) {
                    return false;
                }
            } else if (query_value.front() == '*') {
                // Ends with
                auto suffix = query_value.substr(1);
                if (dataset_value.length() < suffix.length() ||
                    dataset_value.substr(dataset_value.length() -
                                         suffix.length()) != suffix) {
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

auto file_storage::sanitize_uid(std::string_view uid) -> std::string {
    std::string result;
    result.reserve(uid.length());

    for (char c : uid) {
        // UIDs contain digits and dots, which are safe for filesystems
        // Replace any other characters with underscore
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '.') {
            result += c;
        } else {
            result += '_';
        }
    }

    return result;
}

}  // namespace pacs::storage
