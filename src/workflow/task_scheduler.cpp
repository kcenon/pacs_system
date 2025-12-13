/**
 * @file task_scheduler.cpp
 * @brief Implementation of task scheduler service for automated PACS operations
 */

#include "pacs/workflow/task_scheduler.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/storage/file_storage.hpp"
#include "pacs/integration/logger_adapter.hpp"
#include "pacs/integration/monitoring_adapter.hpp"
#include "pacs/integration/thread_adapter.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <ranges>
#include <sstream>

namespace pacs::workflow {

// =============================================================================
// Cron Schedule Implementation
// =============================================================================

auto cron_schedule::parse(const std::string& expr) -> cron_schedule {
    cron_schedule result;
    std::istringstream iss(expr);
    std::vector<std::string> parts;
    std::string part;

    while (iss >> part) {
        parts.push_back(part);
    }

    if (parts.size() >= 1) result.minute = parts[0];
    if (parts.size() >= 2) result.hour = parts[1];
    if (parts.size() >= 3) result.day_of_month = parts[2];
    if (parts.size() >= 4) result.month = parts[3];
    if (parts.size() >= 5) result.day_of_week = parts[4];

    return result;
}

auto cron_schedule::to_string() const -> std::string {
    return minute + " " + hour + " " + day_of_month + " " + month + " " +
           day_of_week;
}

auto cron_schedule::is_valid() const noexcept -> bool {
    // Basic validation - check that fields are not empty
    return !minute.empty() && !hour.empty() && !day_of_month.empty() &&
           !month.empty() && !day_of_week.empty();
}

// =============================================================================
// Construction
// =============================================================================

task_scheduler::task_scheduler(
    storage::index_database& database,
    const task_scheduler_config& config)
    : database_(database)
    , config_(config) {
    // Schedule built-in tasks from config
    if (config_.cleanup) {
        schedule_cleanup(*config_.cleanup);
    }
    if (config_.archive) {
        schedule_archive(*config_.archive);
    }
    if (config_.verification) {
        schedule_verification(*config_.verification);
    }

    if (config_.auto_start && config_.enabled) {
        start();
    }
}

task_scheduler::task_scheduler(
    storage::index_database& database,
    storage::file_storage& file_storage,
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
    const task_scheduler_config& config)
    : database_(database)
    , file_storage_(&file_storage)
    , thread_pool_(std::move(thread_pool))
    , config_(config) {

    // Load persisted tasks if configured
    if (config_.restore_on_startup && !config_.persistence_path.empty()) {
        load_tasks();
    }

    // Schedule built-in tasks from config
    if (config_.cleanup) {
        schedule_cleanup(*config_.cleanup);
    }
    if (config_.archive) {
        schedule_archive(*config_.archive);
    }
    if (config_.verification) {
        schedule_verification(*config_.verification);
    }

    if (config_.auto_start && config_.enabled) {
        start();
    }
}

task_scheduler::~task_scheduler() {
    stop(true);

    // Persist tasks before destruction
    if (!config_.persistence_path.empty()) {
        save_tasks();
    }
}

// =============================================================================
// Lifecycle Management
// =============================================================================

void task_scheduler::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }

    stop_requested_.store(false);
    start_time_ = std::chrono::steady_clock::now();

    scheduler_thread_ = std::thread([this]() {
        run_loop();
    });

    integration::logger_adapter::info(
        "Task scheduler started check_interval_sec={} max_concurrent={}",
        config_.check_interval.count(),
        config_.max_concurrent_tasks);
}

void task_scheduler::stop(bool wait_for_completion) {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

    stop_requested_.store(true);

    // Wake up the scheduler thread
    cv_.notify_all();

    if (wait_for_completion && scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    } else if (scheduler_thread_.joinable()) {
        scheduler_thread_.detach();
    }

    integration::logger_adapter::info("Task scheduler stopped");
}

auto task_scheduler::is_running() const noexcept -> bool {
    return running_.load();
}

// =============================================================================
// Task Scheduling - Cleanup
// =============================================================================

auto task_scheduler::schedule_cleanup(const cleanup_config& config) -> task_id {
    scheduled_task task;
    task.id = "cleanup_task";
    task.name = "Storage Cleanup";
    task.description = "Removes old studies based on retention policy";
    task.type = task_type::cleanup;
    task.task_schedule = config.cleanup_schedule;
    task.enabled = true;
    task.priority = 10;
    task.tags = {"maintenance", "storage"};
    task.callback = create_cleanup_callback(config);
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = task.created_at;

    return schedule(std::move(task));
}

// =============================================================================
// Task Scheduling - Archive
// =============================================================================

