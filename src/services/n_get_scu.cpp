// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file n_get_scu.cpp
 * @brief Implementation of the N-GET SCU service
 *
 * @see DICOM PS3.7 Section 10.1.2 - N-GET Service
 * @see Issue #720 - Implement N-GET SCP/SCU service
 */

#include "pacs/services/n_get_scu.hpp"

#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

n_get_scu::n_get_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

n_get_scu::n_get_scu(const n_get_scu_config& config,
                     std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()),
      config_(config) {}

// =============================================================================
// N-GET Operation
// =============================================================================

network::Result<n_get_result> n_get_scu::get(
    network::association& assoc,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    const std::vector<core::dicom_tag>& attribute_tags) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<n_get_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Validate UIDs
    if (sop_class_uid.empty()) {
        return pacs::pacs_error<n_get_result>(
            pacs::error_codes::n_get_missing_uid,
            "SOP Class UID is required for N-GET");
    }

    if (sop_instance_uid.empty()) {
        return pacs::pacs_error<n_get_result>(
            pacs::error_codes::n_get_missing_uid,
            "SOP Instance UID is required for N-GET");
    }

    // Get accepted presentation context
    auto context_id = assoc.accepted_context_id(sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<n_get_result>(
            pacs::error_codes::n_get_context_not_accepted,
            "No accepted presentation context for SOP Class: " +
            std::string(sop_class_uid));
    }

    // Build N-GET request using factory function
    auto request = make_n_get_rq(
        next_message_id(), sop_class_uid, sop_instance_uid, attribute_tags);

    logger_->debug("Sending N-GET request for instance: " +
                   std::string(sop_instance_uid) +
                   " (attributes: " +
                   (attribute_tags.empty() ? "all" :
                    std::to_string(attribute_tags.size())) + ")");

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-GET: " + send_result.error().message);
        return send_result.error();
    }

    // Receive the response
    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-GET response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    // Verify it's an N-GET response
    if (response.command() != command_field::n_get_rsp) {
        return pacs::pacs_error<n_get_result>(
            pacs::error_codes::n_get_unexpected_command,
            "Expected N-GET-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Build result
    n_get_result result;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    // Extract error comment if present
    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(
            tag_error_comment);
    }

    // Extract dataset (returned attributes) if present
    if (response.has_dataset()) {
        auto dataset_result = response.dataset();
        if (dataset_result.is_ok()) {
            result.attributes = dataset_result.value().get();
        }
    }

    // Update statistics
    gets_performed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-GET successful for instance: " +
                      std::string(sop_instance_uid));
    } else {
        logger_->warn("N-GET returned status 0x" +
                      std::to_string(result.status) +
                      " for instance: " + std::string(sop_instance_uid));
    }

    return result;
}

// =============================================================================
// Statistics
// =============================================================================

size_t n_get_scu::gets_performed() const noexcept {
    return gets_performed_.load(std::memory_order_relaxed);
}

void n_get_scu::reset_statistics() noexcept {
    gets_performed_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Implementation
// =============================================================================

uint16_t n_get_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

}  // namespace pacs::services
