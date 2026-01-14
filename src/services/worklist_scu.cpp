/**
 * @file worklist_scu.cpp
 * @brief Implementation of the Worklist SCU service
 *
 * @see Issue #533 - Implement worklist_scu Library (MWL SCU)
 */

#include "pacs/services/worklist_scu.hpp"
#include "pacs/services/worklist_scp.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

worklist_scu::worklist_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

worklist_scu::worklist_scu(const worklist_scu_config& config,
                           std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()), config_(config) {}

// =============================================================================
// Generic Query Operations
// =============================================================================

network::Result<worklist_result> worklist_scu::query(
    network::association& assoc,
    const worklist_query_keys& keys) {

    auto query_ds = build_query_dataset(keys);
    return query(assoc, query_ds);
}

network::Result<worklist_result> worklist_scu::query(
    network::association& assoc,
    const core::dicom_dataset& query_keys) {

    return query_impl(assoc, query_keys, next_message_id());
}

network::Result<worklist_result> worklist_scu::query_impl(
    network::association& assoc,
    const core::dicom_dataset& query_keys,
    uint16_t message_id) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<worklist_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get accepted presentation context for MWL
    auto context_id = assoc.accepted_context_id(worklist_find_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<worklist_result>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for Modality Worklist: " +
            std::string(worklist_find_sop_class_uid));
    }

    // Build C-FIND-RQ message
    auto request = make_c_find_rq(message_id, worklist_find_sop_class_uid);
    request.set_dataset(query_keys);

    logger_->debug_fmt("Sending MWL C-FIND request (message_id={})", message_id);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        return send_result.error();
    }

    // Receive responses
    worklist_result result;
    bool query_complete = false;

    while (!query_complete) {
        auto recv_result = assoc.receive_dimse(config_.timeout);
        if (recv_result.is_err()) {
            return recv_result.error();
        }

        const auto& [recv_context_id, response] = recv_result.value();

        // Verify it's a C-FIND response
        if (response.command() != command_field::c_find_rsp) {
            return pacs::pacs_error<worklist_result>(
                pacs::error_codes::find_unexpected_command,
                "Expected C-FIND-RSP but received " +
                std::string(to_string(response.command())));
        }

        auto status = response.status();

        if (status == status_pending || status == status_pending_warning) {
            ++result.total_pending;

            if (response.has_dataset()) {
                // Check if we should collect this result
                if (config_.max_results == 0 ||
                    result.items.size() < config_.max_results) {

                    auto dataset_result = response.dataset();
                    if (dataset_result.is_ok()) {
                        result.items.push_back(
                            parse_worklist_item(dataset_result.value().get()));
                    }
                }

                // Check if we should cancel due to max_results
                if (config_.max_results > 0 &&
                    result.items.size() >= config_.max_results &&
                    config_.cancel_on_max) {

                    logger_->debug_fmt(
                        "Max results ({}) reached, sending C-CANCEL",
                        config_.max_results);

                    // Send C-CANCEL
                    auto cancel_result = cancel(assoc, message_id);
                    if (cancel_result.is_err()) {
                        logger_->warn_fmt("Failed to send C-CANCEL: {}",
                                          cancel_result.error().message);
                    }
                }
            }
        } else if (status == status_success) {
            query_complete = true;
            result.status = static_cast<uint16_t>(status);
        } else if (status == status_cancel) {
            query_complete = true;
            result.status = static_cast<uint16_t>(status);
        } else {
            // Error status
            query_complete = true;
            result.status = static_cast<uint16_t>(status);
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Update statistics
    queries_performed_.fetch_add(1, std::memory_order_relaxed);
    total_items_.fetch_add(result.items.size(), std::memory_order_relaxed);

    logger_->debug_fmt("MWL C-FIND completed: {} items in {} ms",
                       result.items.size(), result.elapsed.count());

    return result;
}

// =============================================================================
// Convenience Query Methods
// =============================================================================

network::Result<worklist_result> worklist_scu::query_today(
    network::association& assoc,
    std::string_view station_ae,
    std::string_view modality) {

    worklist_query_keys keys;
    keys.scheduled_station_ae = std::string(station_ae);
    keys.modality = std::string(modality);
    keys.scheduled_date = get_today_date();

    return query(assoc, keys);
}

network::Result<worklist_result> worklist_scu::query_date_range(
    network::association& assoc,
    std::string_view start_date,
    std::string_view end_date,
    std::string_view modality) {

    worklist_query_keys keys;
    // DICOM date range format: YYYYMMDD-YYYYMMDD
    keys.scheduled_date = std::string(start_date) + "-" + std::string(end_date);
    keys.modality = std::string(modality);

    return query(assoc, keys);
}

network::Result<worklist_result> worklist_scu::query_patient(
    network::association& assoc,
    std::string_view patient_id) {

    worklist_query_keys keys;
    keys.patient_id = std::string(patient_id);

    return query(assoc, keys);
}

// =============================================================================
// Streaming Query
// =============================================================================

network::Result<size_t> worklist_scu::query_streaming(
    network::association& assoc,
    const worklist_query_keys& keys,
    worklist_streaming_callback callback) {

    using namespace network::dimse;

    auto message_id = next_message_id();
    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<size_t>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get accepted presentation context for MWL
    auto context_id = assoc.accepted_context_id(worklist_find_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<size_t>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for Modality Worklist");
    }

    // Build query dataset and C-FIND-RQ message
    auto query_ds = build_query_dataset(keys);
    auto request = make_c_find_rq(message_id, worklist_find_sop_class_uid);
    request.set_dataset(query_ds);

    logger_->debug_fmt("Sending streaming MWL C-FIND request (message_id={})",
                       message_id);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        return send_result.error();
    }

    // Receive responses
    size_t count = 0;
    bool query_complete = false;
    bool should_cancel = false;

    while (!query_complete) {
        auto recv_result = assoc.receive_dimse(config_.timeout);
        if (recv_result.is_err()) {
            return recv_result.error();
        }

        const auto& [recv_context_id, response] = recv_result.value();

        // Verify it's a C-FIND response
        if (response.command() != command_field::c_find_rsp) {
            return pacs::pacs_error<size_t>(
                pacs::error_codes::find_unexpected_command,
                "Expected C-FIND-RSP but received " +
                std::string(to_string(response.command())));
        }

        auto status = response.status();

        if (status == status_pending || status == status_pending_warning) {
            if (response.has_dataset()) {
                auto dataset_result = response.dataset();
                if (dataset_result.is_ok()) {
                    ++count;
                    auto item = parse_worklist_item(dataset_result.value().get());

                    // Call the callback
                    if (callback) {
                        if (!callback(item)) {
                            should_cancel = true;
                        }
                    }

                    // Check if we need to cancel
                    if (should_cancel ||
                        (config_.max_results > 0 &&
                         count >= config_.max_results &&
                         config_.cancel_on_max)) {

                        logger_->debug("Cancelling streaming MWL query");
                        auto cancel_result = cancel(assoc, message_id);
                        if (cancel_result.is_err()) {
                            logger_->warn_fmt("Failed to send C-CANCEL: {}",
                                              cancel_result.error().message);
                        }
                        should_cancel = false;  // Already sent cancel
                    }
                }
            }
        } else {
            query_complete = true;
        }
    }

    // Update statistics
    queries_performed_.fetch_add(1, std::memory_order_relaxed);
    total_items_.fetch_add(count, std::memory_order_relaxed);

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    logger_->debug_fmt("Streaming MWL C-FIND completed: {} items in {} ms",
                       count, elapsed.count());

    return count;
}

