/**
 * @file hsm_migration_service.cpp
 * @brief Implementation of background migration service for HSM
 */

#include <pacs/storage/hsm_migration_service.hpp>

#include <algorithm>

namespace pacs::storage {

// ============================================================================
// Construction
// ============================================================================

hsm_migration_service::hsm_migration_service(
    hsm_storage& storage, const migration_service_config& config)
    : storage_(storage), config_(config) {
    if (config_.auto_start) {
        start();
    }
}

hsm_migration_service::hsm_migration_service(
    hsm_storage& storage,
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
    const migration_service_config& config)
    : storage_(storage),
      thread_pool_(std::move(thread_pool)),
      config_(config) {
    if (config_.auto_start) {
        start();
    }
}

hsm_migration_service::~hsm_migration_service() {
    stop(true);
}

// ============================================================================
// Lifecycle Management
// ============================================================================

void hsm_migration_service::start() {
    std::lock_guard lock(mutex_);

    if (running_.load()) {
        return;  // Already running
    }

    stop_requested_.store(false);
    running_.store(true);

    // Set next cycle time
    next_cycle_time_ =
        std::chrono::steady_clock::now() + config_.migration_interval;

    // Start worker thread
    worker_thread_ = std::thread([this]() { run_loop(); });
}

void hsm_migration_service::stop(bool wait_for_completion) {
    {
        std::lock_guard lock(mutex_);

        if (!running_.load()) {
            return;  // Not running
        }

        stop_requested_.store(true);
    }

    // Wake up the worker thread
    cv_.notify_all();

    // Wait for thread to finish
    if (worker_thread_.joinable()) {
        if (wait_for_completion) {
            worker_thread_.join();
        } else {
            worker_thread_.detach();
        }
    }

    running_.store(false);
}

auto hsm_migration_service::is_running() const noexcept -> bool {
    return running_.load();
}

// ============================================================================
// Manual Operations
// ============================================================================

auto hsm_migration_service::run_migration_cycle() -> migration_result {
    return execute_cycle();
}

void hsm_migration_service::trigger_cycle() {
    std::lock_guard lock(mutex_);

    if (!running_.load()) {
        return;
    }

    // Set next cycle time to now to trigger immediate execution
    next_cycle_time_ = std::chrono::steady_clock::now();

    // Wake up the worker thread
    cv_.notify_all();
}

// ============================================================================
// Statistics and Monitoring
// ============================================================================

auto hsm_migration_service::get_last_result() const
    -> std::optional<migration_result> {
    std::lock_guard lock(mutex_);
    return last_result_;
}

auto hsm_migration_service::get_cumulative_stats() const -> migration_result {
    std::lock_guard lock(mutex_);
    return cumulative_stats_;
}

auto hsm_migration_service::time_until_next_cycle() const
    -> std::optional<std::chrono::seconds> {
    std::lock_guard lock(mutex_);

    if (!running_.load()) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    if (next_cycle_time_ <= now) {
        return std::chrono::seconds{0};
    }

    return std::chrono::duration_cast<std::chrono::seconds>(next_cycle_time_ -
                                                            now);
}

auto hsm_migration_service::cycles_completed() const noexcept -> std::size_t {
    return cycles_count_.load();
}

// ============================================================================
// Configuration
// ============================================================================

void hsm_migration_service::set_migration_interval(
    std::chrono::seconds interval) {
    std::lock_guard lock(mutex_);
    config_.migration_interval = interval;
}

auto hsm_migration_service::get_migration_interval() const noexcept
    -> std::chrono::seconds {
    return config_.migration_interval;
}

void hsm_migration_service::set_progress_callback(
    migration_service_config::progress_callback callback) {
    std::lock_guard lock(mutex_);
    config_.on_cycle_complete = std::move(callback);
}

void hsm_migration_service::set_error_callback(
    migration_service_config::error_callback callback) {
    std::lock_guard lock(mutex_);
    config_.on_migration_error = std::move(callback);
}

// ============================================================================
// Internal Methods
// ============================================================================

void hsm_migration_service::run_loop() {
    while (!stop_requested_.load()) {
        std::unique_lock lock(mutex_);

        // Wait until next cycle time or stop requested
        cv_.wait_until(lock, next_cycle_time_, [this]() {
            return stop_requested_.load() ||
                   std::chrono::steady_clock::now() >= next_cycle_time_;
        });

        if (stop_requested_.load()) {
            break;
        }

        // Release lock during migration
        lock.unlock();

        // Execute migration cycle
        auto result = execute_cycle();

        // Update statistics
        update_stats(result);

        // Store result
        lock.lock();
        last_result_ = result;
        cycles_count_++;

        // Schedule next cycle
        next_cycle_time_ =
            std::chrono::steady_clock::now() + config_.migration_interval;

        // Call progress callback (outside of lock)
        auto callback = config_.on_cycle_complete;
        lock.unlock();

        if (callback) {
            callback(result);
        }
    }
}

auto hsm_migration_service::execute_cycle() -> migration_result {
    cycle_in_progress_.store(true);

    auto result = storage_.run_migration_cycle();

    // Call error callbacks for failures
    if (!result.failed_uids.empty() && config_.on_migration_error) {
        for (const auto& uid : result.failed_uids) {
            config_.on_migration_error(uid, "Migration failed");
        }
    }

    cycle_in_progress_.store(false);

    return result;
}

void hsm_migration_service::update_stats(const migration_result& result) {
    std::lock_guard lock(mutex_);

    cumulative_stats_.instances_migrated += result.instances_migrated;
    cumulative_stats_.bytes_migrated += result.bytes_migrated;
    cumulative_stats_.duration += result.duration;
    cumulative_stats_.instances_skipped += result.instances_skipped;

    // Append failed UIDs (keep last N)
    constexpr std::size_t kMaxFailedUids = 100;
    for (const auto& uid : result.failed_uids) {
        if (cumulative_stats_.failed_uids.size() >= kMaxFailedUids) {
            cumulative_stats_.failed_uids.erase(
                cumulative_stats_.failed_uids.begin());
        }
        cumulative_stats_.failed_uids.push_back(uid);
    }
}

}  // namespace pacs::storage
