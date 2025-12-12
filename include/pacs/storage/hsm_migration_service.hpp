/**
 * @file hsm_migration_service.hpp
 * @brief Background migration service for Hierarchical Storage Management
 *
 * This file provides the hsm_migration_service class which manages automatic
 * background migration of DICOM instances between storage tiers based on
 * configurable age policies.
 *
 * @see SRS-STOR-011, FR-4.5 (HSM Migration Service)
 */

#pragma once

#include "hsm_storage.hpp"
#include "hsm_types.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace kcenon::thread {
class thread_pool;
}  // namespace kcenon::thread

namespace kcenon::logger {
class logger;
}  // namespace kcenon::logger

namespace pacs::storage {

/**
 * @brief Configuration for the migration service
 */
struct migration_service_config {
    /// Interval between migration cycles
    std::chrono::seconds migration_interval{3600};  // 1 hour default

    /// Maximum concurrent migrations
    std::size_t max_concurrent_migrations{4};

    /// Whether to start automatically on construction
    bool auto_start{false};

    /// Callback for migration progress updates
    using progress_callback =
        std::function<void(const migration_result& result)>;
    progress_callback on_cycle_complete;

    /// Callback for migration errors
    using error_callback =
        std::function<void(const std::string& uid, const std::string& error)>;
    error_callback on_migration_error;
};

/**
 * @brief Background migration service for HSM
 *
 * Runs periodic migration cycles to move data between storage tiers
 * based on the configured tier policy. Integrates with thread_system
 * for parallel migration operations.
 *
 * Thread Safety:
 * - All methods are thread-safe
 * - Uses condition variables for efficient scheduling
 * - Graceful shutdown support
 *
 * @example
 * @code
 * // Create HSM storage
 * hsm_storage storage{hot, warm, cold};
 *
 * // Create migration service
 * migration_service_config config;
 * config.migration_interval = std::chrono::hours{1};
 * config.on_cycle_complete = [](const migration_result& r) {
 *     std::cout << "Migrated " << r.instances_migrated << " instances\n";
 * };
 *
 * hsm_migration_service service{storage, config};
 * service.start();
 *
 * // Later...
 * service.stop();
 * @endcode
 */
class hsm_migration_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct migration service
     *
     * @param storage Reference to the HSM storage to manage
     * @param config Service configuration
     */
    explicit hsm_migration_service(hsm_storage& storage,
                                   const migration_service_config& config = {});

    /**
     * @brief Construct migration service with thread pool
     *
     * @param storage Reference to the HSM storage to manage
     * @param thread_pool Thread pool for parallel migrations
     * @param config Service configuration
     */
    hsm_migration_service(
        hsm_storage& storage,
        std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
        const migration_service_config& config = {});

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~hsm_migration_service();

    /// Non-copyable
    hsm_migration_service(const hsm_migration_service&) = delete;
    hsm_migration_service& operator=(const hsm_migration_service&) = delete;

    /// Non-movable
    hsm_migration_service(hsm_migration_service&&) = delete;
    hsm_migration_service& operator=(hsm_migration_service&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Start the background migration service
     *
     * Starts the background thread that runs periodic migration cycles.
     * If already started, this is a no-op.
     */
    void start();

    /**
     * @brief Stop the background migration service
     *
     * Gracefully stops the service, waiting for any in-progress migrations
     * to complete. If not started, this is a no-op.
     *
     * @param wait_for_completion If true, waits for current cycle to complete
     */
    void stop(bool wait_for_completion = true);

    /**
     * @brief Check if the service is running
     *
     * @return true if the background service is running
     */
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // =========================================================================
    // Manual Operations
    // =========================================================================

    /**
     * @brief Manually trigger a migration cycle
     *
     * Runs a migration cycle immediately, regardless of the scheduled interval.
     * Can be called whether the service is running or not.
     *
     * @return Migration result with statistics
     */
    [[nodiscard]] auto run_migration_cycle() -> migration_result;

    /**
     * @brief Trigger next cycle immediately
     *
     * Wakes up the background thread to run a migration cycle immediately,
     * without waiting for the scheduled interval. Only works if service
     * is running.
     */
    void trigger_cycle();

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    /**
     * @brief Get the result of the last migration cycle
     *
     * @return Last migration result, or nullopt if no cycle has run
     */
    [[nodiscard]] auto get_last_result() const
        -> std::optional<migration_result>;

    /**
     * @brief Get total statistics since service started
     *
     * @return Cumulative migration statistics
     */
    [[nodiscard]] auto get_cumulative_stats() const -> migration_result;

    /**
     * @brief Get the time until the next scheduled migration
     *
     * @return Duration until next cycle, or nullopt if not scheduled
     */
    [[nodiscard]] auto time_until_next_cycle() const
        -> std::optional<std::chrono::seconds>;

    /**
     * @brief Get the number of cycles completed
     *
     * @return Total number of completed migration cycles
     */
    [[nodiscard]] auto cycles_completed() const noexcept -> std::size_t;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Update the migration interval
     *
     * @param interval New interval between migration cycles
     *
     * @note Takes effect at the next cycle
     */
    void set_migration_interval(std::chrono::seconds interval);

    /**
     * @brief Get the current migration interval
     *
     * @return Current interval between migration cycles
     */
    [[nodiscard]] auto get_migration_interval() const noexcept
        -> std::chrono::seconds;

    /**
     * @brief Set the progress callback
     *
     * @param callback Function called after each migration cycle
     */
    void set_progress_callback(migration_service_config::progress_callback callback);

    /**
     * @brief Set the error callback
     *
     * @param callback Function called when a migration fails
     */
    void set_error_callback(migration_service_config::error_callback callback);

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /**
     * @brief Background thread main loop
     */
    void run_loop();

    /**
     * @brief Execute a single migration cycle
     * @return Migration result
     */
    [[nodiscard]] auto execute_cycle() -> migration_result;

    /**
     * @brief Update cumulative statistics
     * @param result Result from latest cycle
     */
    void update_stats(const migration_result& result);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Reference to managed HSM storage
    hsm_storage& storage_;

    /// Thread pool for parallel migrations (optional)
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;

    /// Service configuration
    migration_service_config config_;

    /// Background worker thread
    std::thread worker_thread_;

    /// Mutex for thread synchronization
    mutable std::mutex mutex_;

    /// Condition variable for sleep/wake
    std::condition_variable cv_;

    /// Flag to signal shutdown
    std::atomic<bool> stop_requested_{false};

    /// Flag indicating service is running
    std::atomic<bool> running_{false};

    /// Flag indicating a cycle is in progress
    std::atomic<bool> cycle_in_progress_{false};

    /// Last migration result
    std::optional<migration_result> last_result_;

    /// Cumulative statistics
    migration_result cumulative_stats_;

    /// Number of completed cycles
    std::atomic<std::size_t> cycles_count_{0};

    /// Time of next scheduled cycle
    std::chrono::steady_clock::time_point next_cycle_time_;
};

}  // namespace pacs::storage
