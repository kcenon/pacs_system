/**
 * @file dimse_process_job.cpp
 * @brief Implementation of DIMSE process job
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #520 - Phase 3: Protocol Handling Jobs
 */

#include <pacs/network/pipeline/jobs/dimse_process_job.hpp>
#include <pacs/network/pipeline/jobs/storage_query_exec_job.hpp>

#include <chrono>

namespace pacs::network::pipeline {

dimse_process_job::dimse_process_job(decoded_pdu pdu,
                                     request_callback on_request,
                                     association_callback on_association,
                                     error_callback on_error)
    : pdu_(std::move(pdu))
    , on_request_(std::move(on_request))
    , on_association_(std::move(on_association))
    , on_error_(std::move(on_error)) {

    context_.session_id = pdu_.session_id;
    context_.stage = pipeline_stage::dimse_process;
    context_.category = job_category::other;
    context_.enqueue_time_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

auto dimse_process_job::execute(pipeline_coordinator& coordinator) -> VoidResult {
    // Handle association PDUs differently
    if (pdu_.type != pacs::network::pdu_type::p_data_tf) {
        return process_association_pdu();
    }

    // Process P-DATA-TF (DIMSE message)
    auto request_result = process_p_data();
    if (!request_result.is_ok()) {
        if (on_error_) {
            auto err = request_result.error();
            on_error_(context_.session_id, err.message);
        }
        return VoidResult(request_result.error());
    }

    auto request = request_result.value();

    // Update context with message ID and category
    context_.message_id = request.message_id;

    // Determine category from command type
    switch (request.command_type) {
        case dimse_command_type::c_echo_rq:
        case dimse_command_type::c_echo_rsp:
            context_.category = job_category::echo;
            break;
        case dimse_command_type::c_store_rq:
        case dimse_command_type::c_store_rsp:
            context_.category = job_category::store;
            break;
        case dimse_command_type::c_find_rq:
        case dimse_command_type::c_find_rsp:
            context_.category = job_category::find;
            break;
        case dimse_command_type::c_get_rq:
        case dimse_command_type::c_get_rsp:
            context_.category = job_category::get;
            break;
        case dimse_command_type::c_move_rq:
        case dimse_command_type::c_move_rsp:
            context_.category = job_category::move;
            break;
        default:
            context_.category = job_category::other;
            break;
    }

    // Invoke callback if set
    if (on_request_) {
        on_request_(request);
    }

    // For response messages, skip execution stage
    uint16_t cmd_type = static_cast<uint16_t>(request.command_type);
    if (cmd_type & 0x8000) {
        // This is a response, not a request - don't submit to execution
        return ok();
    }

    // Create execution job for request
    // Note: The actual service handler would be set by the adapter layer
    auto exec_job = std::make_unique<storage_query_exec_job>(
        std::move(request),
        nullptr  // Handler will be set by adapter
    );

    // Copy context to execution job
    exec_job->get_context() = context_;
    exec_job->get_context().stage = pipeline_stage::storage_query_exec;

    return coordinator.submit_to_stage(
        pipeline_stage::storage_query_exec,
        std::move(exec_job)
    );
}

auto dimse_process_job::get_context() const noexcept -> const job_context& {
    return context_;
}

auto dimse_process_job::get_context() noexcept -> job_context& {
    return context_;
}

auto dimse_process_job::get_name() const -> std::string {
    return "dimse_process_job[session=" +
           std::to_string(context_.session_id) +
           ", pdu_type=" + std::to_string(static_cast<int>(pdu_.type)) + "]";
}

auto dimse_process_job::get_pdu() const noexcept -> const decoded_pdu& {
    return pdu_;
}

auto dimse_process_job::process_p_data() -> Result<dimse_request> {
    dimse_request request;
    request.session_id = pdu_.session_id;
    request.presentation_context_id = pdu_.presentation_context_id;

    // PDV data starts after length (4) + context ID (1) + control header (1)
    if (pdu_.data.size() < 6) {
        return pacs_error<dimse_request>(error_codes::insufficient_data,
                                         "PDV data too short");
    }

    // Extract command from PDV data
    // The command is DICOM dataset starting at offset 6
    const auto& data = pdu_.data;
    size_t offset = 6;

    // Parse DICOM command elements
    // Minimum: (0000,0000) CommandGroupLength + (0000,0100) CommandField
    while (offset + 8 <= data.size()) {
        // Tag: group (2) + element (2)
        uint16_t group = static_cast<uint16_t>(data[offset]) |
                        (static_cast<uint16_t>(data[offset + 1]) << 8);
        uint16_t element = static_cast<uint16_t>(data[offset + 2]) |
                          (static_cast<uint16_t>(data[offset + 3]) << 8);
        offset += 4;

        // VR is implicit for command set (Implicit VR Little Endian)
        // Length (4 bytes)
        if (offset + 4 > data.size()) break;
        uint32_t length = static_cast<uint32_t>(data[offset]) |
                         (static_cast<uint32_t>(data[offset + 1]) << 8) |
                         (static_cast<uint32_t>(data[offset + 2]) << 16) |
                         (static_cast<uint32_t>(data[offset + 3]) << 24);
        offset += 4;

        if (offset + length > data.size()) break;

        // Extract key elements
        if (group == 0x0000) {
            switch (element) {
                case 0x0100:  // CommandField
                    if (length >= 2) {
                        request.command_type = static_cast<dimse_command_type>(
                            static_cast<uint16_t>(data[offset]) |
                            (static_cast<uint16_t>(data[offset + 1]) << 8)
                        );
                    }
                    break;
                case 0x0110:  // MessageID
                    if (length >= 2) {
                        request.message_id = static_cast<uint16_t>(data[offset]) |
                                            (static_cast<uint16_t>(data[offset + 1]) << 8);
                    }
                    break;
                case 0x0002:  // AffectedSOPClassUID
                case 0x0003:  // RequestedSOPClassUID
                    request.sop_class_uid = std::string(
                        reinterpret_cast<const char*>(&data[offset]), length);
                    // Trim trailing nulls/spaces
                    while (!request.sop_class_uid.empty() &&
                           (request.sop_class_uid.back() == '\0' ||
                            request.sop_class_uid.back() == ' ')) {
                        request.sop_class_uid.pop_back();
                    }
                    break;
                case 0x1000:  // AffectedSOPInstanceUID
                case 0x1001:  // RequestedSOPInstanceUID
                    request.sop_instance_uid = std::string(
                        reinterpret_cast<const char*>(&data[offset]), length);
                    while (!request.sop_instance_uid.empty() &&
                           (request.sop_instance_uid.back() == '\0' ||
                            request.sop_instance_uid.back() == ' ')) {
                        request.sop_instance_uid.pop_back();
                    }
                    break;
                case 0x0700:  // Priority
                    if (length >= 2) {
                        request.priority = static_cast<uint16_t>(data[offset]) |
                                          (static_cast<uint16_t>(data[offset + 1]) << 8);
                    }
                    break;
            }
        }

        offset += length;
    }

    // Store full command data for later use
    request.command_data = pdu_.data;

    return ok(std::move(request));
}

auto dimse_process_job::process_association_pdu() -> VoidResult {
    // Invoke association callback
    if (on_association_) {
        on_association_(pdu_.session_id, pdu_.type, pdu_.data);
    }

    // Association PDUs don't go through the execution pipeline
    // They are handled directly by the association state machine
    return ok();
}

}  // namespace pacs::network::pipeline
