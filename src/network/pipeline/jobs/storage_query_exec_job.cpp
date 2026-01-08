/**
 * @file storage_query_exec_job.cpp
 * @brief Implementation of storage/query execution job
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #521 - Phase 4: Execution Jobs
 */

#include <pacs/network/pipeline/jobs/storage_query_exec_job.hpp>
#include <pacs/network/pipeline/jobs/response_encode_job.hpp>

#include <chrono>

namespace pacs::network::pipeline {

storage_query_exec_job::storage_query_exec_job(dimse_request request,
                                               service_handler handler,
                                               completion_callback on_complete,
                                               error_callback on_error)
    : request_(std::move(request))
    , handler_(std::move(handler))
    , on_complete_(std::move(on_complete))
    , on_error_(std::move(on_error)) {

    context_.session_id = request_.session_id;
    context_.message_id = request_.message_id;
    context_.stage = pipeline_stage::storage_query_exec;
    context_.category = get_category_for_command(request_.command_type);
    context_.enqueue_time_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

auto storage_query_exec_job::execute(pipeline_coordinator& coordinator)
    -> VoidResult {

    // Check if handler is set
    if (!handler_) {
        // Create default response (no handler means we need to reject)
        service_result result;
        result.session_id = request_.session_id;
        result.message_id = request_.message_id;
        result.presentation_context_id = request_.presentation_context_id;
        result.sop_class_uid = request_.sop_class_uid;
        result.sop_instance_uid = request_.sop_instance_uid;
        result.status = dimse_status::failure_refused_sop_class_not_supported;
        result.error_comment = "No service handler configured";

        // Determine response type
        uint16_t cmd = static_cast<uint16_t>(request_.command_type);
        result.response_type = static_cast<dimse_command_type>(cmd | 0x8000);

        if (on_error_) {
            on_error_(request_.session_id, "No service handler configured");
        }

        // Still submit to encode stage for proper response
        auto encode_job = std::make_unique<response_encode_job>(std::move(result));
        encode_job->get_context() = context_;
        encode_job->get_context().stage = pipeline_stage::response_encode;

        return coordinator.submit_to_stage(
            pipeline_stage::response_encode,
            std::move(encode_job)
        );
    }

    // Execute the service handler
    auto result = handler_(request_);

    if (!result.is_ok()) {
        if (on_error_) {
            auto err = result.error();
            on_error_(request_.session_id, err.message);
        }

        // Create error response
        service_result error_response;
        error_response.session_id = request_.session_id;
        error_response.message_id = request_.message_id;
        error_response.presentation_context_id = request_.presentation_context_id;
        error_response.sop_class_uid = request_.sop_class_uid;
        error_response.sop_instance_uid = request_.sop_instance_uid;
        error_response.status = dimse_status::failure_unable_to_process;
        error_response.error_comment = result.error().message;

        uint16_t cmd = static_cast<uint16_t>(request_.command_type);
        error_response.response_type = static_cast<dimse_command_type>(cmd | 0x8000);

        auto encode_job = std::make_unique<response_encode_job>(std::move(error_response));
        encode_job->get_context() = context_;
        encode_job->get_context().stage = pipeline_stage::response_encode;

        return coordinator.submit_to_stage(
            pipeline_stage::response_encode,
            std::move(encode_job)
        );
    }

    auto service_result_value = result.value();

    // Invoke completion callback
    if (on_complete_) {
        on_complete_(service_result_value);
    }

    // Submit to encode stage
    auto encode_job = std::make_unique<response_encode_job>(std::move(service_result_value));
    encode_job->get_context() = context_;
    encode_job->get_context().stage = pipeline_stage::response_encode;

    return coordinator.submit_to_stage(
        pipeline_stage::response_encode,
        std::move(encode_job)
    );
}

auto storage_query_exec_job::get_context() const noexcept -> const job_context& {
    return context_;
}

auto storage_query_exec_job::get_context() noexcept -> job_context& {
    return context_;
}

auto storage_query_exec_job::get_name() const -> std::string {
    return "storage_query_exec_job[session=" +
           std::to_string(context_.session_id) +
           ", cmd=" + std::to_string(static_cast<uint16_t>(request_.command_type)) +
           ", msgid=" + std::to_string(request_.message_id) + "]";
}

auto storage_query_exec_job::get_request() const noexcept -> const dimse_request& {
    return request_;
}

auto storage_query_exec_job::get_category_for_command(dimse_command_type type)
    -> job_category {
    switch (type) {
        case dimse_command_type::c_echo_rq:
        case dimse_command_type::c_echo_rsp:
            return job_category::echo;

        case dimse_command_type::c_store_rq:
        case dimse_command_type::c_store_rsp:
            return job_category::store;

        case dimse_command_type::c_find_rq:
        case dimse_command_type::c_find_rsp:
            return job_category::find;

        case dimse_command_type::c_get_rq:
        case dimse_command_type::c_get_rsp:
            return job_category::get;

        case dimse_command_type::c_move_rq:
        case dimse_command_type::c_move_rsp:
            return job_category::move;

        default:
            return job_category::other;
    }
}

}  // namespace pacs::network::pipeline
