/**
 * @file n_get_scp.cpp
 * @brief Implementation of the N-GET SCP service
 *
 * @see DICOM PS3.7 Section 10.1.2 - N-GET Service
 * @see Issue #720 - Implement N-GET SCP/SCU service
 */

#include "pacs/services/n_get_scp.hpp"

#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

n_get_scp::n_get_scp(std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)) {}

// =============================================================================
// Configuration
// =============================================================================

void n_get_scp::set_handler(n_get_handler handler) {
    handler_ = std::move(handler);
}

void n_get_scp::add_supported_sop_class(std::string sop_class_uid) {
    supported_sop_classes_.push_back(std::move(sop_class_uid));
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> n_get_scp::supported_sop_classes() const {
    return supported_sop_classes_;
}

network::Result<std::monostate> n_get_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify command type is N-GET-RQ
    if (request.command() != command_field::n_get_rq) {
        return pacs::pacs_void_error(
            pacs::error_codes::n_get_unexpected_command,
            "Unexpected command for N-GET SCP: " +
            std::string(to_string(request.command())));
    }

    // Verify we have a handler configured
    if (!handler_) {
        return pacs::pacs_void_error(
            pacs::error_codes::n_get_handler_not_set,
            "No N-GET handler configured");
    }

    // Extract Requested SOP Class UID
    auto sop_class_uid = request.requested_sop_class_uid();
    if (sop_class_uid.empty()) {
        return send_n_get_response(
            assoc, context_id, request.message_id(),
            "", "", status_error_missing_attribute);
    }

    // Verify the SOP Class is supported
    if (!supports_sop_class(sop_class_uid)) {
        return send_n_get_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, "", status_refused_sop_class_not_supported);
    }

    // Extract Requested SOP Instance UID
    auto sop_instance_uid = request.requested_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        return send_n_get_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, "", status_error_missing_attribute);
    }

    // Extract Attribute Identifier List (empty = all attributes)
    auto attribute_tags = request.attribute_identifier_list();

    logger_->debug("N-GET request for instance: " + sop_instance_uid +
                   " (attributes requested: " +
                   (attribute_tags.empty() ? "all" :
                    std::to_string(attribute_tags.size())) + ")");

    // Call the handler to retrieve attributes
    auto result = handler_(sop_class_uid, sop_instance_uid, attribute_tags);
    if (result.is_err()) {
        logger_->warn("N-GET handler failed for instance: " + sop_instance_uid +
                       " - " + result.error().message);
        return send_n_get_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, sop_instance_uid,
            status_error_invalid_object_instance);
    }

    // Update statistics
    ++gets_processed_;

    // Send success response with dataset
    auto dataset = std::move(result.value());
    return send_n_get_response(
        assoc, context_id, request.message_id(),
        sop_class_uid, sop_instance_uid,
        status_success, &dataset);
}

std::string_view n_get_scp::service_name() const noexcept {
    return "N-GET SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t n_get_scp::gets_processed() const noexcept {
    return gets_processed_.load();
}

void n_get_scp::reset_statistics() noexcept {
    gets_processed_ = 0;
}

// =============================================================================
// Private Implementation
// =============================================================================

network::Result<std::monostate> n_get_scp::send_n_get_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_class_uid,
    const std::string& sop_instance_uid,
    network::dimse::status_code status,
    core::dicom_dataset* dataset) {

    using namespace network::dimse;

    // Create N-GET response using factory function
    auto response = make_n_get_rsp(
        message_id, sop_class_uid, sop_instance_uid, status);

    // Attach dataset if provided (successful responses include attributes)
    if (dataset != nullptr) {
        response.set_dataset(std::move(*dataset));
    }

    // Send the response
    return assoc.send_dimse(context_id, response);
}

}  // namespace pacs::services
