/**
 * @file print_scp.cpp
 * @brief Implementation of the Print Management SCP service (PS3.4 Annex H)
 */

#include "pacs/services/print_scp.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

#include <chrono>
#include <sstream>

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

print_scp::print_scp(std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)) {}

// =============================================================================
// Configuration
// =============================================================================

void print_scp::set_session_handler(print_session_handler handler) {
    session_handler_ = std::move(handler);
}

void print_scp::set_print_handler(print_action_handler handler) {
    print_handler_ = std::move(handler);
}

void print_scp::set_printer_status_handler(printer_status_handler handler) {
    printer_status_handler_ = std::move(handler);
}

// =============================================================================
// scp_service Interface
// =============================================================================

std::vector<std::string> print_scp::supported_sop_classes() const {
    return {
        std::string(basic_film_session_sop_class_uid),
        std::string(basic_film_box_sop_class_uid),
        std::string(basic_grayscale_image_box_sop_class_uid),
        std::string(basic_color_image_box_sop_class_uid),
        std::string(printer_sop_class_uid),
        std::string(basic_grayscale_print_meta_sop_class_uid),
        std::string(basic_color_print_meta_sop_class_uid),
    };
}

network::Result<std::monostate> print_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    switch (request.command()) {
        case command_field::n_create_rq:
            return handle_n_create(assoc, context_id, request);
        case command_field::n_set_rq:
            return handle_n_set(assoc, context_id, request);
        case command_field::n_get_rq:
            return handle_n_get(assoc, context_id, request);
        case command_field::n_action_rq:
            return handle_n_action(assoc, context_id, request);
        case command_field::n_delete_rq:
            return handle_n_delete(assoc, context_id, request);
        default:
            return pacs::pacs_void_error(
                pacs::error_codes::print_unexpected_command,
                "Unexpected command for Print SCP: " +
                std::string(to_string(request.command())));
    }
}

std::string_view print_scp::service_name() const noexcept {
    return "Print SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t print_scp::sessions_created() const noexcept {
    return sessions_created_.load();
}

size_t print_scp::film_boxes_created() const noexcept {
    return film_boxes_created_.load();
}

size_t print_scp::images_set() const noexcept {
    return images_set_.load();
}

size_t print_scp::prints_executed() const noexcept {
    return prints_executed_.load();
}

size_t print_scp::printer_queries() const noexcept {
    return printer_queries_.load();
}

void print_scp::reset_statistics() noexcept {
    sessions_created_ = 0;
    film_boxes_created_ = 0;
    images_set_ = 0;
    prints_executed_ = 0;
    printer_queries_ = 0;
}

// =============================================================================
// N-CREATE Handler
// =============================================================================

network::Result<std::monostate> print_scp::handle_n_create(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    auto sop_class_uid = request.affected_sop_class_uid();

    if (sop_class_uid == basic_film_session_sop_class_uid) {
        return create_film_session(assoc, context_id, request);
    }
    if (sop_class_uid == basic_film_box_sop_class_uid) {
        return create_film_box(assoc, context_id, request);
    }

    return send_response(
        assoc, context_id, command_field::n_create_rsp,
        request.message_id(), sop_class_uid,
        "", status_refused_sop_class_not_supported);
}

// =============================================================================
// N-SET Handler
// =============================================================================

