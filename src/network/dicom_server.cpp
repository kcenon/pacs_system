/**
 * @file dicom_server.cpp
 * @brief DICOM Server implementation
 */

#include "pacs/network/dicom_server.hpp"
#include "pacs/network/pdu_encoder.hpp"
#include "pacs/network/pdu_decoder.hpp"
#include "pacs/integration/thread_adapter.hpp"

#include <algorithm>
#include <sstream>

#ifdef PACS_WITH_COMMON_SYSTEM
using kcenon::common::error_info;
#endif

namespace pacs::network {

// Registry for in-memory testing
static std::mutex registry_mutex_;
static std::map<uint16_t, dicom_server*> server_registry_;

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

    // Create and start accept worker
    accept_worker_ = std::make_unique<detail::accept_worker>(
        config_.port,
        // Connection callback (currently unused - placeholder for future TCP integration)
        [](uint64_t /*session_id*/) {
            // Future: handle new TCP connection
        },
        // Maintenance callback for periodic idle timeout checks
        [this]() {
            check_idle_timeouts();
        }
    );
    accept_worker_->set_wake_interval(std::chrono::milliseconds(100));

    auto start_result = accept_worker_->start();
    if (start_result.has_error()) {
        running_ = false;
        return error_info("Failed to start accept worker: " +
                         start_result.get_error().to_string());
    }

    // Register in global registry
    {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        server_registry_[config_.port] = this;
    }

    return std::monostate{};
}

void dicom_server::stop(duration timeout) {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

    // Unregister from global registry
    {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        server_registry_.erase(config_.port);
    }

    // Stop accept worker (handles graceful shutdown via on_stop_requested hook)
    if (accept_worker_) {
        accept_worker_->stop();
        accept_worker_.reset();
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
        // Note: With thread_adapter pool, we don't need to join individual threads.
        // The message_loop will exit when running_ is false or association is aborted.
        for (auto& [id, info] : associations_) {
            info->assoc.abort(
                static_cast<uint8_t>(abort_source::service_provider),
                static_cast<uint8_t>(abort_reason::not_specified));
            // Mark as no longer processing (message_loop will check running_ and exit)
            // Note: Already holding associations_mutex_ so no atomic needed
            info->processing = false;
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
// Private Methods - Association Handling
// =============================================================================

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
    associations_.emplace(id, std::make_unique<association_info>(std::move(info)));
}

void dicom_server::remove_association(uint64_t id) {
    std::lock_guard<std::mutex> lock(associations_mutex_);
    auto it = associations_.find(id);
    if (it != associations_.end()) {
        // With thread_adapter pool, no thread join is needed.
        // Just mark as not processing and remove from map.
        // Note: Already holding associations_mutex_ so no atomic needed
        it->second->processing = false;
        associations_.erase(it);
    }
}

void dicom_server::touch_association(uint64_t id) {
    std::lock_guard<std::mutex> lock(associations_mutex_);
    auto it = associations_.find(id);
    if (it != associations_.end()) {
        it->second->last_activity = clock::now();
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
                now - info->last_activity);

            if (idle_duration >= config_.idle_timeout) {
                timed_out.push_back(id);
            }
        }

        // Abort timed-out associations
        for (auto id : timed_out) {
            auto it = associations_.find(id);
            if (it != associations_.end()) {
                it->second->assoc.abort(
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

dicom_server* dicom_server::get_server_on_port(uint16_t port) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = server_registry_.find(port);
    return it != server_registry_.end() ? it->second : nullptr;
}

Result<associate_ac> dicom_server::simulate_association_request(const associate_rq& rq, association* client_peer) {
    // Validate called AE title
    if (!validate_called_ae(rq.called_ae_title)) {
        return error_info("Called AE Title not recognized");
    }

    // Validate calling AE title
    if (!validate_calling_ae(rq.calling_ae_title)) {
        return error_info("Calling AE Title not recognized");
    }

    // Create SCP configuration for negotiation
    scp_config negotiation_config;
    negotiation_config.ae_title = config_.ae_title;
    negotiation_config.max_pdu_length = config_.max_pdu_size;
    negotiation_config.implementation_class_uid = config_.implementation_class_uid;
    negotiation_config.implementation_version_name = config_.implementation_version_name;
    negotiation_config.supported_abstract_syntaxes = supported_sop_classes();
    // For now, accept all transfer syntaxes or define default list
    negotiation_config.supported_transfer_syntaxes = {
        "1.2.840.10008.1.2.1", // Explicit VR LE
        "1.2.840.10008.1.2"    // Implicit VR LE
    };

    // Accept association
    auto assoc = association::accept(rq, negotiation_config);

    if (!assoc.is_established()) {
        return error_info("Association rejected: no acceptable presentation contexts");
    }

    // Build AC PDU before moving association
    auto ac = assoc.build_associate_ac();

    // Register association
    uint64_t id = next_association_id();
    handle_association(id, std::move(assoc));

    // Link peers and start message processing via thread pool
    {
        std::lock_guard<std::mutex> lock(associations_mutex_);
        auto it = associations_.find(id);
        if (it != associations_.end()) {
            // Link peers
            it->second->assoc.set_peer(client_peer);
            client_peer->set_peer(&it->second->assoc);

            // Submit message loop to thread_adapter pool instead of creating dedicated thread.
            // We need to capture the raw pointer to info because unique_ptr is in the map.
            // The map might rehash but the unique_ptr value (the pointer to association_info) is stable.
            auto* info_ptr = it->second.get();

            // Mark as processing before submitting to pool
            // Note: Already holding associations_mutex_ so no atomic needed
            info_ptr->processing = true;

            // Submit with high priority - association message handling is important
            integration::thread_adapter::submit_fire_and_forget(
                [this, info_ptr]() {
                    message_loop(*info_ptr);
                }
            );
        }
    }

    return ac;
}

}  // namespace pacs::network
