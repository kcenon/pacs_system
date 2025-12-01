/**
 * @file mpps_scp.cpp
 * @brief Implementation of the MPPS (Modality Performed Procedure Step) SCP service
 */

#include "pacs/services/mpps_scp.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

mpps_scp::mpps_scp() = default;

// =============================================================================
// Configuration
// =============================================================================

void mpps_scp::set_create_handler(mpps_create_handler handler) {
    create_handler_ = std::move(handler);
}

void mpps_scp::set_set_handler(mpps_set_handler handler) {
    set_handler_ = std::move(handler);
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> mpps_scp::supported_sop_classes() const {
    return {std::string(mpps_sop_class_uid)};
}

network::Result<std::monostate> mpps_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Route to appropriate handler based on command type
    switch (request.command()) {
        case command_field::n_create_rq:
            return handle_n_create(assoc, context_id, request);

        case command_field::n_set_rq:
            return handle_n_set(assoc, context_id, request);

        default:
#ifdef PACS_WITH_COMMON_SYSTEM
            return kcenon::common::error_info(
                std::string("Unexpected command for MPPS SCP: ") +
                std::string(to_string(request.command())));
#else
            return std::string("Unexpected command for MPPS SCP: ") +
                   std::string(to_string(request.command()));
#endif
    }
}

std::string_view mpps_scp::service_name() const noexcept {
    return "MPPS SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t mpps_scp::creates_processed() const noexcept {
    return creates_processed_.load();
}

size_t mpps_scp::sets_processed() const noexcept {
    return sets_processed_.load();
}

size_t mpps_scp::mpps_completed() const noexcept {
    return mpps_completed_.load();
}

size_t mpps_scp::mpps_discontinued() const noexcept {
    return mpps_discontinued_.load();
}

void mpps_scp::reset_statistics() noexcept {
    creates_processed_ = 0;
    sets_processed_ = 0;
    mpps_completed_ = 0;
    mpps_discontinued_ = 0;
}

// =============================================================================
// Private Implementation - N-CREATE Handler
// =============================================================================

network::Result<std::monostate> mpps_scp::handle_n_create(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify we have a handler configured
    if (!create_handler_) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            std::string("No N-CREATE handler configured for MPPS SCP"));
#else
        return std::string("No N-CREATE handler configured for MPPS SCP");
#endif
    }

    // Verify the SOP Class is MPPS
    auto sop_class_uid = request.affected_sop_class_uid();
    if (sop_class_uid != mpps_sop_class_uid) {
        return send_n_create_response(
            assoc, context_id, request.message_id(),
            "", status_refused_sop_class_not_supported);
    }

    // Get the SOP Instance UID from the request
    auto sop_instance_uid = request.affected_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        return send_n_create_response(
            assoc, context_id, request.message_id(),
            "", status_error_missing_attribute);
    }

    // Verify we have a dataset
    if (!request.has_dataset()) {
        return send_n_create_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_cannot_understand);
    }

    const auto& dataset = request.dataset();

    // Build MPPS instance from request data
    mpps_instance instance;
    instance.sop_instance_uid = sop_instance_uid;
    instance.status = mpps_status::in_progress;
    instance.data = dataset;

    // Extract Performed Station AE Title if present
    if (dataset.contains(mpps_tags::performed_station_ae_title)) {
        instance.station_ae = dataset.get_string(mpps_tags::performed_station_ae_title);
    }

    // Validate initial status is IN PROGRESS
    if (dataset.contains(mpps_tags::performed_procedure_step_status)) {
        auto status_str = dataset.get_string(mpps_tags::performed_procedure_step_status);
        auto parsed_status = parse_mpps_status(status_str);

        if (!parsed_status.has_value() ||
            parsed_status.value() != mpps_status::in_progress) {
            // N-CREATE must have IN PROGRESS status
            return send_n_create_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, status_error_cannot_understand);
        }
    }

    // Call the handler to create the MPPS instance
    auto result = create_handler_(instance);
    if (result.is_err()) {
        return send_n_create_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_unable_to_process);
    }

    // Update statistics
    ++creates_processed_;

    // Send success response
    return send_n_create_response(
        assoc, context_id, request.message_id(),
        sop_instance_uid, status_success);
}

