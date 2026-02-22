// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file print_scu.cpp
 * @brief Implementation of the Print Management SCU service (PS3.4 Annex H)
 */

#include "pacs/services/print_scu.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

#include <chrono>
#include <random>
#include <sstream>

namespace pacs::services {

// =============================================================================
// Local Helper Functions
// =============================================================================

namespace {

/// UID root for auto-generated Print UIDs
constexpr const char* uid_root = "1.2.826.0.1.3680043.2.1545.1";

/// Requested SOP Instance UID tag (used in N-SET/N-GET command set)
constexpr core::dicom_tag tag_requested_sop_instance_uid{0x0000, 0x1001};

/**
 * @brief Create N-CREATE request message for a Print SOP Class
 */
network::dimse::dimse_message make_n_create_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    core::dicom_dataset dataset) {

    using namespace network::dimse;

    dimse_message msg{command_field::n_create_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    if (!sop_instance_uid.empty()) {
        msg.set_affected_sop_instance_uid(sop_instance_uid);
    }
    msg.set_dataset(std::move(dataset));

    return msg;
}

/**
 * @brief Create N-SET request message
 */
network::dimse::dimse_message make_n_set_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    core::dicom_dataset modifications) {

    using namespace network::dimse;
    using namespace encoding;

    dimse_message msg{command_field::n_set_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.command_set().set_string(
        tag_requested_sop_instance_uid,
        vr_type::UI,
        std::string(sop_instance_uid));
    msg.set_dataset(std::move(modifications));

    return msg;
}

/**
 * @brief Create N-GET request message
 */
network::dimse::dimse_message make_n_get_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid) {

    using namespace network::dimse;
    using namespace encoding;

    dimse_message msg{command_field::n_get_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.command_set().set_string(
        tag_requested_sop_instance_uid,
        vr_type::UI,
        std::string(sop_instance_uid));

    return msg;
}

/**
 * @brief Create N-ACTION request message
 */
network::dimse::dimse_message make_n_action_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    uint16_t action_type_id) {

    using namespace network::dimse;

    dimse_message msg{command_field::n_action_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_action_type_id(action_type_id);

    return msg;
}

/**
 * @brief Create N-DELETE request message
 */
network::dimse::dimse_message make_n_delete_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid) {

    using namespace network::dimse;

    dimse_message msg{command_field::n_delete_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);

    return msg;
}

}  // namespace

// =============================================================================
// Construction
// =============================================================================

print_scu::print_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

print_scu::print_scu(const print_scu_config& config,
                     std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()),
      config_(config) {}

// =============================================================================
// Film Session Operations
// =============================================================================

network::Result<print_result> print_scu::create_film_session(
    network::association& assoc,
    const print_session_data& data) {

    using namespace network::dimse;
    using namespace encoding;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    auto context_id = find_print_context(assoc, basic_film_session_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_invalid_sop_class,
            "No accepted presentation context for Film Session SOP Class");
    }

    std::string session_uid = data.sop_instance_uid;
    if (session_uid.empty() && config_.auto_generate_uid) {
        session_uid = generate_uid();
    }

    // Build creation dataset
    core::dicom_dataset ds;
    ds.set_string(print_tags::number_of_copies, vr_type::IS,
                  std::to_string(data.number_of_copies));
    if (!data.print_priority.empty()) {
        ds.set_string(print_tags::print_priority, vr_type::CS, data.print_priority);
    }
    if (!data.medium_type.empty()) {
        ds.set_string(print_tags::medium_type, vr_type::CS, data.medium_type);
    }
    if (!data.film_destination.empty()) {
        ds.set_string(print_tags::film_destination, vr_type::CS, data.film_destination);
    }
    if (!data.film_session_label.empty()) {
        ds.set_string(print_tags::film_session_label, vr_type::LO, data.film_session_label);
    }

    auto request = make_n_create_rq(
        next_message_id(),
        basic_film_session_sop_class_uid,
        session_uid,
        std::move(ds));

    logger_->debug("Sending N-CREATE Film Session: " + session_uid);

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-CREATE Film Session: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-CREATE Film Session response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_create_rsp) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_unexpected_command,
            "Expected N-CREATE-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();

    print_result result;
    result.sop_instance_uid = response.affected_sop_instance_uid().empty()
        ? session_uid : std::string(response.affected_sop_instance_uid());
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    sessions_created_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-CREATE Film Session successful: " + result.sop_instance_uid);
    } else {
        logger_->warn("N-CREATE Film Session status 0x" +
                      std::to_string(result.status) + ": " + result.sop_instance_uid);
    }

    return result;
}

