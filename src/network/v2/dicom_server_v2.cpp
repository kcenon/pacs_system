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
 * @file dicom_server_v2.cpp
 * @brief DICOM server V2 implementation using network_system's messaging_server
 *
 * @see Issue #162 - Implement dicom_server_v2 using network_system messaging_server
 */

#include "pacs/network/v2/dicom_server_v2.hpp"

#include <algorithm>
#include <sstream>
#include <thread>

// KCENON_HAS_COMMON_SYSTEM is defined by CMake when common_system is available
#ifndef KCENON_HAS_COMMON_SYSTEM
#define KCENON_HAS_COMMON_SYSTEM 0
#endif

#ifdef PACS_WITH_NETWORK_SYSTEM
#include <kcenon/network/facade/tcp_facade.h>
#endif

#if KCENON_HAS_COMMON_SYSTEM
using kcenon::common::error_info;
#endif

namespace kcenon::pacs::network::v2 {

// =============================================================================
// Construction / Destruction
// =============================================================================

dicom_server_v2::dicom_server_v2(const server_config& config)
    : config_(config) {
    stats_.start_time = clock::now();
    stats_.last_activity = stats_.start_time;
}

dicom_server_v2::~dicom_server_v2() {
    try {
        if (running_) {
            stop();
        }
    } catch (...) {
        // Suppress exceptions in destructor to prevent std::terminate
    }
}

// =============================================================================
// Service Registration
// =============================================================================

void dicom_server_v2::register_service(services::scp_service_ptr service) {
    if (!service) {
        return;
    }

    std::lock_guard<std::mutex> lock(services_mutex_);

    // Register SOP Class mappings
    for (const auto& sop_class : service->supported_sop_classes()) {
        sop_class_to_service_[sop_class] = service.get();
    }

    services_.push_back(std::move(service));
}

std::vector<std::string> dicom_server_v2::supported_sop_classes() const {
    std::lock_guard<std::mutex> lock(services_mutex_);

    std::vector<std::string> sop_classes;
    sop_classes.reserve(sop_class_to_service_.size());

    for (const auto& [uid, _] : sop_class_to_service_) {
        sop_classes.push_back(uid);
    }

    return sop_classes;
}

// =============================================================================
// Lifecycle Management
// =============================================================================

Result<std::monostate> dicom_server_v2::start() {
    if (running_.exchange(true)) {
        return error_info("Server already running");
    }

    // Validate configuration
    if (config_.ae_title.empty()) {
        running_ = false;
        return error_info("AE Title cannot be empty");
    }

    if (config_.ae_title.length() > AE_TITLE_LENGTH) {
        running_ = false;
        return error_info("AE Title exceeds 16 characters");
    }

    if (config_.port == 0) {
        running_ = false;
        return error_info("Invalid port number");
    }

    // Check that at least one service is registered
    {
        std::lock_guard<std::mutex> lock(services_mutex_);
        if (services_.empty()) {
            running_ = false;
            return error_info("No services registered");
        }
    }

    // Reset statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.start_time = clock::now();
        stats_.last_activity = stats_.start_time;
        stats_.total_associations = 0;
        stats_.active_associations = 0;
        stats_.rejected_associations = 0;
        stats_.messages_processed = 0;
        stats_.bytes_received = 0;
        stats_.bytes_sent = 0;
    }

#ifdef PACS_WITH_NETWORK_SYSTEM
    try {
        // Create TCP server via tcp_facade
        kcenon::network::facade::tcp_facade facade;
        kcenon::network::facade::tcp_facade::server_config srv_cfg;
        srv_cfg.server_id = config_.ae_title;
        srv_cfg.port = config_.port;
        server_ = facade.create_server(srv_cfg);

        // Set up server-level callbacks
        server_->set_connection_callback(
            [this](std::shared_ptr<kcenon::network::interfaces::i_session> session) {
                on_connection(std::move(session));
            });

        server_->set_disconnection_callback(
            [this](std::string_view session_id) {
                on_disconnection(session_id);
            });

        server_->set_receive_callback(
            [this](std::string_view session_id,
                   const std::vector<uint8_t>& data) {
                on_receive(session_id, data);
            });

        server_->set_error_callback(
            [this](std::string_view session_id,
                   std::error_code ec) {
                on_network_error(session_id, ec);
            });

        // Start the server on configured port
        auto result = server_->start(config_.port);
        if (result.is_err()) {
            running_ = false;
            server_.reset();
            return error_info("Failed to start server");
        }

        return std::monostate{};
    } catch (const std::exception& e) {
        running_ = false;
        if (server_) {
            server_.reset();
        }
        return error_info(std::string("Exception during server start: ") + e.what());
    } catch (...) {
        running_ = false;
        if (server_) {
            server_.reset();
        }
        return error_info("Unknown exception during server start");
    }
#else
    running_ = false;
    return error_info("dicom_server_v2 requires PACS_WITH_NETWORK_SYSTEM");
#endif
}