auto task_scheduler::schedule_archive(const archive_config& config) -> task_id {
    scheduled_task task;
    task.id = "archive_task";
    task.name = "Study Archival";
    task.description = "Archives studies to secondary storage";
    task.type = task_type::archive;
    task.task_schedule = config.archive_schedule;
    task.enabled = true;
    task.priority = 5;
    task.tags = {"maintenance", "archive"};
    task.callback = create_archive_callback(config);
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = task.created_at;

    return schedule(std::move(task));
}

// =============================================================================
// Task Scheduling - Verification
// =============================================================================

auto task_scheduler::schedule_verification(const verification_config& config)
    -> task_id {
    scheduled_task task;
    task.id = "verification_task";
    task.name = "Data Verification";
    task.description = "Verifies data integrity and consistency";
    task.type = task_type::verification;
    task.task_schedule = config.verification_schedule;
    task.enabled = true;
    task.priority = 8;
    task.tags = {"maintenance", "integrity"};
    task.callback = create_verification_callback(config);
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = task.created_at;

    return schedule(std::move(task));
}

// =============================================================================
// Task Scheduling - Custom
// =============================================================================

auto task_scheduler::schedule(
    const std::string& name,
    const std::string& description,
    std::chrono::seconds interval,
    task_callback_with_result callback) -> task_id {

    scheduled_task task;
    task.id = generate_task_id();
    task.name = name;
    task.description = description;
    task.type = task_type::custom;
    task.task_schedule = interval_schedule{interval, std::nullopt};
    task.enabled = true;
    task.callback = std::move(callback);
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = task.created_at;

    return schedule(std::move(task));
}

auto task_scheduler::schedule(
    const std::string& name,
    const std::string& description,
    const cron_schedule& cron_expr,
    task_callback_with_result callback) -> task_id {

    scheduled_task task;
    task.id = generate_task_id();
    task.name = name;
    task.description = description;
    task.type = task_type::custom;
    task.task_schedule = cron_expr;
    task.enabled = true;
    task.callback = std::move(callback);
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = task.created_at;

    return schedule(std::move(task));
}

auto task_scheduler::schedule_once(
    const std::string& name,
    const std::string& description,
    std::chrono::system_clock::time_point execute_at,
    task_callback_with_result callback) -> task_id {

    scheduled_task task;
    task.id = generate_task_id();
    task.name = name;
    task.description = description;
    task.type = task_type::custom;
    task.task_schedule = one_time_schedule{execute_at};
    task.enabled = true;
    task.callback = std::move(callback);
    task.created_at = std::chrono::system_clock::now();
    task.updated_at = task.created_at;
    task.next_run_at = execute_at;

    return schedule(std::move(task));
}

auto task_scheduler::schedule(scheduled_task task) -> task_id {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    // Calculate next run time if not set
    if (!task.next_run_at) {
        task.next_run_at = calculate_next_run(task.task_schedule);
    }

    auto id = task.id;

    // Check if task already exists
    auto it = tasks_.find(id);
    if (it != tasks_.end()) {
        // Update existing task
        it->second = std::move(task);
        integration::logger_adapter::info(
            "Updated scheduled task task_id={} name={}",
            id, it->second.name);
    } else {
        // Add new task
        tasks_.emplace(id, std::move(task));
        integration::logger_adapter::info(
            "Added scheduled task task_id={} name={}",
            id, tasks_[id].name);
    }

    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.scheduled_tasks = tasks_.size();
    }

    return id;
}

// =============================================================================
// Task Management
// =============================================================================

auto task_scheduler::list_tasks() const -> std::vector<scheduled_task> {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    std::vector<scheduled_task> result;
    result.reserve(tasks_.size());

    for (const auto& [id, task] : tasks_) {
        result.push_back(task);
    }

    return result;
}

auto task_scheduler::list_tasks(task_type type) const
    -> std::vector<scheduled_task> {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    std::vector<scheduled_task> result;

    for (const auto& [id, task] : tasks_) {
        if (task.type == type) {
            result.push_back(task);
        }
    }

    return result;
}

auto task_scheduler::list_tasks(task_state state) const
    -> std::vector<scheduled_task> {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    std::vector<scheduled_task> result;

    for (const auto& [id, task] : tasks_) {
        if (task.state == state) {
            result.push_back(task);
        }
    }

    return result;
}

auto task_scheduler::get_task(const task_id& id) const
    -> std::optional<scheduled_task> {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return std::nullopt;
    }

    return it->second;
}

