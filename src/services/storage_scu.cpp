/**
 * @file storage_scu.cpp
 * @brief Implementation of the Storage SCU service
 */

#include "pacs/services/storage_scu.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

#include <algorithm>
#include <array>

namespace pacs::services {

// =============================================================================
// DICOM Tag Constants (for dataset extraction)
// =============================================================================

namespace {

/// SOP Class UID tag (0008,0016)
constexpr core::dicom_tag tag_sop_class_uid{0x0008, 0x0016};

/// SOP Instance UID tag (0008,0018)
constexpr core::dicom_tag tag_sop_instance_uid{0x0008, 0x0018};

/// Common DICOM file extensions
constexpr std::array<std::string_view, 4> dicom_extensions = {
    ".dcm", ".DCM", ".dicom", ".DICOM"
};

/// Check if a file has a DICOM extension
[[nodiscard]] bool is_dicom_file(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext.empty()) {
        // Files without extension might still be DICOM
        return true;
    }
    return std::find(dicom_extensions.begin(), dicom_extensions.end(), ext)
           != dicom_extensions.end();
}

}  // namespace

// =============================================================================
// Construction
// =============================================================================

storage_scu::storage_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

storage_scu::storage_scu(const storage_scu_config& config,
                         std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()), config_(config) {}

// =============================================================================
// Single Image Operations
// =============================================================================

network::Result<store_result> storage_scu::store(
    network::association& assoc,
    const core::dicom_dataset& dataset) {

    return store_impl(assoc, dataset, next_message_id());
}

