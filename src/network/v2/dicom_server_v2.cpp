// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file dicom_server_v2.cpp
 * @brief DICOM server V2 implementation using network_system's messaging_server
 *
 * @see Issue #162 - Implement dicom_server_v2 using network_system messaging_server
 */

#include "kcenon/pacs/network/v2/dicom_server_v2.h"

#include <algorithm>
#include <sstream>
#include <thread>

// KCENON_HAS_COMMON_SYSTEM is defined by CMake when common_system is available
#ifndef KCENON_HAS_COMMON_SYSTEM
#define KCENON_HAS_COMMON_SYSTEM 0
#endif

#ifdef PACS_WITH_NETWORK_SYSTEM
#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/session/session.h>
#include <kcenon/pacs/compat/factory_result.h>
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
    // Sessions are closed on this thread, never inside network_system's
    // session callbacks (see dicom_association_handler::set_session_closer())
    {
        std::lock_guard<std::mutex> lock(closer_mutex_);
        closer_running_ = true;
    }
    closer_thread_ = std::thread([this]() { run_session_closer(); });

    try {
        // Create TCP server via tcp_facade
        kcenon::network::facade::tcp_facade facade;
        kcenon::network::facade::tcp_facade::server_config srv_cfg;
        srv_cfg.server_id = config_.ae_title;
        srv_cfg.port = config_.port;
        auto server_result = compat::factory_result(facade.create_server(srv_cfg));
        if (server_result.is_err()) {
            running_ = false;
            stop_session_closer();
            return server_result.error();
        }
        server_ = std::move(server_result.value());

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
            stop_session_closer();
            return error_info("Failed to start server");
        }

        return std::monostate{};
    } catch (const std::exception& e) {
        running_ = false;
        if (server_) {
            server_.reset();
        }
        stop_session_closer();
        return error_info(std::string("Exception during server start: ") + e.what());
    } catch (...) {
        running_ = false;
        if (server_) {
            server_.reset();
        }
        stop_session_closer();
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
    // network_system's server->stop() destroys the io_context of its
    // sessions, so every session pointer that the handlers and the closer
    // hold is dropped first: a socket destroyed after its io_context is a
    // use-after-free (it crashes on Windows).
    (void)timeout;

    // Phase 1: Close the handlers and drop them. Collect them first, then
    // stop them without holding the lock, since stop() runs callbacks. A
    // network callback that is running holds its own reference until it
    // returns.
    std::vector<std::shared_ptr<dicom_association_handler>> handlers_to_stop;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_to_stop.reserve(handlers_.size());
        for (auto& [session_id, handler] : handlers_) {
            handlers_to_stop.push_back(handler);
        }
        handlers_.clear();
    }

    for (auto& handler : handlers_to_stop) {
        try {
            handler->stop(false);  // Force abort
        } catch (...) {
            // Suppress exceptions during handler stop
        }
    }
    handlers_to_stop.clear();

    // Phase 2: Close the sessions the handlers queued now, then end the
    // closer thread
    stop_session_closer();

    // Phase 3: Stop network_system's server: the acceptor, any session left
    // (a connection that arrived while stopping), and the io_context
    if (server_) {
        try {
            (void)server_->stop();
        } catch (...) {
            // Suppress exceptions during server stop
        }
    }

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

    if (!session) {
        return;
    }

    if (!running_) {
#ifdef PACS_WITH_NETWORK_SYSTEM
        // A connection that arrives while stop() runs gets no handler
        session->close();
#endif
        return;
    }

    // Check max associations limit
    bool at_limit = false;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        at_limit = config_.max_associations > 0 &&
                   handlers_.size() >= config_.max_associations;
    }
    if (at_limit) {
        // Reject due to resource limit
        report_error("Max associations limit reached, rejecting connection");

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.rejected_associations++;
        }

#ifdef PACS_WITH_NETWORK_SYSTEM
        // Not under handlers_mutex_: closing reports the disconnection to
        // on_disconnection(), which takes it
        session->close();
#endif
        return;
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
    // network_system reports the server id as every session's id(), so a
    // TCP session gets an id of its own and its events are routed to its
    // handler directly (route_session_events())
    const bool routable =
        std::dynamic_pointer_cast<kcenon::network::session::messaging_session>(session) !=
        nullptr;
    const std::string session_id = routable
        ? std::string(session->id()) + "#" +
              std::to_string(next_session_number_.fetch_add(1) + 1)
        : std::string(session->id());
    const auto session_for_routing = session;
#else
    const std::string session_id;
#endif

    // Build service map for handler
    auto service_map = build_service_map();

    // Create handler
    auto handler = std::make_shared<dicom_association_handler>(
        std::move(session), config_, service_map, session_id);

    // Closes from inside network_system callbacks would deadlock
    handler->set_session_closer(
        [this](const dicom_association_handler::session_ptr& closing) {
            close_session_later(closing);
        });

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