auto task_scheduler::cancel_task(const task_id& id) -> bool {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }

    it->second.state = task_state::cancelled;
    it->second.enabled = false;
    it->second.updated_at = std::chrono::system_clock::now();

    integration::logger_adapter::info(
        "Cancelled scheduled task task_id={} name={}",
        id, it->second.name);

    return true;
}

auto task_scheduler::pause_task(const task_id& id) -> bool {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }

    if (it->second.state == task_state::running) {
        return false;  // Cannot pause a running task
    }

    it->second.state = task_state::paused;
    it->second.updated_at = std::chrono::system_clock::now();

    integration::logger_adapter::info(
        "Paused scheduled task task_id={} name={}",
        id, it->second.name);

    return true;
}

auto task_scheduler::resume_task(const task_id& id) -> bool {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }

    if (it->second.state != task_state::paused) {
        return false;  // Not paused
    }

    it->second.state = task_state::pending;
    it->second.next_run_at = calculate_next_run(it->second.task_schedule);
    it->second.updated_at = std::chrono::system_clock::now();

    integration::logger_adapter::info(
        "Resumed scheduled task task_id={} name={}",
        id, it->second.name);

    return true;
}

auto task_scheduler::trigger_task(const task_id& id) -> bool {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }

    if (!it->second.enabled || it->second.state == task_state::running) {
        return false;
    }

    // Set next run to now to trigger immediate execution
    it->second.next_run_at = std::chrono::system_clock::now();

    // Wake up scheduler
    cv_.notify_one();

    integration::logger_adapter::info(
        "Triggered immediate execution task_id={} name={}",
        id, it->second.name);

    return true;
}

auto task_scheduler::update_schedule(const task_id& id, const pacs::workflow::schedule& new_schedule)
    -> bool {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }

    it->second.task_schedule = new_schedule;
    it->second.next_run_at = calculate_next_run(new_schedule);
    it->second.updated_at = std::chrono::system_clock::now();

    integration::logger_adapter::info(
        "Updated schedule for task task_id={} name={}",
        id, it->second.name);

    return true;
}

// =============================================================================
// Execution History
// =============================================================================

auto task_scheduler::get_execution_history(
    const task_id& id,
    std::size_t limit) const -> std::vector<task_execution_record> {
    std::lock_guard<std::mutex> lock(history_mutex_);

    auto it = execution_history_.find(id);
    if (it == execution_history_.end()) {
        return {};
    }

    const auto& records = it->second;
    if (records.size() <= limit) {
        return records;
    }

    // Return most recent records
    return std::vector<task_execution_record>(
        records.end() - static_cast<std::ptrdiff_t>(limit),
        records.end());
}

auto task_scheduler::get_recent_executions(std::size_t limit) const
    -> std::vector<task_execution_record> {
    std::lock_guard<std::mutex> lock(history_mutex_);

    std::vector<task_execution_record> all_records;

    for (const auto& [id, records] : execution_history_) {
        all_records.insert(all_records.end(), records.begin(), records.end());
    }

    // Sort by start time (most recent first)
    std::ranges::sort(all_records, [](const auto& a, const auto& b) {
        return a.started_at > b.started_at;
    });

    if (all_records.size() <= limit) {
        return all_records;
    }

    all_records.resize(limit);
    return all_records;
}

void task_scheduler::clear_history(const task_id& id, std::size_t keep_last) {
    std::lock_guard<std::mutex> lock(history_mutex_);

    auto it = execution_history_.find(id);
    if (it == execution_history_.end()) {
        return;
    }

    auto& records = it->second;
    if (records.size() <= keep_last) {
        return;
    }

    // Keep only the last N records
    records.erase(
        records.begin(),
        records.begin() + static_cast<std::ptrdiff_t>(records.size() - keep_last));
}

// =============================================================================
// Statistics and Monitoring
// =============================================================================

auto task_scheduler::get_stats() const -> scheduler_stats {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    scheduler_stats result = stats_;

    // Calculate uptime
    if (running_.load()) {
        auto now = std::chrono::steady_clock::now();
        result.uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time_);
    }

    result.running_tasks = running_count_.load();

    {
        std::lock_guard<std::mutex> tasks_lock(tasks_mutex_);
        result.scheduled_tasks = tasks_.size();
    }

    return result;
}

auto task_scheduler::pending_count() const noexcept -> std::size_t {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    std::size_t count = 0;
    for (const auto& [id, task] : tasks_) {
        if (task.enabled && task.state == task_state::pending) {
            ++count;
        }
    }
    return count;
}

auto task_scheduler::running_count() const noexcept -> std::size_t {
    return running_count_.load();
}

// =============================================================================
// Persistence
// =============================================================================

