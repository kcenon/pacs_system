/**
 * @file retrieve_scu.cpp
 * @brief Implementation of the Retrieve SCU service (C-MOVE/C-GET)
 *
 * @see Issue #532 - Implement retrieve_scu Library (C-MOVE/C-GET SCU)
 */

#include "pacs/services/retrieve_scu.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

retrieve_scu::retrieve_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

retrieve_scu::retrieve_scu(const retrieve_scu_config& config,
                           std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()), config_(config) {}

// =============================================================================
// C-MOVE Operations
// =============================================================================

network::Result<retrieve_result> retrieve_scu::move(
    network::association& assoc,
    const core::dicom_dataset& query_keys,
    std::string_view destination_ae,
    retrieve_progress_callback progress) {

    return perform_move(assoc, query_keys, destination_ae, progress);
}

network::Result<retrieve_result> retrieve_scu::perform_move(
    network::association& assoc,
    const core::dicom_dataset& query_keys,
    std::string_view destination_ae,
    retrieve_progress_callback progress) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();
    auto message_id = next_message_id();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<retrieve_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Validate move destination
    if (destination_ae.empty()) {
        return pacs::pacs_error<retrieve_result>(
            pacs::error_codes::retrieve_missing_destination,
            "Move destination AE title is required");
    }

    // Get SOP Class UID
    auto sop_class_uid = get_move_sop_class_uid();

    // Get accepted presentation context for this SOP class
    auto context_id = assoc.accepted_context_id(sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<retrieve_result>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for SOP Class: " +
            std::string(sop_class_uid));
    }

    // Build C-MOVE-RQ message
    dimse_message request{command_field::c_move_rq, message_id};
    request.set_affected_sop_class_uid(sop_class_uid);
    request.set_priority(config_.priority);

    // Set Move Destination
    request.command_set().set_string(
        tag_move_destination,
        encoding::vr_type::AE,
        std::string(destination_ae));

    request.set_dataset(query_keys);

    logger_->debug_fmt("Sending C-MOVE request (message_id={}, dest={}, sop_class={})",
                       message_id, destination_ae, sop_class_uid);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        return send_result.error();
    }

    // Initialize progress tracking
    retrieve_progress prog;
    prog.start_time = start_time;

    // Receive responses
    retrieve_result result;
    bool move_complete = false;

    while (!move_complete) {
        auto recv_result = assoc.receive_dimse(config_.timeout);
        if (recv_result.is_err()) {
            return recv_result.error();
        }

        const auto& [recv_context_id, response] = recv_result.value();

        // Verify it's a C-MOVE response
        if (response.command() != command_field::c_move_rsp) {
            return pacs::pacs_error<retrieve_result>(
                pacs::error_codes::retrieve_unexpected_command,
                "Expected C-MOVE-RSP but received " +
                std::string(to_string(response.command())));
        }

        auto status = response.status();

        // Extract sub-operation counts
        if (auto remaining = response.remaining_subops()) {
            prog.remaining = *remaining;
        }
        if (auto completed = response.completed_subops()) {
            prog.completed = *completed;
            result.completed = *completed;
        }
        if (auto failed = response.failed_subops()) {
            prog.failed = *failed;
            result.failed = *failed;
        }
        if (auto warning = response.warning_subops()) {
            prog.warning = *warning;
            result.warning = *warning;
        }

        // Call progress callback
        if (progress) {
            progress(prog);
        }

        // Check completion status
        if (status == status_success) {
            move_complete = true;
            result.final_status = static_cast<uint16_t>(status);
        } else if (status == status_cancel) {
            move_complete = true;
            result.final_status = static_cast<uint16_t>(status);
        } else if ((status & 0xF000) == 0xA000 ||
                   (status & 0xF000) == 0xC000) {
            // Error status (Refused or Failed)
            move_complete = true;
            result.final_status = static_cast<uint16_t>(status);
        }
        // status_pending (0xFF00) continues the loop
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Update statistics
    retrieves_performed_.fetch_add(1, std::memory_order_relaxed);
    instances_retrieved_.fetch_add(result.completed, std::memory_order_relaxed);

    logger_->debug_fmt("C-MOVE completed: {} completed, {} failed in {} ms",
                       result.completed, result.failed, result.elapsed.count());

    return result;
}

// =============================================================================
// C-GET Operations
// =============================================================================

network::Result<retrieve_result> retrieve_scu::get(
    network::association& assoc,
    const core::dicom_dataset& query_keys,
    retrieve_progress_callback progress) {

    return perform_get(assoc, query_keys, progress);
}

