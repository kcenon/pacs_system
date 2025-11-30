/**
 * @file dicom_server.cpp
 * @brief DICOM Server implementation
 */

#include "pacs/network/dicom_server.hpp"
#include "pacs/network/pdu_encoder.hpp"
#include "pacs/network/pdu_decoder.hpp"

#include <algorithm>
#include <sstream>

#ifdef PACS_WITH_COMMON_SYSTEM
using kcenon::common::error_info;
#endif

namespace pacs::network {

// =============================================================================
// Construction / Destruction
// =============================================================================

dicom_server::dicom_server(const server_config& config)
    : config_(config) {
    stats_.start_time = clock::now();
    stats_.last_activity = stats_.start_time;
}

dicom_server::~dicom_server() {
    if (running_) {
        stop();
    }
}

// =============================================================================
// Service Registration
// =============================================================================

void dicom_server::register_service(services::scp_service_ptr service) {
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

std::vector<std::string> dicom_server::supported_sop_classes() const {
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

Result<std::monostate> dicom_server::start() {
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

    // Start accept thread
    accept_thread_ = std::thread([this]() {
        accept_loop();
    });

    return std::monostate{};
}

void dicom_server::stop(duration timeout) {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

    // Wake up the accept loop
    {
        std::lock_guard<std::mutex> lock(shutdown_mutex_);
        shutdown_cv_.notify_all();
    }

    // Wait for accept thread to finish
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    // Wait for active associations to complete (with timeout)
    auto deadline = clock::now() + timeout;
    {
        std::unique_lock<std::mutex> lock(associations_mutex_);
        while (!associations_.empty() && clock::now() < deadline) {
            // Release lock while waiting
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            lock.lock();
        }

        // Force abort remaining associations
        for (auto& [id, info] : associations_) {
            info.assoc.abort(
                static_cast<uint8_t>(abort_source::service_provider),
                static_cast<uint8_t>(abort_reason::not_specified));
        }
        associations_.clear();
    }
}

void dicom_server::wait_for_shutdown() {
    std::unique_lock<std::mutex> lock(shutdown_mutex_);
    shutdown_cv_.wait(lock, [this]() { return !running_; });
}

// =============================================================================
// Status Queries
// =============================================================================

bool dicom_server::is_running() const noexcept {
    return running_;
}

size_t dicom_server::active_associations() const noexcept {
    std::lock_guard<std::mutex> lock(associations_mutex_);
    return associations_.size();
}

server_statistics dicom_server::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    server_statistics result = stats_;
    result.active_associations = active_associations();
    return result;
}

const server_config& dicom_server::config() const noexcept {
    return config_;
}

// =============================================================================
// Callbacks
// =============================================================================

void dicom_server::on_association_established(association_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_established_cb_ = std::move(callback);
}

void dicom_server::on_association_released(association_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_released_cb_ = std::move(callback);
}

void dicom_server::on_error(error_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_error_cb_ = std::move(callback);
}

// =============================================================================
// Private Methods - Accept Loop
// =============================================================================

void dicom_server::accept_loop() {
    // Note: This is a placeholder implementation.
    // Full network_system integration will provide:
    // 1. TCP socket listening via network_system
    // 2. Connection acceptance
    // 3. Thread pool task submission

    while (running_) {
        // Placeholder: Wait for shutdown signal
        // Real implementation would poll for incoming connections
        std::unique_lock<std::mutex> lock(shutdown_mutex_);
        shutdown_cv_.wait_for(lock, std::chrono::milliseconds{100});

        if (!running_) {
            break;
        }

        // Check for idle timeouts periodically
        check_idle_timeouts();
    }
}

void dicom_server::handle_association(uint64_t session_id, association assoc) {
    // Check max associations limit
    {
        std::lock_guard<std::mutex> lock(associations_mutex_);
        if (config_.max_associations > 0 &&
            associations_.size() >= config_.max_associations) {
            // Reject due to resource limit
            assoc.abort(
                static_cast<uint8_t>(abort_source::service_provider),
                static_cast<uint8_t>(abort_reason::not_specified));

            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.rejected_associations++;
            return;
        }
    }

    // Create association info
    association_info info;
    info.id = session_id;
    info.assoc = std::move(assoc);
    info.connected_at = clock::now();
    info.last_activity = info.connected_at;

    // Invoke established callback
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (on_established_cb_) {
            on_established_cb_(info.assoc);
        }
    }

    // Add to active associations
    add_association(std::move(info));

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_associations++;
        stats_.last_activity = clock::now();
    }
}

