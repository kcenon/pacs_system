/**
 * @file worklist_scp.cpp
 * @brief Implementation of the Modality Worklist SCP service
 */

#include "pacs/services/worklist_scp.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

worklist_scp::worklist_scp() = default;

// =============================================================================
// Configuration
// =============================================================================

void worklist_scp::set_handler(worklist_handler handler) {
    handler_ = std::move(handler);
}

void worklist_scp::set_max_results(size_t max) noexcept {
    max_results_ = max;
}

size_t worklist_scp::max_results() const noexcept {
    return max_results_;
}

void worklist_scp::set_cancel_check(worklist_cancel_check check) {
    cancel_check_ = std::move(check);
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> worklist_scp::supported_sop_classes() const {
    return {
        std::string(worklist_find_sop_class_uid)
    };
}

network::Result<std::monostate> worklist_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify the message is a C-FIND request
    if (request.command() != command_field::c_find_rq) {
        return pacs::pacs_void_error(
            pacs::error_codes::worklist_unexpected_command,
            "Expected C-FIND-RQ but received " +
            std::string(to_string(request.command())));
    }

    // Verify we have a handler
    if (!handler_) {
        return pacs::pacs_void_error(
            pacs::error_codes::worklist_handler_not_set,
            "No worklist handler configured");
    }

    // Get the SOP Class UID from the request
    auto sop_class_uid = request.affected_sop_class_uid();

    // Verify the SOP Class is Modality Worklist
    if (sop_class_uid != worklist_find_sop_class_uid) {
        return send_final_response(
            assoc, context_id, request.message_id(),
            status_refused_sop_class_not_supported);
    }

    // Verify we have a dataset (query keys)
    if (!request.has_dataset()) {
        return send_final_response(
            assoc, context_id, request.message_id(),
            status_error_cannot_understand);
    }

    // Get the query keys from the request
    const auto& query_keys = request.dataset();

    // Get calling AE for logging and access control
    std::string calling_ae{assoc.calling_ae()};

    // Call the handler to get matching worklist items
    auto results = handler_(query_keys, calling_ae);

    // Apply max results limit if configured
    if (max_results_ > 0 && results.size() > max_results_) {
        results.resize(max_results_);
    }

    // Send pending responses for each result
    for (const auto& result : results) {
        // Check for cancel request
        if (cancel_check_ && cancel_check_()) {
            // Increment statistics
            ++queries_processed_;

            // Send cancel response
            return send_final_response(
                assoc, context_id, request.message_id(),
                status_cancel);
        }

        // Send pending response with matching worklist item
        auto send_result = send_pending_response(
            assoc, context_id, request.message_id(),
            result);

        if (send_result.is_err()) {
            return send_result;
        }

        // Track items returned
        ++items_returned_;
    }

    // Increment statistics
    ++queries_processed_;

    // Send final success response (no dataset)
    return send_final_response(
        assoc, context_id, request.message_id(),
        status_success);
}

std::string_view worklist_scp::service_name() const noexcept {
    return "Worklist SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t worklist_scp::queries_processed() const noexcept {
    return queries_processed_.load();
}

size_t worklist_scp::items_returned() const noexcept {
    return items_returned_.load();
}

void worklist_scp::reset_statistics() noexcept {
    queries_processed_ = 0;
    items_returned_ = 0;
}

// =============================================================================
// Private Implementation
// =============================================================================

network::Result<std::monostate> worklist_scp::send_pending_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const core::dicom_dataset& result) {

    using namespace network::dimse;

    // Create C-FIND response with pending status
    auto response = make_c_find_rsp(
        message_id,
        worklist_find_sop_class_uid,
        status_pending
    );

    // Attach the matching worklist item dataset
    response.set_dataset(result);

    // Send the response
    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> worklist_scp::send_final_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    network::dimse::status_code status) {

    using namespace network::dimse;

    // Create C-FIND response with final status (no dataset)
    auto response = make_c_find_rsp(
        message_id,
        worklist_find_sop_class_uid,
        status
    );

    // Send the response
    return assoc.send_dimse(context_id, response);
}

}  // namespace pacs::services
