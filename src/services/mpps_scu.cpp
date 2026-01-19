/**
 * @file mpps_scu.cpp
 * @brief Implementation of the MPPS (Modality Performed Procedure Step) SCU service
 */

#include "pacs/services/mpps_scu.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

namespace pacs::services {

// =============================================================================
// Local Helper Functions
// =============================================================================

namespace {

/// UID root for auto-generated MPPS UIDs
constexpr const char* uid_root = "1.2.826.0.1.3680043.2.1545";

/// Requested SOP Instance UID tag (used in N-SET command set)
constexpr core::dicom_tag tag_requested_sop_instance_uid{0x0000, 0x1001};

/**
 * @brief Create N-CREATE request message for MPPS
 */
network::dimse::dimse_message make_n_create_rq(
    uint16_t message_id,
    std::string_view sop_instance_uid,
    core::dicom_dataset dataset) {

    using namespace network::dimse;

    dimse_message msg{command_field::n_create_rq, message_id};
    msg.set_affected_sop_class_uid(mpps_sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_dataset(std::move(dataset));

    return msg;
}

/**
 * @brief Create N-SET request message for MPPS
 */
network::dimse::dimse_message make_n_set_rq(
    uint16_t message_id,
    std::string_view sop_instance_uid,
    core::dicom_dataset modifications) {

    using namespace network::dimse;
    using namespace encoding;

    dimse_message msg{command_field::n_set_rq, message_id};
    msg.set_affected_sop_class_uid(mpps_sop_class_uid);

    // For N-SET, use Requested SOP Instance UID in command set
    msg.command_set().set_string(
        tag_requested_sop_instance_uid,
        vr_type::UI,
        std::string(sop_instance_uid));
    msg.set_dataset(std::move(modifications));

    return msg;
}

}  // namespace

// =============================================================================
// Construction
// =============================================================================

mpps_scu::mpps_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

mpps_scu::mpps_scu(const mpps_scu_config& config,
                   std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()),
      config_(config) {}

// =============================================================================
// N-CREATE Operation
// =============================================================================

network::Result<mpps_result> mpps_scu::create(
    network::association& assoc,
    const mpps_create_data& data) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get accepted presentation context for MPPS
    auto context_id = assoc.accepted_context_id(mpps_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::mpps_context_not_accepted,
            "No accepted presentation context for MPPS SOP Class");
    }

    // Generate or use provided MPPS UID
    std::string mpps_uid = data.mpps_sop_instance_uid;
    if (mpps_uid.empty() && config_.auto_generate_uid) {
        mpps_uid = generate_mpps_uid();
    }

    if (mpps_uid.empty()) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::mpps_missing_uid,
            "MPPS SOP Instance UID is required");
    }

    // Build the MPPS creation dataset
    auto dataset = build_create_dataset(data);

    // Create the N-CREATE request
    auto request = make_n_create_rq(next_message_id(), mpps_uid, std::move(dataset));

    logger_->debug("Sending N-CREATE request for MPPS: " + mpps_uid);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-CREATE: " + send_result.error().message);
        return send_result.error();
    }

    // Receive the response
    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-CREATE response: " + recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    // Verify it's an N-CREATE response
    if (response.command() != command_field::n_create_rsp) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::mpps_unexpected_command,
            "Expected N-CREATE-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Build result
    mpps_result result;
    result.mpps_sop_instance_uid = mpps_uid;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    // Extract error comment if present
    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    // Update statistics
    creates_performed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-CREATE successful for MPPS: " + mpps_uid);
    } else {
        logger_->warn("N-CREATE returned status 0x" +
                      std::to_string(result.status) + " for MPPS: " + mpps_uid);
    }

    return result;
}

// =============================================================================
// N-SET Operations
// =============================================================================