network::Result<std::monostate> print_scp::handle_n_set(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    auto sop_class_uid = request.affected_sop_class_uid();
    if (sop_class_uid.empty()) {
        sop_class_uid = request.command_set().get_string(
            tag_requested_sop_class_uid);
    }

    auto sop_instance_uid = request.command_set().get_string(
        tag_requested_sop_instance_uid);
    if (sop_instance_uid.empty()) {
        sop_instance_uid = request.affected_sop_instance_uid();
    }

    if (sop_instance_uid.empty()) {
        return send_response(
            assoc, context_id, command_field::n_set_rsp,
            request.message_id(), sop_class_uid,
            "", status_error_missing_attribute);
    }

    // Handle Image Box N-SET (grayscale or color)
    if (sop_class_uid == basic_grayscale_image_box_sop_class_uid ||
        sop_class_uid == basic_color_image_box_sop_class_uid) {

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = image_boxes_.find(sop_instance_uid);
        if (it == image_boxes_.end()) {
            return send_response(
                assoc, context_id, command_field::n_set_rsp,
                request.message_id(), sop_class_uid,
                sop_instance_uid, status_error_invalid_object_instance);
        }

        if (request.has_dataset()) {
            it->second.data = request.dataset().value().get();
            it->second.has_pixel_data = true;
        }

        ++images_set_;

        return send_response(
            assoc, context_id, command_field::n_set_rsp,
            request.message_id(), sop_class_uid,
            sop_instance_uid, status_success);
    }

    // Handle Film Session N-SET
    if (sop_class_uid == basic_film_session_sop_class_uid) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(sop_instance_uid);
        if (it == sessions_.end()) {
            return send_response(
                assoc, context_id, command_field::n_set_rsp,
                request.message_id(), sop_class_uid,
                sop_instance_uid, status_error_invalid_object_instance);
        }

        if (request.has_dataset()) {
            const auto& ds = request.dataset().value().get();
            it->second.data = ds;

            if (ds.contains(print_tags::number_of_copies)) {
                auto copies_str = ds.get_string(print_tags::number_of_copies);
                if (!copies_str.empty()) {
                    it->second.number_of_copies =
                        static_cast<uint32_t>(std::stoul(copies_str));
                }
            }
            if (ds.contains(print_tags::print_priority)) {
                it->second.print_priority = ds.get_string(print_tags::print_priority);
            }
            if (ds.contains(print_tags::medium_type)) {
                it->second.medium_type = ds.get_string(print_tags::medium_type);
            }
        }

        return send_response(
            assoc, context_id, command_field::n_set_rsp,
            request.message_id(), sop_class_uid,
            sop_instance_uid, status_success);
    }

    // Handle Film Box N-SET
    if (sop_class_uid == basic_film_box_sop_class_uid) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = film_boxes_.find(sop_instance_uid);
        if (it == film_boxes_.end()) {
            return send_response(
                assoc, context_id, command_field::n_set_rsp,
                request.message_id(), sop_class_uid,
                sop_instance_uid, status_error_invalid_object_instance);
        }

        if (request.has_dataset()) {
            const auto& ds = request.dataset().value().get();
            it->second.data = ds;

            if (ds.contains(print_tags::image_display_format)) {
                it->second.image_display_format =
                    ds.get_string(print_tags::image_display_format);
            }
            if (ds.contains(print_tags::film_orientation)) {
                it->second.film_orientation = ds.get_string(print_tags::film_orientation);
            }
            if (ds.contains(print_tags::film_size_id)) {
                it->second.film_size_id = ds.get_string(print_tags::film_size_id);
            }
        }

        return send_response(
            assoc, context_id, command_field::n_set_rsp,
            request.message_id(), sop_class_uid,
            sop_instance_uid, status_success);
    }

    return send_response(
        assoc, context_id, command_field::n_set_rsp,
        request.message_id(), sop_class_uid,
        sop_instance_uid, status_refused_sop_class_not_supported);
}

// =============================================================================
// N-GET Handler (Printer Status)
// =============================================================================