network::Result<store_result> storage_scu::store_impl(
    network::association& assoc,
    const core::dicom_dataset& dataset,
    uint16_t message_id) {

    using namespace network::dimse;

    // Extract SOP Class UID from dataset
    const auto sop_class_uid = dataset.get_string(tag_sop_class_uid);
    if (sop_class_uid.empty()) {
        return pacs::pacs_error<store_result>(
            pacs::error_codes::store_missing_sop_class_uid,
            "Missing SOP Class UID in dataset");
    }

    // Extract SOP Instance UID from dataset
    const auto sop_instance_uid = dataset.get_string(tag_sop_instance_uid);
    if (sop_instance_uid.empty()) {
        return pacs::pacs_error<store_result>(
            pacs::error_codes::store_missing_sop_instance_uid,
            "Missing SOP Instance UID in dataset");
    }

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<store_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get accepted presentation context for this SOP class
    auto context_id = assoc.accepted_context_id(sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<store_result>(
            pacs::error_codes::store_no_accepted_context,
            "No accepted presentation context for SOP Class: " + sop_class_uid);
    }

    // Build C-STORE-RQ message
    auto request = make_c_store_rq(
        message_id,
        sop_class_uid,
        sop_instance_uid,
        config_.default_priority
    );

    // Attach the dataset
    request.set_dataset(dataset);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        failures_.fetch_add(1, std::memory_order_relaxed);
        return send_result.error();
    }

    // Receive the response
    auto recv_result = assoc.receive_dimse(config_.response_timeout);
    if (recv_result.is_err()) {
        failures_.fetch_add(1, std::memory_order_relaxed);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    // Verify it's a C-STORE response
    if (response.command() != command_field::c_store_rsp) {
        failures_.fetch_add(1, std::memory_order_relaxed);
        return pacs::pacs_error<store_result>(
            pacs::error_codes::store_unexpected_command,
            "Expected C-STORE-RSP but received " +
            std::string(to_string(response.command())));
    }

    // Build result from response
    store_result result;
    result.sop_instance_uid = sop_instance_uid;
    result.status = static_cast<uint16_t>(response.status());

    // Extract error comment if present
    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    // Update statistics
    if (result.is_success() || result.is_warning()) {
        images_sent_.fetch_add(1, std::memory_order_relaxed);
        // Estimate dataset size
        bytes_sent_.fetch_add(
            dataset.size() * sizeof(uint32_t),
            std::memory_order_relaxed
        );
    } else {
        failures_.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

// =============================================================================
// Batch Operations
// =============================================================================

std::vector<store_result> storage_scu::store_batch(
    network::association& assoc,
    const std::vector<core::dicom_dataset>& datasets,
    store_progress_callback progress_callback) {

    std::vector<store_result> results;
    results.reserve(datasets.size());

    const size_t total = datasets.size();
    size_t completed = 0;

    for (const auto& dataset : datasets) {
        auto result = store(assoc, dataset);

        if (result.is_ok()) {
            results.push_back(std::move(result.value()));
        } else {
            // Create a failure result
            store_result failure_result;
            failure_result.sop_instance_uid = dataset.get_string(tag_sop_instance_uid);
            failure_result.status = static_cast<uint16_t>(storage_status::cannot_understand);
            failure_result.error_comment = result.error().message;
            results.push_back(std::move(failure_result));

            // Stop on error if configured
            if (!config_.continue_on_error) {
                break;
            }
        }

        ++completed;

        // Report progress
        if (progress_callback) {
            progress_callback(completed, total);
        }
    }

    return results;
}

// =============================================================================
// File-based Operations
// =============================================================================

network::Result<store_result> storage_scu::store_file(
    network::association& /* assoc */,
    const std::filesystem::path& file_path) {

    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        return pacs::pacs_error<store_result>(
            pacs::error_codes::file_not_found_service,
            "File not found: " + file_path.string());
    }

    // Check if it's a regular file
    if (!std::filesystem::is_regular_file(file_path)) {
        return pacs::pacs_error<store_result>(
            pacs::error_codes::not_a_regular_file,
            "Not a regular file: " + file_path.string());
    }

    // TODO: Implement DICOM file parsing
    // For now, return an error indicating file parsing is not yet implemented
    // This would require the core module's file parser
    return pacs::pacs_error<store_result>(
        pacs::error_codes::file_parsing_not_implemented,
        "DICOM file parsing not yet implemented");
}

std::vector<store_result> storage_scu::store_directory(
    network::association& assoc,
    const std::filesystem::path& directory,
    bool recursive,
    store_progress_callback progress_callback) {

    std::vector<store_result> results;

    // Collect all DICOM files
    auto files = collect_dicom_files(directory, recursive);
    if (files.empty()) {
        return results;
    }

    const size_t total = files.size();
    size_t completed = 0;

    for (const auto& file_path : files) {
        auto result = store_file(assoc, file_path);

        if (result.is_ok()) {
            results.push_back(std::move(result.value()));
        } else {
            // Create a failure result
            store_result failure_result;
            failure_result.sop_instance_uid = file_path.filename().string();
            failure_result.status = static_cast<uint16_t>(storage_status::cannot_understand);
            failure_result.error_comment = result.error().message;
            results.push_back(std::move(failure_result));

            // Stop on error if configured
            if (!config_.continue_on_error) {
                break;
            }
        }

        ++completed;

        // Report progress
        if (progress_callback) {
            progress_callback(completed, total);
        }
    }

    return results;
}

std::vector<std::filesystem::path> storage_scu::collect_dicom_files(
    const std::filesystem::path& directory,
    bool recursive) const {

    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(directory) ||
        !std::filesystem::is_directory(directory)) {
        return files;
    }

    if (recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && is_dicom_file(entry.path())) {
                files.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry :
             std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && is_dicom_file(entry.path())) {
                files.push_back(entry.path());
            }
        }
    }

    // Sort files for deterministic order
    std::sort(files.begin(), files.end());

    return files;
}

// =============================================================================
// Statistics
// =============================================================================

size_t storage_scu::images_sent() const noexcept {
    return images_sent_.load(std::memory_order_relaxed);
}

size_t storage_scu::failures() const noexcept {
    return failures_.load(std::memory_order_relaxed);
}

size_t storage_scu::bytes_sent() const noexcept {
    return bytes_sent_.load(std::memory_order_relaxed);
}

void storage_scu::reset_statistics() noexcept {
    images_sent_.store(0, std::memory_order_relaxed);
    failures_.store(0, std::memory_order_relaxed);
    bytes_sent_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Helpers
// =============================================================================

uint16_t storage_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    // Wrap around at 0xFFFF, skip 0 (reserved)
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

}  // namespace pacs::services