network::Result<mpps_result> mpps_scu::set(
    network::association& assoc,
    const mpps_set_data& data) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Validate MPPS UID is provided
    if (data.mpps_sop_instance_uid.empty()) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::mpps_missing_uid,
            "MPPS SOP Instance UID is required for N-SET");
    }

    // Validate status is not IN PROGRESS (can only set to COMPLETED or DISCONTINUED)
    if (data.status == mpps_status::in_progress) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::mpps_invalid_status_transition,
            "Cannot set MPPS status back to IN PROGRESS");
    }

    // Get accepted presentation context for MPPS
    auto context_id = assoc.accepted_context_id(mpps_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::mpps_context_not_accepted,
            "No accepted presentation context for MPPS SOP Class");
    }

    // Build the modification dataset
    auto dataset = build_set_dataset(data);

    // Create the N-SET request
    auto request = make_n_set_rq(
        next_message_id(),
        data.mpps_sop_instance_uid,
        std::move(dataset));

    logger_->debug("Sending N-SET request for MPPS: " + data.mpps_sop_instance_uid);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-SET: " + send_result.error().message);
        return send_result.error();
    }

    // Receive the response
    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-SET response: " + recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    // Verify it's an N-SET response
    if (response.command() != command_field::n_set_rsp) {
        return pacs::pacs_error<mpps_result>(
            pacs::error_codes::mpps_unexpected_command,
            "Expected N-SET-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Build result
    mpps_result result;
    result.mpps_sop_instance_uid = data.mpps_sop_instance_uid;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    // Extract error comment if present
    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    // Update statistics
    sets_performed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-SET successful for MPPS: " + data.mpps_sop_instance_uid +
                      " (status: " + std::string(to_string(data.status)) + ")");
    } else {
        logger_->warn("N-SET returned status 0x" +
                      std::to_string(result.status) + " for MPPS: " +
                      data.mpps_sop_instance_uid);
    }

    return result;
}

network::Result<mpps_result> mpps_scu::complete(
    network::association& assoc,
    std::string_view mpps_uid,
    const std::vector<performed_series_info>& performed_series) {

    mpps_set_data data;
    data.mpps_sop_instance_uid = std::string(mpps_uid);
    data.status = mpps_status::completed;
    data.procedure_step_end_date = get_current_date();
    data.procedure_step_end_time = get_current_time();
    data.performed_series = performed_series;

    return set(assoc, data);
}

network::Result<mpps_result> mpps_scu::discontinue(
    network::association& assoc,
    std::string_view mpps_uid,
    std::string_view reason) {

    mpps_set_data data;
    data.mpps_sop_instance_uid = std::string(mpps_uid);
    data.status = mpps_status::discontinued;
    data.procedure_step_end_date = get_current_date();
    data.procedure_step_end_time = get_current_time();
    data.discontinuation_reason = std::string(reason);

    return set(assoc, data);
}

// =============================================================================
// Statistics
// =============================================================================

size_t mpps_scu::creates_performed() const noexcept {
    return creates_performed_.load(std::memory_order_relaxed);
}

size_t mpps_scu::sets_performed() const noexcept {
    return sets_performed_.load(std::memory_order_relaxed);
}

