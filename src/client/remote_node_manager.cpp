/**
 * @file remote_node_manager.cpp
 * @brief Implementation of the Remote Node Manager
 */

#include "pacs/client/remote_node_manager.hpp"
#include "pacs/storage/node_repository.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/network/dimse/status_codes.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace pacs::client {

// =============================================================================
// Constants
// =============================================================================

namespace {

/// Verification SOP Class UID (C-ECHO)
constexpr std::string_view verification_sop_class_uid = "1.2.840.10008.1.1";

/// Default transfer syntaxes for C-ECHO
const std::vector<std::string> default_transfer_syntaxes = {
    "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
    "1.2.840.10008.1.2"     // Implicit VR Little Endian
};

}  // namespace

// =============================================================================
// Implementation Structure
// =============================================================================

struct remote_node_manager::impl {
    // Repository for persistence
    std::shared_ptr<storage::node_repository> repo;

    // Logger
    std::shared_ptr<di::ILogger> logger;

    // Configuration
    node_manager_config config;

    // Status callback
    node_status_callback status_callback;
    std::mutex callback_mutex;

    // In-memory node cache
    std::unordered_map<std::string, remote_node> node_cache;
    mutable std::mutex cache_mutex;

    // Statistics per node
    std::unordered_map<std::string, node_statistics> statistics;
    mutable std::mutex stats_mutex;

    // Connection pool (node_id -> available associations)
    struct pooled_association {
        std::unique_ptr<network::association> assoc;
        std::chrono::steady_clock::time_point acquired_at;
    };
    std::unordered_map<std::string, std::deque<pooled_association>> connection_pool;
    mutable std::mutex pool_mutex;

    // Health check control
    std::atomic<bool> health_check_running{false};
    std::thread health_check_thread;
    std::condition_variable health_check_cv;
    std::mutex health_check_mutex;

    // =========================================================================
    // Helper Methods
    // =========================================================================

    void notify_status_change(std::string_view node_id, node_status status) {
        std::lock_guard<std::mutex> lock(callback_mutex);
        if (status_callback) {
            status_callback(node_id, status);
        }
    }

    void update_node_status(const std::string& node_id, node_status status,
                            const std::string& error_msg = "") {
        // Update cache
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto it = node_cache.find(node_id);
            if (it != node_cache.end()) {
                auto old_status = it->second.status;
                it->second.status = status;
                if (status == node_status::online) {
                    it->second.last_verified = std::chrono::system_clock::now();
                } else if (status == node_status::error || status == node_status::offline) {
                    it->second.last_error = std::chrono::system_clock::now();
                    it->second.last_error_message = error_msg;
                }

                // Notify if status changed
                if (old_status != status) {
                    notify_status_change(node_id, status);
                }
            }
        }