void dicom_server::message_loop(association_info& info) {
    while (running_ && info.assoc.is_established()) {
        // Receive DIMSE message with timeout
        auto result = info.assoc.receive_dimse(
            std::chrono::duration_cast<association::duration>(config_.idle_timeout));

        if (result.is_err()) {
            // Check if it was a timeout vs. actual error
            if (info.assoc.is_established()) {
                // Still established, might be timeout - check idle
                auto now = clock::now();
                auto idle_duration = std::chrono::duration_cast<std::chrono::seconds>(
                    now - info.last_activity);

                if (config_.idle_timeout.count() > 0 &&
                    idle_duration >= config_.idle_timeout) {
                    // Idle timeout - abort association
                    info.assoc.abort(
                        static_cast<uint8_t>(abort_source::service_provider),
                        static_cast<uint8_t>(abort_reason::not_specified));
                }
            }
            break;
        }

        // Update activity timestamp
        info.last_activity = clock::now();
        touch_association(info.id);

        // Dispatch message to service
        auto& [context_id, msg] = result.value();
        auto dispatch_result = dispatch_to_service(info.assoc, context_id, msg);

        if (dispatch_result.is_err()) {
            report_error(dispatch_result.error().message);
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.messages_processed++;
            stats_.last_activity = clock::now();
        }
    }

    // Association ended - invoke released callback
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (on_released_cb_) {
            on_released_cb_(info.assoc);
        }
    }

    // Remove from active associations
    remove_association(info.id);
}

Result<std::monostate> dicom_server::dispatch_to_service(
    association& assoc,
    uint8_t context_id,
    const dimse::dimse_message& msg) {

    // Get the SOP Class from the message
    std::string sop_class_uid = msg.affected_sop_class_uid();

    if (sop_class_uid.empty()) {
        return error_info("Cannot determine SOP Class UID from message");
    }

    // Find the appropriate service
    auto* service = find_service(sop_class_uid);
    if (!service) {
        return error_info("No service registered for SOP Class: " + sop_class_uid);
    }

    // Dispatch to service
    return service->handle_message(assoc, context_id, msg);
}

bool dicom_server::validate_calling_ae(const std::string& calling_ae) const {
    if (config_.ae_whitelist.empty()) {
        return true;  // No whitelist = accept all
    }

    if (config_.accept_unknown_calling_ae) {
        return true;
    }

    return std::find(config_.ae_whitelist.begin(),
                     config_.ae_whitelist.end(),
                     calling_ae) != config_.ae_whitelist.end();
}

bool dicom_server::validate_called_ae(const std::string& called_ae) const {
    return called_ae == config_.ae_title;
}

services::scp_service* dicom_server::find_service(
    const std::string& sop_class_uid) const {

    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = sop_class_to_service_.find(sop_class_uid);
    if (it != sop_class_to_service_.end()) {
        return it->second;
    }
    return nullptr;
}

void dicom_server::add_association(association_info info) {
    std::lock_guard<std::mutex> lock(associations_mutex_);
    auto id = info.id;
    associations_.emplace(id, std::move(info));
}

void dicom_server::remove_association(uint64_t id) {
    std::lock_guard<std::mutex> lock(associations_mutex_);
    associations_.erase(id);
}

void dicom_server::touch_association(uint64_t id) {
    std::lock_guard<std::mutex> lock(associations_mutex_);
    auto it = associations_.find(id);
    if (it != associations_.end()) {
        it->second.last_activity = clock::now();
    }
}

void dicom_server::check_idle_timeouts() {
    if (config_.idle_timeout.count() == 0) {
        return;  // No timeout configured
    }

    auto now = clock::now();
    std::vector<uint64_t> timed_out;

    {
        std::lock_guard<std::mutex> lock(associations_mutex_);
        for (auto& [id, info] : associations_) {
            auto idle_duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - info.last_activity);

            if (idle_duration >= config_.idle_timeout) {
                timed_out.push_back(id);
            }
        }

        // Abort timed-out associations
        for (auto id : timed_out) {
            auto it = associations_.find(id);
            if (it != associations_.end()) {
                it->second.assoc.abort(
                    static_cast<uint8_t>(abort_source::service_provider),
                    static_cast<uint8_t>(abort_reason::not_specified));
            }
        }
    }
}

uint64_t dicom_server::next_association_id() {
    return association_id_counter_.fetch_add(1, std::memory_order_relaxed);
}

void dicom_server::report_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (on_error_cb_) {
        on_error_cb_(error);
    }
}

}  // namespace pacs::network