network::Result<print_result> print_scu::delete_film_session(
    network::association& assoc,
    std::string_view session_uid) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    auto context_id = find_print_context(assoc, basic_film_session_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_invalid_sop_class,
            "No accepted presentation context for Film Session SOP Class");
    }

    auto request = make_n_delete_rq(
        next_message_id(),
        basic_film_session_sop_class_uid,
        session_uid);

    logger_->debug("Sending N-DELETE Film Session: " + std::string(session_uid));

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-DELETE Film Session: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-DELETE Film Session response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_delete_rsp) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_unexpected_command,
            "Expected N-DELETE-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();

    print_result result;
    result.sop_instance_uid = std::string(session_uid);
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    if (result.is_success()) {
        logger_->info("N-DELETE Film Session successful: " + std::string(session_uid));
    } else {
        logger_->warn("N-DELETE Film Session status 0x" +
                      std::to_string(result.status) + ": " + std::string(session_uid));
    }

    return result;
}

// =============================================================================
// Film Box Operations
// =============================================================================

network::Result<print_result> print_scu::create_film_box(
    network::association& assoc,
    const print_film_box_data& data) {

    using namespace network::dimse;
    using namespace encoding;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    auto context_id = find_print_context(assoc, basic_film_box_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_invalid_sop_class,
            "No accepted presentation context for Film Box SOP Class");
    }

    // Build creation dataset
    core::dicom_dataset ds;
    if (!data.image_display_format.empty()) {
        ds.set_string(print_tags::image_display_format, vr_type::ST,
                      data.image_display_format);
    }
    if (!data.film_orientation.empty()) {
        ds.set_string(print_tags::film_orientation, vr_type::CS, data.film_orientation);
    }
    if (!data.film_size_id.empty()) {
        ds.set_string(print_tags::film_size_id, vr_type::CS, data.film_size_id);
    }
    if (!data.magnification_type.empty()) {
        ds.set_string(print_tags::magnification_type, vr_type::CS,
                      data.magnification_type);
    }

    // Add referenced film session sequence
    if (!data.film_session_uid.empty()) {
        auto& seq = ds.get_or_create_sequence(
            print_tags::referenced_film_session_sequence);
        core::dicom_dataset ref_item;
        ref_item.set_string(core::tags::referenced_sop_class_uid, vr_type::UI,
                           std::string(basic_film_session_sop_class_uid));
        ref_item.set_string(core::tags::referenced_sop_instance_uid, vr_type::UI,
                           data.film_session_uid);
        seq.push_back(std::move(ref_item));
    }

    auto request = make_n_create_rq(
        next_message_id(),
        basic_film_box_sop_class_uid,
        "",  // SCP generates the UID
        std::move(ds));

    logger_->debug("Sending N-CREATE Film Box");

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-CREATE Film Box: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-CREATE Film Box response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_create_rsp) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_unexpected_command,
            "Expected N-CREATE-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();

    print_result result;
    result.sop_instance_uid = std::string(response.affected_sop_instance_uid());
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    // Capture response dataset (contains Referenced Image Box Sequence)
    if (response.has_dataset()) {
        result.response_data = response.dataset().value().get();
    }

    film_boxes_created_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-CREATE Film Box successful: " + result.sop_instance_uid);
    } else {
        logger_->warn("N-CREATE Film Box status 0x" +
                      std::to_string(result.status));
    }

    return result;
}

network::Result<print_result> print_scu::print_film_box(
    network::association& assoc,
    std::string_view film_box_uid) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    auto context_id = find_print_context(assoc, basic_film_box_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_invalid_sop_class,
            "No accepted presentation context for Film Box SOP Class");
    }

    // Action Type ID 1 = Print
    auto request = make_n_action_rq(
        next_message_id(),
        basic_film_box_sop_class_uid,
        film_box_uid,
        1);

    logger_->debug("Sending N-ACTION Print Film Box: " + std::string(film_box_uid));

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-ACTION Print: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-ACTION Print response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_action_rsp) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_unexpected_command,
            "Expected N-ACTION-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();

    print_result result;
    result.sop_instance_uid = std::string(film_box_uid);
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    prints_executed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-ACTION Print successful: " + std::string(film_box_uid));
    } else {
        logger_->warn("N-ACTION Print status 0x" +
                      std::to_string(result.status) + ": " + std::string(film_box_uid));
    }

    return result;
}

network::Result<print_result> print_scu::delete_film_box(
    network::association& assoc,
    std::string_view film_box_uid) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    auto context_id = find_print_context(assoc, basic_film_box_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_invalid_sop_class,
            "No accepted presentation context for Film Box SOP Class");
    }

    auto request = make_n_delete_rq(
        next_message_id(),
        basic_film_box_sop_class_uid,
        film_box_uid);

    logger_->debug("Sending N-DELETE Film Box: " + std::string(film_box_uid));

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-DELETE Film Box: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-DELETE Film Box response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_delete_rsp) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_unexpected_command,
            "Expected N-DELETE-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();

    print_result result;
    result.sop_instance_uid = std::string(film_box_uid);
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    if (result.is_success()) {
        logger_->info("N-DELETE Film Box successful: " + std::string(film_box_uid));
    } else {
        logger_->warn("N-DELETE Film Box status 0x" +
                      std::to_string(result.status) + ": " + std::string(film_box_uid));
    }

    return result;
}