void dicom_server_v2::stop(duration timeout) {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

#ifdef PACS_WITH_NETWORK_SYSTEM
    // Phase 1: Stop accepting new connections
    if (server_) {
        try {
            // Stop the server - this closes the acceptor
            (void)server_->stop();
        } catch (...) {
            // Suppress exceptions during server stop
        }
    }

    // Phase 2: Wait for handlers to complete gracefully
    auto deadline = clock::now() + timeout;
    {
        std::unique_lock<std::mutex> lock(handlers_mutex_);
        while (!handlers_.empty() && clock::now() < deadline) {
            // Release lock while waiting
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            lock.lock();
        }
    }

    // Phase 3: Force stop remaining handlers
    // Collect handlers first, then stop them without holding the lock
    // to avoid potential deadlock if handler->stop() triggers callbacks
    std::vector<std::shared_ptr<dicom_association_handler>> handlers_to_stop;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_to_stop.reserve(handlers_.size());
        for (auto& [session_id, handler] : handlers_) {
            handlers_to_stop.push_back(handler);
        }
        handlers_.clear();
    }

    // Stop handlers without holding the lock
    for (auto& handler : handlers_to_stop) {
        try {
            handler->stop(false);  // Force abort
        } catch (...) {
            // Suppress exceptions during handler stop
        }
    }
    handlers_to_stop.clear();

    // Allow any pending callbacks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    // Clear callbacks to break reference cycles before server is destroyed.
    // The server object itself is kept alive until dicom_server_v2 is destroyed
    // to avoid use-after-free in the adapter's background I/O threads.
    if (server_) {
        server_->set_connection_callback(nullptr);
        server_->set_disconnection_callback(nullptr);
        server_->set_receive_callback(nullptr);
        server_->set_error_callback(nullptr);
    }
#else
    (void)timeout;  // Unused without network_system
#endif

    // Notify shutdown waiters
    {
        std::lock_guard<std::mutex> lock(shutdown_mutex_);
        shutdown_cv_.notify_all();
    }
}

void dicom_server_v2::wait_for_shutdown() {
    std::unique_lock<std::mutex> lock(shutdown_mutex_);
    shutdown_cv_.wait(lock, [this]() { return !running_; });
}

// =============================================================================
// Status Queries
// =============================================================================

bool dicom_server_v2::is_running() const noexcept {
    return running_;
}

size_t dicom_server_v2::active_associations() const noexcept {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    return handlers_.size();
}

server_statistics dicom_server_v2::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    server_statistics result = stats_;
    result.active_associations = active_associations();
    return result;
}

const server_config& dicom_server_v2::config() const noexcept {
    return config_;
}

// =============================================================================
// Callbacks
// =============================================================================

void dicom_server_v2::on_association_established(association_established_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_established_cb_ = std::move(callback);
}

void dicom_server_v2::on_association_closed(association_closed_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_closed_cb_ = std::move(callback);
}

void dicom_server_v2::on_error(error_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_error_cb_ = std::move(callback);
}

// =============================================================================
// Network System Callbacks
// =============================================================================

void dicom_server_v2::on_connection(
    std::shared_ptr<kcenon::network::interfaces::i_session> session) {

    if (!running_ || !session) {
        return;
    }

    // Check max associations limit
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (config_.max_associations > 0 &&
            handlers_.size() >= config_.max_associations) {
            // Reject due to resource limit
            report_error("Max associations limit reached, rejecting connection");

            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.rejected_associations++;

#ifdef PACS_WITH_NETWORK_SYSTEM
            session->close();
#endif
            return;
        }
    }

    // Create handler for this session
    create_handler(std::move(session));
}

void dicom_server_v2::on_disconnection(std::string_view session_id) {
    // Skip if server is shutting down
    if (!running_) {
        return;
    }

    std::string sid(session_id);

    // Notify handler of disconnection before removing it
    auto handler = find_handler(sid);
    if (handler) {
        handler->handle_disconnect();
    }

    // Remove handler for this session
    remove_handler(sid);
}

void dicom_server_v2::on_receive(
    std::string_view session_id,
    const std::vector<uint8_t>& data) {

    std::string sid(session_id);

    // Find handler and forward data
    auto handler = find_handler(sid);
    if (handler) {
        // Forward data to handler for PDU processing
        handler->feed_data(data);

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.bytes_received += data.size();
            stats_.last_activity = clock::now();
        }
    }
}

