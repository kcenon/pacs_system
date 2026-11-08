// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_scp.cpp
 * @brief Implementation of the Storage SCP service
 */

#include "kcenon/pacs/services/storage_scp.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/events.h"
#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/dimse/command_field.h"
#include "kcenon/pacs/network/dimse/status_codes.h"
#include "kcenon/pacs/security/atna_service_auditor.h"
#include "kcenon/pacs/services/sop_class_registry.h"

#include <kcenon/common/patterns/event_bus.h>

#include <algorithm>

namespace kcenon::pacs::services {

// =============================================================================
// Construction
// =============================================================================

storage_scp::storage_scp(std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)) {}

storage_scp::storage_scp(const storage_scp_config& config,
                         std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)), config_(config) {}

// =============================================================================
// Handler Registration
// =============================================================================

void storage_scp::set_handler(storage_handler handler) {
    handler_ = std::move(handler);
    context_handler_ = {};
}

void storage_scp::set_context_handler(context_storage_handler handler) {
    context_handler_ = std::move(handler);
    handler_ = {};
}

void storage_scp::set_pre_store_handler(pre_store_handler handler) {
    pre_store_handler_ = std::move(handler);
}

void storage_scp::set_post_store_handler(post_store_handler handler) {
    post_store_handler_ = std::move(handler);
}

void storage_scp::set_audit_handler(
    std::shared_ptr<kcenon::pacs::security::atna_service_auditor> auditor) {
    auditor_ = std::move(auditor);
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> storage_scp::supported_sop_classes() const {
    if (!config_.accepted_sop_classes.empty()) {
        return config_.accepted_sop_classes;
    }
    return get_standard_storage_sop_classes();
}

network::Result<std::monostate> storage_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify the message is a C-STORE request
    if (request.command() != command_field::c_store_rq) {
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::store_unexpected_command,
            "Expected C-STORE-RQ but received " +
            std::string(to_string(request.command())));
    }

    // Extract SOP Class and Instance UIDs from the command set
    const auto sop_class_uid = request.affected_sop_class_uid();
    const auto sop_instance_uid = request.affected_sop_instance_uid();

    // Verify the request has a dataset
    if (!request.has_dataset()) {
        auto response = make_c_store_rsp(
            request.message_id(),
            sop_class_uid,
            sop_instance_uid,
            static_cast<status_code>(storage_status::cannot_understand)
        );
        return assoc.send_dimse(context_id, response);
    }

    // Get dataset reference (already validated above)
    const auto& dataset = request.dataset().value().get();

    const auto syntax = assoc.context_transfer_syntax(context_id);
    if (syntax.is_err()
        || core::dicom_file::validate_pixel_data_representation(dataset, syntax.value()).is_err()
        || (syntax.value().is_encapsulated() && dataset.get(core::tags::pixel_data)
            && !dataset.get(core::tags::pixel_data)->is_encapsulated_pixel_data())) {
        return assoc.send_dimse(context_id, make_c_store_rsp(
            request.message_id(), sop_class_uid, sop_instance_uid,
            static_cast<status_code>(storage_status::cannot_understand)));
    }

    // Pre-store validation
    if (pre_store_handler_ && !pre_store_handler_(dataset)) {
        auto response = make_c_store_rsp(
            request.message_id(),
            sop_class_uid,
            sop_instance_uid,
            static_cast<status_code>(storage_status::cannot_understand)
        );
        return assoc.send_dimse(context_id, response);
    }

    // Determine storage status
    storage_status status = storage_status::success;

    if (context_handler_) {
        const std::string calling_ae{assoc.calling_ae()};
        status = context_handler_({dataset, calling_ae, sop_class_uid,
                                   sop_instance_uid, syntax.value()});
    } else if (handler_) {
        // Call the registered storage handler
        status = handler_(
            dataset,
            std::string(assoc.calling_ae()),
            sop_class_uid,
            sop_instance_uid
        );
    }

    // Update statistics and notify on success or warning
    if (!is_failure(status)) {
        images_received_.fetch_add(1, std::memory_order_relaxed);
        // Estimate dataset size - use element count as approximation
        // In production, this would use actual serialized size
        bytes_received_.fetch_add(
            dataset.size() * sizeof(uint32_t),
            std::memory_order_relaxed
        );

        // Call post-store handler for cache invalidation and notifications
        auto patient_id = dataset.get_string(core::tags::patient_id);
        auto study_uid = dataset.get_string(core::tags::study_instance_uid);
        auto series_uid = dataset.get_string(core::tags::series_instance_uid);

        if (post_store_handler_) {
            post_store_handler_(
                dataset,
                patient_id,
                study_uid,
                series_uid,
                sop_instance_uid
            );
        }

        // Publish image received event
        kcenon::common::get_event_bus().publish(
            kcenon::pacs::events::image_received_event{
                patient_id,
                study_uid,
                series_uid,
                sop_instance_uid,
                sop_class_uid,
                std::string(assoc.calling_ae()),
                bytes_received_.load(std::memory_order_relaxed)
            }
        );
    } else {
        // Storage failed - publish failure event
        auto patient_id = dataset.get_string(core::tags::patient_id);

        kcenon::common::get_event_bus().publish(
            kcenon::pacs::events::storage_failed_event{
                patient_id,
                sop_instance_uid,
                std::string(assoc.calling_ae()),
                static_cast<int>(status),
                "C-STORE operation failed"
            }
        );
    }

    // Emit ATNA audit event
    if (auditor_) {
        auto audit_study_uid = dataset.get_string(core::tags::study_instance_uid);
        auto audit_patient_id = dataset.get_string(core::tags::patient_id);
        auditor_->audit_instance_stored(
            std::string(assoc.calling_ae()),
            std::string(assoc.called_ae()),
            audit_study_uid,
            audit_patient_id,
            !is_failure(status));
    }

    // Build and send the response
    auto response = make_c_store_rsp(
        request.message_id(),
        sop_class_uid,
        sop_instance_uid,
        static_cast<status_code>(status)
    );

    return assoc.send_dimse(context_id, response);
}

std::string_view storage_scp::service_name() const noexcept {
    return "Storage SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t storage_scp::images_received() const noexcept {
    return images_received_.load(std::memory_order_relaxed);
}

size_t storage_scp::bytes_received() const noexcept {
    return bytes_received_.load(std::memory_order_relaxed);
}

void storage_scp::reset_statistics() noexcept {
    images_received_.store(0, std::memory_order_relaxed);
    bytes_received_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Standard Storage SOP Classes
// =============================================================================

std::vector<std::string> get_standard_storage_sop_classes() {
    // One list only: the registry backs this default and the conformance statement,
    // so advertising a class cannot drift from accepting it (#1318).
    auto classes = sop_class_registry::instance().get_all_storage_classes(true);
    std::sort(classes.begin(), classes.end());
    return classes;
}

}  // namespace kcenon::pacs::services
