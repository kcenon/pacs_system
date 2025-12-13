/**
 * @file task_scheduler_config.hpp
 * @brief Configuration for task scheduler service
 *
 * This file provides configuration structures for the task_scheduler
 * which schedules and executes recurring maintenance tasks.
 *
 * @see SRS-WKF-002 - Task Scheduler Service Specification
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace pacs::workflow {

// =============================================================================
// Schedule Expression Types
// =============================================================================

/**
 * @brief Simple interval-based schedule
 *
 * Executes the task at fixed intervals.
 */
struct interval_schedule {
    /// Interval between executions
    std::chrono::seconds interval{3600};  // Default: 1 hour

    /// Optional start time for first execution
    std::optional<std::chrono::system_clock::time_point> start_at;
};

/**
 * @brief Cron-like schedule expression
 *
 * Supports cron-style scheduling with minute, hour, day, month, weekday.
 * Special values: * (any), ranges (1-5), lists (1,3,5), steps (*\/5)
 */
struct cron_schedule {
    /// Minute (0-59, or "*")
    std::string minute{"*"};

    /// Hour (0-23, or "*")
    std::string hour{"*"};

    /// Day of month (1-31, or "*")
    std::string day_of_month{"*"};

    /// Month (1-12, or "*")
    std::string month{"*"};

    /// Day of week (0-6, Sunday=0, or "*")
    std::string day_of_week{"*"};

    /**
     * @brief Create a schedule that runs every N minutes
     * @param n Minutes between runs
     */
    [[nodiscard]] static auto every_minutes(int n) -> cron_schedule {
        cron_schedule s;
        s.minute = "*/" + std::to_string(n);
        return s;
    }

    /**
     * @brief Create a schedule that runs every N hours
     * @param n Hours between runs
     */
    [[nodiscard]] static auto every_hours(int n) -> cron_schedule {
        cron_schedule s;
        s.minute = "0";
        s.hour = "*/" + std::to_string(n);
        return s;
    }

    /**
     * @brief Create a daily schedule at specific time
     * @param hour Hour of day (0-23)
     * @param minute Minute of hour (0-59)
     */
    [[nodiscard]] static auto daily_at(int hour, int minute = 0) -> cron_schedule {
        cron_schedule s;
        s.minute = std::to_string(minute);
        s.hour = std::to_string(hour);
        return s;
    }

    /**
     * @brief Create a weekly schedule
     * @param day_of_week Day of week (0=Sunday, 6=Saturday)
     * @param hour Hour of day
     * @param minute Minute of hour
     */
    [[nodiscard]] static auto weekly_on(int day_of_week, int hour = 0, int minute = 0)
        -> cron_schedule {
        cron_schedule s;
        s.minute = std::to_string(minute);
        s.hour = std::to_string(hour);
        s.day_of_week = std::to_string(day_of_week);
        return s;
    }

    /**
     * @brief Parse a cron expression string
     * @param expr Cron expression (e.g., "0 2 * * *" for daily at 2am)
     * @return Parsed cron_schedule
     */
    [[nodiscard]] static auto parse(const std::string& expr) -> cron_schedule;

    /**
     * @brief Convert to cron expression string
     * @return Cron expression string
     */
    [[nodiscard]] auto to_string() const -> std::string;

    /**
     * @brief Check if the schedule is valid
     * @return true if all fields are valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;
};

/**
 * @brief One-time execution at specific time
 */
struct one_time_schedule {
    /// Execution time
    std::chrono::system_clock::time_point execute_at;
};

/**
 * @brief Combined schedule type
 */
using schedule = std::variant<interval_schedule, cron_schedule, one_time_schedule>;

// =============================================================================
// Task Types and States
// =============================================================================

/**
 * @brief Task type enumeration
 */
enum class task_type {
    cleanup,       ///< Storage cleanup task
    archive,       ///< Study archival task
    verification,  ///< Data integrity verification
    custom         ///< User-defined task
};

/**
 * @brief Task execution state
 */
enum class task_state {
    pending,    ///< Waiting for scheduled time
    running,    ///< Currently executing
    completed,  ///< Completed successfully
    failed,     ///< Execution failed
    cancelled,  ///< Cancelled by user
    paused      ///< Temporarily paused
};

/**
 * @brief Convert task_state to string
 */
[[nodiscard]] inline auto to_string(task_state state) -> std::string {
    switch (state) {
        case task_state::pending: return "pending";
        case task_state::running: return "running";
        case task_state::completed: return "completed";
        case task_state::failed: return "failed";
        case task_state::cancelled: return "cancelled";
        case task_state::paused: return "paused";
        default: return "unknown";
    }
}

/**
 * @brief Convert task_type to string
 */
[[nodiscard]] inline auto to_string(task_type type) -> std::string {
    switch (type) {
        case task_type::cleanup: return "cleanup";
        case task_type::archive: return "archive";
        case task_type::verification: return "verification";
        case task_type::custom: return "custom";
        default: return "unknown";
    }
}

// =============================================================================
// Task Execution Records
// =============================================================================

/**
 * @brief Record of a single task execution
 */
struct task_execution_record {
    /// Execution ID (unique per execution)
    std::string execution_id;

