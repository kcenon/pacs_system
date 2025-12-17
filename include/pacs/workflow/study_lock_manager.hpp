/**
 * @file study_lock_manager.hpp
 * @brief Study lock manager for modification control and concurrent access
 *
 * This file provides the study_lock_manager class which manages locks on DICOM
 * studies to prevent concurrent modifications and ensure data integrity during
 * operations like migrations and updates.
 *
 * @see SRS-WKF-003 - Study Lock Manager Specification
 * @see FR-4.8 - Study Modification Control
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

// Forward declarations for kcenon ecosystem
namespace kcenon::common {
template <typename T>
class Result;
struct error_info;
}  // namespace kcenon::common

namespace pacs::workflow {

// =============================================================================
// Lock Types and Configuration
// =============================================================================

/**
 * @brief Type of lock to acquire on a study
 */
enum class lock_type {
    exclusive,  ///< No other access allowed (for modifications)
    shared,     ///< Read-only access allowed (for read operations)
    migration   ///< Special lock for migration operations (highest priority)
};

/**
 * @brief Convert lock_type to string
 */
[[nodiscard]] inline auto to_string(lock_type type) -> std::string {
    switch (type) {
        case lock_type::exclusive: return "exclusive";
        case lock_type::shared: return "shared";
        case lock_type::migration: return "migration";
        default: return "unknown";
    }
}

/**
 * @brief Parse lock_type from string
 */
[[nodiscard]] inline auto parse_lock_type(const std::string& str)
    -> std::optional<lock_type> {
    if (str == "exclusive") return lock_type::exclusive;
    if (str == "shared") return lock_type::shared;
    if (str == "migration") return lock_type::migration;
    return std::nullopt;
}

// =============================================================================
// Lock Token and Information
// =============================================================================

/**
 * @brief Unique identifier for a lock
 */
struct lock_token {
    /// Unique token ID
    std::string token_id;

    /// Study UID that is locked
    std::string study_uid;

    /// Type of lock held
    lock_type type{lock_type::exclusive};

    /// When the lock was acquired
    std::chrono::system_clock::time_point acquired_at;

    /// When the lock expires (if timeout set)
    std::optional<std::chrono::system_clock::time_point> expires_at;

    /**
     * @brief Check if the token is valid (not expired)
     */
    [[nodiscard]] auto is_valid() const -> bool {
        if (!expires_at) return true;
        return std::chrono::system_clock::now() < *expires_at;
    }

    /**
     * @brief Check if the token has expired
     */
    [[nodiscard]] auto is_expired() const -> bool {
        return !is_valid();
    }

    /**
     * @brief Get remaining time until expiration
     */
    [[nodiscard]] auto remaining_time() const
        -> std::optional<std::chrono::milliseconds> {
        if (!expires_at) return std::nullopt;
        auto now = std::chrono::system_clock::now();
        if (now >= *expires_at) return std::chrono::milliseconds{0};
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            *expires_at - now);
    }
};

/**
 * @brief Detailed information about a lock on a study
 */
struct lock_info {
    /// Study UID that is locked
    std::string study_uid;

    /// Type of lock held
    lock_type type{lock_type::exclusive};

    /// Reason for the lock
    std::string reason;

    /// Who holds the lock (user/service identifier)
    std::string holder;

    /// Lock token ID
    std::string token_id;

    /// When the lock was acquired
    std::chrono::system_clock::time_point acquired_at;

    /// When the lock expires (if timeout set)
    std::optional<std::chrono::system_clock::time_point> expires_at;

    /// Number of shared lock holders (for shared locks)
    std::size_t shared_count{0};

    /**
     * @brief Get lock duration
     */
    [[nodiscard]] auto duration() const -> std::chrono::milliseconds {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - acquired_at);
    }

    /**
     * @brief Check if the lock has expired
     */
    [[nodiscard]] auto is_expired() const -> bool {
        if (!expires_at) return false;
        return std::chrono::system_clock::now() >= *expires_at;
    }
};

// =============================================================================
// Lock Manager Configuration
// =============================================================================

/**
 * @brief Configuration for the study lock manager
 */
struct study_lock_manager_config {
    /// Default lock timeout (0 = no timeout)
    std::chrono::seconds default_timeout{0};

    /// Maximum time to wait when trying to acquire a lock
    std::chrono::milliseconds acquire_wait_timeout{5000};

    /// How often to check for expired locks
    std::chrono::seconds cleanup_interval{60};

    /// Enable automatic cleanup of expired locks
    bool auto_cleanup{true};