auto task_scheduler::save_tasks() const -> bool {
    if (config_.persistence_path.empty()) {
        return false;
    }

    try {
        std::string json = serialize_tasks();

        std::ofstream file(config_.persistence_path);
        if (!file) {
            integration::logger_adapter::error(
                "Failed to open persistence file path={}",
                config_.persistence_path);
            return false;
        }

        file << json;
        file.close();

        integration::logger_adapter::debug(
            "Saved tasks to persistence path={}",
            config_.persistence_path);

        return true;
    } catch (const std::exception& e) {
        integration::logger_adapter::error(
            "Failed to save tasks error={}", e.what());
        return false;
    }
}

auto task_scheduler::load_tasks() -> std::size_t {
    if (config_.persistence_path.empty()) {
        return 0;
    }

    try {
        std::ifstream file(config_.persistence_path);
        if (!file) {
            integration::logger_adapter::debug(
                "No persistence file found path={}",
                config_.persistence_path);
            return 0;
        }

        std::string json((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
        file.close();

        std::size_t count = deserialize_tasks(json);

        integration::logger_adapter::info(
            "Loaded tasks from persistence path={} count={}",
            config_.persistence_path, count);

        return count;
    } catch (const std::exception& e) {
        integration::logger_adapter::error(
            "Failed to load tasks error={}", e.what());
        return 0;
    }
}

// =============================================================================
// Configuration
// =============================================================================

void task_scheduler::set_task_complete_callback(
    task_scheduler_config::task_complete_callback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.on_task_complete = std::move(callback);
}

void task_scheduler::set_error_callback(
    task_scheduler_config::task_error_callback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.on_task_error = std::move(callback);
}

// =============================================================================
// Internal Methods
// =============================================================================

void task_scheduler::run_loop() {
    integration::logger_adapter::debug("Task scheduler thread started");

    while (!stop_requested_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait for check interval or until woken up
        cv_.wait_for(lock, config_.check_interval, [this]() {
            return stop_requested_.load();
        });

        if (stop_requested_.load()) {
            break;
        }

        lock.unlock();
        execute_cycle();
    }

    integration::logger_adapter::debug("Task scheduler thread stopped");
}

void task_scheduler::execute_cycle() {
    auto now = std::chrono::system_clock::now();
    std::vector<task_id> due_tasks;

    // Find due tasks
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);

        for (auto& [id, task] : tasks_) {
            if (!task.enabled || task.state != task_state::pending) {
                continue;
            }

            if (task.next_run_at && *task.next_run_at <= now) {
                due_tasks.push_back(id);
            }
        }
    }

    if (due_tasks.empty()) {
        return;
    }

    // Sort by priority (higher first)
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        std::ranges::sort(due_tasks, [this](const auto& a, const auto& b) {
            return tasks_[a].priority > tasks_[b].priority;
        });
    }

    // Execute tasks (respecting max concurrent limit)
    std::size_t executed = 0;
    std::size_t succeeded = 0;
    std::size_t failed = 0;

    for (const auto& id : due_tasks) {
        if (running_count_.load() >= config_.max_concurrent_tasks) {
            break;  // Max concurrent reached
        }

        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            continue;
        }

        auto& task = it->second;

        // Mark as running
        task.state = task_state::running;
        ++running_count_;

        // Execute task
        auto record = execute_task(task);

        // Update task state
        task.state = (record.state == task_state::completed)
            ? task_state::pending
            : record.state;
        task.last_run_at = record.started_at;
        task.last_execution = record;
        ++task.execution_count;

        if (record.state == task_state::completed) {
            ++task.success_count;
            ++succeeded;
        } else {
            ++task.failure_count;
            ++failed;
        }

        // Calculate next run time
        if (task.enabled && task.state == task_state::pending) {
            task.next_run_at = calculate_next_run(
                task.task_schedule,
                std::chrono::system_clock::now());

            // For one-time tasks, disable after execution
            if (std::holds_alternative<one_time_schedule>(task.task_schedule)) {
                task.enabled = false;
            }
        }

        --running_count_;
        ++executed;

        // Record execution history
        record_execution(id, record);
        update_stats(record);

        // Invoke callbacks
        if (config_.on_task_complete) {
            config_.on_task_complete(id, record);
        }
        if (record.state == task_state::failed && config_.on_task_error) {
            config_.on_task_error(id, record.error_message.value_or("Unknown error"));
        }
    }

    // Log cycle summary
    if (executed > 0) {
        integration::logger_adapter::info(
            "Scheduler cycle completed executed={} succeeded={} failed={}",
            executed, succeeded, failed);

        // Record metrics
        integration::monitoring_adapter::increment_counter(
            "scheduler_tasks_executed", static_cast<int64_t>(executed));
        integration::monitoring_adapter::increment_counter(
            "scheduler_tasks_succeeded", static_cast<int64_t>(succeeded));
        integration::monitoring_adapter::increment_counter(
            "scheduler_tasks_failed", static_cast<int64_t>(failed));

        // Update stats
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.last_cycle_at = std::chrono::system_clock::now();
        }

        // Invoke cycle callback
        if (config_.on_cycle_complete) {
            config_.on_cycle_complete(executed, succeeded, failed);
        }
    }
}

