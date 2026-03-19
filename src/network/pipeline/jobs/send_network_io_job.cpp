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
 * @file send_network_io_job.cpp
 * @brief Implementation of network I/O send job
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #519 - Phase 2: Network I/O Jobs
 */

#include <pacs/network/pipeline/jobs/send_network_io_job.hpp>

#include <chrono>

namespace kcenon::pacs::network::pipeline {

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

}  // namespace kcenon::pacs::network::pipeline