    /// Maximum number of concurrent shared locks
    std::size_t max_shared_locks{100};

    /// Allow force unlock for admin operations
    bool allow_force_unlock{true};
};

// =============================================================================
// Lock Statistics
// =============================================================================

/**
 * @brief Statistics for lock manager operations
 */
struct lock_manager_stats {
    /// Number of currently held locks
    std::size_t active_locks{0};

    /// Number of exclusive locks
    std::size_t exclusive_locks{0};

    /// Number of shared locks
    std::size_t shared_locks{0};

    /// Number of migration locks
    std::size_t migration_locks{0};

    /// Total locks acquired
    std::size_t total_acquisitions{0};

    /// Total locks released
    std::size_t total_releases{0};

    /// Lock acquisitions that timed out
    std::size_t timeout_count{0};

    /// Locks that were forcibly released
    std::size_t force_unlock_count{0};

    /// Average lock duration
    std::chrono::milliseconds avg_lock_duration{0};

    /// Maximum lock duration observed
    std::chrono::milliseconds max_lock_duration{0};

    /// Number of lock contention events
    std::size_t contention_count{0};
};

// =============================================================================
// Error Codes
// =============================================================================

/**
 * @brief Error codes for lock operations
 */
namespace lock_error {
    /// Lock already held by another holder
    constexpr int already_locked = -100;

    /// Lock not found
    constexpr int not_found = -101;

    /// Invalid token
    constexpr int invalid_token = -102;

    /// Lock timeout exceeded
    constexpr int timeout = -103;

    /// Lock has expired
    constexpr int expired = -104;

    /// Permission denied (force unlock not allowed)
    constexpr int permission_denied = -105;

    /// Invalid lock type
    constexpr int invalid_type = -106;

    /// Maximum shared locks exceeded
    constexpr int max_shared_exceeded = -107;

    /// Cannot upgrade lock (shared to exclusive)
    constexpr int upgrade_failed = -108;
}  // namespace lock_error

// =============================================================================
// Study Lock Manager Class
// =============================================================================

/**
 * @class study_lock_manager
 * @brief Manages locks on DICOM studies for concurrent access control
 *
 * The study_lock_manager provides thread-safe locking mechanisms for DICOM
 * studies to prevent concurrent modifications and ensure data integrity.
 *
 * ## Key Features
 *
 * - **Exclusive Locks**: Prevent all other access to a study
 * - **Shared Locks**: Allow concurrent read access
 * - **Migration Locks**: High-priority locks for migration operations
 * - **Automatic Timeout**: Locks can expire after a configured duration
 * - **Force Unlock**: Admin capability to forcibly release locks
 *
 * ## Integration with kcenon Ecosystem
 *
 * - **thread_system**: Thread-safe operations via shared_mutex
 * - **logger_system**: Audit trails for lock operations
 * - **monitoring_system**: Lock contention and duration metrics
 * - **common_system**: Result<T> for error handling
 *
 * ## Lock Priority
 *
 * When multiple lock requests compete:
 * 1. Migration locks have highest priority
 * 2. Exclusive locks block new shared locks
 * 3. Shared locks can coexist with other shared locks
 *
 * @example
 * @code
 * study_lock_manager_config config;
 * config.default_timeout = std::chrono::minutes{30};
 *
 * study_lock_manager manager{config};
 *
 * // Acquire exclusive lock
 * auto lock_result = manager.lock("1.2.3.4.5", "User modification");
 * if (lock_result.is_ok()) {
 *     auto token = lock_result.value();
 *
 *     // Perform modifications...
 *
 *     // Release lock
 *     manager.unlock(token);
 * }
 *
 * // Check if locked
 * if (manager.is_locked("1.2.3.4.5")) {
 *     auto info = manager.get_lock_info("1.2.3.4.5");
 *     // Handle locked study...
 * }
 * @endcode
 */
class study_lock_manager {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct lock manager with default configuration
     */
    study_lock_manager();

    /**
     * @brief Construct lock manager with custom configuration
     *
     * @param config Lock manager configuration
     */
    explicit study_lock_manager(const study_lock_manager_config& config);

    /**
     * @brief Destructor - releases all locks
     */
    ~study_lock_manager();

    /// Non-copyable
    study_lock_manager(const study_lock_manager&) = delete;
    study_lock_manager& operator=(const study_lock_manager&) = delete;

    /// Movable
    study_lock_manager(study_lock_manager&&) noexcept;
    study_lock_manager& operator=(study_lock_manager&&) noexcept;