auto task_scheduler::execute_task(scheduled_task& task)
    -> task_execution_record {
    task_execution_record record;
    record.execution_id = generate_execution_id();
    record.task_id = task.id;
    record.started_at = std::chrono::system_clock::now();
    record.state = task_state::running;

    integration::logger_adapter::debug(
        "Executing task task_id={} name={}",
        task.id, task.name);

    auto start_time = std::chrono::steady_clock::now();

    try {
        if (!task.callback) {
            record.state = task_state::failed;
            record.error_message = "No callback defined";
        } else {
            auto result = task.callback();

            if (result.has_value()) {
                record.state = task_state::failed;
                record.error_message = *result;
            } else {
                record.state = task_state::completed;
            }
        }
    } catch (const std::exception& e) {
        record.state = task_state::failed;
        record.error_message = e.what();
        integration::logger_adapter::error(
            "Task execution failed task_id={} error={}",
            task.id, e.what());
    }

    record.ended_at = std::chrono::system_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);

    integration::logger_adapter::info(
        "Task execution completed task_id={} name={} state={} duration_ms={}",
        task.id, task.name, to_string(record.state), duration.count());

    // Record metrics
    integration::monitoring_adapter::record_histogram(
        "scheduler_task_duration_ms",
        static_cast<double>(duration.count()));

    return record;
}

auto task_scheduler::calculate_next_run(
    const pacs::workflow::schedule& sched,
    std::chrono::system_clock::time_point from) const
    -> std::optional<std::chrono::system_clock::time_point> {

    return std::visit([this, from](const auto& s) ->
        std::optional<std::chrono::system_clock::time_point> {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, interval_schedule>) {
            if (s.start_at && *s.start_at > from) {
                return s.start_at;
            }
            return from + s.interval;
        } else if constexpr (std::is_same_v<T, cron_schedule>) {
            return calculate_next_cron_run(s, from);
        } else if constexpr (std::is_same_v<T, one_time_schedule>) {
            if (s.execute_at > from) {
                return s.execute_at;
            }
            return std::nullopt;  // Already passed
        }

        return std::nullopt;
    }, sched);
}

auto task_scheduler::calculate_next_cron_run(
    const cron_schedule& cron,
    std::chrono::system_clock::time_point from) const
    -> std::optional<std::chrono::system_clock::time_point> {

    // Simple cron calculation - advance minute by minute until match
    // This is a simplified implementation; production would use more
    // efficient algorithms

    auto time = from + std::chrono::minutes{1};  // Start from next minute
    auto end_time = from + std::chrono::hours{24 * 365};  // Max 1 year ahead

    // Helper to parse cron field
    auto matches_field = [](const std::string& field, int value, [[maybe_unused]] int max) -> bool {
        if (field == "*") return true;

        // Check for step values (*/n)
        if (field.starts_with("*/")) {
            int step = std::stoi(field.substr(2));
            return (value % step) == 0;
        }

        // Check for range (n-m)
        auto dash_pos = field.find('-');
        if (dash_pos != std::string::npos) {
            int start = std::stoi(field.substr(0, dash_pos));
            int end = std::stoi(field.substr(dash_pos + 1));
            return value >= start && value <= end;
        }

        // Check for list (n,m,o)
        if (field.find(',') != std::string::npos) {
            std::istringstream iss(field);
            std::string item;
            while (std::getline(iss, item, ',')) {
                if (std::stoi(item) == value) {
                    return true;
                }
            }
            return false;
        }

        // Single value
        return std::stoi(field) == value;
    };

    while (time < end_time) {
        auto time_t = std::chrono::system_clock::to_time_t(time);
        std::tm tm = *std::localtime(&time_t);

        bool matches =
            matches_field(cron.minute, tm.tm_min, 59) &&
            matches_field(cron.hour, tm.tm_hour, 23) &&
            matches_field(cron.day_of_month, tm.tm_mday, 31) &&
            matches_field(cron.month, tm.tm_mon + 1, 12) &&
            matches_field(cron.day_of_week, tm.tm_wday, 6);

        if (matches) {
            // Round to start of minute
            auto duration = time.time_since_epoch();
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
            return std::chrono::system_clock::time_point(minutes);
        }

        time += std::chrono::minutes{1};
    }

    return std::nullopt;
}

