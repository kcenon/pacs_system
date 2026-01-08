/**
 * @file send_network_io_job.cpp
 * @brief Implementation of network I/O send job
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #519 - Phase 2: Network I/O Jobs
 */

#include <pacs/network/pipeline/jobs/send_network_io_job.hpp>

#include <chrono>

namespace pacs::network::pipeline {

send_network_io_job::send_network_io_job(uint64_t session_id,
                                         std::vector<uint8_t> data,
                                         send_function send_fn,
                                         completion_callback on_complete,
                                         error_callback on_error)
    : data_(std::move(data))
    , send_fn_(std::move(send_fn))
    , on_complete_(std::move(on_complete))
    , on_error_(std::move(on_error)) {

    context_.session_id = session_id;
    context_.stage = pipeline_stage::network_send;
    context_.category = job_category::other;
    context_.enqueue_time_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

auto send_network_io_job::execute(pipeline_coordinator& /*coordinator*/)
    -> VoidResult {

    // Validate we have data and send function
    if (data_.empty()) {
        if (on_error_) {
            on_error_(context_.session_id, "Empty data to send");
        }
        return pacs_void_error(error_codes::invalid_argument,
                               "Empty data to send");
    }

    if (!send_fn_) {
        if (on_error_) {
            on_error_(context_.session_id, "No send function provided");
        }
        return pacs_void_error(error_codes::invalid_argument,
                               "No send function provided");
    }

    // Perform the actual network send
    auto result = send_fn_(context_.session_id, data_);

    if (!result.is_ok()) {
        if (on_error_) {
            auto err = result.error();
            on_error_(context_.session_id, err.message);
        }
        if (on_complete_) {
            on_complete_(context_.session_id, false, 0);
        }
        return result;
    }

    // Success - invoke completion callback
    if (on_complete_) {
        on_complete_(context_.session_id, true, data_.size());
    }

    return ok();
}

auto send_network_io_job::get_context() const noexcept -> const job_context& {
    return context_;
}

auto send_network_io_job::get_context() noexcept -> job_context& {
    return context_;
}

auto send_network_io_job::get_name() const -> std::string {
    return "send_network_io_job[session=" +
           std::to_string(context_.session_id) +
           ", bytes=" + std::to_string(data_.size()) + "]";
}

auto send_network_io_job::get_data() const noexcept
    -> const std::vector<uint8_t>& {
    return data_;
}

auto send_network_io_job::get_session_id() const noexcept -> uint64_t {
    return context_.session_id;
}

}  // namespace pacs::network::pipeline
