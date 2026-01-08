/**
 * @file pipeline_adapter.hpp
 * @brief Adapter for integrating pipeline with existing DICOM components
 *
 * This adapter bridges the pipeline infrastructure with existing
 * DICOM server components, enabling gradual migration to the new
 * architecture.
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #523 - Phase 6: Integration
 */

#pragma once

#include <pacs/network/pipeline/pipeline_coordinator.hpp>
#include <pacs/network/pipeline/pipeline_job_types.hpp>
#include <pacs/network/pipeline/jobs/receive_network_io_job.hpp>
#include <pacs/network/pipeline/jobs/send_network_io_job.hpp>
#include <pacs/network/pipeline/jobs/storage_query_exec_job.hpp>
#include <pacs/core/result.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pacs::network::pipeline {

/**
 * @struct session_context
 * @brief Context for a DICOM association session
 */
struct session_context {
    /// Session identifier
    uint64_t session_id;

    /// Remote AE Title
    std::string remote_ae_title;

    /// Local AE Title
    std::string local_ae_title;

    /// Maximum PDU size negotiated
    uint32_t max_pdu_size{16384};

    /// Creation timestamp
    std::chrono::steady_clock::time_point created_at;

    /// Last activity timestamp
    std::chrono::steady_clock::time_point last_activity;
};

/**
 * @class pipeline_adapter
 * @brief Adapter for integrating pipeline with DICOM server components
 *
 * The adapter provides:
 * - Session management for active associations
 * - Service handler registration for DIMSE operations
 * - Network send/receive callbacks
 * - Graceful shutdown coordination
 *
 * @example
 * @code
 * // Create and configure adapter
 * pipeline_config config;
 * config.execution_workers = 16;
 *
 * auto adapter = std::make_shared<pipeline_adapter>(config);
 *
 * // Register service handlers
 * adapter->register_c_store_handler([](const dimse_request& req) {
 *     // Handle C-STORE
 *     return create_success_result(req);
 * });
 *
 * // Set network callback
 * adapter->set_send_callback([](uint64_t session_id, const std::vector<uint8_t>& data) {
 *     // Send data via network
 *     return ok();
 * });
 *
 * // Start pipeline
 * adapter->start();
 *
 * // Handle incoming data
 * adapter->on_data_received(session_id, pdu_bytes);
 * @endcode
 */
class pipeline_adapter {
public:
    /// Type for network send callback
    using send_callback = std::function<VoidResult(uint64_t session_id,
                                                   const std::vector<uint8_t>& data)>;

    /// Type for association event callback
    using association_callback = std::function<void(uint64_t session_id,
                                                    pacs::network::pdu_type type,
                                                    const std::vector<uint8_t>& data)>;

    /// Type for session event callback
    using session_event_callback = std::function<void(uint64_t session_id,
                                                      const std::string& event)>;

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct adapter with default configuration
     */
    pipeline_adapter();

    /**
     * @brief Construct adapter with custom configuration
     * @param config Pipeline configuration
     */
    explicit pipeline_adapter(const pipeline_config& config);

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~pipeline_adapter();