auto task_scheduler::generate_task_id() const -> task_id {
    auto id = next_task_id_.fetch_add(1);
    return "task_" + std::to_string(id);
}

auto task_scheduler::generate_execution_id() const -> std::string {
    auto id = next_execution_id_.fetch_add(1);

    // Add timestamp for uniqueness
    auto now = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return "exec_" + std::to_string(millis) + "_" + std::to_string(id);
}

auto task_scheduler::create_cleanup_callback(const cleanup_config& config)
    -> task_callback_with_result {
    return [this, config]() -> std::optional<std::string> {
        integration::logger_adapter::info(
            "Running cleanup task retention_days={}",
            config.default_retention.count());

        try {
            // Calculate cutoff date
            auto now = std::chrono::system_clock::now();
            auto cutoff = now - config.default_retention;

            // Convert to YYYYMMDD format for database query
            auto cutoff_time_t = std::chrono::system_clock::to_time_t(cutoff);
            std::tm tm = *std::localtime(&cutoff_time_t);
            std::ostringstream date_oss;
            date_oss << std::put_time(&tm, "%Y%m%d");
            std::string cutoff_date = date_oss.str();

            // Query for studies older than cutoff
            storage::study_query query;
            query.study_date_to = cutoff_date;
            query.limit = config.max_deletions_per_cycle;

            auto studies = database_.search_studies(query);

            std::size_t deleted_count = 0;
            std::size_t skipped_count = 0;

            for (const auto& study : studies) {
                // Check modality-specific retention
                auto modality_retention = config.retention_for(study.modalities_in_study);
                auto modality_cutoff = now - modality_retention;
                auto modality_cutoff_t = std::chrono::system_clock::to_time_t(modality_cutoff);
                std::tm mod_tm = *std::localtime(&modality_cutoff_t);
                std::ostringstream mod_oss;
                mod_oss << std::put_time(&mod_tm, "%Y%m%d");
                std::string modality_cutoff_date = mod_oss.str();

                // Skip if study is newer than modality-specific retention
                if (study.study_date > modality_cutoff_date) {
                    ++skipped_count;
                    continue;
                }

                // Check exclusion patterns
                bool excluded = false;
                for (const auto& pattern : config.exclude_patterns) {
                    if (study.study_description.find(pattern) != std::string::npos) {
                        excluded = true;
                        break;
                    }
                }
                if (excluded) {
                    ++skipped_count;
                    continue;
                }

                if (config.dry_run) {
                    integration::logger_adapter::info(
                        "Dry-run: would delete study study_uid={} study_date={} modality={}",
                        study.study_uid, study.study_date, study.modalities_in_study);
                    ++deleted_count;
                    continue;
                }

                // Delete files from storage if not database_only
                if (!config.database_only && file_storage_ != nullptr) {
                    auto file_paths = database_.get_study_files(study.study_uid);
                    for (const auto& file_path : file_paths) {
                        // Extract SOP UID from file path for removal
                        std::filesystem::path p(file_path);
                        std::string sop_uid = p.stem().string();
                        auto remove_result = file_storage_->remove(sop_uid);
                        if (remove_result.is_err()) {
                            integration::logger_adapter::warn(
                                "Failed to remove file file_path={} error={}",
                                file_path, remove_result.error().message);
                        }
                    }
                }

                // Delete from database
                auto delete_result = database_.delete_study(study.study_uid);
                if (delete_result.is_err()) {
                    integration::logger_adapter::error(
                        "Failed to delete study study_uid={} error={}",
                        study.study_uid, delete_result.error().message);
                    continue;
                }

                ++deleted_count;
                integration::logger_adapter::debug(
                    "Deleted study study_uid={} study_date={}",
                    study.study_uid, study.study_date);
            }

            integration::logger_adapter::info(
                "Cleanup task completed deleted={} skipped={} dry_run={}",
                deleted_count, skipped_count, config.dry_run);

            return std::nullopt;  // Success
        } catch (const std::exception& e) {
            return std::string("Cleanup failed: ") + e.what();
        }
    };
}

