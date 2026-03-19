/**
 * @file ups_query_scp.cpp
 * @brief Implementation of the UPS Query SCP service (C-FIND for workitems)
 */

#include "pacs/services/ups/ups_query_scp.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace kcenon::pacs::services {

// =============================================================================
// Construction
// =============================================================================

ups_query_scp::ups_query_scp(std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)) {}

// =============================================================================
// Configuration
// =============================================================================

void ups_query_scp::set_handler(ups_query_handler handler) {
    handler_ = std::move(handler);
}

void ups_query_scp::set_max_results(size_t max) noexcept {
    max_results_ = max;
}

size_t ups_query_scp::max_results() const noexcept {
    return max_results_;
}

void ups_query_scp::set_cancel_check(ups_query_cancel_check check) {
    cancel_check_ = std::move(check);
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> ups_query_scp::supported_sop_classes() const {
    return {
        std::string(ups_query_find_sop_class_uid)
    };
}

network::Result<std::monostate> ups_query_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify the message is a C-FIND request
    if (request.command() != command_field::c_find_rq) {
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::ups_unexpected_command,
            "Expected C-FIND-RQ but received " +
            std::string(to_string(request.command())));
    }

    // Verify we have a handler
    if (!handler_) {
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::ups_handler_not_set,
            "No UPS query handler configured");
    }

    // Get the SOP Class UID from the request
    auto sop_class_uid = request.affected_sop_class_uid();

    // Verify the SOP Class is UPS Query FIND
    if (sop_class_uid != ups_query_find_sop_class_uid) {
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
    const auto& query_keys = request.dataset().value().get();

    // Get calling AE for logging and access control
    std::string calling_ae{assoc.calling_ae()};

    logger_->debug("Processing UPS C-FIND query from " + calling_ae);

    // Call the handler to get matching workitems
    auto results = handler_(query_keys, calling_ae);

    // Apply max results limit if configured
    if (max_results_ > 0 && results.size() > max_results_) {
        results.resize(max_results_);
    }

    // Send pending responses for each result
    for (const auto& result : results) {
        // Check for cancel request
        if (cancel_check_ && cancel_check_()) {
            ++queries_processed_;

            return send_final_response(
                assoc, context_id, request.message_id(),
                status_cancel);
        }

        // Send pending response with matching workitem
        auto send_result = send_pending_response(
            assoc, context_id, request.message_id(),
            result);

        if (send_result.is_err()) {
            return send_result;
        }

        ++items_returned_;
    }

    // Increment statistics
    ++queries_processed_;

    logger_->info("UPS C-FIND completed: " + std::to_string(results.size()) +
                  " workitems matched");

    // Send final success response (no dataset)
    return send_final_response(
        assoc, context_id, request.message_id(),
        status_success);
}

std::string_view ups_query_scp::service_name() const noexcept {
    return "UPS Query SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t ups_query_scp::queries_processed() const noexcept {
    return queries_processed_.load();
}

size_t ups_query_scp::items_returned() const noexcept {
    return items_returned_.load();
}

void ups_query_scp::reset_statistics() noexcept {
    queries_processed_ = 0;
    items_returned_ = 0;
}

// =============================================================================
// Private Implementation
// =============================================================================

network::Result<std::monostate> ups_query_scp::send_pending_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const core::dicom_dataset& result) {

    using namespace network::dimse;

    auto response = make_c_find_rsp(
        message_id,
        ups_query_find_sop_class_uid,
        status_pending
    );

    response.set_dataset(result);

    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> ups_query_scp::send_final_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    network::dimse::status_code status) {

    using namespace network::dimse;

    auto response = make_c_find_rsp(
        message_id,
        ups_query_find_sop_class_uid,
        status
    );

    return assoc.send_dimse(context_id, response);
}

}  // namespace kcenon::pacs::services