        // Update repository (ignore result as this is best-effort)
        if (repo) {
            [[maybe_unused]] auto result = repo->update_status(node_id, status, error_msg);
        }
    }

    pacs::VoidResult perform_echo(const remote_node& node) {
        using namespace network;

        // Build association config
        association_config assoc_config;
        assoc_config.calling_ae_title = config.local_ae_title;
        assoc_config.called_ae_title = node.ae_title;
        assoc_config.proposed_contexts.push_back({
            1,
            std::string(verification_sop_class_uid),
            default_transfer_syntaxes
        });

        // Connect
        auto connect_result = association::connect(
            node.host,
            node.port,
            assoc_config,
            std::chrono::duration_cast<association::duration>(node.connection_timeout)
        );

        if (connect_result.is_err()) {
            return pacs::pacs_void_error(
                pacs::error_codes::connection_failed,
                "Failed to connect to node: " + node.node_id,
                connect_result.error().message);
        }

        auto& assoc = connect_result.value();

        // Get accepted context
        auto context_id = assoc.accepted_context_id(verification_sop_class_uid);
        if (!context_id) {
            assoc.abort();
            return pacs::pacs_void_error(
                pacs::error_codes::no_acceptable_context,
                "Verification SOP Class not accepted by " + node.node_id);
        }

        // Build C-ECHO-RQ
        auto echo_rq = dimse::make_c_echo_rq(1);

        // Send request
        auto send_result = assoc.send_dimse(*context_id, echo_rq);
        if (send_result.is_err()) {
            assoc.abort();
            return pacs::pacs_void_error(
                pacs::error_codes::send_failed,
                "Failed to send C-ECHO-RQ to " + node.node_id);
        }

        // Receive response
        auto recv_result = assoc.receive_dimse(
            std::chrono::duration_cast<association::duration>(node.dimse_timeout)
        );

        if (recv_result.is_err()) {
            assoc.abort();
            return pacs::pacs_void_error(
                pacs::error_codes::receive_failed,
                "Failed to receive C-ECHO-RSP from " + node.node_id);
        }

        const auto& [recv_context_id, response] = recv_result.value();

        // Verify response
        if (response.command() != dimse::command_field::c_echo_rsp) {
            assoc.abort();
            return pacs::pacs_void_error(
                pacs::error_codes::dimse_error,
                "Unexpected response from " + node.node_id);
        }

        // Check status (status_success is a constexpr, not enum member)
        if (response.status() != dimse::status_success) {
            [[maybe_unused]] auto release_result = assoc.release();
            return pacs::pacs_void_error(
                pacs::error_codes::dimse_error,
                "C-ECHO failed with status: " +
                std::to_string(static_cast<uint16_t>(response.status())));
        }

        // Release association
        [[maybe_unused]] auto release_result = assoc.release();

        return pacs::ok();
    }

    void health_check_loop() {
        while (health_check_running.load()) {
            // Verify all nodes
            std::vector<std::string> node_ids;
            {
                std::lock_guard<std::mutex> lock(cache_mutex);
                for (const auto& [id, _] : node_cache) {
                    node_ids.push_back(id);
                }
            }

            for (const auto& id : node_ids) {
                if (!health_check_running.load()) break;

                std::optional<remote_node> node;
                {
                    std::lock_guard<std::mutex> lock(cache_mutex);
                    auto it = node_cache.find(id);
                    if (it != node_cache.end()) {
                        node = it->second;
                    }
                }

                if (node) {
                    update_node_status(id, node_status::verifying);

                    auto result = perform_echo(*node);
                    if (result.is_ok()) {
                        update_node_status(id, node_status::online);

                        // Update statistics
                        std::lock_guard<std::mutex> lock(stats_mutex);
                        statistics[id].successful_operations++;
                        statistics[id].last_activity = std::chrono::system_clock::now();
                    } else {
                        update_node_status(id, node_status::offline, result.error().message);

                        // Update statistics
                        std::lock_guard<std::mutex> lock(stats_mutex);
                        statistics[id].failed_operations++;
                    }
                }
            }

            // Wait for next interval
            std::unique_lock<std::mutex> lock(health_check_mutex);
            health_check_cv.wait_for(lock, config.health_check_interval, [this] {
                return !health_check_running.load();
            });
        }
    }

    void load_nodes_from_repo() {
        if (!repo) return;

        auto nodes_result = repo->find_all();
        if (nodes_result.is_err()) return;

        std::lock_guard<std::mutex> lock(cache_mutex);
        for (auto& node : nodes_result.value()) {
            node_cache[node.node_id] = std::move(node);
        }
    }
};

// =============================================================================
// Construction / Destruction
// =============================================================================

remote_node_manager::remote_node_manager(
    std::shared_ptr<storage::node_repository> repo,
    node_manager_config config,
    std::shared_ptr<di::ILogger> logger)
    : impl_(std::make_unique<impl>()) {

    impl_->repo = std::move(repo);
    impl_->config = std::move(config);
    impl_->logger = logger ? std::move(logger) : di::null_logger();

    // Load existing nodes from repository
    impl_->load_nodes_from_repo();

    // Auto-start health check if configured
    if (impl_->config.auto_start_health_check) {
        start_health_check();
    }
}

remote_node_manager::~remote_node_manager() {
    stop_health_check();

    // Clear connection pool
    {
        std::lock_guard<std::mutex> lock(impl_->pool_mutex);
        impl_->connection_pool.clear();
    }
}

// =============================================================================
// Node CRUD Operations
// =============================================================================

pacs::VoidResult remote_node_manager::add_node(const remote_node& node) {
    if (node.node_id.empty()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Node ID cannot be empty");
    }

    if (node.ae_title.empty()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "AE Title cannot be empty");
    }

    if (node.host.empty()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Host cannot be empty");
    }

    // Check for duplicate
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        if (impl_->node_cache.find(node.node_id) != impl_->node_cache.end()) {
            return pacs::pacs_void_error(
                pacs::error_codes::already_exists,
                "Node with ID already exists: " + node.node_id);
        }
    }

    // Persist to repository
    if (impl_->repo) {
        auto result = impl_->repo->save(node);
        if (result.is_err()) {
            return pacs::pacs_void_error(
                result.error().code,
                "Failed to persist node: " + result.error().message);
        }
    }

    // Add to cache
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        impl_->node_cache[node.node_id] = node;
    }

    impl_->logger->info_fmt("Added remote node: {} ({}:{})",
                            node.node_id, node.host, node.port);

    return pacs::ok();
}