    // Non-copyable, non-movable
    pipeline_adapter(const pipeline_adapter&) = delete;
    pipeline_adapter& operator=(const pipeline_adapter&) = delete;
    pipeline_adapter(pipeline_adapter&&) = delete;
    pipeline_adapter& operator=(pipeline_adapter&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Start the pipeline adapter
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto start() -> VoidResult;

    /**
     * @brief Stop the pipeline adapter
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto stop() -> VoidResult;

    /**
     * @brief Check if the adapter is running
     */
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // =========================================================================
    // Session Management
    // =========================================================================

    /**
     * @brief Register a new session
     * @param session_id Unique session identifier
     * @param context Session context information
     */
    void register_session(uint64_t session_id, session_context context);

    /**
     * @brief Unregister a session
     * @param session_id Session to unregister
     */
    void unregister_session(uint64_t session_id);

    /**
     * @brief Get session context
     * @param session_id Session identifier
     * @return Session context if found
     */
    [[nodiscard]] auto get_session(uint64_t session_id)
        -> std::optional<session_context>;

    /**
     * @brief Get number of active sessions
     */
    [[nodiscard]] auto get_active_session_count() const noexcept -> size_t;

    // =========================================================================
    // Data Handling
    // =========================================================================

    /**
     * @brief Handle incoming data from network
     *
     * Submits received PDU data to the pipeline for processing.
     *
     * @param session_id Session that received the data
     * @param data Raw PDU bytes
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto on_data_received(uint64_t session_id,
                                        std::vector<uint8_t> data) -> VoidResult;

    /**
     * @brief Handle connection closed event
     * @param session_id Session that was closed
     */
    void on_connection_closed(uint64_t session_id);

    // =========================================================================
    // Service Handler Registration
    // =========================================================================

    /// Service handler type
    using service_handler = storage_query_exec_job::service_handler;

    /**
     * @brief Register C-STORE handler
     * @param handler Handler function
     */
    void register_c_store_handler(service_handler handler);

    /**
     * @brief Register C-FIND handler
     * @param handler Handler function
     */
    void register_c_find_handler(service_handler handler);

    /**
     * @brief Register C-GET handler
     * @param handler Handler function
     */
    void register_c_get_handler(service_handler handler);

    /**
     * @brief Register C-MOVE handler
     * @param handler Handler function
     */
    void register_c_move_handler(service_handler handler);

    /**
     * @brief Register C-ECHO handler
     * @param handler Handler function
     */
    void register_c_echo_handler(service_handler handler);

    // =========================================================================
    // Callback Registration
    // =========================================================================

    /**
     * @brief Set the network send callback
     * @param callback Function to send data over network
     */
    void set_send_callback(send_callback callback);

    /**
     * @brief Set the association event callback
     * @param callback Function to handle association PDUs
     */
    void set_association_callback(association_callback callback);

    /**
     * @brief Set the session event callback
     * @param callback Function to handle session events
     */
    void set_session_event_callback(session_event_callback callback);

    // =========================================================================
    // Metrics & Statistics
    // =========================================================================

    /**
     * @brief Get the pipeline metrics
     */
    [[nodiscard]] auto get_metrics() noexcept -> pipeline_metrics&;

    /**
     * @brief Get the pipeline metrics (const)
     */
    [[nodiscard]] auto get_metrics() const noexcept -> const pipeline_metrics&;

    /**
     * @brief Get the underlying coordinator
     */
    [[nodiscard]] auto get_coordinator() noexcept -> pipeline_coordinator&;

    /**
     * @brief Get the underlying coordinator (const)
     */
    [[nodiscard]] auto get_coordinator() const noexcept
        -> const pipeline_coordinator&;

private:
    /// Pipeline coordinator
    std::unique_ptr<pipeline_coordinator> coordinator_;

    /// Session registry
    std::unordered_map<uint64_t, session_context> sessions_;
    mutable std::mutex sessions_mutex_;

    /// Service handlers
    service_handler c_store_handler_;
    service_handler c_find_handler_;
    service_handler c_get_handler_;
    service_handler c_move_handler_;
    service_handler c_echo_handler_;
    mutable std::mutex handlers_mutex_;

    /// Callbacks
    send_callback send_callback_;
    association_callback association_callback_;
    session_event_callback session_event_callback_;
    mutable std::mutex callbacks_mutex_;

    /// Get service handler for command type
    [[nodiscard]] auto get_handler_for_command(dimse_command_type type)
        -> service_handler;

    /// Handle job completion
    void on_job_completed(const job_context& ctx, bool success);

    /// Handle backpressure
    void on_backpressure(pipeline_stage stage, size_t queue_depth);
};

}  // namespace pacs::network::pipeline