// =============================================================================
// Private Implementation - N-SET Handler
// =============================================================================

network::Result<std::monostate> mpps_scp::handle_n_set(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify we have a handler configured
    if (!set_handler_) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            std::string("No N-SET handler configured for MPPS SCP"));
#else
        return std::string("No N-SET handler configured for MPPS SCP");
#endif
    }

    // Verify the SOP Class is MPPS
    auto sop_class_uid = request.affected_sop_class_uid();
    if (sop_class_uid.empty()) {
        // For N-SET, use requested SOP class if affected is empty
        sop_class_uid = request.command_set().get_string(
            tag_requested_sop_class_uid);
    }

    if (sop_class_uid != mpps_sop_class_uid) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            "", status_refused_sop_class_not_supported);
    }

    // Get the SOP Instance UID (from Requested SOP Instance UID for N-SET)
    auto sop_instance_uid = request.command_set().get_string(
        tag_requested_sop_instance_uid);

    if (sop_instance_uid.empty()) {
        // Try affected SOP instance UID as fallback
        sop_instance_uid = request.affected_sop_instance_uid();
    }

    if (sop_instance_uid.empty()) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            "", status_error_missing_attribute);
    }

    // Verify we have a dataset with modifications
    if (!request.has_dataset()) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_cannot_understand);
    }

    const auto& dataset = request.dataset();

    // Extract and validate the new status
    if (!dataset.contains(mpps_tags::performed_procedure_step_status)) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_missing_attribute);
    }

    auto status_str = dataset.get_string(mpps_tags::performed_procedure_step_status);
    auto new_status = parse_mpps_status(status_str);

    if (!new_status.has_value()) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_cannot_understand);
    }

    // Validate status transition (must be to COMPLETED or DISCONTINUED)
    if (new_status.value() == mpps_status::in_progress) {
        // Cannot set back to IN PROGRESS
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_cannot_understand);
    }

    // Call the handler to update the MPPS instance
    auto result = set_handler_(sop_instance_uid, dataset, new_status.value());
    if (result.is_err()) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_unable_to_process);
    }

    // Update statistics
    ++sets_processed_;
    if (new_status.value() == mpps_status::completed) {
        ++mpps_completed_;
    } else if (new_status.value() == mpps_status::discontinued) {
        ++mpps_discontinued_;
    }

    // Send success response
    return send_n_set_response(
        assoc, context_id, request.message_id(),
        sop_instance_uid, status_success);
}

// =============================================================================
// Private Implementation - Response Helpers
// =============================================================================

network::Result<std::monostate> mpps_scp::send_n_create_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid,
    network::dimse::status_code status) {

    using namespace network::dimse;

    // Create N-CREATE response message
    dimse_message response{command_field::n_create_rsp, 0};
    response.set_message_id_responded_to(message_id);
    response.set_affected_sop_class_uid(mpps_sop_class_uid);
    response.set_status(status);

    if (!sop_instance_uid.empty()) {
        response.set_affected_sop_instance_uid(sop_instance_uid);
    }

    // Send the response
    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> mpps_scp::send_n_set_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid,
    network::dimse::status_code status) {

    using namespace network::dimse;

    // Create N-SET response message
    dimse_message response{command_field::n_set_rsp, 0};
    response.set_message_id_responded_to(message_id);
    response.set_affected_sop_class_uid(mpps_sop_class_uid);
    response.set_status(status);

    if (!sop_instance_uid.empty()) {
        response.set_affected_sop_instance_uid(sop_instance_uid);
    }

    // Send the response
    return assoc.send_dimse(context_id, response);
}

}  // namespace pacs::services