network::Result<retrieve_result> retrieve_scu::perform_get(
    network::association& assoc,
    const core::dicom_dataset& query_keys,
    retrieve_progress_callback progress) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();
    auto message_id = next_message_id();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<retrieve_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get SOP Class UID
    auto sop_class_uid = get_get_sop_class_uid();

    // Get accepted presentation context for this SOP class
    auto context_id = assoc.accepted_context_id(sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<retrieve_result>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for SOP Class: " +
            std::string(sop_class_uid));
    }

    // Build C-GET-RQ message
    dimse_message request{command_field::c_get_rq, message_id};
    request.set_affected_sop_class_uid(sop_class_uid);
    request.set_priority(config_.priority);
    request.set_dataset(query_keys);

    logger_->debug_fmt("Sending C-GET request (message_id={}, sop_class={})",
                       message_id, sop_class_uid);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        return send_result.error();
    }

    // Initialize progress tracking
    retrieve_progress prog;
    prog.start_time = start_time;

    // Receive responses (C-GET-RSP and C-STORE-RQ interleaved)
    retrieve_result result;
    bool retrieve_complete = false;

    while (!retrieve_complete) {
        auto recv_result = assoc.receive_dimse(config_.timeout);
        if (recv_result.is_err()) {
            return recv_result.error();
        }

        auto& [recv_context_id, msg] = recv_result.value();
        auto cmd = msg.command();

        if (cmd == command_field::c_get_rsp) {
            auto status = msg.status();

            // Extract sub-operation counts
            if (auto remaining = msg.remaining_subops()) {
                prog.remaining = *remaining;
            }
            if (auto completed = msg.completed_subops()) {
                prog.completed = *completed;
                result.completed = *completed;
            }
            if (auto failed = msg.failed_subops()) {
                prog.failed = *failed;
                result.failed = *failed;
            }
            if (auto warning = msg.warning_subops()) {
                prog.warning = *warning;
                result.warning = *warning;
            }

            // Call progress callback
            if (progress) {
                progress(prog);
            }

            // Check completion status
            if (status == status_success) {
                retrieve_complete = true;
                result.final_status = static_cast<uint16_t>(status);
            } else if (status == status_cancel) {
                retrieve_complete = true;
                result.final_status = static_cast<uint16_t>(status);
            } else if ((status & 0xF000) == 0xA000 ||
                       (status & 0xF000) == 0xC000) {
                // Error status
                retrieve_complete = true;
                result.final_status = static_cast<uint16_t>(status);
            }

        } else if (cmd == command_field::c_store_rq) {
            // Incoming C-STORE sub-operation - store the received instance
            if (msg.has_dataset()) {
                auto dataset_result = msg.dataset();
                if (dataset_result.is_ok()) {
                    result.received_instances.push_back(dataset_result.value().get());

                    // Update bytes transferred (approximate)
                    bytes_retrieved_.fetch_add(1024, std::memory_order_relaxed);
                }
            }

            // Send C-STORE response
            auto sop_class = msg.affected_sop_class_uid();
            auto sop_instance = msg.affected_sop_instance_uid();

            auto store_rsp = make_c_store_rsp(
                msg.message_id(),
                sop_class,
                sop_instance,
                status_success
            );

            auto send_rsp_result = assoc.send_dimse(recv_context_id, store_rsp);
            if (send_rsp_result.is_err()) {
                logger_->warn_fmt("Failed to send C-STORE response: {}",
                                  send_rsp_result.error().message);
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Update statistics
    retrieves_performed_.fetch_add(1, std::memory_order_relaxed);
    instances_retrieved_.fetch_add(result.completed, std::memory_order_relaxed);

    logger_->debug_fmt("C-GET completed: {} completed, {} failed, {} received in {} ms",
                       result.completed, result.failed,
                       result.received_instances.size(), result.elapsed.count());

    return result;
}

// =============================================================================
// Convenience Methods
// =============================================================================

network::Result<retrieve_result> retrieve_scu::retrieve_study(
    network::association& assoc,
    std::string_view study_uid,
    retrieve_progress_callback progress) {

    auto query_ds = build_study_query(study_uid);

    if (config_.mode == retrieve_mode::c_move) {
        if (config_.move_destination.empty()) {
            return pacs::pacs_error<retrieve_result>(
                pacs::error_codes::retrieve_missing_destination,
                "Move destination is required for C-MOVE mode");
        }
        return move(assoc, query_ds, config_.move_destination, progress);
    } else {
        return get(assoc, query_ds, progress);
    }
}

network::Result<retrieve_result> retrieve_scu::retrieve_series(
    network::association& assoc,
    std::string_view series_uid,
    retrieve_progress_callback progress) {

    auto query_ds = build_series_query(series_uid);

    if (config_.mode == retrieve_mode::c_move) {
        if (config_.move_destination.empty()) {
            return pacs::pacs_error<retrieve_result>(
                pacs::error_codes::retrieve_missing_destination,
                "Move destination is required for C-MOVE mode");
        }
        return move(assoc, query_ds, config_.move_destination, progress);
    } else {
        return get(assoc, query_ds, progress);
    }
}

network::Result<retrieve_result> retrieve_scu::retrieve_instance(
    network::association& assoc,
    std::string_view sop_instance_uid,
    retrieve_progress_callback progress) {

    auto query_ds = build_instance_query(sop_instance_uid);

    if (config_.mode == retrieve_mode::c_move) {
        if (config_.move_destination.empty()) {
            return pacs::pacs_error<retrieve_result>(
                pacs::error_codes::retrieve_missing_destination,
                "Move destination is required for C-MOVE mode");
        }
        return move(assoc, query_ds, config_.move_destination, progress);
    } else {
        return get(assoc, query_ds, progress);
    }
}

// =============================================================================
// C-CANCEL Support
// =============================================================================

network::Result<std::monostate> retrieve_scu::cancel(
    network::association& assoc,
    uint16_t message_id) {

    using namespace network::dimse;

    auto sop_class_uid = (config_.mode == retrieve_mode::c_move)
        ? get_move_sop_class_uid()
        : get_get_sop_class_uid();

    auto context_id = assoc.accepted_context_id(sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<std::monostate>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for cancel");
    }

    // Build C-CANCEL-RQ message
    dimse_message cancel_rq{command_field::c_cancel_rq, message_id};
    return assoc.send_dimse(*context_id, cancel_rq);
}

// =============================================================================
// Configuration
// =============================================================================

void retrieve_scu::set_config(const retrieve_scu_config& config) {
    config_ = config;
}

void retrieve_scu::set_move_destination(std::string_view ae_title) {
    config_.move_destination = std::string(ae_title);
}

const retrieve_scu_config& retrieve_scu::config() const noexcept {
    return config_;
}

// =============================================================================
// Statistics
// =============================================================================

size_t retrieve_scu::retrieves_performed() const noexcept {
    return retrieves_performed_.load(std::memory_order_relaxed);
}

size_t retrieve_scu::instances_retrieved() const noexcept {
    return instances_retrieved_.load(std::memory_order_relaxed);
}

size_t retrieve_scu::bytes_retrieved() const noexcept {
    return bytes_retrieved_.load(std::memory_order_relaxed);
}

void retrieve_scu::reset_statistics() noexcept {
    retrieves_performed_.store(0, std::memory_order_relaxed);
    instances_retrieved_.store(0, std::memory_order_relaxed);
    bytes_retrieved_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Implementation
// =============================================================================

uint16_t retrieve_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    // Wrap around at 0xFFFF, skip 0 (reserved)
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

std::string_view retrieve_scu::get_move_sop_class_uid() const noexcept {
    switch (config_.model) {
        case query_model::patient_root:
            return patient_root_move_sop_class_uid;
        case query_model::study_root:
            return study_root_move_sop_class_uid;
        default:
            return study_root_move_sop_class_uid;
    }
}

std::string_view retrieve_scu::get_get_sop_class_uid() const noexcept {
    switch (config_.model) {
        case query_model::patient_root:
            return patient_root_get_sop_class_uid;
        case query_model::study_root:
            return study_root_get_sop_class_uid;
        default:
            return study_root_get_sop_class_uid;
    }
}

core::dicom_dataset retrieve_scu::build_study_query(
    std::string_view study_uid) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
    ds.set_string(tags::study_instance_uid, vr_type::UI, std::string(study_uid));

    return ds;
}

core::dicom_dataset retrieve_scu::build_series_query(
    std::string_view series_uid) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "SERIES");
    ds.set_string(tags::series_instance_uid, vr_type::UI, std::string(series_uid));

    return ds;
}

core::dicom_dataset retrieve_scu::build_instance_query(
    std::string_view sop_instance_uid) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "IMAGE");
    ds.set_string(tags::sop_instance_uid, vr_type::UI, std::string(sop_instance_uid));

    return ds;
}

}  // namespace pacs::services