void dicom_server_v2::on_network_error(
    std::string_view session_id,
    std::error_code ec) {

    // Skip if server is shutting down
    if (!running_) {
        return;
    }

    std::string sid(session_id);

    std::ostringstream oss;
    oss << "Network error on session " << sid
        << ": " << ec.message() << " (" << ec.value() << ")";
    report_error(oss.str());

    // Notify handler of the error
    auto handler = find_handler(sid);
    if (handler) {
        handler->handle_error(ec);
    }

    // Remove the handler - it will clean itself up
    remove_handler(sid);
}

// =============================================================================
// Handler Management
// =============================================================================

void dicom_server_v2::create_handler(
    std::shared_ptr<kcenon::network::interfaces::i_session> session) {

#ifdef PACS_WITH_NETWORK_SYSTEM
    const std::string session_id(session->id());
#else
    const std::string session_id;
    (void)session;
#endif

    // Build service map for handler
    auto service_map = build_service_map();

    // Create handler
    auto handler = std::make_shared<dicom_association_handler>(
        std::move(session), config_, service_map);

    // Set up access control if configured
    {
        std::lock_guard<std::mutex> acl_lock(acl_mutex_);
        if (access_control_) {
            handler->set_access_control(access_control_);
            handler->set_access_control_enabled(access_control_enabled_);
        }
    }

    // Set up handler callbacks
    auto weak_this = std::weak_ptr<dicom_server_v2*>(
        std::shared_ptr<dicom_server_v2*>(nullptr, [](dicom_server_v2**) {}));
    // Note: We can't use shared_from_this() since dicom_server_v2 doesn't inherit
    // from enable_shared_from_this. Instead, capture 'this' directly since the
    // handler's lifetime is bounded by the server's lifetime.

    handler->set_established_callback(
        [this](const std::string& sid, const std::string& calling_ae,
               const std::string& called_ae) {
            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_associations++;
                stats_.last_activity = clock::now();
            }

            // Forward to user callback
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (on_established_cb_) {
                    on_established_cb_(sid, calling_ae, called_ae);
                }
            }
        });

    handler->set_closed_callback(
        [this](const std::string& sid, bool graceful) {
            // Forward to user callback
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (on_closed_cb_) {
                    on_closed_cb_(sid, graceful);
                }
            }

            // Remove handler from map
            remove_handler(sid);
        });

    handler->set_error_callback(
        [this](const std::string& /*sid*/, const std::string& error) {
            report_error(error);
        });

    // Register handler
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_[session_id] = handler;
    }

    // Start handler (begins processing PDUs)
    handler->start();
}

void dicom_server_v2::remove_handler(const std::string& session_id) {
    // Skip if server is shutting down - handlers are cleaned up in stop()
    if (!running_) {
        return;
    }

    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_.find(session_id);
    if (it != handlers_.end()) {
        // Handler cleans itself up via stop() in destructor
        handlers_.erase(it);
    }
}

std::shared_ptr<dicom_association_handler>
dicom_server_v2::find_handler(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_.find(session_id);
    if (it != handlers_.end()) {
        return it->second;
    }
    return nullptr;
}

void dicom_server_v2::check_idle_timeouts() {
    if (config_.idle_timeout.count() == 0) {
        return;  // No timeout configured
    }

    auto now = clock::now();
    std::vector<std::string> timed_out;

    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        for (const auto& [session_id, handler] : handlers_) {
            auto idle_duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - handler->last_activity());

            if (idle_duration >= config_.idle_timeout) {
                timed_out.push_back(session_id);
            }
        }
    }

    // Stop timed-out handlers
    for (const auto& session_id : timed_out) {
        auto handler = find_handler(session_id);
        if (handler) {
            handler->stop(false);  // Force abort
        }
    }
}

// =============================================================================
// Internal Helpers
// =============================================================================

dicom_association_handler::service_map dicom_server_v2::build_service_map() const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    return sop_class_to_service_;
}

void dicom_server_v2::report_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (on_error_cb_) {
        on_error_cb_(error);
    }
}

// =============================================================================
// Security / Access Control
// =============================================================================

void dicom_server_v2::set_access_control(
    std::shared_ptr<security::access_control_manager> acm) {
    std::lock_guard<std::mutex> lock(acl_mutex_);
    access_control_ = std::move(acm);
    if (access_control_) {
        access_control_enabled_ = true;
    }
}

std::shared_ptr<security::access_control_manager>
dicom_server_v2::get_access_control() const noexcept {
    std::lock_guard<std::mutex> lock(acl_mutex_);
    return access_control_;
}

void dicom_server_v2::set_access_control_enabled(bool enabled) {
    access_control_enabled_ = enabled;
}

bool dicom_server_v2::is_access_control_enabled() const noexcept {
    return access_control_enabled_;
}

}  // namespace kcenon::pacs::network::v2