auto task_scheduler::create_archive_callback(const archive_config& config)
    -> task_callback_with_result {
    return [this, config]() -> std::optional<std::string> {
        integration::logger_adapter::info(
            "Running archive task archive_after_days={} destination={}",
            config.archive_after.count(),
            config.destination);

        try {
            // Calculate cutoff date
            auto now = std::chrono::system_clock::now();
            auto cutoff = now - config.archive_after;

            // Convert to YYYYMMDD format
            auto cutoff_time_t = std::chrono::system_clock::to_time_t(cutoff);
            std::tm tm = *std::localtime(&cutoff_time_t);
            std::ostringstream date_oss;
            date_oss << std::put_time(&tm, "%Y%m%d");
            std::string cutoff_date = date_oss.str();

            // Query for studies older than cutoff
            storage::study_query query;
            query.study_date_to = cutoff_date;
            query.limit = config.max_archives_per_cycle;

            auto studies = database_.search_studies(query);

            std::size_t archived_count = 0;
            std::size_t failed_count = 0;

            // Create destination directory if needed
            std::filesystem::path dest_path(config.destination);
            if (!std::filesystem::exists(dest_path)) {
                std::filesystem::create_directories(dest_path);
            }

            for (const auto& study : studies) {
                // Get all files for this study
                auto file_paths = database_.get_study_files(study.study_uid);
                if (file_paths.empty()) {
                    continue;
                }

                // Create study archive directory
                std::filesystem::path study_dest = dest_path / study.study_uid;
                if (!std::filesystem::exists(study_dest)) {
                    std::filesystem::create_directories(study_dest);
                }

                bool archive_success = true;

                for (const auto& src_file : file_paths) {
                    std::filesystem::path src_path(src_file);
                    if (!std::filesystem::exists(src_path)) {
                        integration::logger_adapter::warn(
                            "Source file not found file_path={}", src_file);
                        continue;
                    }

                    // Determine destination filename
                    std::filesystem::path dest_file = study_dest / src_path.filename();

                    try {
                        // Copy file to archive location
                        std::filesystem::copy_file(
                            src_path, dest_file,
                            std::filesystem::copy_options::overwrite_existing);

                        // Verify copy if configured
                        if (config.verify_after_archive) {
                            auto src_size = std::filesystem::file_size(src_path);
                            auto dest_size = std::filesystem::file_size(dest_file);
                            if (src_size != dest_size) {
                                integration::logger_adapter::error(
                                    "Archive verification failed: size mismatch "
                                    "src={} dest={}", src_file, dest_file.string());
                                archive_success = false;
                                break;
                            }
                        }
                    } catch (const std::filesystem::filesystem_error& e) {
                        integration::logger_adapter::error(
                            "Failed to archive file src={} dest={} error={}",
                            src_file, dest_file.string(), e.what());
                        archive_success = false;
                        break;
                    }
                }

                if (archive_success) {
                    ++archived_count;

                    // Delete originals if configured
                    if (config.delete_after_archive && file_storage_ != nullptr) {
                        for (const auto& file_path : file_paths) {
                            std::filesystem::path p(file_path);
                            std::string sop_uid = p.stem().string();
                            (void)file_storage_->remove(sop_uid);
                        }
                        (void)database_.delete_study(study.study_uid);
                    }

                    integration::logger_adapter::debug(
                        "Archived study study_uid={} files={}",
                        study.study_uid, file_paths.size());
                } else {
                    ++failed_count;
                }
            }

            integration::logger_adapter::info(
                "Archive task completed archived={} failed={}",
                archived_count, failed_count);

            if (failed_count > 0) {
                return "Archive completed with " + std::to_string(failed_count) + " failures";
            }

            return std::nullopt;  // Success
        } catch (const std::exception& e) {
            return std::string("Archive failed: ") + e.what();
        }
    };
}

