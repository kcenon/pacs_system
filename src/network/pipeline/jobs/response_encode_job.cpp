/**
 * @file response_encode_job.cpp
 * @brief Implementation of response encode job
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #522 - Phase 5: Response Jobs
 */

#include <pacs/network/pipeline/jobs/response_encode_job.hpp>
#include <pacs/network/pipeline/jobs/send_network_io_job.hpp>

#include <chrono>

namespace pacs::network::pipeline {

response_encode_job::response_encode_job(service_result result,
                                         uint32_t max_pdu_size,
                                         encode_callback on_encoded,
                                         error_callback on_error)
    : result_(std::move(result))
    , max_pdu_size_(max_pdu_size)
    , on_encoded_(std::move(on_encoded))
    , on_error_(std::move(on_error)) {

    context_.session_id = result_.session_id;
    context_.message_id = result_.message_id;
    context_.stage = pipeline_stage::response_encode;
    context_.enqueue_time_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

auto response_encode_job::execute(pipeline_coordinator& coordinator) -> VoidResult {
    // Encode the response
    auto encode_result = encode_response();
    if (!encode_result.is_ok()) {
        if (on_error_) {
            auto err = encode_result.error();
            on_error_(result_.session_id, err.message);
        }
        return VoidResult(encode_result.error());
    }

    auto responses = encode_result.value();

    // Submit each encoded response to the send stage
    for (auto& response : responses) {
        // Invoke callback if set
        if (on_encoded_) {
            on_encoded_(response);
        }

        // Create send job
        // Note: send_fn will be set by the adapter layer
        auto send_job = std::make_unique<send_network_io_job>(
            response.session_id,
            std::move(response.pdu_data),
            nullptr  // send_fn set by adapter
        );

        send_job->get_context() = context_;
        send_job->get_context().stage = pipeline_stage::network_send;

        auto submit_result = coordinator.submit_to_stage(
            pipeline_stage::network_send,
            std::move(send_job)
        );

        if (!submit_result.is_ok()) {
            return submit_result;
        }
    }

    return ok();
}

auto response_encode_job::get_context() const noexcept -> const job_context& {
    return context_;
}

auto response_encode_job::get_context() noexcept -> job_context& {
    return context_;
}

auto response_encode_job::get_name() const -> std::string {
    return "response_encode_job[session=" +
           std::to_string(context_.session_id) +
           ", msgid=" + std::to_string(result_.message_id) +
           ", status=" + std::to_string(static_cast<uint16_t>(result_.status)) + "]";
}

auto response_encode_job::get_result() const noexcept -> const service_result& {
    return result_;
}

auto response_encode_job::encode_response()
    -> Result<std::vector<encoded_response>> {

    std::vector<encoded_response> responses;

    // Encode DIMSE command
    auto command_result = encode_dimse_command();
    if (!command_result.is_ok()) {
        return Result<std::vector<encoded_response>>(command_result.error());
    }

    auto command_data = command_result.value();

    // Fragment the command data if needed
    auto command_fragments = fragment_data(command_data);

    // Create P-DATA-TF PDUs for command
    for (size_t i = 0; i < command_fragments.size(); ++i) {
        encoded_response response;
        response.session_id = result_.session_id;
        response.message_id = result_.message_id;
        response.is_final = (i == command_fragments.size() - 1) && result_.data_set.empty();

        // Build P-DATA-TF PDU
        // PDU: Type (1) + Reserved (1) + Length (4) + PDV Items
        // PDV: Length (4) + Presentation Context ID (1) + Control Header (1) + Data

        const auto& frag = command_fragments[i];
        size_t pdv_length = 2 + frag.size();  // Context ID + Control + Data
        size_t pdu_length = 4 + pdv_length;    // PDV Length + PDV

        response.pdu_data.resize(6 + pdu_length);

        // PDU header
        response.pdu_data[0] = 0x04;  // P-DATA-TF
        response.pdu_data[1] = 0x00;  // Reserved

        // PDU length (big-endian)
        response.pdu_data[2] = static_cast<uint8_t>((pdu_length >> 24) & 0xFF);
        response.pdu_data[3] = static_cast<uint8_t>((pdu_length >> 16) & 0xFF);
        response.pdu_data[4] = static_cast<uint8_t>((pdu_length >> 8) & 0xFF);
        response.pdu_data[5] = static_cast<uint8_t>(pdu_length & 0xFF);

        // PDV length (big-endian)
        size_t offset = 6;
        response.pdu_data[offset++] = static_cast<uint8_t>((pdv_length >> 24) & 0xFF);
        response.pdu_data[offset++] = static_cast<uint8_t>((pdv_length >> 16) & 0xFF);
        response.pdu_data[offset++] = static_cast<uint8_t>((pdv_length >> 8) & 0xFF);
        response.pdu_data[offset++] = static_cast<uint8_t>(pdv_length & 0xFF);

        // Presentation context ID
        response.pdu_data[offset++] = result_.presentation_context_id;

        // Control header: command (bit 0) + last fragment (bit 1)
        uint8_t control = 0x01;  // Command fragment
        if (i == command_fragments.size() - 1) {
            control |= 0x02;  // Last command fragment
        }
        response.pdu_data[offset++] = control;

        // Copy fragment data
        std::copy(frag.begin(), frag.end(), response.pdu_data.begin() + offset);

        responses.push_back(std::move(response));
    }

    // Encode data set if present
    if (!result_.data_set.empty()) {
        auto data_fragments = fragment_data(result_.data_set);

        for (size_t i = 0; i < data_fragments.size(); ++i) {
            encoded_response response;
            response.session_id = result_.session_id;
            response.message_id = result_.message_id;
            response.is_final = (i == data_fragments.size() - 1);

            const auto& frag = data_fragments[i];
            size_t pdv_length = 2 + frag.size();
            size_t pdu_length = 4 + pdv_length;

            response.pdu_data.resize(6 + pdu_length);

            // PDU header
            response.pdu_data[0] = 0x04;
            response.pdu_data[1] = 0x00;

            response.pdu_data[2] = static_cast<uint8_t>((pdu_length >> 24) & 0xFF);
            response.pdu_data[3] = static_cast<uint8_t>((pdu_length >> 16) & 0xFF);
            response.pdu_data[4] = static_cast<uint8_t>((pdu_length >> 8) & 0xFF);
            response.pdu_data[5] = static_cast<uint8_t>(pdu_length & 0xFF);

            size_t offset = 6;
            response.pdu_data[offset++] = static_cast<uint8_t>((pdv_length >> 24) & 0xFF);
            response.pdu_data[offset++] = static_cast<uint8_t>((pdv_length >> 16) & 0xFF);
            response.pdu_data[offset++] = static_cast<uint8_t>((pdv_length >> 8) & 0xFF);
            response.pdu_data[offset++] = static_cast<uint8_t>(pdv_length & 0xFF);

            response.pdu_data[offset++] = result_.presentation_context_id;

            uint8_t control = 0x00;  // Data fragment
            if (i == data_fragments.size() - 1) {
                control |= 0x02;  // Last data fragment
            }
            response.pdu_data[offset++] = control;

            std::copy(frag.begin(), frag.end(), response.pdu_data.begin() + offset);

            responses.push_back(std::move(response));
        }
    }

    return ok(std::move(responses));
}

auto response_encode_job::encode_dimse_command()
    -> Result<std::vector<uint8_t>> {

    std::vector<uint8_t> command;
    command.reserve(256);  // Typical command size

    // Helper to append element (Implicit VR Little Endian)
    auto append_element = [&command](uint16_t group, uint16_t element,
                                     const void* data, uint32_t length) {
        // Tag
        command.push_back(static_cast<uint8_t>(group & 0xFF));
        command.push_back(static_cast<uint8_t>((group >> 8) & 0xFF));
        command.push_back(static_cast<uint8_t>(element & 0xFF));
        command.push_back(static_cast<uint8_t>((element >> 8) & 0xFF));
        // Length
        command.push_back(static_cast<uint8_t>(length & 0xFF));
        command.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
        command.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
        command.push_back(static_cast<uint8_t>((length >> 24) & 0xFF));
        // Data
        const auto* bytes = static_cast<const uint8_t*>(data);
        command.insert(command.end(), bytes, bytes + length);
    };

    auto append_uint16 = [&append_element](uint16_t group, uint16_t element,
                                           uint16_t value) {
        append_element(group, element, &value, 2);
    };

    auto append_string = [&append_element](uint16_t group, uint16_t element,
                                           const std::string& value) {
        std::string padded = value;
        if (padded.size() % 2 != 0) {
            padded.push_back(' ');  // Pad to even length
        }
        append_element(group, element, padded.data(),
                       static_cast<uint32_t>(padded.size()));
    };

    // Placeholder for CommandGroupLength (will be filled after encoding)
    size_t group_length_offset = command.size();
    uint32_t placeholder = 0;
    append_element(0x0000, 0x0000, &placeholder, 4);

    // AffectedSOPClassUID (0000,0002) or RequestedSOPClassUID (0000,0003)
    if (!result_.sop_class_uid.empty()) {
        append_string(0x0000, 0x0002, result_.sop_class_uid);
    }

    // CommandField (0000,0100)
    append_uint16(0x0000, 0x0100, static_cast<uint16_t>(result_.response_type));

    // MessageIDBeingRespondedTo (0000,0120)
    append_uint16(0x0000, 0x0120, result_.message_id);

    // CommandDataSetType (0000,0800)
    // 0x0101 = no data set, 0x0001 = data set present
    uint16_t data_set_type = result_.data_set.empty() ? 0x0101 : 0x0001;
    append_uint16(0x0000, 0x0800, data_set_type);

    // Status (0000,0900)
    append_uint16(0x0000, 0x0900, static_cast<uint16_t>(result_.status));

    // AffectedSOPInstanceUID (0000,1000) if present
    if (!result_.sop_instance_uid.empty()) {
        append_string(0x0000, 0x1000, result_.sop_instance_uid);
    }

    // For C-GET/C-MOVE responses, include sub-operation counts
    if (result_.response_type == dimse_command_type::c_get_rsp ||
        result_.response_type == dimse_command_type::c_move_rsp) {
        if (result_.status == dimse_status::pending ||
            result_.status == dimse_status::pending_warning) {
            append_uint16(0x0000, 0x1020, result_.remaining_sub_ops);
            append_uint16(0x0000, 0x1021, result_.completed_sub_ops);
            append_uint16(0x0000, 0x1022, result_.failed_sub_ops);
            append_uint16(0x0000, 0x1023, result_.warning_sub_ops);
        }
    }

    // Error comment if present
    if (!result_.error_comment.empty()) {
        append_string(0x0000, 0x0902, result_.error_comment);
    }

    // Update CommandGroupLength
    uint32_t group_length = static_cast<uint32_t>(command.size() - group_length_offset - 8);
    command[group_length_offset + 4] = static_cast<uint8_t>(group_length & 0xFF);
    command[group_length_offset + 5] = static_cast<uint8_t>((group_length >> 8) & 0xFF);
    command[group_length_offset + 6] = static_cast<uint8_t>((group_length >> 16) & 0xFF);
    command[group_length_offset + 7] = static_cast<uint8_t>((group_length >> 24) & 0xFF);

    return ok(std::move(command));
}

auto response_encode_job::fragment_data(const std::vector<uint8_t>& data)
    -> std::vector<std::vector<uint8_t>> {

    std::vector<std::vector<uint8_t>> fragments;

    // Max fragment size = max_pdu_size - PDU header (6) - PDV header (6)
    size_t max_fragment_size = max_pdu_size_ - 12;
    if (max_fragment_size > 16376) {
        max_fragment_size = 16376;  // Reasonable default
    }

    size_t offset = 0;
    while (offset < data.size()) {
        size_t fragment_size = std::min(max_fragment_size, data.size() - offset);
        fragments.emplace_back(data.begin() + offset,
                               data.begin() + offset + fragment_size);
        offset += fragment_size;
    }

    if (fragments.empty()) {
        fragments.emplace_back();  // Empty fragment for empty data
    }

    return fragments;
}

}  // namespace pacs::network::pipeline
