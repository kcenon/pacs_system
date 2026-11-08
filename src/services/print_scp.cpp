// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file print_scp.cpp
 * @brief Implementation of the Print Management SCP service (PS3.4 Annex H)
 */

#include "kcenon/pacs/services/print_scp.h"

#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/encoding/vr_type.h"
#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/dimse/command_field.h"
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dimse/status_codes.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <optional>
#include <sstream>
#include <string_view>

namespace kcenon::pacs::services {

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

void print_scp::set_job_handler(print_job_handler handler) {
    job_handler_ = std::move(handler);
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
            return kcenon::pacs::pacs_void_error(
                kcenon::pacs::error_codes::print_unexpected_command,
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
// Annex H rules shared by the handlers
// =============================================================================

namespace {

/// PS3.4 H.4.2.2: the requested printing could not be queued.
constexpr network::dimse::status_code status_print_queue_full = 0xC602;
/// PS3.4 H.4.2.2.4: the Film Box holds no images, an empty page.
constexpr network::dimse::status_code status_print_empty_page = 0xB603;
/// PS3.4 H.4.1.2.4: the Film Session holds no Film Box.
constexpr network::dimse::status_code status_print_no_film_box = 0xC600;
constexpr uint16_t print_action_type = 1;
/// Far above any real layout; it keeps a hostile format from allocating without bound.
constexpr int max_image_boxes = 4096;

/// IS (PS3.5 6.2): digits, an optional sign, and spaces around them.
[[nodiscard]] std::optional<uint32_t> parse_copies(std::string_view text) {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    uint32_t value = 0;
    const auto* end = text.data() + text.size();
    const auto [next, error] = std::from_chars(text.data(), end, value);
    if (error != std::errc{} || next != end || value == 0) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<std::vector<int>> parse_counts(std::string_view text) {
    std::vector<int> counts;
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto part = text.substr(0, comma);
        int value = 0;
        const auto* end = part.data() + part.size();
        const auto [next, error] = std::from_chars(part.data(), end, value);
        if (error != std::errc{} || next != end || value <= 0 || value > max_image_boxes) {
            return std::nullopt;
        }
        counts.push_back(value);
        if (comma == std::string_view::npos) {
            return counts;
        }
        text.remove_prefix(comma + 1);
    }
    return std::nullopt;
}

/// Image boxes of an Image Display Format (PS3.3 C.13.3): STANDARD\C,R, ROW\R1,R2,... or COL\C1,C2,...
[[nodiscard]] std::optional<int> image_box_count(std::string_view format) {
    const auto backslash = format.find('\\');
    if (backslash == std::string_view::npos) {
        return std::nullopt;
    }
    const auto kind = format.substr(0, backslash);
    const auto counts = parse_counts(format.substr(backslash + 1));
    if (!counts) {
        return std::nullopt;
    }
    int total = 0;
    if (kind == "STANDARD") {
        if (counts->size() != 2) {
            return std::nullopt;
        }
        total = (*counts)[0] * (*counts)[1];
    } else if (kind == "ROW" || kind == "COL") {
        for (const auto count : *counts) {
            total += count;
        }
    } else {
        return std::nullopt;
    }
    if (total <= 0 || total > max_image_boxes) {
        return std::nullopt;
    }
    return total;
}

[[nodiscard]] std::string_view negotiated_syntax(const network::association& assoc, uint8_t context_id) {
    for (const auto& context : assoc.accepted_contexts()) {
        if (context.id == context_id) {
            return context.abstract_syntax;
        }
    }
    return {};
}

}  // namespace

const film_session* print_scp::owned_session(const std::string& uid, const network::association& assoc) const {
    const auto found = sessions_.find(uid);
    return found != sessions_.end() && found->second.owner == &assoc ? &found->second : nullptr;
}

film_box* print_scp::owned_box(const std::string& uid, const network::association& assoc) {
    const auto found = film_boxes_.find(uid);
    if (found == film_boxes_.end() || owned_session(found->second.film_session_uid, assoc) == nullptr) {
        return nullptr;
    }
    return &found->second;
}

image_box* print_scp::owned_image_box(const std::string& uid, const network::association& assoc) {
    const auto found = image_boxes_.find(uid);
    if (found == image_boxes_.end() || owned_box(found->second.film_box_uid, assoc) == nullptr) {
        return nullptr;
    }
    return &found->second;
}

void print_scp::association_ended(const network::association& assoc) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto session = sessions_.begin(); session != sessions_.end();) {
        if (session->second.owner != &assoc) {
            ++session;
            continue;
        }
        remove_session_contents(session->second);
        session = sessions_.erase(session);
    }
}

void print_scp::remove_session_contents(const film_session& session) {
    for (const auto& box_uid : session.film_box_uids) {
        const auto box = film_boxes_.find(box_uid);
        if (box == film_boxes_.end()) {
            continue;
        }
        for (const auto& image_uid : box->second.image_box_uids) {
            image_boxes_.erase(image_uid);
        }
        film_boxes_.erase(box);
    }
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

    const std::string sop_class_uid{request.requested_sop_class_uid()};
    const std::string sop_instance_uid{request.requested_sop_instance_uid()};

    if (sop_instance_uid.empty()) {
        return send_response(
            assoc, context_id, command_field::n_set_rsp,
            request.message_id(), sop_class_uid,
            "", status_error_missing_attribute);
    }

    const core::dicom_dataset no_changes;
    const auto& changes = request.has_dataset() ? request.dataset().value().get() : no_changes;
    const auto answer = [&](status_code status) {
        return send_response(
            assoc, context_id, command_field::n_set_rsp,
            request.message_id(), sop_class_uid,
            sop_instance_uid, status);
    };

    std::lock_guard<std::mutex> lock(mutex_);

    // Handle Image Box N-SET (grayscale or color)
    if (sop_class_uid == basic_grayscale_image_box_sop_class_uid ||
        sop_class_uid == basic_color_image_box_sop_class_uid) {

        auto* box = owned_image_box(sop_instance_uid, assoc);
        if (box == nullptr) {
            return answer(status_error_no_such_object_instance);
        }

        // Table H.4-7: the position and the image sequence of this class are Type 1.
        const auto image_tag = sop_class_uid == basic_color_image_box_sop_class_uid
            ? print_tags::basic_color_image_sequence
            : print_tags::basic_grayscale_image_sequence;
        const auto* images = changes.get_sequence(image_tag);
        if (images == nullptr || images->empty() || !changes.contains(print_tags::image_position)) {
            return answer(status_error_missing_attribute);
        }
        const auto position = changes.get_numeric<uint16_t>(print_tags::image_position);
        if (!position || *position != box->image_position) {
            return answer(status_error_invalid_attribute_value);
        }

        box->data = changes;
        box->has_pixel_data = images->front().contains(print_tags::pixel_data);

        ++images_set_;
        return answer(status_success);
    }

    // Handle Film Session N-SET
    if (sop_class_uid == basic_film_session_sop_class_uid) {
        const auto found = sessions_.find(sop_instance_uid);
        if (found == sessions_.end() || found->second.owner != &assoc) {
            return answer(status_error_no_such_object_instance);
        }

        if (changes.contains(print_tags::number_of_copies)) {
            const auto copies = parse_copies(changes.get_string(print_tags::number_of_copies));
            if (!copies) {
                return answer(status_error_invalid_attribute_value);
            }
            found->second.number_of_copies = *copies;
        }
        found->second.data = changes;
        if (changes.contains(print_tags::print_priority)) {
            found->second.print_priority = changes.get_string(print_tags::print_priority);
        }
        if (changes.contains(print_tags::medium_type)) {
            found->second.medium_type = changes.get_string(print_tags::medium_type);
        }
        if (changes.contains(print_tags::film_destination)) {
            found->second.film_destination = changes.get_string(print_tags::film_destination);
        }
        return answer(status_success);
    }

    // Handle Film Box N-SET
    if (sop_class_uid == basic_film_box_sop_class_uid) {
        auto* box = owned_box(sop_instance_uid, assoc);
        if (box == nullptr) {
            return answer(status_error_no_such_object_instance);
        }

        box->data = changes;
        // Table H.4-5: the layout is fixed at N-CREATE, because the Image Boxes follow from it.
        if (changes.contains(print_tags::film_orientation)) {
            box->film_orientation = changes.get_string(print_tags::film_orientation);
        }
        if (changes.contains(print_tags::film_size_id)) {
            box->film_size_id = changes.get_string(print_tags::film_size_id);
        }
        return answer(status_success);
    }

    return answer(status_refused_sop_class_not_supported);
}

// =============================================================================
// N-GET Handler (Printer Status)
// =============================================================================

network::Result<std::monostate> print_scp::handle_n_get(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;
    using namespace encoding;

    const std::string sop_class_uid{request.requested_sop_class_uid()};
    const std::string sop_instance_uid{request.requested_sop_instance_uid()};

    if (sop_class_uid != printer_sop_class_uid) {
        return send_response(
            assoc, context_id, command_field::n_get_rsp,
            request.message_id(), sop_class_uid,
            sop_instance_uid, status_refused_sop_class_not_supported);
    }

    // The Printer exists once, under its well-known UID (PS3.4 H.4.6).
    if (sop_instance_uid != printer_sop_instance_uid) {
        return send_response(
            assoc, context_id, command_field::n_get_rsp,
            request.message_id(), sop_class_uid,
            sop_instance_uid, status_error_no_such_object_instance);
    }

    ++printer_queries_;

    // Build printer status response
    core::dicom_dataset response_ds;

    if (printer_status_handler_) {
        auto result = printer_status_handler_();
        if (result.is_ok()) {
            auto& [status, info_ds] = result.value();
            response_ds = std::move(info_ds);
            response_ds.set_string(print_tags::printer_status_tag, vr_type::CS,
                                   std::string(to_string(status)));
            if (!response_ds.contains(print_tags::printer_status_info)) {
                response_ds.set_string(print_tags::printer_status_info, vr_type::CS, "NORMAL");
            }
        } else {
            return send_response(
                assoc, context_id, command_field::n_get_rsp,
                request.message_id(), sop_class_uid,
                sop_instance_uid, status_error_processing_failure);
        }
    } else {
        // Default: report printer as normal
        response_ds.set_string(print_tags::printer_status_tag, vr_type::CS, "NORMAL");
        response_ds.set_string(print_tags::printer_status_info, vr_type::CS, "NORMAL");
        response_ds.set_string(print_tags::printer_name, vr_type::LO, "PACS_PRINTER");
    }

    // An empty identifier list means every attribute (PS3.7 10.1.2.1.4).
    const auto wanted = request.attribute_identifier_list();
    if (!wanted.empty()) {
        core::dicom_dataset selected;
        for (const auto tag : wanted) {
            if (const auto* element = response_ds.get(tag)) {
                selected.insert(*element);
            }
        }
        response_ds = std::move(selected);
    }

    // Build response message
    dimse_message response{command_field::n_get_rsp, 0};
    response.set_message_id_responded_to(request.message_id());
    response.set_affected_sop_class_uid(printer_sop_class_uid);
    response.set_affected_sop_instance_uid(sop_instance_uid);
    response.set_status(status_success);
    response.set_dataset(std::move(response_ds));
    return assoc.send_dimse(context_id, response);
}

// =============================================================================
// N-ACTION Handler (Print Film Box or Film Session)
// =============================================================================

network::Result<std::monostate> print_scp::print_boxes(
    const film_session& session, const std::vector<std::string>& box_uids,
    const std::string& requested_uid, bool& any_image) {

    for (const auto& box_uid : box_uids) {
        const auto box = film_boxes_.find(box_uid);
        if (box == film_boxes_.end()) {
            continue;
        }
        print_job job;
        job.session = session;
        job.box = box->second;
        for (const auto& image_uid : box->second.image_box_uids) {
            const auto image = image_boxes_.find(image_uid);
            if (image != image_boxes_.end() && image->second.has_pixel_data) {
                job.images.push_back(image->second);
            }
        }
        if (job.images.empty()) {
            continue;
        }
        any_image = true;
        if (job_handler_) {
            if (auto queued = job_handler_(job); queued.is_err()) {
                return queued;
            }
        }
    }
    if (any_image && print_handler_) {
        return print_handler_(requested_uid);
    }
    return std::monostate{};
}

network::Result<std::monostate> print_scp::handle_n_action(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    const std::string sop_class_uid{request.requested_sop_class_uid()};
    const std::string sop_instance_uid{request.requested_sop_instance_uid()};
    const auto action_type = request.action_type_id();

    // Build action response with action type ID
    const auto answer = [&](status_code status) {
        dimse_message response{command_field::n_action_rsp, 0};
        response.set_message_id_responded_to(request.message_id());
        response.set_affected_sop_class_uid(sop_class_uid);
        if (!sop_instance_uid.empty()) {
            response.set_affected_sop_instance_uid(sop_instance_uid);
        }
        response.set_status(status);
        if (action_type) {
            response.set_action_type_id(*action_type);
        }
        return assoc.send_dimse(context_id, response);
    };

    // N-ACTION is used on Film Session or Film Box to initiate printing
    if (sop_class_uid != basic_film_session_sop_class_uid &&
        sop_class_uid != basic_film_box_sop_class_uid) {
        return answer(status_refused_sop_class_not_supported);
    }
    if (sop_instance_uid.empty()) {
        return answer(status_error_missing_attribute);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    bool any_image = false;
    network::Result<std::monostate> printed = std::monostate{};
    if (sop_class_uid == basic_film_box_sop_class_uid) {
        const auto* box = owned_box(sop_instance_uid, assoc);
        if (box == nullptr) {
            return answer(status_error_no_such_object_instance);
        }
        if (!action_type || *action_type != print_action_type) {
            return answer(status_error_no_such_action_type);
        }
        printed = print_boxes(*owned_session(box->film_session_uid, assoc), {sop_instance_uid}, sop_instance_uid, any_image);
    } else {
        const auto* session = owned_session(sop_instance_uid, assoc);
        if (session == nullptr) {
            return answer(status_error_no_such_object_instance);
        }
        if (!action_type || *action_type != print_action_type) {
            return answer(status_error_no_such_action_type);
        }
        if (session->film_box_uids.empty()) {
            return answer(status_print_no_film_box);
        }
        printed = print_boxes(*session, session->film_box_uids, sop_instance_uid, any_image);
    }
    if (printed.is_err()) {
        logger_->error("Print job for " + sop_instance_uid + ": " + printed.error().message);
        return answer(status_print_queue_full);
    }
    if (!any_image) {
        return answer(status_print_empty_page);
    }

    ++prints_executed_;
    return answer(status_success);
}

// =============================================================================
// N-DELETE Handler
// =============================================================================

network::Result<std::monostate> print_scp::handle_n_delete(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    const std::string sop_class_uid{request.requested_sop_class_uid()};
    const std::string sop_instance_uid{request.requested_sop_instance_uid()};

    const auto answer = [&](status_code status) {
        return send_response(
            assoc, context_id, command_field::n_delete_rsp,
            request.message_id(), sop_class_uid,
            sop_instance_uid, status);
    };

    if (sop_instance_uid.empty()) {
        return answer(status_error_missing_attribute);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (sop_class_uid == basic_film_session_sop_class_uid) {
        const auto found = sessions_.find(sop_instance_uid);
        if (found == sessions_.end() || found->second.owner != &assoc) {
            return answer(status_error_no_such_object_instance);
        }
        remove_session_contents(found->second);
        sessions_.erase(found);

    } else if (sop_class_uid == basic_film_box_sop_class_uid) {
        const auto* box = owned_box(sop_instance_uid, assoc);
        if (box == nullptr) {
            return answer(status_error_no_such_object_instance);
        }

        // Delete associated image boxes
        for (const auto& image_uid : box->image_box_uids) {
            image_boxes_.erase(image_uid);
        }

        // Remove from parent session's film box list
        auto& box_uids = sessions_.at(box->film_session_uid).film_box_uids;
        box_uids.erase(std::remove(box_uids.begin(), box_uids.end(), sop_instance_uid), box_uids.end());
        film_boxes_.erase(sop_instance_uid);

    } else {
        return answer(status_refused_sop_class_not_supported);
    }

    return answer(status_success);
}

// =============================================================================
// Film Session Creation
// =============================================================================

network::Result<std::monostate> print_scp::create_film_session(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    std::string sop_instance_uid{request.affected_sop_instance_uid()};
    if (sop_instance_uid.empty()) {
        sop_instance_uid = generate_uid();
    }

    const auto answer = [&](status_code status) {
        return send_response(
            assoc, context_id, command_field::n_create_rsp,
            request.message_id(), basic_film_session_sop_class_uid,
            sop_instance_uid, status);
    };

    film_session session;
    session.sop_instance_uid = sop_instance_uid;
    session.owner = &assoc;

    if (request.has_dataset()) {
        const auto& ds = request.dataset().value().get();
        session.data = ds;

        if (ds.contains(print_tags::number_of_copies)) {
            const auto copies = parse_copies(ds.get_string(print_tags::number_of_copies));
            if (!copies) {
                return answer(status_error_invalid_attribute_value);
            }
            session.number_of_copies = *copies;
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

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.contains(sop_instance_uid)) {
            return answer(status_error_duplicate_sop_instance);
        }
        // PS3.4 H.4.1: an association has one Film Session at a time.
        for (const auto& [uid, existing] : sessions_) {
            if (existing.owner == &assoc) {
                return answer(status_error_resource_limitation);
            }
        }

        // Call session handler
        if (session_handler_) {
            auto result = session_handler_(session);
            if (result.is_err()) {
                return answer(status_error_unable_to_process);
            }
        }
        sessions_.emplace(sop_instance_uid, std::move(session));
    }

    ++sessions_created_;
    return answer(status_success);
}

// =============================================================================
// Film Box Creation
// =============================================================================

network::Result<std::monostate> print_scp::create_film_box(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;
    using namespace encoding;

    std::string sop_instance_uid{request.affected_sop_instance_uid()};
    if (sop_instance_uid.empty()) {
        sop_instance_uid = generate_uid();
    }

    const auto answer = [&](status_code status) {
        return send_response(
            assoc, context_id, command_field::n_create_rsp,
            request.message_id(), basic_film_box_sop_class_uid,
            sop_instance_uid, status);
    };

    if (!request.has_dataset()) {
        return answer(status_error_missing_attribute);
    }
    const auto& ds = request.dataset().value().get();

    // Table H.4-5: the Image Display Format and the Film Session it belongs to are Type 1.
    const auto format = ds.get_string(print_tags::image_display_format);
    const auto* session_reference = ds.get_sequence(print_tags::referenced_film_session_sequence);
    if (format.empty() || session_reference == nullptr || session_reference->empty()) {
        return answer(status_error_missing_attribute);
    }
    const auto boxes = image_box_count(format);
    if (!boxes) {
        return answer(status_error_invalid_attribute_value);
    }
    const auto session_uid = session_reference->front().get_string(core::tags::referenced_sop_instance_uid);

    std::lock_guard<std::mutex> lock(mutex_);

    if (owned_session(session_uid, assoc) == nullptr) {
        return answer(status_error_invalid_attribute_value);
    }
    if (film_boxes_.contains(sop_instance_uid)) {
        return answer(status_error_duplicate_sop_instance);
    }

    film_box box;
    box.sop_instance_uid = sop_instance_uid;
    box.film_session_uid = session_uid;
    box.data = ds;
    box.image_display_format = format;
    if (ds.contains(print_tags::film_orientation)) {
        box.film_orientation = ds.get_string(print_tags::film_orientation);
    }
    if (ds.contains(print_tags::film_size_id)) {
        box.film_size_id = ds.get_string(print_tags::film_size_id);
    }

    for (int position = 1; position <= *boxes; ++position) {
        image_box image;
        image.sop_instance_uid = generate_uid();
        image.film_box_uid = sop_instance_uid;
        image.image_position = static_cast<uint16_t>(position);
        box.image_box_uids.push_back(image.sop_instance_uid);
        image_boxes_.emplace(image.sop_instance_uid, std::move(image));
    }

    // A Color Print Meta context prints Color Image Boxes (PS3.4 H.3.2).
    const std::string image_box_class{negotiated_syntax(assoc, context_id) == basic_color_print_meta_sop_class_uid
        ? basic_color_image_box_sop_class_uid
        : basic_grayscale_image_box_sop_class_uid};

    // Answer with the attributes as created, plus the Image Boxes the SCU must fill in.
    core::dicom_dataset response_ds = ds;
    auto& sequence = response_ds.get_or_create_sequence(print_tags::referenced_image_box_sequence);
    for (const auto& image_uid : box.image_box_uids) {
        core::dicom_dataset reference;
        reference.set_string(core::tags::referenced_sop_class_uid, vr_type::UI, image_box_class);
        reference.set_string(core::tags::referenced_sop_instance_uid, vr_type::UI, image_uid);
        sequence.push_back(std::move(reference));
    }

    sessions_.at(session_uid).film_box_uids.push_back(sop_instance_uid);
    film_boxes_.emplace(sop_instance_uid, std::move(box));
    ++film_boxes_created_;

    dimse_message response{command_field::n_create_rsp, 0};
    response.set_message_id_responded_to(request.message_id());
    response.set_affected_sop_class_uid(basic_film_box_sop_class_uid);
    response.set_affected_sop_instance_uid(sop_instance_uid);
    response.set_status(status_success);
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

}  // namespace kcenon::pacs::services