auto task_scheduler::create_verification_callback(const verification_config& config)
    -> task_callback_with_result {
    return [this, config]() -> std::optional<std::string> {
        integration::logger_adapter::info(
            "Running verification task check_checksums={} check_db={}",
            config.check_checksums,
            config.check_db_consistency);

        try {
            std::size_t verified = 0;
            std::size_t errors = 0;
            std::size_t missing_files = 0;

            // Database integrity check
            if (config.check_db_consistency) {
                auto db_result = database_.verify_integrity();
                if (db_result.is_err()) {
                    integration::logger_adapter::error(
                        "Database integrity check failed error={}",
                        db_result.error().message);
                    ++errors;
                } else {
                    integration::logger_adapter::debug("Database integrity check passed");
                }
            }

            // Get studies for verification
            storage::study_query query;
            query.limit = config.max_verifications_per_cycle;
            auto studies = database_.search_studies(query);

            for (const auto& study : studies) {
                // Get all files for this study
                auto file_paths = database_.get_study_files(study.study_uid);

                for (const auto& file_path : file_paths) {
                    std::filesystem::path path(file_path);

                    // Check file existence
                    if (!std::filesystem::exists(path)) {
                        ++missing_files;
                        integration::logger_adapter::warn(
                            "Missing file detected file_path={} study_uid={}",
                            file_path, study.study_uid);

                        if (config.repair_on_failure) {
                            // Extract SOP UID and remove orphaned database record
                            std::string sop_uid = path.stem().string();
                            (void)database_.delete_instance(sop_uid);
                            integration::logger_adapter::info(
                                "Removed orphaned database record sop_uid={}", sop_uid);
                        }
                        continue;
                    }

                    // Checksum verification
                    if (config.check_checksums) {
                        // Get file size as basic verification
                        // Full checksum verification would require storing checksums in DB
                        try {
                            auto file_size = std::filesystem::file_size(path);
                            if (file_size == 0) {
                                ++errors;
                                integration::logger_adapter::warn(
                                    "Empty file detected file_path={}", file_path);
                            }
                        } catch (const std::filesystem::filesystem_error& e) {
                            ++errors;
                            integration::logger_adapter::error(
                                "Cannot read file file_path={} error={}",
                                file_path, e.what());
                        }
                    }

                    ++verified;
                }
            }

            // File storage integrity check
            if (file_storage_ != nullptr) {
                auto storage_result = file_storage_->verify_integrity();
                if (storage_result.is_err()) {
                    integration::logger_adapter::warn(
                        "Storage integrity check reported issues error={}",
                        storage_result.error().message);
                }
            }

            integration::logger_adapter::info(
                "Verification task completed verified={} errors={} missing_files={}",
                verified, errors, missing_files);

            if (errors > 0 || missing_files > 0) {
                std::ostringstream oss;
                oss << "Verification found " << errors << " errors and "
                    << missing_files << " missing files";
                return oss.str();
            }

            return std::nullopt;  // Success
        } catch (const std::exception& e) {
            return std::string("Verification failed: ") + e.what();
        }
    };
}

void task_scheduler::record_execution(const task_id& task_id,
                                     const task_execution_record& record) {
    std::lock_guard<std::mutex> lock(history_mutex_);

    auto& history = execution_history_[task_id];
    history.push_back(record);

    // Limit history size (keep last 1000 records per task)
    constexpr std::size_t max_history = 1000;
    if (history.size() > max_history) {
        history.erase(
            history.begin(),
            history.begin() + static_cast<std::ptrdiff_t>(history.size() - max_history));
    }
}

void task_scheduler::update_stats(const task_execution_record& record) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    ++stats_.total_executions;

    if (record.state == task_state::completed) {
        ++stats_.successful_executions;
    } else if (record.state == task_state::failed) {
        ++stats_.failed_executions;
    } else if (record.state == task_state::cancelled) {
        ++stats_.cancelled_executions;
    }

    // Update average and max execution time
    if (auto dur = record.duration()) {
        auto ms = dur->count();

        // Update max
        if (ms > stats_.max_execution_time.count()) {
            stats_.max_execution_time = std::chrono::milliseconds(ms);
        }

        // Update running average
        if (stats_.total_executions == 1) {
            stats_.avg_execution_time = std::chrono::milliseconds(ms);
        } else {
            auto current_avg = stats_.avg_execution_time.count();
            auto new_avg = current_avg +
                           (ms - current_avg) /
                           static_cast<int64_t>(stats_.total_executions);
            stats_.avg_execution_time = std::chrono::milliseconds(new_avg);
        }
    }
}

auto task_scheduler::serialize_tasks() const -> std::string {
    // Simple JSON serialization
    // In production, use a proper JSON library

    std::lock_guard<std::mutex> lock(tasks_mutex_);

    std::ostringstream oss;
    oss << "{\n  \"tasks\": [\n";

    bool first = true;
    for (const auto& [id, task] : tasks_) {
        if (!first) oss << ",\n";
        first = false;

        oss << "    {\n";
        oss << "      \"id\": \"" << task.id << "\",\n";
        oss << "      \"name\": \"" << task.name << "\",\n";
        oss << "      \"type\": \"" << to_string(task.type) << "\",\n";
        oss << "      \"enabled\": " << (task.enabled ? "true" : "false") << ",\n";
        oss << "      \"priority\": " << task.priority << "\n";
        oss << "    }";
    }

    oss << "\n  ]\n}";

    return oss.str();
}

auto task_scheduler::deserialize_tasks(const std::string& json) -> std::size_t {
    // Simple JSON parsing - in production, use a proper JSON library
    // For now, just return 0 as we need a JSON parser for proper implementation
    (void)json;
    return 0;
}

}  // namespace pacs::workflow
