/**
 * @file accept_worker.cpp
 * @brief Implementation of the accept_worker class
 */

#include "pacs/network/detail/accept_worker.hpp"

#include <sstream>

namespace pacs::network::detail {

// =============================================================================
// Construction / Destruction
// =============================================================================

accept_worker::accept_worker(
    uint16_t port,
    connection_callback on_connection,
    maintenance_callback on_maintenance)
    : thread_base("accept_worker")
    , port_(port)
    , on_connection_(std::move(on_connection))
    , on_maintenance_(std::move(on_maintenance)) {
}

accept_worker::~accept_worker() {
    // Ensure graceful shutdown if still running
    if (is_running()) {
        stop();
    }
}

// =============================================================================
// Configuration
// =============================================================================

void accept_worker::set_max_pending_connections(int backlog) {
    backlog_ = backlog;
}

uint16_t accept_worker::port() const noexcept {
    return port_;
}

int accept_worker::max_pending_connections() const noexcept {
    return backlog_;
}

// =============================================================================
// Status
// =============================================================================

bool accept_worker::is_accepting() const noexcept {
    return accepting_.load(std::memory_order_acquire);
}

std::string accept_worker::to_string() const {
    std::ostringstream oss;
    oss << "accept_worker{"
        << "port=" << port_
        << ", backlog=" << backlog_
        << ", accepting=" << (is_accepting() ? "true" : "false")
        << ", running=" << (is_running() ? "true" : "false")
        << ", sessions=" << session_id_counter_.load(std::memory_order_relaxed)
        << "}";
    return oss.str();
}

// =============================================================================
// thread_base Overrides
// =============================================================================

accept_worker::result_void accept_worker::before_start() {
    // Initialize resources before starting the accept loop
    //
    // Current placeholder implementation:
    // - Sets accepting flag to true
    //
    // Future network_system integration will:
    // - Create TCP acceptor bound to port_
    // - Set socket options (reuse address, etc.)
    // - Start listening with backlog_ pending connections

    accepting_.store(true, std::memory_order_release);
    return {};
}

accept_worker::result_void accept_worker::do_work() {
    // Main work routine for the accept loop
    //
    // Current placeholder implementation:
    // - Waits for wake interval or shutdown signal
    // - Invokes maintenance callback for periodic tasks
    //
    // Future network_system integration will:
    // - Poll TCP acceptor with timeout
    // - Accept new connections
    // - Generate session ID and invoke connection callback
    // - Handle accept errors gracefully

    // Wait for wake interval or shutdown notification
    {
        std::unique_lock<std::mutex> lock(shutdown_mutex_);
        auto wake_interval = get_wake_interval();
        if (wake_interval.has_value()) {
            shutdown_cv_.wait_for(lock, wake_interval.value());
        } else {
            // Default wait of 100ms if no wake interval set
            shutdown_cv_.wait_for(lock, std::chrono::milliseconds{100});
        }
    }

    // Invoke maintenance callback for periodic tasks (e.g., idle timeout checks)
    if (on_maintenance_) {
        on_maintenance_();
    }

    return {};
}

accept_worker::result_void accept_worker::after_stop() {
    // Clean up resources after the accept loop stops
    //
    // Current placeholder implementation:
    // - Sets accepting flag to false
    //
    // Future network_system integration will:
    // - Close TCP acceptor
    // - Clean up any pending accept operations

    accepting_.store(false, std::memory_order_release);
    return {};
}

bool accept_worker::should_continue_work() const {
    // Accept worker should always continue until explicitly stopped
    // The wake interval determines how often do_work() is called
    return is_accepting();
}

void accept_worker::on_stop_requested() {
    // Signal the accept loop to wake up and check for shutdown
    //
    // Current implementation:
    // - Notifies shutdown condition variable
    //
    // Future network_system integration will:
    // - Cancel any pending async accept operations
    // - Close acceptor to unblock synchronous operations

    std::lock_guard<std::mutex> lock(shutdown_mutex_);
    shutdown_cv_.notify_all();
}

// =============================================================================
// Private Methods
// =============================================================================

uint64_t accept_worker::next_session_id() {
    return session_id_counter_.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace pacs::network::detail