pacs::VoidResult remote_node_manager::update_node(const remote_node& node) {
    // Check if exists
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        if (impl_->node_cache.find(node.node_id) == impl_->node_cache.end()) {
            return pacs::pacs_void_error(
                pacs::error_codes::not_found,
                "Node not found: " + node.node_id);
        }
    }

    // Update repository
    if (impl_->repo) {
        auto result = impl_->repo->save(node);
        if (result.is_err()) {
            return pacs::pacs_void_error(
                result.error().code,
                "Failed to update node: " + result.error().message);
        }
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        impl_->node_cache[node.node_id] = node;
    }

    impl_->logger->info_fmt("Updated remote node: {}", node.node_id);

    return pacs::ok();
}

pacs::VoidResult remote_node_manager::remove_node(std::string_view node_id) {
    std::string id_str(node_id);

    // Check if exists
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        if (impl_->node_cache.find(id_str) == impl_->node_cache.end()) {
            return pacs::pacs_void_error(
                pacs::error_codes::not_found,
                "Node not found: " + id_str);
        }
    }

    // Remove from repository
    if (impl_->repo) {
        auto result = impl_->repo->remove(std::string(node_id));
        if (result.is_err()) {
            return result;
        }
    }

    // Remove from cache
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        impl_->node_cache.erase(id_str);
    }

    // Remove from connection pool
    {
        std::lock_guard<std::mutex> lock(impl_->pool_mutex);
        impl_->connection_pool.erase(id_str);
    }

    // Remove statistics
    {
        std::lock_guard<std::mutex> lock(impl_->stats_mutex);
        impl_->statistics.erase(id_str);
    }

    impl_->logger->info_fmt("Removed remote node: {}", id_str);

    return pacs::ok();
}

std::optional<remote_node> remote_node_manager::get_node(std::string_view node_id) const {
    std::lock_guard<std::mutex> lock(impl_->cache_mutex);
    auto it = impl_->node_cache.find(std::string(node_id));
    if (it != impl_->node_cache.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<remote_node> remote_node_manager::list_nodes() const {
    std::vector<remote_node> result;
    std::lock_guard<std::mutex> lock(impl_->cache_mutex);
    result.reserve(impl_->node_cache.size());
    for (const auto& [_, node] : impl_->node_cache) {
        result.push_back(node);
    }
    return result;
}

std::vector<remote_node> remote_node_manager::list_nodes_by_status(node_status status) const {
    std::vector<remote_node> result;
    std::lock_guard<std::mutex> lock(impl_->cache_mutex);
    for (const auto& [_, node] : impl_->node_cache) {
        if (node.status == status) {
            result.push_back(node);
        }
    }
    return result;
}

// =============================================================================
// Connection Verification
// =============================================================================

pacs::VoidResult remote_node_manager::verify_node(std::string_view node_id) {
    std::optional<remote_node> node;
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        auto it = impl_->node_cache.find(std::string(node_id));
        if (it != impl_->node_cache.end()) {
            node = it->second;
        }
    }

    if (!node) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Node not found: " + std::string(node_id));
    }

    impl_->update_node_status(std::string(node_id), node_status::verifying);

    auto result = impl_->perform_echo(*node);

    if (result.is_ok()) {
        impl_->update_node_status(std::string(node_id), node_status::online);
        if (impl_->repo) {
            [[maybe_unused]] auto update_result = impl_->repo->update_last_verified(node_id);
        }
    } else {
        impl_->update_node_status(std::string(node_id), node_status::offline,
                                  result.error().message);
    }

    return result;
}

std::future<pacs::VoidResult> remote_node_manager::verify_node_async(
    std::string_view node_id) {

    std::string id_str(node_id);

    return std::async(std::launch::async, [this, id_str]() {
        return verify_node(id_str);
    });
}

void remote_node_manager::verify_all_nodes_async() {
    std::vector<std::string> node_ids;
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        for (const auto& [id, _] : impl_->node_cache) {
            node_ids.push_back(id);
        }
    }

    for (const auto& id : node_ids) {
        std::thread([this, id]() {
            [[maybe_unused]] auto result = verify_node(id);
        }).detach();
    }
}

// =============================================================================
// Association Pool Management
// =============================================================================