    // =========================================================================
    // Lock Acquisition
    // =========================================================================

    /**
     * @brief Acquire an exclusive lock on a study
     *
     * @param study_uid Study UID to lock
     * @param reason Reason for acquiring the lock
     * @param holder Identifier of the lock holder (default: current thread)
     * @param timeout Optional timeout for the lock (0 = use default)
     * @return Result containing lock_token or error
     */
    [[nodiscard]] auto lock(
        const std::string& study_uid,
        const std::string& reason,
        const std::string& holder = "",
        std::chrono::seconds timeout = std::chrono::seconds{0})
        -> kcenon::common::Result<lock_token>;

    /**
     * @brief Acquire a lock of specific type on a study
     *
     * @param study_uid Study UID to lock
     * @param type Type of lock to acquire
     * @param reason Reason for acquiring the lock
     * @param holder Identifier of the lock holder
     * @param timeout Optional timeout for the lock
     * @return Result containing lock_token or error
     */
    [[nodiscard]] auto lock(
        const std::string& study_uid,
        lock_type type,
        const std::string& reason,
        const std::string& holder = "",
        std::chrono::seconds timeout = std::chrono::seconds{0})
        -> kcenon::common::Result<lock_token>;

    /**
     * @brief Try to acquire a lock without blocking
     *
     * @param study_uid Study UID to lock
     * @param type Type of lock to acquire
     * @param reason Reason for acquiring the lock
     * @param holder Identifier of the lock holder
     * @param timeout Optional timeout for the lock
     * @return Result containing lock_token or error if lock unavailable
     */
    [[nodiscard]] auto try_lock(
        const std::string& study_uid,
        lock_type type,
        const std::string& reason,
        const std::string& holder = "",
        std::chrono::seconds timeout = std::chrono::seconds{0})
        -> kcenon::common::Result<lock_token>;

    // =========================================================================
    // Lock Release
    // =========================================================================

    /**
     * @brief Release a lock using its token
     *
     * @param token Lock token received from lock()
     * @return Result indicating success or error
     */
    [[nodiscard]] auto unlock(const lock_token& token)
        -> kcenon::common::Result<std::monostate>;

    /**
     * @brief Release a lock by study UID and holder
     *
     * @param study_uid Study UID to unlock
     * @param holder Identifier of the lock holder
     * @return Result indicating success or error
     */
    [[nodiscard]] auto unlock(
        const std::string& study_uid,
        const std::string& holder)
        -> kcenon::common::Result<std::monostate>;

    /**
     * @brief Force release a lock (admin operation)
     *
     * @param study_uid Study UID to unlock
     * @param admin_reason Reason for force unlock
     * @return Result indicating success or error
     */
    [[nodiscard]] auto force_unlock(
        const std::string& study_uid,
        const std::string& admin_reason = "")
        -> kcenon::common::Result<std::monostate>;

    /**
     * @brief Release all locks held by a specific holder
     *
     * @param holder Identifier of the lock holder
     * @return Number of locks released
     */
    auto unlock_all_by_holder(const std::string& holder) -> std::size_t;

    // =========================================================================
    // Lock Status
    // =========================================================================

    /**
     * @brief Check if a study is locked
     *
     * @param study_uid Study UID to check
     * @return true if the study is locked
     */
    [[nodiscard]] auto is_locked(const std::string& study_uid) const -> bool;

    /**
     * @brief Check if a study has a specific lock type
     *
     * @param study_uid Study UID to check
     * @param type Lock type to check for
     * @return true if the study has the specified lock type
     */
    [[nodiscard]] auto is_locked(
        const std::string& study_uid,
        lock_type type) const -> bool;

    /**
     * @brief Get lock information for a study
     *
     * @param study_uid Study UID to query
     * @return Lock information if locked, nullopt otherwise
     */
    [[nodiscard]] auto get_lock_info(const std::string& study_uid) const
        -> std::optional<lock_info>;

    /**
     * @brief Get lock information by token ID
     *
     * @param token_id Token ID to query
     * @return Lock information if found, nullopt otherwise
     */
    [[nodiscard]] auto get_lock_info_by_token(const std::string& token_id) const
        -> std::optional<lock_info>;

    /**
     * @brief Validate a lock token
     *
     * @param token Lock token to validate
     * @return true if the token is valid and the lock is still held
     */
    [[nodiscard]] auto validate_token(const lock_token& token) const -> bool;