#ifdef PACS_WITH_NETWORK_SYSTEM
    // This runs in the connection callback on the network thread, before
    // the session's first read completes, so no data is routed the old way
    if (routable) {
        (void)route_session_events(session_for_routing, session_id, handler);
    }
#endif

    // Start handler (begins processing PDUs)
    handler->start();
}

bool dicom_server_v2::route_session_events(
    const std::shared_ptr<kcenon::network::interfaces::i_session>& session,
    const std::string& key,
    const std::weak_ptr<dicom_association_handler>& handler) {
#ifdef PACS_WITH_NETWORK_SYSTEM
    auto messaging =
        std::dynamic_pointer_cast<kcenon::network::session::messaging_session>(session);
    if (!messaging) {
        return false;
    }

    // The callbacks capture this: stop() runs before destruction, and after
    // its server_->stop() and closer thread join no session callback runs
    messaging->set_receive_callback(
        [this, handler](const std::vector<uint8_t>& data) {
            auto target = handler.lock();
            if (!target) {
                return;
            }
            target->feed_data(data);

            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.bytes_received += data.size();
            stats_.last_activity = clock::now();
        });

    // A read error (the peer closing included) comes before the session
    // stops. The handler reports it through its error callback.
    messaging->set_error_callback(
        [this, handler, key](std::error_code ec) {
            if (auto target = handler.lock()) {
                target->handle_error(ec);
            }
            remove_handler(key);
        });

    // The disconnection callback stays as network_system set it: through it
    // tcp_server_adapter drops the session it tracks, and a session it kept
    // past server_->stop() would outlive its io_context.

    return true;
#else
    (void)session;
    (void)key;
    (void)handler;
    return false;
#endif
}

void dicom_server_v2::close_session_later(
    const std::shared_ptr<kcenon::network::interfaces::i_session>& session) {
    if (!session) {
        return;
    }
#ifdef PACS_WITH_NETWORK_SYSTEM
    if (!session->is_connected()) {
        return;  // The peer closed it already
    }
#endif

    // The peer closes first in the normal case; the delay never drops below
    // the time a queued write of the last PDU needs
    constexpr duration minimum_close_delay{500};
    const auto delay = (std::max)(
        std::chrono::duration_cast<duration>(config_.association_timeout),
        minimum_close_delay);
    {
        std::lock_guard<std::mutex> lock(closer_mutex_);
        if (closer_running_) {
            sessions_to_close_.emplace_back(clock::now() + delay, session);
            closer_cv_.notify_one();
            return;
        }
    }
#ifdef PACS_WITH_NETWORK_SYSTEM
    // The closer has stopped, which stop() does only after it closed every
    // handler; a closed handler does not close again, so this does not run
    // inside a network_system callback
    session->close();
#endif
}

void dicom_server_v2::run_session_closer() {
    std::unique_lock<std::mutex> lock(closer_mutex_);
    for (;;) {
        std::vector<std::shared_ptr<kcenon::network::interfaces::i_session>> due;
        if (!closer_running_) {
            // Stopping: close everything that is left now
            for (auto& [deadline, session] : sessions_to_close_) {
                due.push_back(std::move(session));
            }
            sessions_to_close_.clear();
        } else if (sessions_to_close_.empty()) {
            closer_cv_.wait(lock);
            continue;
        } else {
            const auto now = clock::now();
            auto earliest = sessions_to_close_.front().first;
            for (auto it = sessions_to_close_.begin(); it != sessions_to_close_.end();) {
                if (it->first <= now) {
                    due.push_back(std::move(it->second));
                    it = sessions_to_close_.erase(it);
                } else {
                    earliest = (std::min)(earliest, it->first);
                    ++it;
                }
            }
            if (due.empty()) {
                closer_cv_.wait_until(lock, earliest);
                continue;
            }
        }

        const bool stopping = !closer_running_;
        lock.unlock();
#ifdef PACS_WITH_NETWORK_SYSTEM
        for (auto& session : due) {
            try {
                session->close();
            } catch (...) {
                // Suppress exceptions during session cleanup
            }
        }
#endif
        due.clear();
        if (stopping) {
            return;
        }
        lock.lock();
    }
}

void dicom_server_v2::stop_session_closer() {
    {
        std::lock_guard<std::mutex> lock(closer_mutex_);
        closer_running_ = false;
    }
    closer_cv_.notify_all();
    if (!closer_thread_.joinable()) {
        return;
    }
    if (closer_thread_.get_id() == std::this_thread::get_id()) {
        // stop() from a callback the closer thread runs: it drains and
        // exits on its own
        closer_thread_.detach();
    } else {
        closer_thread_.join();
    }
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
