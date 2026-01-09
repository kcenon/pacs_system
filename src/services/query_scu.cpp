/**
 * @file query_scu.cpp
 * @brief Implementation of the Query SCU service
 *
 * @see Issue #531 - Implement query_scu Library (C-FIND SCU)
 */

#include "pacs/services/query_scu.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

query_scu::query_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

query_scu::query_scu(const query_scu_config& config,
                     std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()), config_(config) {}

// =============================================================================
// Generic Query Operations
// =============================================================================

network::Result<query_result> query_scu::find(
    network::association& assoc,
    const core::dicom_dataset& query_keys) {

    return find_impl(assoc, query_keys, next_message_id());
}

network::Result<query_result> query_scu::find_impl(
    network::association& assoc,
    const core::dicom_dataset& query_keys,
    uint16_t message_id) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<query_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get SOP Class UID
    auto sop_class_uid = get_sop_class_uid();

    // Get accepted presentation context for this SOP class
    auto context_id = assoc.accepted_context_id(sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<query_result>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for SOP Class: " +
            std::string(sop_class_uid));
    }

    // Build C-FIND-RQ message
    auto request = make_c_find_rq(message_id, sop_class_uid);
    request.set_dataset(query_keys);

    logger_->debug_fmt("Sending C-FIND request (message_id={}, sop_class={})",
                       message_id, sop_class_uid);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        return send_result.error();
    }

    // Receive responses
    query_result result;
    bool query_complete = false;

    while (!query_complete) {
        auto recv_result = assoc.receive_dimse(config_.timeout);
        if (recv_result.is_err()) {
            return recv_result.error();
        }

        const auto& [recv_context_id, response] = recv_result.value();

        // Verify it's a C-FIND response
        if (response.command() != command_field::c_find_rsp) {
            return pacs::pacs_error<query_result>(
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
                    result.matches.size() < config_.max_results) {

                    auto dataset_result = response.dataset();
                    if (dataset_result.is_ok()) {
                        result.matches.push_back(dataset_result.value().get());
                    }
                }

                // Check if we should cancel due to max_results
                if (config_.max_results > 0 &&
                    result.matches.size() >= config_.max_results &&
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
    total_matches_.fetch_add(result.matches.size(), std::memory_order_relaxed);

    logger_->debug_fmt("C-FIND completed: {} matches in {} ms",
                       result.matches.size(), result.elapsed.count());

    return result;
}

network::Result<size_t> query_scu::find_streaming(
    network::association& assoc,
    const core::dicom_dataset& query_keys,
    query_streaming_callback callback) {

    using namespace network::dimse;

    auto message_id = next_message_id();
    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<size_t>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get SOP Class UID
    auto sop_class_uid = get_sop_class_uid();

    // Get accepted presentation context for this SOP class
    auto context_id = assoc.accepted_context_id(sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<size_t>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for SOP Class: " +
            std::string(sop_class_uid));
    }

    // Build C-FIND-RQ message
    auto request = make_c_find_rq(message_id, sop_class_uid);
    request.set_dataset(query_keys);

    logger_->debug_fmt("Sending streaming C-FIND request (message_id={})",
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

                    // Call the callback
                    if (callback) {
                        if (!callback(dataset_result.value().get())) {
                            should_cancel = true;
                        }
                    }

                    // Check if we need to cancel
                    if (should_cancel ||
                        (config_.max_results > 0 &&
                         count >= config_.max_results &&
                         config_.cancel_on_max)) {

                        logger_->debug("Cancelling streaming query");
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
    total_matches_.fetch_add(count, std::memory_order_relaxed);

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    logger_->debug_fmt("Streaming C-FIND completed: {} results in {} ms",
                       count, elapsed.count());

    return count;
}

// =============================================================================
// Typed Convenience Methods
// =============================================================================

network::Result<query_result> query_scu::find_patients(
    network::association& assoc,
    const patient_query_keys& keys) {

    auto saved_level = config_.level;
    config_.level = query_level::patient;

    auto query_ds = build_query_dataset(keys);
    auto result = find(assoc, query_ds);

    config_.level = saved_level;
    return result;
}

network::Result<query_result> query_scu::find_studies(
    network::association& assoc,
    const study_query_keys& keys) {

    auto saved_level = config_.level;
    config_.level = query_level::study;

    auto query_ds = build_query_dataset(keys);
    auto result = find(assoc, query_ds);

    config_.level = saved_level;
    return result;
}

network::Result<query_result> query_scu::find_series(
    network::association& assoc,
    const series_query_keys& keys) {

    auto saved_level = config_.level;
    config_.level = query_level::series;

    auto query_ds = build_query_dataset(keys);
    auto result = find(assoc, query_ds);

    config_.level = saved_level;
    return result;
}

network::Result<query_result> query_scu::find_instances(
    network::association& assoc,
    const instance_query_keys& keys) {

    auto saved_level = config_.level;
    config_.level = query_level::image;

    auto query_ds = build_query_dataset(keys);
    auto result = find(assoc, query_ds);

    config_.level = saved_level;
    return result;
}

// =============================================================================
// C-CANCEL Support
// =============================================================================

network::Result<std::monostate> query_scu::cancel(
    network::association& assoc,
    uint16_t message_id) {

    using namespace network::dimse;

    auto sop_class_uid = get_sop_class_uid();

    auto context_id = assoc.accepted_context_id(sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<std::monostate>(
            pacs::error_codes::no_acceptable_context,
            "No accepted presentation context for cancel");
    }

    // Build C-CANCEL-RQ manually since there's no factory function
    dimse_message cancel_rq{command_field::c_cancel_rq, message_id};
    return assoc.send_dimse(*context_id, cancel_rq);
}

// =============================================================================
// Configuration
// =============================================================================

void query_scu::set_config(const query_scu_config& config) {
    config_ = config;
}

const query_scu_config& query_scu::config() const noexcept {
    return config_;
}

// =============================================================================
// Statistics
// =============================================================================

size_t query_scu::queries_performed() const noexcept {
    return queries_performed_.load(std::memory_order_relaxed);
}

size_t query_scu::total_matches() const noexcept {
    return total_matches_.load(std::memory_order_relaxed);
}

void query_scu::reset_statistics() noexcept {
    queries_performed_.store(0, std::memory_order_relaxed);
    total_matches_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Implementation
// =============================================================================

uint16_t query_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    // Wrap around at 0xFFFF, skip 0 (reserved)
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

core::dicom_dataset query_scu::build_query_dataset(
    const patient_query_keys& keys) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Set Query/Retrieve Level
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "PATIENT");

    // Set query keys (empty string = return key, non-empty = match key)
    ds.set_string(tags::patient_name, vr_type::PN, keys.patient_name);
    ds.set_string(tags::patient_id, vr_type::LO, keys.patient_id);
    ds.set_string(tags::patient_birth_date, vr_type::DA, keys.birth_date);
    ds.set_string(tags::patient_sex, vr_type::CS, keys.sex);

    // Add standard return keys
    ds.set_string(tags::number_of_patient_related_studies, vr_type::IS, "");
    ds.set_string(tags::number_of_patient_related_series, vr_type::IS, "");
    ds.set_string(tags::number_of_patient_related_instances, vr_type::IS, "");

    return ds;
}

core::dicom_dataset query_scu::build_query_dataset(
    const study_query_keys& keys) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Set Query/Retrieve Level
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");

    // Patient info (return keys)
    ds.set_string(tags::patient_name, vr_type::PN, "");
    ds.set_string(tags::patient_id, vr_type::LO, keys.patient_id);

    // Study keys
    ds.set_string(tags::study_instance_uid, vr_type::UI, keys.study_uid);
    ds.set_string(tags::study_date, vr_type::DA, keys.study_date);
    ds.set_string(tags::study_time, vr_type::TM, "");
    ds.set_string(tags::accession_number, vr_type::SH, keys.accession_number);
    ds.set_string(tags::study_id, vr_type::SH, "");
    ds.set_string(tags::study_description, vr_type::LO, keys.study_description);
    ds.set_string(tags::modalities_in_study, vr_type::CS, keys.modality);

    // Standard return keys
    ds.set_string(tags::number_of_study_related_series, vr_type::IS, "");
    ds.set_string(tags::number_of_study_related_instances, vr_type::IS, "");

    return ds;
}

core::dicom_dataset query_scu::build_query_dataset(
    const series_query_keys& keys) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Set Query/Retrieve Level
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "SERIES");

    // Study UID (required for series query)
    ds.set_string(tags::study_instance_uid, vr_type::UI, keys.study_uid);

    // Series keys
    ds.set_string(tags::series_instance_uid, vr_type::UI, keys.series_uid);
    ds.set_string(tags::modality, vr_type::CS, keys.modality);
    ds.set_string(tags::series_number, vr_type::IS, keys.series_number);
    ds.set_string(tags::series_description, vr_type::LO, "");

    // Standard return keys
    ds.set_string(tags::number_of_series_related_instances, vr_type::IS, "");

    return ds;
}

core::dicom_dataset query_scu::build_query_dataset(
    const instance_query_keys& keys) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Set Query/Retrieve Level
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "IMAGE");

    // Series UID (required for instance query)
    ds.set_string(tags::series_instance_uid, vr_type::UI, keys.series_uid);

    // Instance keys
    ds.set_string(tags::sop_instance_uid, vr_type::UI, keys.sop_instance_uid);
    ds.set_string(tags::sop_class_uid, vr_type::UI, "");
    ds.set_string(tags::instance_number, vr_type::IS, keys.instance_number);

    return ds;
}

std::string_view query_scu::get_sop_class_uid() const noexcept {
    return get_find_sop_class_uid(config_.model);
}

}  // namespace pacs::services