    /**
     * @brief Refresh a lock (extend its timeout)
     *
     * @param token Lock token to refresh
     * @param extension Additional time to extend the lock
     * @return Result containing updated lock_token or error
     */
    [[nodiscard]] auto refresh_lock(
        const lock_token& token,
        std::chrono::seconds extension = std::chrono::seconds{0})
        -> kcenon::common::Result<lock_token>;

    // =========================================================================
    // Lock Queries
    // =========================================================================

    /**
     * @brief Get all currently held locks
     *
     * @return Vector of lock information for all held locks
     */
    [[nodiscard]] auto get_all_locks() const -> std::vector<lock_info>;

    /**
     * @brief Get all locks held by a specific holder
     *
     * @param holder Identifier of the lock holder
     * @return Vector of lock information for the holder's locks
     */
    [[nodiscard]] auto get_locks_by_holder(const std::string& holder) const
        -> std::vector<lock_info>;

    /**
     * @brief Get all locks of a specific type
     *
     * @param type Lock type to filter by
     * @return Vector of lock information for matching locks
     */
    [[nodiscard]] auto get_locks_by_type(lock_type type) const
        -> std::vector<lock_info>;

    /**
     * @brief Get all expired locks
     *
     * @return Vector of lock information for expired locks
     */
    [[nodiscard]] auto get_expired_locks() const -> std::vector<lock_info>;

    // =========================================================================
    // Maintenance
    // =========================================================================

    /**
     * @brief Clean up expired locks
     *
     * @return Number of expired locks cleaned up
     */
    auto cleanup_expired_locks() -> std::size_t;

    /**
     * @brief Get lock manager statistics
     *
     * @return Current statistics
     */
    [[nodiscard]] auto get_stats() const -> lock_manager_stats;

    /**
     * @brief Reset statistics counters
     */
    void reset_stats();

    /**
     * @brief Get the current configuration
     *
     * @return Current configuration
     */
    [[nodiscard]] auto get_config() const -> const study_lock_manager_config&;

    /**
     * @brief Update configuration
     *
     * Note: Changes to some settings may not take effect immediately.
     *
     * @param config New configuration
     */
    void set_config(const study_lock_manager_config& config);

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    /// Callback type for lock events
    using lock_event_callback = std::function<void(
        const std::string& study_uid,
        const lock_info& info)>;

    /**
     * @brief Set callback for lock acquisition events
     *
     * @param callback Callback function
     */
    void set_on_lock_acquired(lock_event_callback callback);

    /**
     * @brief Set callback for lock release events
     *
     * @param callback Callback function
     */
    void set_on_lock_released(lock_event_callback callback);

    /**
     * @brief Set callback for lock expiration events
     *
     * @param callback Callback function
     */
    void set_on_lock_expired(lock_event_callback callback);

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct lock_entry {
        lock_info info;
        std::vector<std::string> shared_holders;  // For shared locks
    };

    // =========================================================================
    // Internal Methods
    // =========================================================================

    /**
     * @brief Generate a unique token ID
     */
    [[nodiscard]] auto generate_token_id() const -> std::string;

    /**
     * @brief Get holder identifier (uses thread ID if empty)
     */
    [[nodiscard]] auto resolve_holder(const std::string& holder) const
        -> std::string;

    /**
     * @brief Calculate expiration time
     */
    [[nodiscard]] auto calculate_expiry(std::chrono::seconds timeout) const
        -> std::optional<std::chrono::system_clock::time_point>;

    /**
     * @brief Check if a lock can be acquired
     */
    [[nodiscard]] auto can_acquire_lock(
        const std::string& study_uid,
        lock_type type) const -> bool;

    /**
     * @brief Update statistics on lock acquisition
     */
    void record_acquisition(lock_type type);

    /**
     * @brief Update statistics on lock release
     */
    void record_release(lock_type type, std::chrono::milliseconds duration);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Configuration
    study_lock_manager_config config_;

    /// Lock entries (study_uid -> lock_entry)
    std::map<std::string, lock_entry> locks_;

    /// Token to study UID mapping for fast lookup
    std::map<std::string, std::string> token_to_study_;

    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;

    /// Statistics
    mutable lock_manager_stats stats_;

    /// Mutex for statistics
    mutable std::mutex stats_mutex_;

    /// Next token ID counter
    mutable std::atomic<uint64_t> next_token_id_{1};

    /// Event callbacks
    lock_event_callback on_lock_acquired_;
    lock_event_callback on_lock_released_;
    lock_event_callback on_lock_expired_;
};

}  // namespace pacs::workflow