void mpps_scu::reset_statistics() noexcept {
    creates_performed_.store(0, std::memory_order_relaxed);
    sets_performed_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Implementation - Dataset Building
// =============================================================================

core::dicom_dataset mpps_scu::build_create_dataset(const mpps_create_data& data) const {
    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Performed Procedure Step Status - always IN PROGRESS for N-CREATE
    ds.set_string(mpps_tags::performed_procedure_step_status, vr_type::CS, "IN PROGRESS");

    // Performed Procedure Step Timing
    std::string start_date = data.procedure_step_start_date.empty()
        ? get_current_date() : data.procedure_step_start_date;
    std::string start_time = data.procedure_step_start_time.empty()
        ? get_current_time() : data.procedure_step_start_time;

    ds.set_string(mpps_tags::performed_procedure_step_start_date, vr_type::DA, start_date);
    ds.set_string(mpps_tags::performed_procedure_step_start_time, vr_type::TM, start_time);

    // Modality and Station
    if (!data.modality.empty()) {
        ds.set_string(tags::modality, vr_type::CS, data.modality);
    }
    if (!data.station_ae_title.empty()) {
        ds.set_string(mpps_tags::performed_station_ae_title, vr_type::AE, data.station_ae_title);
    }
    if (!data.station_name.empty()) {
        ds.set_string(mpps_tags::performed_station_name, vr_type::SH, data.station_name);
    }

    // Procedure Step ID
    if (!data.scheduled_procedure_step_id.empty()) {
        ds.set_string(mpps_tags::performed_procedure_step_id, vr_type::SH,
                      data.scheduled_procedure_step_id);
    }

    // Procedure Description
    if (!data.procedure_description.empty()) {
        ds.set_string(mpps_tags::performed_procedure_step_description, vr_type::LO,
                      data.procedure_description);
    }

    // Patient Information (required)
    std::string patient_name = data.patient_name.empty() ? "ANONYMOUS" : data.patient_name;
    ds.set_string(tags::patient_name, vr_type::PN, patient_name);

    if (!data.patient_id.empty()) {
        ds.set_string(tags::patient_id, vr_type::LO, data.patient_id);
    }
    if (!data.patient_birth_date.empty()) {
        ds.set_string(tags::patient_birth_date, vr_type::DA, data.patient_birth_date);
    }
    if (!data.patient_sex.empty()) {
        ds.set_string(tags::patient_sex, vr_type::CS, data.patient_sex);
    }

    // Study reference
    if (!data.study_instance_uid.empty()) {
        ds.set_string(tags::study_instance_uid, vr_type::UI, data.study_instance_uid);
    }
    if (!data.accession_number.empty()) {
        ds.set_string(tags::accession_number, vr_type::SH, data.accession_number);
    }

    // Performing Information
    if (!data.performing_physician.empty()) {
        ds.set_string(mpps_tags::performing_physicians_name, vr_type::PN,
                      data.performing_physician);
    }
    if (!data.operator_name.empty()) {
        ds.set_string(mpps_tags::operators_name, vr_type::PN, data.operator_name);
    }

    return ds;
}

core::dicom_dataset mpps_scu::build_set_dataset(const mpps_set_data& data) const {
    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Update status
    ds.set_string(mpps_tags::performed_procedure_step_status, vr_type::CS,
                  std::string(to_string(data.status)));

    // End date/time
    std::string end_date = data.procedure_step_end_date.empty()
        ? get_current_date() : data.procedure_step_end_date;
    std::string end_time = data.procedure_step_end_time.empty()
        ? get_current_time() : data.procedure_step_end_time;

    ds.set_string(mpps_tags::performed_procedure_step_end_date, vr_type::DA, end_date);
    ds.set_string(mpps_tags::performed_procedure_step_end_time, vr_type::TM, end_time);

    // Add performed series sequence for COMPLETED status
    if (data.status == mpps_status::completed && !data.performed_series.empty()) {
        auto& seq = ds.get_or_create_sequence(mpps_tags::performed_series_sequence);
        for (const auto& s : data.performed_series) {
            dicom_dataset series_item;

            if (!s.series_uid.empty()) {
                series_item.set_string(tags::series_instance_uid, vr_type::UI, s.series_uid);
            }
            if (!s.series_description.empty()) {
                series_item.set_string(mpps_tags::series_description, vr_type::LO,
                                       s.series_description);
            }
            if (!s.modality.empty()) {
                series_item.set_string(tags::modality, vr_type::CS, s.modality);
            }
            if (!s.performing_physician.empty()) {
                series_item.set_string(mpps_tags::performing_physicians_name, vr_type::PN,
                                       s.performing_physician);
            }
            if (!s.operator_name.empty()) {
                series_item.set_string(mpps_tags::operators_name, vr_type::PN, s.operator_name);
            }

            seq.push_back(std::move(series_item));
        }
    }

    return ds;
}

// =============================================================================
// Private Implementation - Utility Functions
// =============================================================================

std::string mpps_scu::generate_mpps_uid() const {
    static std::mt19937_64 gen{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return std::string(uid_root) + "." + std::to_string(timestamp) +
           "." + std::to_string(dist(gen) % 100000);
}

std::string mpps_scu::get_current_date() const {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y%m%d");
    return oss.str();
}

std::string mpps_scu::get_current_time() const {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "%H%M%S");
    return oss.str();
}

uint16_t mpps_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    // Wrap around at 0xFFFF, skip 0 (reserved)
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

}  // namespace pacs::services