// =============================================================================
// C-CANCEL Support
// =============================================================================

network::Result<std::monostate> worklist_scu::cancel(
    network::association& assoc,
    uint16_t message_id) {

    using namespace network::dimse;

    auto context_id = assoc.accepted_context_id(worklist_find_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<std::monostate>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for cancel");
    }

    // Build C-CANCEL-RQ
    dimse_message cancel_rq{command_field::c_cancel_rq, message_id};
    return assoc.send_dimse(*context_id, cancel_rq);
}

// =============================================================================
// Configuration
// =============================================================================

void worklist_scu::set_config(const worklist_scu_config& config) {
    config_ = config;
}

const worklist_scu_config& worklist_scu::config() const noexcept {
    return config_;
}

// =============================================================================
// Statistics
// =============================================================================

size_t worklist_scu::queries_performed() const noexcept {
    return queries_performed_.load(std::memory_order_relaxed);
}

size_t worklist_scu::total_items() const noexcept {
    return total_items_.load(std::memory_order_relaxed);
}

void worklist_scu::reset_statistics() noexcept {
    queries_performed_.store(0, std::memory_order_relaxed);
    total_items_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Implementation
// =============================================================================

uint16_t worklist_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    // Wrap around at 0xFFFF, skip 0 (reserved)
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

std::string worklist_scu::get_today_date() {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y%m%d");
    return oss.str();
}

worklist_item worklist_scu::parse_worklist_item(
    const core::dicom_dataset& ds) const {

    using namespace core;

    worklist_item item;

    // Patient demographics
    item.patient_name = ds.get_string(tags::patient_name);
    item.patient_id = ds.get_string(tags::patient_id);
    item.patient_birth_date = ds.get_string(tags::patient_birth_date);
    item.patient_sex = ds.get_string(tags::patient_sex);

    // Study-level attributes
    item.study_instance_uid = ds.get_string(tags::study_instance_uid);
    item.accession_number = ds.get_string(tags::accession_number);
    item.referring_physician = ds.get_string(tags::referring_physician_name);
    item.institution = ds.get_string(tags::institution_name);

    // Requested Procedure attributes
    item.requested_procedure_id = ds.get_string(tags::requested_procedure_id);
    item.requested_procedure_description = ds.get_string(tags::study_description);

    // Scheduled Procedure Step attributes (flat structure)
    item.scheduled_station_ae = ds.get_string(tags::scheduled_station_ae_title);
    item.modality = ds.get_string(tags::modality);
    item.scheduled_date = ds.get_string(tags::scheduled_procedure_step_start_date);
    item.scheduled_time = ds.get_string(tags::scheduled_procedure_step_start_time);
    item.scheduled_procedure_step_id = ds.get_string(tags::scheduled_procedure_step_id);
    item.scheduled_procedure_step_description =
        ds.get_string(tags::scheduled_procedure_step_description);

    // Store original dataset for full access
    item.dataset = ds;

    return item;
}

core::dicom_dataset worklist_scu::build_query_dataset(
    const worklist_query_keys& keys) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Patient demographics (return keys with optional search criteria)
    ds.set_string(tags::patient_name, vr_type::PN, keys.patient_name);
    ds.set_string(tags::patient_id, vr_type::LO, keys.patient_id);

    if (!keys.patient_birth_date.empty()) {
        ds.set_string(tags::patient_birth_date, vr_type::DA, keys.patient_birth_date);
    } else {
        ds.set_string(tags::patient_birth_date, vr_type::DA, "");
    }

    if (!keys.patient_sex.empty()) {
        ds.set_string(tags::patient_sex, vr_type::CS, keys.patient_sex);
    } else {
        ds.set_string(tags::patient_sex, vr_type::CS, "");
    }

    // Study-level return keys
    ds.set_string(tags::study_instance_uid, vr_type::UI, "");
    ds.set_string(tags::accession_number, vr_type::SH, keys.accession_number);
    ds.set_string(tags::referring_physician_name, vr_type::PN, keys.referring_physician);
    ds.set_string(tags::institution_name, vr_type::LO, keys.institution);

    // Requested Procedure attributes
    ds.set_string(tags::requested_procedure_id, vr_type::SH, keys.requested_procedure_id);
    ds.set_string(tags::study_description, vr_type::LO, keys.requested_procedure_description);

    // Scheduled Procedure Step attributes (flat structure)
    ds.set_string(tags::scheduled_station_ae_title, vr_type::AE, keys.scheduled_station_ae);
    ds.set_string(tags::scheduled_procedure_step_start_date, vr_type::DA, keys.scheduled_date);
    ds.set_string(tags::scheduled_procedure_step_start_time, vr_type::TM, keys.scheduled_time);
    ds.set_string(tags::modality, vr_type::CS, keys.modality);
    ds.set_string(tags::scheduled_performing_physician_name, vr_type::PN, keys.scheduled_physician);
    ds.set_string(tags::scheduled_procedure_step_id, vr_type::SH, keys.scheduled_procedure_step_id);

    // Additional return keys
    ds.set_string(tags::scheduled_procedure_step_description, vr_type::LO, "");
    ds.set_string(tags::scheduled_station_name, vr_type::SH, "");
    ds.set_string(tags::scheduled_procedure_step_location, vr_type::SH, "");

    return ds;
}

}  // namespace pacs::services
