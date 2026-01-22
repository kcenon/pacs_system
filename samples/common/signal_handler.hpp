/**
 * @file signal_handler.hpp
 * @brief Cross-platform signal handling for graceful shutdown
 *
 * This file provides signal handling utilities for developer samples,
 * enabling graceful shutdown on SIGINT/SIGTERM across different platforms.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <functional>
#include <mutex>

namespace pacs::samples {

/**
 * @brief Cross-platform signal handler for graceful shutdown
 *
 * Provides static methods for installing signal handlers and checking
 * shutdown state. Supports SIGINT (Ctrl+C) and SIGTERM on POSIX systems,
 * and SIGINT/SIGBREAK on Windows.
 *
 * Thread Safety: All methods are thread-safe.
 *
 * @example
 * @code
 * signal_handler::install([]() {
 *     std::cout << "Shutting down...\n";
 * });
 *
 * while (!signal_handler::should_shutdown()) {
 *     // Main loop
 * }
 *
 * // Or use wait_for_shutdown for blocking
 * signal_handler::wait_for_shutdown();
 * @endcode
 */
class signal_handler {
public:
    /// Callback type for shutdown notification
    using shutdown_callback = std::function<void()>;

    // ========================================================================
    // Static Interface
    // ========================================================================

    /**
     * @brief Install signal handlers with optional callback
     * @param callback Function to call when shutdown signal is received
     *
     * Installs handlers for SIGINT and SIGTERM (POSIX) or SIGINT and
     * SIGBREAK (Windows). The callback is invoked once when the first
     * signal is received.
     */
    static void install(shutdown_callback callback = nullptr);

    /**
     * @brief Check if shutdown has been requested
     * @return true if a shutdown signal has been received
     */
    [[nodiscard]] static auto should_shutdown() noexcept -> bool;

    /**
     * @brief Block until a shutdown signal is received
     *
     * This method blocks the calling thread until SIGINT/SIGTERM
     * is received. Useful for main threads that need to wait for
     * graceful shutdown.
     */
    static void wait_for_shutdown();

    /**
     * @brief Request shutdown programmatically
     *
     * Sets the shutdown flag and notifies waiting threads.
     * Useful for triggering shutdown from application code.
     */
    static void request_shutdown();

    /**
     * @brief Reset the shutdown state
     *
     * Clears the shutdown flag. Useful for tests or when restarting
     * a service within the same process.
     */
    static void reset();

private:
    /// Signal handler function for C signal API
    static void handler(int signal);

    /// Shutdown flag
    static std::atomic<bool> shutdown_requested_;

    /// Callback function
    static shutdown_callback callback_;

    /// Mutex for condition variable
    static std::mutex mutex_;

    /// Condition variable for wait_for_shutdown
    static std::condition_variable cv_;

    /// Flag to prevent multiple callback invocations
    static std::atomic<bool> callback_invoked_;
};

// ============================================================================
// RAII Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for automatic signal handler setup and cleanup
 *
 * Installs signal handlers on construction and provides convenient
 * methods for checking and waiting for shutdown.
 *
 * @example
 * @code
 * int main() {
 *     scoped_signal_handler signals([]() {
 *         std::cout << "Received shutdown signal\n";
 *     });
 *
 *     // Start services...
 *
 *     signals.wait();  // Block until shutdown
 *
 *     // Cleanup...
 *     return 0;
 * }
 * @endcode
 */
class scoped_signal_handler {
public:
    /**
     * @brief Construct and install signal handlers
     * @param callback Optional callback for shutdown notification
     */
    explicit scoped_signal_handler(signal_handler::shutdown_callback callback = nullptr);

    /**
     * @brief Destructor - resets signal handler state
     */
    ~scoped_signal_handler();

    // Non-copyable, non-movable
    scoped_signal_handler(const scoped_signal_handler&) = delete;
    auto operator=(const scoped_signal_handler&) -> scoped_signal_handler& = delete;
    scoped_signal_handler(scoped_signal_handler&&) = delete;
    auto operator=(scoped_signal_handler&&) -> scoped_signal_handler& = delete;

    /**
     * @brief Block until shutdown signal is received
     */
    void wait();

    /**
     * @brief Check if shutdown has been requested
     * @return true if shutdown signal received
     */
    [[nodiscard]] auto should_shutdown() const noexcept -> bool;
};

}  // namespace pacs::samples
