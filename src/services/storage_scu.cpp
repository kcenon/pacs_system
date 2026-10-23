// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_scu.cpp
 * @brief Implementation of the Storage SCU service
 */

#include "kcenon/pacs/services/storage_scu.h"
#include "kcenon/pacs/services/storage_scp.h"
#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/dimse/command_field.h"
#include "kcenon/pacs/network/dimse/status_codes.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

namespace kcenon::pacs::services {

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

/// Check if the DIMSE layer can encode a dataset in a transfer syntax; it
/// writes datasets only in Implicit or Explicit VR Little Endian
[[nodiscard]] bool is_dimse_encodable(std::string_view transfer_syntax_uid) {
    return transfer_syntax_uid ==
               encoding::transfer_syntax::implicit_vr_little_endian.uid() ||
           transfer_syntax_uid ==
               encoding::transfer_syntax::explicit_vr_little_endian.uid();
}

/// Find an accepted presentation context for a SOP Class that carries a
/// dataset encoded in @p transfer_syntax without converting it
[[nodiscard]] std::optional<uint8_t> find_matching_context(
    const network::association& assoc,
    std::string_view sop_class_uid,
    const encoding::transfer_syntax& transfer_syntax) {

    std::optional<uint8_t> native_context;
    for (const auto& ctx : assoc.accepted_contexts()) {
        if (!ctx.is_accepted() || ctx.abstract_syntax != sop_class_uid) {
            continue;
        }
        if (transfer_syntax.is_encapsulated()) {
            // Encapsulated Pixel Data is sent as it is, so only a context
            // that accepted the same transfer syntax can carry it
            if (ctx.transfer_syntax == transfer_syntax.uid()) {
                return ctx.id;
            }
        } else if (is_dimse_encodable(ctx.transfer_syntax)) {
            // The DIMSE layer encodes a native dataset in the context's VR
            // encoding; prefer the dataset's own syntax
            if (ctx.transfer_syntax == transfer_syntax.uid()) {
                return ctx.id;
            }
            if (!native_context) {
                native_context = ctx.id;
            }
        }
    }
    return native_context;
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

    // A dataset without a known transfer syntax is native data
    return store_impl(assoc, dataset,
                      encoding::transfer_syntax::explicit_vr_little_endian,
                      next_message_id());
}

network::Result<store_result> storage_scu::store(
    network::association& assoc,
    const core::dicom_dataset& dataset,
    const encoding::transfer_syntax& transfer_syntax) {

    return store_impl(assoc, dataset, transfer_syntax, next_message_id());
}

network::Result<store_result> storage_scu::store_impl(
    network::association& assoc,
    const core::dicom_dataset& dataset,
    const encoding::transfer_syntax& transfer_syntax,
    uint16_t message_id) {

    using namespace network::dimse;

    // Extract SOP Class UID from dataset
    const auto sop_class_uid = dataset.get_string(tag_sop_class_uid);
    if (sop_class_uid.empty()) {
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::store_missing_sop_class_uid,
            "Missing SOP Class UID in dataset");
    }

    // Extract SOP Instance UID from dataset
    const auto sop_instance_uid = dataset.get_string(tag_sop_instance_uid);
    if (sop_instance_uid.empty()) {
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::store_missing_sop_instance_uid,
            "Missing SOP Instance UID in dataset");
    }

    // Verify association is established
    if (!assoc.is_established()) {
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::association_not_established,
            "Association not established");
    }

    if (!transfer_syntax.is_valid()) {
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::unsupported_transfer_syntax,
            "Unknown transfer syntax: " + std::string(transfer_syntax.uid()));
    }

    // Get an accepted presentation context that carries the dataset as it is
    auto context_id = find_matching_context(assoc, sop_class_uid, transfer_syntax);
    if (!context_id) {
        if (!assoc.has_accepted_context(sop_class_uid)) {
            return kcenon::pacs::pacs_error<store_result>(
                kcenon::pacs::error_codes::store_no_accepted_context,
                "No accepted presentation context for SOP Class: " + sop_class_uid);
        }
        // Sending on another context would label the Pixel Data with a
        // transfer syntax it is not encoded in
        const auto error_msg =
            "No accepted presentation context for SOP Class " + sop_class_uid +
            " carries " + std::string(transfer_syntax.name()) + " (" +
            std::string(transfer_syntax.uid()) + "), and storage_scu does not "
            "transcode Pixel Data; SOP Instance " + sop_instance_uid +
            " was not sent";
        logger_->error(error_msg);
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::store_no_accepted_context, error_msg);
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
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::store_unexpected_command,
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
    network::association& assoc,
    const std::filesystem::path& file_path) {

    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::file_not_found_service,
            "File not found: " + file_path.string());
    }

    // Check if it's a regular file
    if (!std::filesystem::is_regular_file(file_path)) {
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::not_a_regular_file,
            "Not a regular file: " + file_path.string());
    }

    // Parse the DICOM file
    auto file_result = core::dicom_file::open(file_path);
    if (file_result.is_err()) {
        const auto error_msg = "Failed to parse DICOM file: " +
                               file_path.string() + ": " +
                               file_result.error().message;
        logger_->error(error_msg);
        return kcenon::pacs::pacs_error<store_result>(
            kcenon::pacs::error_codes::file_parse_failed, error_msg);
    }

    // Send the dataset in the file's transfer syntax: encapsulated Pixel Data
    // stays as it is, so it needs a context that accepted that syntax
    const auto& file = file_result.value();
    return store(assoc, file.dataset(), file.transfer_syntax());
}

std::vector<store_result> storage_scu::store_files(
    network::association& assoc,
    const std::vector<std::filesystem::path>& file_paths,
    store_progress_callback progress_callback) {

    std::vector<store_result> results;
    results.reserve(file_paths.size());

    const size_t total = file_paths.size();
    size_t completed = 0;

    for (const auto& file_path : file_paths) {
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

std::vector<store_result> storage_scu::store_directory(
    network::association& assoc,
    const std::filesystem::path& directory,
    bool recursive,
    store_progress_callback progress_callback) {

    // Collect all DICOM files from the directory
    auto files = collect_dicom_files(directory, recursive);

    // Delegate to store_files for the actual processing
    return store_files(assoc, files, progress_callback);
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
// Presentation Context Negotiation
// =============================================================================

std::vector<std::string> storage_scu::proposed_transfer_syntaxes(
    const encoding::transfer_syntax& transfer_syntax) {

    if (!transfer_syntax.is_valid()) {
        return {};
    }

    // store() sends encapsulated Pixel Data only in its own syntax
    if (transfer_syntax.is_encapsulated()) {
        return {std::string(transfer_syntax.uid())};
    }

    const std::string implicit_le{
        encoding::transfer_syntax::implicit_vr_little_endian.uid()};
    const std::string explicit_le{
        encoding::transfer_syntax::explicit_vr_little_endian.uid()};

    if (transfer_syntax.uid() == implicit_le) {
        return {implicit_le, explicit_le};
    }

    // Explicit VR Little Endian, and native syntaxes the DIMSE layer cannot
    // encode (Explicit VR Big Endian, Deflated), which go out re-encoded
    return {explicit_le, implicit_le};
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

}  // namespace kcenon::pacs::services