network::Result<std::monostate> print_scp::handle_n_get(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    auto sop_class_uid = request.affected_sop_class_uid();
    if (sop_class_uid.empty()) {
        sop_class_uid = request.command_set().get_string(
            tag_requested_sop_class_uid);
    }

    if (sop_class_uid != printer_sop_class_uid) {
        return send_response(
            assoc, context_id, command_field::n_get_rsp,
            request.message_id(), sop_class_uid,
            "", status_refused_sop_class_not_supported);
    }

    ++printer_queries_;

    // Build printer status response
    core::dicom_dataset response_ds;

    using namespace encoding;

    if (printer_status_handler_) {
        auto result = printer_status_handler_();
        if (result.is_ok()) {
            auto& [status, info_ds] = result.value();
            response_ds = std::move(info_ds);
            response_ds.set_string(print_tags::printer_status_tag, vr_type::CS,
                                   std::string(to_string(status)));
        } else {
            response_ds.set_string(print_tags::printer_status_tag, vr_type::CS, "NORMAL");
            response_ds.set_string(print_tags::printer_status_info, vr_type::ST, "");
        }
    } else {
        // Default: report printer as normal
        response_ds.set_string(print_tags::printer_status_tag, vr_type::CS, "NORMAL");
        response_ds.set_string(print_tags::printer_status_info, vr_type::ST, "");
        response_ds.set_string(print_tags::printer_name, vr_type::LO, "PACS_PRINTER");
    }

    // Build response message
    dimse_message response{command_field::n_get_rsp, 0};
    response.set_message_id_responded_to(request.message_id());
    response.set_affected_sop_class_uid(printer_sop_class_uid);
    response.set_status(status_success);

    auto sop_instance_uid = request.command_set().get_string(
        tag_requested_sop_instance_uid);
    if (sop_instance_uid.empty()) {
        sop_instance_uid = request.affected_sop_instance_uid();
    }
    if (!sop_instance_uid.empty()) {
        response.set_affected_sop_instance_uid(sop_instance_uid);
    }

    response.set_dataset(std::move(response_ds));
    return assoc.send_dimse(context_id, response);
}

// =============================================================================
// N-ACTION Handler (Print Film Box)
// =============================================================================

network::Result<std::monostate> print_scp::handle_n_action(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    auto sop_class_uid = request.affected_sop_class_uid();

    // N-ACTION is used on Film Session or Film Box to initiate printing
    if (sop_class_uid != basic_film_session_sop_class_uid &&
        sop_class_uid != basic_film_box_sop_class_uid) {
        return send_response(
            assoc, context_id, command_field::n_action_rsp,
            request.message_id(), sop_class_uid,
            "", status_refused_sop_class_not_supported);
    }

    auto sop_instance_uid = request.affected_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        return send_response(
            assoc, context_id, command_field::n_action_rsp,
            request.message_id(), sop_class_uid,
            "", status_error_missing_attribute);
    }

    // Verify the object exists
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (sop_class_uid == basic_film_box_sop_class_uid) {
            if (film_boxes_.find(sop_instance_uid) == film_boxes_.end()) {
                return send_response(
                    assoc, context_id, command_field::n_action_rsp,
                    request.message_id(), sop_class_uid,
                    sop_instance_uid, status_error_invalid_object_instance);
            }
        } else {
            if (sessions_.find(sop_instance_uid) == sessions_.end()) {
                return send_response(
                    assoc, context_id, command_field::n_action_rsp,
                    request.message_id(), sop_class_uid,
                    sop_instance_uid, status_error_invalid_object_instance);
            }
        }
    }

    // Call print handler
    if (print_handler_) {
        auto result = print_handler_(sop_instance_uid);
        if (result.is_err()) {
            return send_response(
                assoc, context_id, command_field::n_action_rsp,
                request.message_id(), sop_class_uid,
                sop_instance_uid, status_error_unable_to_process);
        }
    }

    ++prints_executed_;

    // Build action response with action type ID
    dimse_message response{command_field::n_action_rsp, 0};
    response.set_message_id_responded_to(request.message_id());
    response.set_affected_sop_class_uid(sop_class_uid);
    response.set_affected_sop_instance_uid(sop_instance_uid);
    response.set_status(status_success);

    auto action_type = request.action_type_id();
    if (action_type.has_value()) {
        response.set_action_type_id(action_type.value());
    }

    return assoc.send_dimse(context_id, response);
}

// =============================================================================
// N-DELETE Handler
// =============================================================================