pacs::Result<std::unique_ptr<network::association>> remote_node_manager::acquire_association(
    std::string_view node_id,
    std::span<const std::string> sop_classes) {

    std::string id_str(node_id);
    std::optional<remote_node> node;

    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex);
        auto it = impl_->node_cache.find(id_str);
        if (it != impl_->node_cache.end()) {
            node = it->second;
        }
    }

    if (!node) {
        return pacs::pacs_error<std::unique_ptr<network::association>>(
            pacs::error_codes::not_found,
            "Node not found: " + id_str);
    }

    // Try to get from pool
    {
        std::lock_guard<std::mutex> lock(impl_->pool_mutex);
        auto it = impl_->connection_pool.find(id_str);
        if (it != impl_->connection_pool.end() && !it->second.empty()) {
            auto pooled = std::move(it->second.front());
            it->second.pop_front();

            // Check if connection is still valid and not expired
            auto age = std::chrono::steady_clock::now() - pooled.acquired_at;
            if (age < impl_->config.pool_connection_ttl &&
                pooled.assoc && pooled.assoc->is_established()) {
                impl_->logger->debug_fmt("Reusing pooled association for {}", id_str);
                return pacs::ok(std::move(pooled.assoc));
            }
        }
    }

    // Create new association
    network::association_config assoc_config;
    assoc_config.calling_ae_title = impl_->config.local_ae_title;
    assoc_config.called_ae_title = node->ae_title;

    uint8_t context_id = 1;
    for (const auto& sop_class : sop_classes) {
        assoc_config.proposed_contexts.push_back({
            context_id,
            sop_class,
            default_transfer_syntaxes
        });
        context_id += 2;  // Context IDs must be odd
    }

    auto connect_result = network::association::connect(
        node->host,
        node->port,
        assoc_config,
        std::chrono::duration_cast<network::association::duration>(node->connection_timeout)
    );

    if (connect_result.is_err()) {
        impl_->update_node_status(id_str, node_status::offline,
                                  connect_result.error().message);
        return connect_result.error();
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(impl_->stats_mutex);
        impl_->statistics[id_str].total_connections++;
        impl_->statistics[id_str].active_connections++;
    }

    return pacs::ok(std::make_unique<network::association>(std::move(connect_result.value())));
}

void remote_node_manager::release_association(
    std::string_view node_id,
    std::unique_ptr<network::association> assoc) {

    std::string id_str(node_id);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(impl_->stats_mutex);
        auto it = impl_->statistics.find(id_str);
        if (it != impl_->statistics.end() && it->second.active_connections > 0) {
            it->second.active_connections--;
        }
    }

    if (!assoc || !assoc->is_established()) {
        return;
    }

    // Check pool capacity
    {
        std::lock_guard<std::mutex> lock(impl_->pool_mutex);
        auto& pool = impl_->connection_pool[id_str];
        if (pool.size() < impl_->config.max_pool_connections_per_node) {
            impl::pooled_association pooled;
            pooled.assoc = std::move(assoc);
            pooled.acquired_at = std::chrono::steady_clock::now();
            pool.push_back(std::move(pooled));
            impl_->logger->debug_fmt("Returned association to pool for {}", id_str);
            return;
        }
    }

    // Pool is full, release the association
    [[maybe_unused]] auto release_result = assoc->release();
}

// =============================================================================
// Health Check Scheduler
// =============================================================================

void remote_node_manager::start_health_check() {
    if (impl_->health_check_running.load()) {
        return;
    }

    impl_->health_check_running.store(true);
    impl_->health_check_thread = std::thread([this]() {
        impl_->health_check_loop();
    });

    impl_->logger->info("Started health check scheduler");
}

void remote_node_manager::stop_health_check() {
    if (!impl_->health_check_running.load()) {
        return;
    }

    impl_->health_check_running.store(false);
    impl_->health_check_cv.notify_all();

    if (impl_->health_check_thread.joinable()) {
        impl_->health_check_thread.join();
    }

    impl_->logger->info("Stopped health check scheduler");
}

bool remote_node_manager::is_health_check_running() const noexcept {
    return impl_->health_check_running.load();
}

// =============================================================================
// Status Monitoring
// =============================================================================

node_status remote_node_manager::get_status(std::string_view node_id) const {
    std::lock_guard<std::mutex> lock(impl_->cache_mutex);
    auto it = impl_->node_cache.find(std::string(node_id));
    if (it != impl_->node_cache.end()) {
        return it->second.status;
    }
    return node_status::unknown;
}

void remote_node_manager::set_status_callback(node_status_callback callback) {
    std::lock_guard<std::mutex> lock(impl_->callback_mutex);
    impl_->status_callback = std::move(callback);
}

// =============================================================================
// Statistics
// =============================================================================

node_statistics remote_node_manager::get_statistics(std::string_view node_id) const {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    auto it = impl_->statistics.find(std::string(node_id));
    if (it != impl_->statistics.end()) {
        return it->second;
    }
    return {};
}

void remote_node_manager::reset_statistics(std::string_view node_id) {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    if (node_id.empty()) {
        impl_->statistics.clear();
    } else {
        impl_->statistics.erase(std::string(node_id));
    }
}

// =============================================================================
// Configuration
// =============================================================================

const node_manager_config& remote_node_manager::config() const noexcept {
    return impl_->config;
}

void remote_node_manager::set_config(node_manager_config new_config) {
    impl_->config = std::move(new_config);
}

}  // namespace pacs::client
