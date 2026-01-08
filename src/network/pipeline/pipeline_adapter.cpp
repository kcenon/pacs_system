/**
 * @file pipeline_adapter.cpp
 * @brief Implementation of pipeline adapter
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #523 - Phase 6: Integration
 */

#include <pacs/network/pipeline/pipeline_adapter.hpp>
#include <pacs/network/pipeline/jobs/dimse_process_job.hpp>

#include <chrono>

namespace pacs::network::pipeline {

// =============================================================================
// Construction & Destruction
// =============================================================================

pipeline_adapter::pipeline_adapter()
    : pipeline_adapter(pipeline_config{}) {
}

pipeline_adapter::pipeline_adapter(const pipeline_config& config)
    : coordinator_(std::make_unique<pipeline_coordinator>(config)) {

    // Set up internal callbacks
    coordinator_->set_job_completion_callback(
        [this](const job_context& ctx, bool success) {
            on_job_completed(ctx, success);
        }
    );

    coordinator_->set_backpressure_callback(
        [this](pipeline_stage stage, size_t depth) {
            on_backpressure(stage, depth);
        }
    );
}

pipeline_adapter::~pipeline_adapter() {
    if (is_running()) {
        static_cast<void>(stop());
    }
}

// =============================================================================
// Lifecycle Management
// =============================================================================

auto pipeline_adapter::start() -> VoidResult {
    return coordinator_->start();
}

auto pipeline_adapter::stop() -> VoidResult {
    return coordinator_->stop();
}

auto pipeline_adapter::is_running() const noexcept -> bool {
    return coordinator_->is_running();
}

// =============================================================================
// Session Management
// =============================================================================

void pipeline_adapter::register_session(uint64_t session_id,
                                        session_context context) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    context.session_id = session_id;
    context.created_at = std::chrono::steady_clock::now();
    context.last_activity = context.created_at;
    sessions_[session_id] = std::move(context);

    // Update metrics
    coordinator_->get_metrics().increment_active_associations();

    // Notify session event
    session_event_callback callback;
    {
        std::lock_guard<std::mutex> cb_lock(callbacks_mutex_);
        callback = session_event_callback_;
    }
    if (callback) {
        callback(session_id, "session_registered");
    }
}

void pipeline_adapter::unregister_session(uint64_t session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    if (sessions_.erase(session_id) > 0) {
        coordinator_->get_metrics().decrement_active_associations();

        // Notify session event
        session_event_callback callback;
        {
            std::lock_guard<std::mutex> cb_lock(callbacks_mutex_);
            callback = session_event_callback_;
        }
        if (callback) {
            callback(session_id, "session_unregistered");
        }
    }
}

auto pipeline_adapter::get_session(uint64_t session_id)
    -> std::optional<session_context> {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto pipeline_adapter::get_active_session_count() const noexcept -> size_t {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

// =============================================================================
// Data Handling
// =============================================================================

auto pipeline_adapter::on_data_received(uint64_t session_id,
                                        std::vector<uint8_t> data) -> VoidResult {
    if (!is_running()) {
        return pacs_void_error(error_codes::not_initialized,
                               "Pipeline adapter is not running");
    }

    // Update session activity
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second.last_activity = std::chrono::steady_clock::now();
        }
    }

    // Get send callback for use in jobs
    send_callback send_fn;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        send_fn = send_callback_;
    }

    // Get association callback
    association_callback assoc_cb;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        assoc_cb = association_callback_;
    }

    // Create receive job
    auto job = std::make_unique<receive_network_io_job>(
        session_id,
        std::move(data),
        nullptr,  // on_data callback not needed
        nullptr   // on_error callback not needed
    );

    return coordinator_->submit_to_stage(
        pipeline_stage::network_receive,
        std::move(job)
    );
}

void pipeline_adapter::on_connection_closed(uint64_t session_id) {
    unregister_session(session_id);
}

// =============================================================================
// Service Handler Registration
// =============================================================================

void pipeline_adapter::register_c_store_handler(service_handler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    c_store_handler_ = std::move(handler);
}

void pipeline_adapter::register_c_find_handler(service_handler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    c_find_handler_ = std::move(handler);
}

void pipeline_adapter::register_c_get_handler(service_handler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    c_get_handler_ = std::move(handler);
}

void pipeline_adapter::register_c_move_handler(service_handler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    c_move_handler_ = std::move(handler);
}

void pipeline_adapter::register_c_echo_handler(service_handler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    c_echo_handler_ = std::move(handler);
}

// =============================================================================
// Callback Registration
// =============================================================================

void pipeline_adapter::set_send_callback(send_callback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    send_callback_ = std::move(callback);
}

void pipeline_adapter::set_association_callback(association_callback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    association_callback_ = std::move(callback);
}

void pipeline_adapter::set_session_event_callback(
    session_event_callback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    session_event_callback_ = std::move(callback);
}

// =============================================================================
// Metrics & Statistics
// =============================================================================

auto pipeline_adapter::get_metrics() noexcept -> pipeline_metrics& {
    return coordinator_->get_metrics();
}

auto pipeline_adapter::get_metrics() const noexcept -> const pipeline_metrics& {
    return coordinator_->get_metrics();
}

auto pipeline_adapter::get_coordinator() noexcept -> pipeline_coordinator& {
    return *coordinator_;
}

auto pipeline_adapter::get_coordinator() const noexcept
    -> const pipeline_coordinator& {
    return *coordinator_;
}

// =============================================================================
// Private Methods
// =============================================================================

auto pipeline_adapter::get_handler_for_command(dimse_command_type type)
    -> service_handler {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    switch (type) {
        case dimse_command_type::c_store_rq:
            return c_store_handler_;

        case dimse_command_type::c_find_rq:
            return c_find_handler_;

        case dimse_command_type::c_get_rq:
            return c_get_handler_;

        case dimse_command_type::c_move_rq:
            return c_move_handler_;

        case dimse_command_type::c_echo_rq:
            return c_echo_handler_;

        default:
            return nullptr;
    }
}

void pipeline_adapter::on_job_completed(const job_context& ctx, bool success) {
    // Record end-to-end metrics if this is the final stage
    if (ctx.stage == pipeline_stage::network_send) {
        auto now = std::chrono::steady_clock::now();
        auto enqueue_time = std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(ctx.enqueue_time_ns)
        );
        auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - enqueue_time
        ).count();

        coordinator_->get_metrics().record_operation_completion(
            ctx.category,
            static_cast<uint64_t>(latency_ns),
            success
        );
    }
}

void pipeline_adapter::on_backpressure(pipeline_stage stage, size_t queue_depth) {
    // Log backpressure event
    // In a full implementation, this could trigger adaptive throttling
    session_event_callback callback;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callback = session_event_callback_;
    }

    if (callback) {
        std::string event = "backpressure_" + std::string(get_stage_name(stage)) +
                           "_depth_" + std::to_string(queue_depth);
        callback(0, event);  // session_id 0 for system events
    }
}

}  // namespace pacs::network::pipeline