network::Result<std::monostate> print_scp::handle_n_delete(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    auto sop_class_uid = request.affected_sop_class_uid();
    auto sop_instance_uid = request.affected_sop_instance_uid();

    if (sop_instance_uid.empty()) {
        return send_response(
            assoc, context_id, command_field::n_delete_rsp,
            request.message_id(), sop_class_uid,
            "", status_error_missing_attribute);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (sop_class_uid == basic_film_session_sop_class_uid) {
        auto it = sessions_.find(sop_instance_uid);
        if (it == sessions_.end()) {
            return send_response(
                assoc, context_id, command_field::n_delete_rsp,
                request.message_id(), sop_class_uid,
                sop_instance_uid, status_error_invalid_object_instance);
        }

        // Delete associated film boxes and their image boxes
        for (const auto& fb_uid : it->second.film_box_uids) {
            auto fb_it = film_boxes_.find(fb_uid);
            if (fb_it != film_boxes_.end()) {
                for (const auto& ib_uid : fb_it->second.image_box_uids) {
                    image_boxes_.erase(ib_uid);
                }
                film_boxes_.erase(fb_it);
            }
        }
        sessions_.erase(it);

    } else if (sop_class_uid == basic_film_box_sop_class_uid) {
        auto it = film_boxes_.find(sop_instance_uid);
        if (it == film_boxes_.end()) {
            return send_response(
                assoc, context_id, command_field::n_delete_rsp,
                request.message_id(), sop_class_uid,
                sop_instance_uid, status_error_invalid_object_instance);
        }

        // Delete associated image boxes
        for (const auto& ib_uid : it->second.image_box_uids) {
            image_boxes_.erase(ib_uid);
        }

        // Remove from parent session's film box list
        for (auto& [_, session] : sessions_) {
            auto& fb_uids = session.film_box_uids;
            fb_uids.erase(
                std::remove(fb_uids.begin(), fb_uids.end(), sop_instance_uid),
                fb_uids.end());
        }
        film_boxes_.erase(it);

    } else {
        return send_response(
            assoc, context_id, command_field::n_delete_rsp,
            request.message_id(), sop_class_uid,
            sop_instance_uid, status_refused_sop_class_not_supported);
    }

    return send_response(
        assoc, context_id, command_field::n_delete_rsp,
        request.message_id(), sop_class_uid,
        sop_instance_uid, status_success);
}

// =============================================================================
// Film Session Creation
// =============================================================================

network::Result<std::monostate> print_scp::create_film_session(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    auto sop_instance_uid = request.affected_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        sop_instance_uid = generate_uid();
    }

    film_session session;
    session.sop_instance_uid = sop_instance_uid;

    if (request.has_dataset()) {
        const auto& ds = request.dataset().value().get();
        session.data = ds;

        if (ds.contains(print_tags::number_of_copies)) {
            auto copies_str = ds.get_string(print_tags::number_of_copies);
            if (!copies_str.empty()) {
                session.number_of_copies =
                    static_cast<uint32_t>(std::stoul(copies_str));
            }
        }
        if (ds.contains(print_tags::print_priority)) {
            session.print_priority = ds.get_string(print_tags::print_priority);
        }
        if (ds.contains(print_tags::medium_type)) {
            session.medium_type = ds.get_string(print_tags::medium_type);
        }
        if (ds.contains(print_tags::film_destination)) {
            session.film_destination = ds.get_string(print_tags::film_destination);
        }
    }

    // Call session handler
    if (session_handler_) {
        auto result = session_handler_(session);
        if (result.is_err()) {
            return send_response(
                assoc, context_id, command_field::n_create_rsp,
                request.message_id(), basic_film_session_sop_class_uid,
                sop_instance_uid, status_error_unable_to_process);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.emplace(sop_instance_uid, std::move(session));
    }

    ++sessions_created_;

    return send_response(
        assoc, context_id, command_field::n_create_rsp,
        request.message_id(), basic_film_session_sop_class_uid,
        sop_instance_uid, status_success);
}

// =============================================================================
// Film Box Creation
// =============================================================================

network::Result<std::monostate> print_scp::create_film_box(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    auto sop_instance_uid = request.affected_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        sop_instance_uid = generate_uid();
    }

    film_box box;
    box.sop_instance_uid = sop_instance_uid;

    if (request.has_dataset()) {
        const auto& ds = request.dataset().value().get();
        box.data = ds;

        if (ds.contains(print_tags::image_display_format)) {
            box.image_display_format = ds.get_string(print_tags::image_display_format);
        }
        if (ds.contains(print_tags::film_orientation)) {
            box.film_orientation = ds.get_string(print_tags::film_orientation);
        }
        if (ds.contains(print_tags::film_size_id)) {
            box.film_size_id = ds.get_string(print_tags::film_size_id);
        }
    }

    // Parse image display format to determine image box count
    // Format: "STANDARD\C,R" where C=columns, R=rows
    uint16_t num_image_boxes = 1;
    auto format = box.image_display_format;
    auto backslash_pos = format.find('\\');
    if (backslash_pos != std::string::npos) {
        auto dims = format.substr(backslash_pos + 1);
        auto comma_pos = dims.find(',');
        if (comma_pos != std::string::npos) {
            auto cols = static_cast<uint16_t>(
                std::stoul(dims.substr(0, comma_pos)));
            auto rows = static_cast<uint16_t>(
                std::stoul(dims.substr(comma_pos + 1)));
            num_image_boxes = static_cast<uint16_t>(cols * rows);
        }
    }

    // Create image boxes for this film box
    std::lock_guard<std::mutex> lock(mutex_);

    // Link to parent film session if found
    for (auto& [_, session] : sessions_) {
        session.film_box_uids.push_back(sop_instance_uid);
        box.film_session_uid = session.sop_instance_uid;
        break;  // Link to first (most recent) session
    }

    for (uint16_t i = 1; i <= num_image_boxes; ++i) {
        image_box ib;
        ib.sop_instance_uid = generate_uid();
        ib.film_box_uid = sop_instance_uid;
        ib.image_position = i;
        box.image_box_uids.push_back(ib.sop_instance_uid);
        image_boxes_.emplace(ib.sop_instance_uid, std::move(ib));
    }

    film_boxes_.emplace(sop_instance_uid, std::move(box));
    ++film_boxes_created_;

    // Build response with Referenced Image Box Sequence
    dimse_message response{command_field::n_create_rsp, 0};
    response.set_message_id_responded_to(request.message_id());
    response.set_affected_sop_class_uid(basic_film_box_sop_class_uid);
    response.set_affected_sop_instance_uid(sop_instance_uid);
    response.set_status(status_success);

    // Add referenced image box UIDs in response dataset
    using namespace encoding;
    core::dicom_dataset response_ds;
    auto& fb = film_boxes_.at(sop_instance_uid);
    auto& seq = response_ds.get_or_create_sequence(
        print_tags::referenced_image_box_sequence);
    for (size_t i = 0; i < fb.image_box_uids.size(); ++i) {
        core::dicom_dataset ref_item;
        ref_item.set_string(core::tags::referenced_sop_class_uid, vr_type::UI,
                           std::string(basic_grayscale_image_box_sop_class_uid));
        ref_item.set_string(core::tags::referenced_sop_instance_uid, vr_type::UI,
                           fb.image_box_uids[i]);
        seq.push_back(std::move(ref_item));
    }
    response.set_dataset(std::move(response_ds));

    return assoc.send_dimse(context_id, response);
}

// =============================================================================
// Response Helper
// =============================================================================

network::Result<std::monostate> print_scp::send_response(
    network::association& assoc,
    uint8_t context_id,
    network::dimse::command_field response_type,
    uint16_t message_id,
    std::string_view sop_class_uid,
    const std::string& sop_instance_uid,
    network::dimse::status_code status) {

    using namespace network::dimse;

    dimse_message response{response_type, 0};
    response.set_message_id_responded_to(message_id);
    response.set_affected_sop_class_uid(sop_class_uid);
    response.set_status(status);

    if (!sop_instance_uid.empty()) {
        response.set_affected_sop_instance_uid(sop_instance_uid);
    }

    return assoc.send_dimse(context_id, response);
}

// =============================================================================
// UID Generation
// =============================================================================

auto print_scp::generate_uid() -> std::string {
    auto counter = uid_counter_.fetch_add(1);
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    std::ostringstream oss;
    oss << "2.25." << ms << "." << counter;
    return oss.str();
}

}  // namespace pacs::services