    /// Task ID
    std::string task_id;

    /// Start time
    std::chrono::system_clock::time_point started_at;

    /// End time (if completed)
    std::optional<std::chrono::system_clock::time_point> ended_at;

    /// Final state
    task_state state{task_state::pending};

    /// Error message (if failed)
    std::optional<std::string> error_message;

    /// Execution result details (JSON)
    std::optional<std::string> result_json;

    /**
     * @brief Get execution duration
     * @return Duration if completed, nullopt otherwise
     */
    [[nodiscard]] auto duration() const
        -> std::optional<std::chrono::milliseconds> {
        if (ended_at) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                *ended_at - started_at);
        }
        return std::nullopt;
    }
};

// =============================================================================
// Scheduled Task Definition
// =============================================================================

/**
 * @brief Unique task identifier
 */
using task_id = std::string;

/**
 * @brief Task callback function type
 *
 * @return true if task completed successfully, false otherwise
 */
using task_callback = std::function<bool()>;

/**
 * @brief Task callback with result details
 *
 * @return Optional error message (nullopt = success)
 */
using task_callback_with_result = std::function<std::optional<std::string>()>;

/**
 * @brief Scheduled task definition
 */
struct scheduled_task {
    /// Unique task ID
    task_id id;

    /// Human-readable task name
    std::string name;

    /// Task description
    std::string description;

    /// Task type
    task_type type{task_type::custom};

    /// Schedule for execution
    schedule task_schedule;

    /// Current state
    task_state state{task_state::pending};

    /// Task callback
    task_callback_with_result callback;

    /// Whether task is enabled
    bool enabled{true};

    /// Task priority (higher = more important)
    int priority{0};

    /// Tags for categorization
    std::set<std::string> tags;

    /// Maximum execution time (0 = no limit)
    std::chrono::seconds timeout{0};

    /// Number of retry attempts on failure
    std::size_t max_retries{0};

    /// Delay between retries
    std::chrono::seconds retry_delay{60};

    /// Creation time
    std::chrono::system_clock::time_point created_at;

    /// Last modification time
    std::chrono::system_clock::time_point updated_at;

    /// Next scheduled execution time
    std::optional<std::chrono::system_clock::time_point> next_run_at;

    /// Last execution time
    std::optional<std::chrono::system_clock::time_point> last_run_at;

    /// Last execution result
    std::optional<task_execution_record> last_execution;

    /// Total execution count
    std::size_t execution_count{0};

    /// Successful execution count
    std::size_t success_count{0};

    /// Failed execution count
    std::size_t failure_count{0};
};

// =============================================================================
// Cleanup Task Configuration
// =============================================================================

/**
 * @brief Configuration for cleanup scheduling
 */
struct cleanup_config {
    /// Default retention period
    std::chrono::days default_retention{365};

    /// Modality-specific retention periods
    std::map<std::string, std::chrono::days> modality_retention;

    /// Study description patterns to exclude from cleanup
    std::set<std::string> exclude_patterns;

    /// Verify study is not locked before deletion
    bool verify_not_locked{true};

    /// Perform dry run (report only, no deletion)
    bool dry_run{false};

    /// Maximum studies to delete per cycle
    std::size_t max_deletions_per_cycle{100};

    /// Delete from database only (keep files)
    bool database_only{false};

    /// Schedule for cleanup task
    schedule cleanup_schedule{cron_schedule::daily_at(2, 0)};  // 2:00 AM

    /**
     * @brief Get retention period for a modality
     * @param modality DICOM modality code
     * @return Retention period (modality-specific or default)
     */
    [[nodiscard]] auto retention_for(const std::string& modality) const
        -> std::chrono::days {
        auto it = modality_retention.find(modality);
        return it != modality_retention.end() ? it->second : default_retention;
    }
};

// =============================================================================
// Archive Task Configuration
// =============================================================================

