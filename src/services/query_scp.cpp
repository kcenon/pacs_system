/**
 * @file query_scp.cpp
 * @brief Implementation of the Query SCP service
 */

#include "pacs/services/query_scp.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

query_scp::query_scp() = default;

// =============================================================================
// Configuration
// =============================================================================

void query_scp::set_handler(query_handler handler) {
    handler_ = std::move(handler);
}

void query_scp::set_max_results(size_t max) noexcept {
    max_results_ = max;
}

size_t query_scp::max_results() const noexcept {
    return max_results_;
}

void query_scp::set_cancel_check(cancel_check check) {
    cancel_check_ = std::move(check);
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> query_scp::supported_sop_classes() const {
    return {
        std::string(patient_root_find_sop_class_uid),
        std::string(study_root_find_sop_class_uid)
    };
}

network::Result<std::monostate> query_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify the message is a C-FIND request
    if (request.command() != command_field::c_find_rq) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            std::string("Expected C-FIND-RQ but received ") +
            std::string(to_string(request.command())));
#else
        return std::string("Expected C-FIND-RQ but received ") +
               std::string(to_string(request.command()));
#endif
    }

    // Verify we have a handler
    if (!handler_) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            std::string("No query handler configured"));
#else
        return std::string("No query handler configured");
#endif
    }

    // Get the SOP Class UID from the request
    auto sop_class_uid = request.affected_sop_class_uid();

    // Verify we have a dataset
    if (!request.has_dataset()) {
        return send_final_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, status_error_cannot_understand);
    }

    // Extract query level
    const auto& query_keys = request.dataset();
    auto level = extract_query_level(query_keys);

    if (!level.has_value()) {
        // Invalid or missing query level
        return send_final_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, status_error_cannot_understand);
    }

    // Get calling AE for logging and access control
    std::string calling_ae{assoc.calling_ae()};

    // Call the handler to get matching results
    auto results = handler_(level.value(), query_keys, calling_ae);

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
                sop_class_uid, status_cancel);
        }

        // Send pending response with matching dataset
        auto send_result = send_pending_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, result);

        if (send_result.is_err()) {
            return send_result;
        }
    }

    // Increment statistics
    ++queries_processed_;

    // Send final success response (no dataset)
    return send_final_response(
        assoc, context_id, request.message_id(),
        sop_class_uid, status_success);
}

std::string_view query_scp::service_name() const noexcept {
    return "Query SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t query_scp::queries_processed() const noexcept {
    return queries_processed_.load();
}

void query_scp::reset_statistics() noexcept {
    queries_processed_ = 0;
}

// =============================================================================
// Private Implementation
// =============================================================================

std::optional<query_level> query_scp::extract_query_level(
    const core::dicom_dataset& dataset) const {

    // Get Query/Retrieve Level tag (0008,0052)
    auto level_str = dataset.get_string(core::tags::query_retrieve_level);

    if (level_str.empty()) {
        return std::nullopt;
    }

    return parse_query_level(level_str);
}

network::Result<std::monostate> query_scp::send_pending_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    std::string_view sop_class_uid,
    const core::dicom_dataset& result) {

    using namespace network::dimse;

    // Create C-FIND response with pending status
    auto response = make_c_find_rsp(
        message_id,
        sop_class_uid,
        status_pending
    );

    // Attach the matching dataset
    response.set_dataset(result);

    // Send the response
    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> query_scp::send_final_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    std::string_view sop_class_uid,
    network::dimse::status_code status) {

    using namespace network::dimse;

    // Create C-FIND response with final status (no dataset)
    auto response = make_c_find_rsp(
        message_id,
        sop_class_uid,
        status
    );

    // Send the response
    return assoc.send_dimse(context_id, response);
}

}  // namespace pacs::services
