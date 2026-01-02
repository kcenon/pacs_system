/**
 * @file accept_worker.hpp
 * @brief Worker thread for accepting incoming DICOM connections
 *
 * This file provides the accept_worker class which inherits from thread_base
 * to manage the TCP accept loop for the DICOM server. It replaces direct
 * std::thread usage with thread_system's lifecycle management.
 *
 * @see DES-NET-005 - DICOM Server Class Design Specification
 * @see Issue #156 - Implement accept_worker class inheriting from thread_base
 */

#ifndef PACS_NETWORK_DETAIL_ACCEPT_WORKER_HPP
#define PACS_NETWORK_DETAIL_ACCEPT_WORKER_HPP

#include <kcenon/thread/core/thread_base.h>
#include <kcenon/common/patterns/result.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace pacs::network::detail {

namespace common = kcenon::common;

/**
 * @brief Worker thread for accepting incoming DICOM connections
 *
 * The accept_worker class provides a thread_base-based implementation for
 * the DICOM server's accept loop. It manages:
 * - TCP socket listening (placeholder for future network_system integration)
 * - Graceful shutdown via stop token
 * - Integration with thread_system's lifecycle management
 *
 * This class replaces direct std::thread usage in dicom_server, providing:
 * - jthread support when USE_STD_JTHREAD is defined
 * - Cancellation token integration
 * - Thread monitoring compatibility
 * - Consistent architecture with thread_system
 *
 * @note Current implementation is a placeholder that signals readiness for
 *       network_system TCP integration. The actual socket accept logic will
 *       be implemented when network_system provides TCP server support.
 *
 * @example Usage in dicom_server
 * @code
 * accept_worker_ = std::make_unique<detail::accept_worker>(
 *     config_.port,
 *     [this](uint64_t session_id) {
 *         // Handle new connection
 *     },
 *     [this]() {
 *         check_idle_timeouts();
 *     }
 * );
 * accept_worker_->set_wake_interval(std::chrono::milliseconds(100));
 * auto result = accept_worker_->start();
 * @endcode
 */
class accept_worker : public kcenon::thread::thread_base {
public:
    // =========================================================================
    // Type Aliases
    // =========================================================================

    using result_void = common::VoidResult;

    /// Callback type for new connection events
    /// @param session_id Unique identifier for the new session
    using connection_callback = std::function<void(uint64_t session_id)>;

    /// Callback type for periodic maintenance tasks
    using maintenance_callback = std::function<void()>;

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Constructs an accept_worker with the specified configuration
     *
     * @param port The TCP port to listen on for incoming connections
     * @param on_connection Callback invoked when a new connection is accepted
     * @param on_maintenance Optional callback for periodic maintenance tasks
     *                       (e.g., idle timeout checks)
     *
     * @note The on_connection callback is invoked from the worker thread.
     *       Implementations must be thread-safe.
     */
    explicit accept_worker(
        uint16_t port,
        connection_callback on_connection,
        maintenance_callback on_maintenance = nullptr);

    /**
     * @brief Destructor
     *
     * Ensures graceful shutdown by calling stop() if still running.
     */
    ~accept_worker() override;

    // Non-copyable, non-movable (owns thread resources)
    accept_worker(const accept_worker&) = delete;
    accept_worker& operator=(const accept_worker&) = delete;
    accept_worker(accept_worker&&) = delete;
    accept_worker& operator=(accept_worker&&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Sets the maximum number of pending connections in the listen queue
     *
     * @param backlog Maximum pending connections (default: 128)
     * @note Must be called before start()
     */
    void set_max_pending_connections(int backlog);

    /**
     * @brief Gets the configured port number
     * @return The TCP port this worker listens on
     */
    [[nodiscard]] uint16_t port() const noexcept;

    /**
     * @brief Gets the current backlog setting
     * @return Maximum pending connections in listen queue
     */
    [[nodiscard]] int max_pending_connections() const noexcept;

    // =========================================================================
    // Status
    // =========================================================================

    /**
     * @brief Checks if the worker is actively accepting connections
     * @return true if accepting connections, false otherwise
     */
    [[nodiscard]] bool is_accepting() const noexcept;

    /**
     * @brief Gets a string representation of the worker state
     * @return Human-readable status string
     */
    [[nodiscard]] std::string to_string() const override;

protected:
    // =========================================================================
    // thread_base Overrides
    // =========================================================================

    /**
     * @brief Initializes resources before the worker thread starts
     *
     * Currently a placeholder. Will initialize TCP acceptor when
     * network_system integration is complete.
     *
     * @return Success or error with description
     */
    result_void before_start() override;

    /**
     * @brief Main work routine - checks for incoming connections
     *
     * This method is called repeatedly by the thread_base framework.
     * Currently implements placeholder logic that:
     * - Waits for wake interval or shutdown signal
     * - Invokes maintenance callback if set
     *
     * Future implementation will:
     * - Poll TCP acceptor for incoming connections
     * - Accept connections and invoke connection callback
     * - Handle connection errors gracefully
     *
     * @return Success or error with description
     */
    result_void do_work() override;

    /**
     * @brief Cleans up resources after the worker thread stops
     *
     * Currently a placeholder. Will close TCP acceptor when
     * network_system integration is complete.
     *
     * @return Success or error with description
     */
    result_void after_stop() override;

    /**
     * @brief Determines whether there is pending work that must complete
     *
     * Returns false because the accept worker has no pending work items
     * that must complete before shutdown. This allows graceful shutdown
     * when stop() is called.
     *
     * @return false (no pending work requiring completion)
     */
    [[nodiscard]] bool should_continue_work() const override;

    /**
     * @brief Called when stop() is requested
     *
     * This hook is called from the thread calling stop() before the worker
     * thread actually stops. Currently a no-op as the base class handles
     * waking up the worker thread.
     *
     * Future implementation will:
     * - Cancel any pending async accept operations
     * - Close the acceptor to unblock accept calls
     */
    void on_stop_requested() override;

private:
    // =========================================================================
    // Private Methods
    // =========================================================================

    /**
     * @brief Generates a unique session ID for new connections
     * @return Unique session identifier
     */
    [[nodiscard]] uint64_t next_session_id();

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// TCP port to listen on
    uint16_t port_;

    /// Callback for new connections
    connection_callback on_connection_;

    /// Optional callback for maintenance tasks
    maintenance_callback on_maintenance_;

    /// Maximum pending connections in listen queue
    int backlog_{128};

    /// Session ID counter for unique connection identification
    std::atomic<uint64_t> session_id_counter_{0};

    /// Flag indicating if actively accepting connections
    std::atomic<bool> accepting_{false};

#ifdef _WIN32
    /// Listen socket handle (Windows)
    SOCKET listen_socket_{INVALID_SOCKET};

    /// WSA initialized flag
    bool wsa_initialized_{false};
#else
    /// Listen socket file descriptor (POSIX)
    int listen_socket_{-1};
#endif
};

}  // namespace pacs::network::detail

#endif  // PACS_NETWORK_DETAIL_ACCEPT_WORKER_HPP