/**
 * @brief Archive destination type
 */
enum class archive_destination_type {
    local_path,    ///< Local filesystem path
    network_share, ///< Network share (SMB/NFS)
    cloud_s3,      ///< AWS S3 or compatible
    cloud_azure,   ///< Azure Blob Storage
    tape           ///< Tape library
};

/**
 * @brief Configuration for archive scheduling
 */
struct archive_config {
    /// Archive studies older than this
    std::chrono::days archive_after{90};

    /// Archive destination type
    archive_destination_type destination_type{archive_destination_type::local_path};

    /// Destination path or URL
    std::string destination;

    /// Credentials (if required)
    std::optional<std::string> credentials_key;

    /// Verify archive integrity after transfer
    bool verify_after_archive{true};

    /// Delete original after successful archive
    bool delete_after_archive{false};

    /// Compress archives
    bool compress{true};

    /// Compression level (1-9)
    int compression_level{6};

    /// Maximum studies to archive per cycle
    std::size_t max_archives_per_cycle{50};

    /// Schedule for archive task
    schedule archive_schedule{cron_schedule::daily_at(3, 0)};  // 3:00 AM
};

// =============================================================================
// Verification Task Configuration
// =============================================================================

/**
 * @brief Configuration for verification scheduling
 */
struct verification_config {
    /// Interval between verification runs
    std::chrono::hours interval{24};

    /// Verify file checksums (MD5/SHA256)
    bool check_checksums{true};

    /// Verify database-storage consistency
    bool check_db_consistency{true};

    /// Verify DICOM structure
    bool check_dicom_structure{false};

    /// Attempt repair on failure
    bool repair_on_failure{false};

    /// Maximum studies to verify per cycle
    std::size_t max_verifications_per_cycle{1000};

    /// Hash algorithm for checksum verification
    std::string hash_algorithm{"SHA256"};

    /// Schedule for verification task
    schedule verification_schedule{cron_schedule::daily_at(4, 0)};  // 4:00 AM
};

// =============================================================================
// Task Scheduler Service Configuration
// =============================================================================

/**
 * @brief Configuration for the task scheduler service
 */
struct task_scheduler_config {
    /// Enable/disable the scheduler service
    bool enabled{true};

    /// Whether to start automatically on construction
    bool auto_start{false};

    /// Maximum concurrent task executions
    std::size_t max_concurrent_tasks{4};

    /// Scheduler check interval (how often to check for due tasks)
    std::chrono::seconds check_interval{60};

    /// Path to persist scheduled tasks (empty = no persistence)
    std::string persistence_path;

    /// Restore tasks from persistence on startup
    bool restore_on_startup{true};

    /// Cleanup configuration
    std::optional<cleanup_config> cleanup;

    /// Archive configuration
    std::optional<archive_config> archive;

    /// Verification configuration
    std::optional<verification_config> verification;

    /// Callback when any task completes
    using task_complete_callback =
        std::function<void(const task_id& id,
                          const task_execution_record& record)>;
    task_complete_callback on_task_complete;

    /// Callback when any task fails
    using task_error_callback =
        std::function<void(const task_id& id,
                          const std::string& error)>;
    task_error_callback on_task_error;

    /// Callback when scheduler cycle completes
    using cycle_complete_callback =
        std::function<void(std::size_t tasks_executed,
                          std::size_t tasks_succeeded,
                          std::size_t tasks_failed)>;
    cycle_complete_callback on_cycle_complete;

    /**
     * @brief Check if configuration is valid
     * @return true if configuration is valid for operation
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return enabled && max_concurrent_tasks > 0;
    }
};

// =============================================================================
// Task Scheduler Statistics
// =============================================================================

/**
 * @brief Statistics for task scheduler operations
 */
struct scheduler_stats {
    /// Number of tasks currently scheduled
    std::size_t scheduled_tasks{0};

    /// Number of tasks currently running
    std::size_t running_tasks{0};

    /// Total tasks executed
    std::size_t total_executions{0};

    /// Successful executions
    std::size_t successful_executions{0};

    /// Failed executions
    std::size_t failed_executions{0};

    /// Cancelled executions
    std::size_t cancelled_executions{0};

    /// Average execution time
    std::chrono::milliseconds avg_execution_time{0};

    /// Maximum execution time observed
    std::chrono::milliseconds max_execution_time{0};

    /// Scheduler uptime
    std::chrono::seconds uptime{0};

    /// Last cycle time
    std::optional<std::chrono::system_clock::time_point> last_cycle_at;
};

}  // namespace pacs::workflow
