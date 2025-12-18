/**
 * @file storage_scp.cpp
 * @brief Implementation of the Storage SCP service
 */

#include "pacs/services/storage_scp.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

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
}

void storage_scp::set_pre_store_handler(pre_store_handler handler) {
    pre_store_handler_ = std::move(handler);
}

void storage_scp::set_post_store_handler(post_store_handler handler) {
    post_store_handler_ = std::move(handler);
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
        return pacs::pacs_void_error(
            pacs::error_codes::store_unexpected_command,
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

    // Pre-store validation
    if (pre_store_handler_ && !pre_store_handler_(request.dataset())) {
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

    if (handler_) {
        // Call the registered storage handler
        status = handler_(
            request.dataset(),
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
            request.dataset().size() * sizeof(uint32_t),
            std::memory_order_relaxed
        );

        // Call post-store handler for cache invalidation and notifications
        if (post_store_handler_) {
            const auto& dataset = request.dataset();
            auto patient_id = dataset.get_string(core::tags::patient_id);
            auto study_uid = dataset.get_string(core::tags::study_instance_uid);
            auto series_uid = dataset.get_string(core::tags::series_instance_uid);

            post_store_handler_(
                dataset,
                patient_id,
                study_uid,
                series_uid,
                sop_instance_uid
            );
        }
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
    return {
        // CT
        std::string(ct_image_storage_uid),
        std::string(enhanced_ct_image_storage_uid),

        // MR
        std::string(mr_image_storage_uid),
        std::string(enhanced_mr_image_storage_uid),

        // CR/DX
        std::string(cr_image_storage_uid),
        std::string(dx_image_storage_presentation_uid),
        std::string(dx_image_storage_processing_uid),

        // US
        std::string(us_image_storage_uid),

        // Secondary Capture
        std::string(secondary_capture_image_storage_uid),

        // RT
        std::string(rt_image_storage_uid),
        std::string(rt_dose_storage_uid),
        std::string(rt_structure_set_storage_uid),
        std::string(rt_plan_storage_uid),

        // NM/PET
        "1.2.840.10008.5.1.4.1.1.20",      // NM Image Storage
        "1.2.840.10008.5.1.4.1.1.128",     // PET Image Storage
        "1.2.840.10008.5.1.4.1.1.130",     // Enhanced PET Image Storage

        // XA/RF
        "1.2.840.10008.5.1.4.1.1.12.1",    // XA Image Storage
        "1.2.840.10008.5.1.4.1.1.12.2",    // XRF Image Storage

        // Mammography
        "1.2.840.10008.5.1.4.1.1.1.2",     // Digital Mammography X-Ray - Presentation
        "1.2.840.10008.5.1.4.1.1.1.2.1",   // Digital Mammography X-Ray - Processing

        // Multi-frame
        "1.2.840.10008.5.1.4.1.1.7.1",     // Multi-frame Single Bit SC
        "1.2.840.10008.5.1.4.1.1.7.2",     // Multi-frame Grayscale Byte SC
        "1.2.840.10008.5.1.4.1.1.7.3",     // Multi-frame Grayscale Word SC
        "1.2.840.10008.5.1.4.1.1.7.4",     // Multi-frame True Color SC

        // SR (Structured Reports)
        "1.2.840.10008.5.1.4.1.1.88.11",   // Basic Text SR
        "1.2.840.10008.5.1.4.1.1.88.22",   // Enhanced SR
        "1.2.840.10008.5.1.4.1.1.88.33",   // Comprehensive SR
        "1.2.840.10008.5.1.4.1.1.88.34",   // Comprehensive 3D SR

        // Other common types
        "1.2.840.10008.5.1.4.1.1.104.1",   // Encapsulated PDF
        "1.2.840.10008.5.1.4.1.1.104.2",   // Encapsulated CDA
    };
}

}  // namespace pacs::services