// =============================================================================
// Image Box Operations
// =============================================================================

network::Result<print_result> print_scu::set_image_box(
    network::association& assoc,
    std::string_view image_box_uid,
    const print_image_data& data,
    bool use_color) {

    using namespace network::dimse;
    using namespace encoding;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    auto sop_class_uid = use_color
        ? basic_color_image_box_sop_class_uid
        : basic_grayscale_image_box_sop_class_uid;

    auto context_id = find_print_context(assoc, sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_invalid_sop_class,
            "No accepted presentation context for Image Box SOP Class");
    }

    // Build modification dataset with pixel data in the appropriate sequence
    core::dicom_dataset ds = data.pixel_data;

    if (data.image_position > 0) {
        ds.set_string(print_tags::image_position, vr_type::US,
                      std::to_string(data.image_position));
    }

    auto request = make_n_set_rq(
        next_message_id(),
        sop_class_uid,
        image_box_uid,
        std::move(ds));

    logger_->debug("Sending N-SET Image Box: " + std::string(image_box_uid));

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-SET Image Box: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-SET Image Box response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_set_rsp) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_unexpected_command,
            "Expected N-SET-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();

    print_result result;
    result.sop_instance_uid = std::string(image_box_uid);
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    images_set_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-SET Image Box successful: " + std::string(image_box_uid));
    } else {
        logger_->warn("N-SET Image Box status 0x" +
                      std::to_string(result.status) + ": " + std::string(image_box_uid));
    }

    return result;
}

// =============================================================================
// Printer Status
// =============================================================================

network::Result<print_result> print_scu::query_printer_status(
    network::association& assoc) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    auto context_id = find_print_context(assoc, printer_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_invalid_sop_class,
            "No accepted presentation context for Printer SOP Class");
    }

    // Well-Known Printer SOP Instance UID (PS3.4 H.4.17)
    constexpr std::string_view printer_instance_uid =
        "1.2.840.10008.5.1.1.17";

    auto request = make_n_get_rq(
        next_message_id(),
        printer_sop_class_uid,
        printer_instance_uid);

    logger_->debug("Sending N-GET Printer Status");

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-GET Printer Status: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-GET Printer Status response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_get_rsp) {
        return pacs::pacs_error<print_result>(
            pacs::error_codes::print_unexpected_command,
            "Expected N-GET-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();

    print_result result;
    result.sop_instance_uid = std::string(printer_instance_uid);
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    if (response.has_dataset()) {
        result.response_data = response.dataset().value().get();
    }

    printer_queries_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-GET Printer Status successful");
    } else {
        logger_->warn("N-GET Printer Status returned 0x" +
                      std::to_string(result.status));
    }

    return result;
}

// =============================================================================
// Statistics
// =============================================================================

size_t print_scu::sessions_created() const noexcept {
    return sessions_created_.load(std::memory_order_relaxed);
}

size_t print_scu::film_boxes_created() const noexcept {
    return film_boxes_created_.load(std::memory_order_relaxed);
}

size_t print_scu::images_set() const noexcept {
    return images_set_.load(std::memory_order_relaxed);
}

size_t print_scu::prints_executed() const noexcept {
    return prints_executed_.load(std::memory_order_relaxed);
}

size_t print_scu::printer_queries() const noexcept {
    return printer_queries_.load(std::memory_order_relaxed);
}

void print_scu::reset_statistics() noexcept {
    sessions_created_.store(0, std::memory_order_relaxed);
    film_boxes_created_.store(0, std::memory_order_relaxed);
    images_set_.store(0, std::memory_order_relaxed);
    prints_executed_.store(0, std::memory_order_relaxed);
    printer_queries_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Implementation
// =============================================================================

std::optional<uint8_t> print_scu::find_print_context(
    network::association& assoc,
    std::string_view sop_class_uid) const {

    // Try the specific SOP class first
    auto context = assoc.accepted_context_id(sop_class_uid);
    if (context) {
        return context;
    }

    // Fall back to Meta SOP Classes which bundle multiple print SOP classes
    context = assoc.accepted_context_id(basic_grayscale_print_meta_sop_class_uid);
    if (context) {
        return context;
    }

    context = assoc.accepted_context_id(basic_color_print_meta_sop_class_uid);
    return context;
}

std::string print_scu::generate_uid() const {
    static std::mt19937_64 gen{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return std::string(uid_root) + "." + std::to_string(timestamp) +
           "." + std::to_string(dist(gen) % 100000);
}

uint16_t print_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

}  // namespace pacs::services
