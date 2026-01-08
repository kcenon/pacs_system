/**
 * @file receive_network_io_job.cpp
 * @brief Implementation of network I/O receive job
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #519 - Phase 2: Network I/O Jobs
 */

#include <pacs/network/pipeline/jobs/receive_network_io_job.hpp>
#include <pacs/network/pipeline/jobs/pdu_decode_job.hpp>

#include <chrono>

namespace pacs::network::pipeline {

receive_network_io_job::receive_network_io_job(uint64_t session_id,
                                               std::vector<uint8_t> data,
                                               data_callback on_data,
                                               error_callback on_error)
    : data_(std::move(data))
    , on_data_(std::move(on_data))
    , on_error_(std::move(on_error)) {

    context_.session_id = session_id;
    context_.stage = pipeline_stage::network_receive;
    context_.category = job_category::other;
    context_.enqueue_time_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

auto receive_network_io_job::execute(pipeline_coordinator& coordinator)
    -> VoidResult {

    // Validate we have data
    if (data_.empty()) {
        if (on_error_) {
            on_error_(context_.session_id, "Empty data received");
        }
        return pacs_void_error(error_codes::invalid_argument,
                               "Empty data received");
    }

    // Invoke callback if set
    if (on_data_) {
        on_data_(context_.session_id, data_);
    }

    // Generate job ID and update context
    context_.job_id = coordinator.generate_job_id();

    // Create decode job and submit to next stage
    auto decode_job = std::make_unique<pdu_decode_job>(
        context_.session_id,
        std::move(data_)
    );

    // Copy context to decode job
    decode_job->get_context() = context_;
    decode_job->get_context().stage = pipeline_stage::pdu_decode;

    return coordinator.submit_to_stage(
        pipeline_stage::pdu_decode,
        std::move(decode_job)
    );
}

auto receive_network_io_job::get_context() const noexcept -> const job_context& {
    return context_;
}

auto receive_network_io_job::get_context() noexcept -> job_context& {
    return context_;
}

auto receive_network_io_job::get_name() const -> std::string {
    return "receive_network_io_job[session=" +
           std::to_string(context_.session_id) + "]";
}

auto receive_network_io_job::get_data() const noexcept
    -> const std::vector<uint8_t>& {
    return data_;
}

auto receive_network_io_job::get_session_id() const noexcept -> uint64_t {
    return context_.session_id;
}

}  // namespace pacs::network::pipeline
