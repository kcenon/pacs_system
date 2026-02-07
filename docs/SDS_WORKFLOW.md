# SDS - Workflow Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-04
> **Status:** Initial (Network Operations Pending - C-FIND/C-MOVE TODO)

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-WKF-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#474](https://github.com/kcenon/pacs_system/issues/474), [#468](https://github.com/kcenon/pacs_system/issues/468) |

### Related Requirements

| Requirement | Description |
|-------------|-------------|
| SRS-WKF-001 | Auto Prefetch Service Specification |
| SRS-WKF-002 | Task Scheduler Service Specification |
| SRS-WKF-003 | Study Lock Manager Specification |
| FR-4.6 | Automatic Prior Study Prefetch |
| FR-4.7 | Automated Maintenance Tasks |
| FR-4.8 | Study Modification Control |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Auto Prefetch Service](#2-auto-prefetch-service)
- [3. Prefetch Configuration](#3-prefetch-configuration)
- [4. Task Scheduler](#4-task-scheduler)
- [5. Task Scheduler Configuration](#5-task-scheduler-configuration)
- [6. Study Lock Manager](#6-study-lock-manager)
- [7. Class Diagrams](#7-class-diagrams)
- [8. Sequence Diagrams](#8-sequence-diagrams)
- [9. Traceability](#9-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the Workflow module for the PACS System. The module provides:

- **Auto Prefetch Service**: Automatic prefetching of prior patient studies from remote PACS
- **Task Scheduler**: Scheduling and execution of recurring maintenance tasks
- **Study Lock Manager**: Locking mechanisms for concurrent access control

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| Auto Prefetch Service | 1 header, 1 source | DES-WKF-001 |
| Prefetch Configuration | 1 header | DES-WKF-002 |
| Task Scheduler | 1 header, 1 source | DES-WKF-003 |
| Task Scheduler Configuration | 1 header | DES-WKF-004 |
| Study Lock Manager | 1 header, 1 source | DES-WKF-005 |

### 1.3 Design Identifier Convention

```
DES-WKF-<NUMBER>

Where:
- DES: Design Specification prefix
- WKF: Workflow module identifier
- NUMBER: 3-digit sequential number (001-005)
```

### 1.4 Integration with kcenon Ecosystem

| System | Integration Point |
|--------|-------------------|
| thread_system | Thread pool for parallel prefetch and task execution |
| logger_system | Audit logging for workflow operations |
| monitoring_system | Metrics for prefetch success/failure rates |
| common_system | Result<T> for error handling, IExecutor interface (Issue #487) |

#### 1.4.1 IExecutor Integration (Issue #487)

Both `auto_prefetch_service` and `task_scheduler` support the standardized `IExecutor` interface from `common_system`, enabling:

- **Dependency Injection**: Executor can be injected through constructors
- **Testability**: Mock executors can be used in unit tests
- **Flexibility**: Different executor implementations can be swapped

**Available Constructors:**

```cpp
// Legacy thread_pool constructor
task_scheduler(database, file_storage, thread_pool, config);
auto_prefetch_service(database, thread_pool, config);

// IExecutor constructor (recommended)
task_scheduler(database, file_storage, executor, config);
auto_prefetch_service(database, executor, config);
```

**Adapter Classes:**

- `thread_pool_executor_adapter`: Wraps `thread_pool` with `IExecutor` interface
- `lambda_job`: Wraps callables as `IJob` for executor submission
- `make_executor()`: Factory function to create executors from pool interfaces

---

## 2. Auto Prefetch Service

### 2.1 DES-WKF-001: Auto Prefetch Service

**Traces to:** SRS-WKF-001 (Auto Prefetch Service), FR-4.6 (Automatic Prior Study Prefetch)

**File:** `include/pacs/workflow/auto_prefetch_service.hpp`

The auto_prefetch_service automatically prefetches prior patient studies from remote PACS servers when patients appear in the modality worklist.

#### 2.1.1 Class Definition

```cpp
namespace pacs::workflow {

struct prefetch_request {
    std::string patient_id;
    std::string patient_name;
    std::string scheduled_modality;
    std::string scheduled_body_part;
    std::string scheduled_study_uid;
    std::chrono::system_clock::time_point request_time;
    std::size_t retry_count{0};
};

class auto_prefetch_service {
public:
    // Construction
    explicit auto_prefetch_service(
        storage::index_database& database,
        const prefetch_service_config& config = {});

    auto_prefetch_service(
        storage::index_database& database,
        std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
        const prefetch_service_config& config = {});

    ~auto_prefetch_service();

    // Lifecycle Management
    void enable();
    void start();
    void disable(bool wait_for_completion = true);
    void stop(bool wait_for_completion = true);
    [[nodiscard]] auto is_enabled() const noexcept -> bool;
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // Manual Operations
    [[nodiscard]] auto prefetch_priors(
        const std::string& patient_id,
        std::chrono::days lookback = std::chrono::days{365})
        -> prefetch_result;

    void trigger_for_worklist(
        const std::vector<storage::worklist_item>& worklist_items);
    void trigger_cycle();
    [[nodiscard]] auto run_prefetch_cycle() -> prefetch_result;

    // Worklist Event Handler
    void on_worklist_query(
        const std::vector<storage::worklist_item>& worklist_items);

    // Statistics and Monitoring
    [[nodiscard]] auto get_last_result() const -> std::optional<prefetch_result>;
    [[nodiscard]] auto get_cumulative_stats() const -> prefetch_result;
    [[nodiscard]] auto time_until_next_cycle() const
        -> std::optional<std::chrono::seconds>;
    [[nodiscard]] auto cycles_completed() const noexcept -> std::size_t;
    [[nodiscard]] auto pending_requests() const noexcept -> std::size_t;

    // Configuration
    void set_prefetch_interval(std::chrono::seconds interval);
    [[nodiscard]] auto get_prefetch_interval() const noexcept
        -> std::chrono::seconds;
    void set_prefetch_criteria(const prefetch_criteria& criteria);
    [[nodiscard]] auto get_prefetch_criteria() const noexcept
        -> const prefetch_criteria&;

private:
    void run_loop();
    [[nodiscard]] auto execute_cycle() -> prefetch_result;
    [[nodiscard]] auto process_request(const prefetch_request& request)
        -> prefetch_result;
    [[nodiscard]] auto query_prior_studies(
        const remote_pacs_config& pacs_config,
        const std::string& patient_id,
        std::chrono::days lookback) -> std::vector<prior_study_info>;
    [[nodiscard]] auto filter_studies(
        const std::vector<prior_study_info>& studies,
        const prefetch_request& request) -> std::vector<prior_study_info>;
    [[nodiscard]] auto study_exists_locally(const std::string& study_uid) -> bool;
    [[nodiscard]] auto prefetch_study(
        const remote_pacs_config& pacs_config,
        const prior_study_info& study) -> bool;

    storage::index_database& database_;
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;
    prefetch_service_config config_;
    std::thread worker_thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> enabled_{false};
    std::queue<prefetch_request> request_queue_;
    std::set<std::string> queued_patients_;
};

} // namespace pacs::workflow
```

#### 2.1.2 Prefetch Workflow

```
Worklist Query
     │
     ▼
┌────────────────────────┐
│ Extract Patient IDs    │
└───────────┬────────────┘
            │
            ▼
┌────────────────────────┐
│ Queue Prefetch Request │
└───────────┬────────────┘
            │
    ┌───────┴───────┐
    ▼               ▼
┌────────┐     ┌────────┐
│Worker 1│     │Worker N│  (Parallel via thread_pool)
└───┬────┘     └───┬────┘
    │              │
    ▼              ▼
┌─────────────────────────┐
│ Query Remote PACS       │
│ (C-FIND for priors)     │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Filter by Criteria      │
│ (modality, date, etc.)  │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Check Local Storage     │
│ (skip if present)       │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ C-MOVE from Remote      │
│ (prefetch images)       │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Update Statistics       │
└─────────────────────────┘
```

#### 2.1.3 Key Features

| Feature | Description |
|---------|-------------|
| Worklist-Triggered Prefetch | Automatically prefetches priors when a patient appears in MWL |
| Configurable Selection | Filter priors by modality, body part, lookback period |
| Multi-Source Support | Can prefetch from multiple remote PACS servers |
| Parallel Processing | Uses thread_pool for concurrent prefetches |
| Rate Limiting | Prevents overloading remote PACS with requests |
| Retry Logic | Automatically retries failed prefetches |
| Deduplication | Avoids duplicate requests for the same patient |

#### 2.1.4 Design Decisions

- **Background Worker Thread**: Dedicated thread for prefetch cycle management
- **Queue-Based Processing**: Request queue with patient deduplication
- **Configurable Intervals**: Adjustable prefetch cycle timing
- **Non-Blocking Operations**: Worklist handlers return immediately

---

## 3. Prefetch Configuration

### 3.1 DES-WKF-002: Prefetch Configuration

**Traces to:** SRS-WKF-001 (Auto Prefetch Service), FR-4.6 (Automatic Prior Study Prefetch)

**File:** `include/pacs/workflow/prefetch_config.hpp`

Configuration structures for the auto prefetch service.

#### 3.1.1 Remote PACS Configuration

```cpp
namespace pacs::workflow {

struct remote_pacs_config {
    std::string ae_title;                    // Remote PACS AE title
    std::string host;                        // Remote PACS hostname
    uint16_t port{11112};                    // Remote PACS port
    std::string local_ae_title{"PACS_PREFETCH"};
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds association_timeout{60};
    bool use_tls{false};

    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !ae_title.empty() && !host.empty() && port > 0;
    }
};

} // namespace pacs::workflow
```

#### 3.1.2 Prefetch Selection Criteria

```cpp
namespace pacs::workflow {

struct prefetch_criteria {
    std::chrono::days lookback_period{365};      // How far back to look
    std::size_t max_studies_per_patient{10};     // Maximum studies to prefetch
    std::size_t max_series_per_study{0};         // 0 = unlimited
    std::set<std::string> include_modalities;   // Empty = all
    std::set<std::string> exclude_modalities;
    std::set<std::string> include_body_parts;
    bool prefer_same_modality{true};             // Prefer matching modality
    bool prefer_same_body_part{true};            // Prefer matching body part
};

} // namespace pacs::workflow
```

#### 3.1.3 Prefetch Result Statistics

```cpp
namespace pacs::workflow {

struct prefetch_result {
    std::size_t patients_processed{0};
    std::size_t studies_prefetched{0};
    std::size_t series_prefetched{0};
    std::size_t instances_prefetched{0};
    std::size_t studies_failed{0};
    std::size_t studies_already_present{0};
    std::size_t bytes_downloaded{0};
    std::chrono::milliseconds duration{0};
    std::chrono::system_clock::time_point timestamp;

    [[nodiscard]] auto is_successful() const noexcept -> bool {
        return studies_failed == 0;
    }

    auto operator+=(const prefetch_result& other) -> prefetch_result&;
};

} // namespace pacs::workflow
```

#### 3.1.4 Prior Study Information

```cpp
namespace pacs::workflow {

struct prior_study_info {
    std::string study_instance_uid;
    std::string patient_id;
    std::string patient_name;
    std::string study_date;              // YYYYMMDD format
    std::string study_description;
    std::set<std::string> modalities;
    std::string body_part_examined;
    std::string accession_number;
    std::size_t number_of_series{0};
    std::size_t number_of_instances{0};
};

} // namespace pacs::workflow
```

#### 3.1.5 Service Configuration

```cpp
namespace pacs::workflow {

struct prefetch_service_config {
    bool enabled{true};
    std::chrono::seconds prefetch_interval{300};    // 5 minutes
    std::size_t max_concurrent_prefetches{4};
    bool auto_start{false};
    std::vector<remote_pacs_config> remote_pacs;
    prefetch_criteria criteria;
    std::size_t rate_limit_per_minute{0};           // 0 = unlimited
    bool retry_on_failure{true};
    std::size_t max_retry_attempts{3};
    std::chrono::seconds retry_delay{60};

    using cycle_complete_callback =
        std::function<void(const prefetch_result& result)>;
    cycle_complete_callback on_cycle_complete;

    using prefetch_complete_callback =
        std::function<void(const std::string& patient_id,
                          const prior_study_info& study,
                          bool success,
                          const std::string& error_message)>;
    prefetch_complete_callback on_prefetch_complete;

    using error_callback =
        std::function<void(const std::string& patient_id,
                          const std::string& study_uid,
                          const std::string& error)>;
    error_callback on_prefetch_error;

    [[nodiscard]] auto is_valid() const noexcept -> bool;
};

} // namespace pacs::workflow
```

---

## 4. Task Scheduler

### 4.1 DES-WKF-003: Task Scheduler Service

**Traces to:** SRS-WKF-002 (Task Scheduler Service), FR-4.7 (Automated Maintenance Tasks)

**File:** `include/pacs/workflow/task_scheduler.hpp`

The task_scheduler manages recurring maintenance tasks such as cleanup, archiving, and verification.

#### 4.1.1 Class Definition

```cpp
namespace pacs::workflow {

class task_scheduler {
public:
    // Construction
    explicit task_scheduler(
        storage::index_database& database,
        const task_scheduler_config& config = {});

    task_scheduler(
        storage::index_database& database,
        storage::file_storage& file_storage,
        std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
        const task_scheduler_config& config = {});

    ~task_scheduler();

    // Lifecycle Management
    void start();
    void stop(bool wait_for_completion = true);
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // Task Scheduling - Cleanup
    auto schedule_cleanup(const cleanup_config& config) -> task_id;

    // Task Scheduling - Archive
    auto schedule_archive(const archive_config& config) -> task_id;

    // Task Scheduling - Verification
    auto schedule_verification(const verification_config& config) -> task_id;

    // Task Scheduling - Custom
    auto schedule(
        const std::string& name,
        const std::string& description,
        std::chrono::seconds interval,
        task_callback_with_result callback) -> task_id;

    auto schedule(
        const std::string& name,
        const std::string& description,
        const cron_schedule& cron_expr,
        task_callback_with_result callback) -> task_id;

    auto schedule_once(
        const std::string& name,
        const std::string& description,
        std::chrono::system_clock::time_point execute_at,
        task_callback_with_result callback) -> task_id;

    auto schedule(scheduled_task task) -> task_id;

    // Task Management
    [[nodiscard]] auto list_tasks() const -> std::vector<scheduled_task>;
    [[nodiscard]] auto list_tasks(task_type type) const -> std::vector<scheduled_task>;
    [[nodiscard]] auto list_tasks(task_state state) const -> std::vector<scheduled_task>;
    [[nodiscard]] auto get_task(const task_id& id) const -> std::optional<scheduled_task>;
    auto cancel_task(const task_id& id) -> bool;
    auto pause_task(const task_id& id) -> bool;
    auto resume_task(const task_id& id) -> bool;
    auto trigger_task(const task_id& id) -> bool;
    auto update_schedule(const task_id& id, const schedule& new_schedule) -> bool;

    // Execution History
    [[nodiscard]] auto get_execution_history(
        const task_id& id,
        std::size_t limit = 100) const -> std::vector<task_execution_record>;
    [[nodiscard]] auto get_recent_executions(std::size_t limit = 100) const
        -> std::vector<task_execution_record>;
    void clear_history(const task_id& id, std::size_t keep_last = 0);

    // Statistics and Monitoring
    [[nodiscard]] auto get_stats() const -> scheduler_stats;
    [[nodiscard]] auto pending_count() const noexcept -> std::size_t;
    [[nodiscard]] auto running_count() const noexcept -> std::size_t;

    // Persistence
    auto save_tasks() const -> bool;
    auto load_tasks() -> std::size_t;

private:
    void run_loop();
    void execute_cycle();
    auto execute_task(scheduled_task& task) -> task_execution_record;
    [[nodiscard]] auto calculate_next_run(
        const schedule& sched,
        std::chrono::system_clock::time_point from) const
        -> std::optional<std::chrono::system_clock::time_point>;

    storage::index_database& database_;
    storage::file_storage* file_storage_{nullptr};
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;
    task_scheduler_config config_;
    std::thread scheduler_thread_;
    std::map<task_id, scheduled_task> tasks_;
    std::map<task_id, std::vector<task_execution_record>> execution_history_;
    std::atomic<bool> running_{false};
    scheduler_stats stats_;
};

} // namespace pacs::workflow
```

#### 4.1.2 Task Execution Flow

```
Scheduler Thread
     │
     ▼
┌────────────────────────┐
│ Check Due Tasks        │ (every check_interval)
└───────────┬────────────┘
            │
            ▼
┌────────────────────────┐
│ Sort by Priority       │
└───────────┬────────────┘
            │
    ┌───────┴───────┐
    ▼               ▼
┌────────┐     ┌────────┐
│Task 1  │     │Task N  │  (Parallel via thread_pool)
└───┬────┘     └───┬────┘
    │              │
    ▼              ▼
┌─────────────────────────┐
│ Execute Callback        │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Record Result           │
│ (success/failure)       │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Calculate Next Run      │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Notify Callbacks        │
└─────────────────────────┘
```

#### 4.1.3 Key Features

| Feature | Description |
|---------|-------------|
| Cron-like Scheduling | Complex schedule expressions (e.g., "0 2 * * *") |
| Interval Scheduling | Simple fixed-interval execution |
| One-time Tasks | Single execution at specific time |
| Persistence | Tasks survive service restarts |
| Parallel Execution | Concurrent task execution with thread pool |
| Task Management | List, cancel, pause, resume, trigger tasks |
| Execution History | Track task execution records |
| Priority Scheduling | Execute higher priority tasks first |

#### 4.1.4 Built-in Task Types

| Task Type | Description |
|-----------|-------------|
| Cleanup | Automatic removal of old studies based on retention policies |
| Archive | Moving studies to secondary storage (HSM, cloud, tape) |
| Verification | Periodic integrity checks and consistency validation |
| Custom | User-defined tasks with custom callbacks |

---

## 5. Task Scheduler Configuration

### 5.1 DES-WKF-004: Task Scheduler Configuration

**Traces to:** SRS-WKF-002 (Task Scheduler Service), FR-4.7 (Automated Maintenance Tasks)

**File:** `include/pacs/workflow/task_scheduler_config.hpp`

Configuration structures for the task scheduler service.

#### 5.1.1 Schedule Expression Types

```cpp
namespace pacs::workflow {

// Simple interval-based schedule
struct interval_schedule {
    std::chrono::seconds interval{3600};  // Default: 1 hour
    std::optional<std::chrono::system_clock::time_point> start_at;
};

// Cron-like schedule expression
struct cron_schedule {
    std::string minute{"*"};        // 0-59, or "*"
    std::string hour{"*"};          // 0-23, or "*"
    std::string day_of_month{"*"}; // 1-31, or "*"
    std::string month{"*"};         // 1-12, or "*"
    std::string day_of_week{"*"};  // 0-6 (Sunday=0), or "*"

    [[nodiscard]] static auto every_minutes(int n) -> cron_schedule;
    [[nodiscard]] static auto every_hours(int n) -> cron_schedule;
    [[nodiscard]] static auto daily_at(int hour, int minute = 0) -> cron_schedule;
    [[nodiscard]] static auto weekly_on(int day_of_week, int hour = 0, int minute = 0)
        -> cron_schedule;
    [[nodiscard]] static auto parse(const std::string& expr) -> cron_schedule;
    [[nodiscard]] auto to_string() const -> std::string;
    [[nodiscard]] auto is_valid() const noexcept -> bool;
};

// One-time execution
struct one_time_schedule {
    std::chrono::system_clock::time_point execute_at;
};

// Combined schedule type
using schedule = std::variant<interval_schedule, cron_schedule, one_time_schedule>;

} // namespace pacs::workflow
```

#### 5.1.2 Task Types and States

```cpp
namespace pacs::workflow {

enum class task_type {
    cleanup,       // Storage cleanup task
    archive,       // Study archival task
    verification,  // Data integrity verification
    custom         // User-defined task
};

enum class task_state {
    pending,    // Waiting for scheduled time
    running,    // Currently executing
    completed,  // Completed successfully
    failed,     // Execution failed
    cancelled,  // Cancelled by user
    paused      // Temporarily paused
};

} // namespace pacs::workflow
```

#### 5.1.3 Scheduled Task Definition

```cpp
namespace pacs::workflow {

using task_id = std::string;
using task_callback = std::function<bool()>;
using task_callback_with_result = std::function<std::optional<std::string>()>;

struct scheduled_task {
    task_id id;
    std::string name;
    std::string description;
    task_type type{task_type::custom};
    schedule task_schedule;
    task_state state{task_state::pending};
    task_callback_with_result callback;
    bool enabled{true};
    int priority{0};                    // Higher = more important
    std::set<std::string> tags;
    std::chrono::seconds timeout{0};    // 0 = no limit
    std::size_t max_retries{0};
    std::chrono::seconds retry_delay{60};
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::optional<std::chrono::system_clock::time_point> next_run_at;
    std::optional<std::chrono::system_clock::time_point> last_run_at;
    std::optional<task_execution_record> last_execution;
    std::size_t execution_count{0};
    std::size_t success_count{0};
    std::size_t failure_count{0};
};

} // namespace pacs::workflow
```

#### 5.1.4 Cleanup Task Configuration

```cpp
namespace pacs::workflow {

struct cleanup_config {
    std::chrono::days default_retention{365};
    std::map<std::string, std::chrono::days> modality_retention;
    std::set<std::string> exclude_patterns;
    bool verify_not_locked{true};
    bool dry_run{false};
    std::size_t max_deletions_per_cycle{100};
    bool database_only{false};
    schedule cleanup_schedule{cron_schedule::daily_at(2, 0)};  // 2:00 AM

    [[nodiscard]] auto retention_for(const std::string& modality) const
        -> std::chrono::days;
};

} // namespace pacs::workflow
```

#### 5.1.5 Archive Task Configuration

```cpp
namespace pacs::workflow {

enum class archive_destination_type {
    local_path,    // Local filesystem path
    network_share, // Network share (SMB/NFS)
    cloud_s3,      // AWS S3 or compatible
    cloud_azure,   // Azure Blob Storage
    tape           // Tape library
};

struct archive_config {
    std::chrono::days archive_after{90};
    archive_destination_type destination_type{archive_destination_type::local_path};
    std::string destination;
    std::optional<std::string> credentials_key;
    bool verify_after_archive{true};
    bool delete_after_archive{false};
    bool compress{true};
    int compression_level{6};
    std::size_t max_archives_per_cycle{50};
    schedule archive_schedule{cron_schedule::daily_at(3, 0)};  // 3:00 AM
};

} // namespace pacs::workflow
```

#### 5.1.6 Verification Task Configuration

```cpp
namespace pacs::workflow {

struct verification_config {
    std::chrono::hours interval{24};
    bool check_checksums{true};
    bool check_db_consistency{true};
    bool check_dicom_structure{false};
    bool repair_on_failure{false};
    std::size_t max_verifications_per_cycle{1000};
    std::string hash_algorithm{"SHA256"};
    schedule verification_schedule{cron_schedule::daily_at(4, 0)};  // 4:00 AM
};

} // namespace pacs::workflow
```

#### 5.1.7 Task Scheduler Service Configuration

```cpp
namespace pacs::workflow {

struct task_scheduler_config {
    bool enabled{true};
    bool auto_start{false};
    std::size_t max_concurrent_tasks{4};
    std::chrono::seconds check_interval{60};
    std::string persistence_path;
    bool restore_on_startup{true};
    std::optional<cleanup_config> cleanup;
    std::optional<archive_config> archive;
    std::optional<verification_config> verification;

    using task_complete_callback =
        std::function<void(const task_id& id,
                          const task_execution_record& record)>;
    task_complete_callback on_task_complete;

    using task_error_callback =
        std::function<void(const task_id& id, const std::string& error)>;
    task_error_callback on_task_error;

    using cycle_complete_callback =
        std::function<void(std::size_t tasks_executed,
                          std::size_t tasks_succeeded,
                          std::size_t tasks_failed)>;
    cycle_complete_callback on_cycle_complete;

    [[nodiscard]] auto is_valid() const noexcept -> bool;
};

} // namespace pacs::workflow
```

#### 5.1.8 Scheduler Statistics

```cpp
namespace pacs::workflow {

struct scheduler_stats {
    std::size_t scheduled_tasks{0};
    std::size_t running_tasks{0};
    std::size_t total_executions{0};
    std::size_t successful_executions{0};
    std::size_t failed_executions{0};
    std::size_t cancelled_executions{0};
    std::chrono::milliseconds avg_execution_time{0};
    std::chrono::milliseconds max_execution_time{0};
    std::chrono::seconds uptime{0};
    std::optional<std::chrono::system_clock::time_point> last_cycle_at;
};

} // namespace pacs::workflow
```

---

## 6. Study Lock Manager

### 6.1 DES-WKF-005: Study Lock Manager

**Traces to:** SRS-WKF-003 (Study Lock Manager), FR-4.8 (Study Modification Control)

**File:** `include/pacs/workflow/study_lock_manager.hpp`

The study_lock_manager provides thread-safe locking mechanisms for DICOM studies to prevent concurrent modifications and ensure data integrity.

#### 6.1.1 Lock Types

```cpp
namespace pacs::workflow {

enum class lock_type {
    exclusive,  // No other access allowed (for modifications)
    shared,     // Read-only access allowed (for read operations)
    migration   // Special lock for migration operations (highest priority)
};

} // namespace pacs::workflow
```

#### 6.1.2 Lock Token and Information

```cpp
namespace pacs::workflow {

struct lock_token {
    std::string token_id;
    std::string study_uid;
    lock_type type{lock_type::exclusive};
    std::chrono::system_clock::time_point acquired_at;
    std::optional<std::chrono::system_clock::time_point> expires_at;

    [[nodiscard]] auto is_valid() const -> bool;
    [[nodiscard]] auto is_expired() const -> bool;
    [[nodiscard]] auto remaining_time() const
        -> std::optional<std::chrono::milliseconds>;
};

struct lock_info {
    std::string study_uid;
    lock_type type{lock_type::exclusive};
    std::string reason;
    std::string holder;
    std::string token_id;
    std::chrono::system_clock::time_point acquired_at;
    std::optional<std::chrono::system_clock::time_point> expires_at;
    std::size_t shared_count{0};

    [[nodiscard]] auto duration() const -> std::chrono::milliseconds;
    [[nodiscard]] auto is_expired() const -> bool;
};

} // namespace pacs::workflow
```

#### 6.1.3 Lock Manager Configuration

```cpp
namespace pacs::workflow {

struct study_lock_manager_config {
    std::chrono::seconds default_timeout{0};          // 0 = no timeout
    std::chrono::milliseconds acquire_wait_timeout{5000};
    std::chrono::seconds cleanup_interval{60};
    bool auto_cleanup{true};
    std::size_t max_shared_locks{100};
    bool allow_force_unlock{true};
};

} // namespace pacs::workflow
```

#### 6.1.4 Class Definition

```cpp
namespace pacs::workflow {

class study_lock_manager {
public:
    // Construction
    study_lock_manager();
    explicit study_lock_manager(const study_lock_manager_config& config);
    ~study_lock_manager();

    // Lock Acquisition
    [[nodiscard]] auto lock(
        const std::string& study_uid,
        const std::string& reason,
        const std::string& holder = "",
        std::chrono::seconds timeout = std::chrono::seconds{0})
        -> kcenon::common::Result<lock_token>;

    [[nodiscard]] auto lock(
        const std::string& study_uid,
        lock_type type,
        const std::string& reason,
        const std::string& holder = "",
        std::chrono::seconds timeout = std::chrono::seconds{0})
        -> kcenon::common::Result<lock_token>;

    [[nodiscard]] auto try_lock(
        const std::string& study_uid,
        lock_type type,
        const std::string& reason,
        const std::string& holder = "",
        std::chrono::seconds timeout = std::chrono::seconds{0})
        -> kcenon::common::Result<lock_token>;

    // Lock Release
    [[nodiscard]] auto unlock(const lock_token& token)
        -> kcenon::common::Result<std::monostate>;

    [[nodiscard]] auto unlock(
        const std::string& study_uid,
        const std::string& holder)
        -> kcenon::common::Result<std::monostate>;

    [[nodiscard]] auto force_unlock(
        const std::string& study_uid,
        const std::string& admin_reason = "")
        -> kcenon::common::Result<std::monostate>;

    auto unlock_all_by_holder(const std::string& holder) -> std::size_t;

    // Lock Status
    [[nodiscard]] auto is_locked(const std::string& study_uid) const -> bool;
    [[nodiscard]] auto is_locked(
        const std::string& study_uid, lock_type type) const -> bool;
    [[nodiscard]] auto get_lock_info(const std::string& study_uid) const
        -> std::optional<lock_info>;
    [[nodiscard]] auto get_lock_info_by_token(const std::string& token_id) const
        -> std::optional<lock_info>;
    [[nodiscard]] auto validate_token(const lock_token& token) const -> bool;
    [[nodiscard]] auto refresh_lock(
        const lock_token& token,
        std::chrono::seconds extension = std::chrono::seconds{0})
        -> kcenon::common::Result<lock_token>;

    // Lock Queries
    [[nodiscard]] auto get_all_locks() const -> std::vector<lock_info>;
    [[nodiscard]] auto get_locks_by_holder(const std::string& holder) const
        -> std::vector<lock_info>;
    [[nodiscard]] auto get_locks_by_type(lock_type type) const
        -> std::vector<lock_info>;
    [[nodiscard]] auto get_expired_locks() const -> std::vector<lock_info>;

    // Maintenance
    auto cleanup_expired_locks() -> std::size_t;
    [[nodiscard]] auto get_stats() const -> lock_manager_stats;
    void reset_stats();
    [[nodiscard]] auto get_config() const -> const study_lock_manager_config&;
    void set_config(const study_lock_manager_config& config);

    // Event Callbacks
    using lock_event_callback = std::function<void(
        const std::string& study_uid, const lock_info& info)>;
    void set_on_lock_acquired(lock_event_callback callback);
    void set_on_lock_released(lock_event_callback callback);
    void set_on_lock_expired(lock_event_callback callback);

private:
    study_lock_manager_config config_;
    std::map<std::string, lock_entry> locks_;
    std::map<std::string, std::string> token_to_study_;
    mutable std::shared_mutex mutex_;
    mutable lock_manager_stats stats_;
    mutable std::atomic<uint64_t> next_token_id_{1};
    lock_event_callback on_lock_acquired_;
    lock_event_callback on_lock_released_;
    lock_event_callback on_lock_expired_;
};

} // namespace pacs::workflow
```

#### 6.1.5 Lock Priority and Behavior

| Lock Type | Priority | Behavior |
|-----------|----------|----------|
| Migration | Highest | Blocks all other locks; used for migration operations |
| Exclusive | Medium | Prevents all other access; for modifications |
| Shared | Lowest | Allows concurrent read access; coexists with other shared locks |

#### 6.1.6 Lock Acquisition Rules

```
When acquiring a lock:

1. Migration Lock:
   - Blocks if any lock exists
   - Once acquired, blocks all other lock attempts

2. Exclusive Lock:
   - Blocks if any lock exists (shared, exclusive, or migration)
   - Once acquired, blocks all other lock attempts

3. Shared Lock:
   - Blocks if exclusive or migration lock exists
   - Can coexist with other shared locks
   - Respects max_shared_locks limit
```

#### 6.1.7 Error Codes

```cpp
namespace pacs::workflow::lock_error {
    constexpr int already_locked = -100;
    constexpr int not_found = -101;
    constexpr int invalid_token = -102;
    constexpr int timeout = -103;
    constexpr int expired = -104;
    constexpr int permission_denied = -105;
    constexpr int invalid_type = -106;
    constexpr int max_shared_exceeded = -107;
    constexpr int upgrade_failed = -108;
}
```

#### 6.1.8 Lock Statistics

```cpp
namespace pacs::workflow {

struct lock_manager_stats {
    std::size_t active_locks{0};
    std::size_t exclusive_locks{0};
    std::size_t shared_locks{0};
    std::size_t migration_locks{0};
    std::size_t total_acquisitions{0};
    std::size_t total_releases{0};
    std::size_t timeout_count{0};
    std::size_t force_unlock_count{0};
    std::chrono::milliseconds avg_lock_duration{0};
    std::chrono::milliseconds max_lock_duration{0};
    std::size_t contention_count{0};
};

} // namespace pacs::workflow
```

---

## 7. Class Diagrams

### 7.1 Workflow Module Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         pacs::workflow                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌─────────────────────────┐    ┌─────────────────────────┐         │
│  │  auto_prefetch_service  │    │    task_scheduler       │         │
│  ├─────────────────────────┤    ├─────────────────────────┤         │
│  │ - database_: &database  │    │ - database_: &database  │         │
│  │ - thread_pool_: shared  │    │ - file_storage_: *ptr   │         │
│  │ - config_: config       │    │ - thread_pool_: shared  │         │
│  │ - request_queue_: queue │    │ - tasks_: map           │         │
│  ├─────────────────────────┤    ├─────────────────────────┤         │
│  │ + start()               │    │ + start()               │         │
│  │ + stop()                │    │ + stop()                │         │
│  │ + prefetch_priors()     │    │ + schedule()            │         │
│  │ + on_worklist_query()   │    │ + cancel_task()         │         │
│  └──────────┬──────────────┘    └──────────┬──────────────┘         │
│             │                              │                         │
│             │ uses                         │ uses                    │
│             ▼                              ▼                         │
│  ┌─────────────────────────┐    ┌─────────────────────────┐         │
│  │  prefetch_service_config│    │ task_scheduler_config   │         │
│  ├─────────────────────────┤    ├─────────────────────────┤         │
│  │ + remote_pacs: vector   │    │ + cleanup: optional     │         │
│  │ + criteria: criteria    │    │ + archive: optional     │         │
│  │ + interval: seconds     │    │ + verification: optional│         │
│  └─────────────────────────┘    └─────────────────────────┘         │
│                                                                      │
│  ┌─────────────────────────┐                                        │
│  │   study_lock_manager    │                                        │
│  ├─────────────────────────┤                                        │
│  │ - config_: config       │                                        │
│  │ - locks_: map           │                                        │
│  │ - mutex_: shared_mutex  │                                        │
│  ├─────────────────────────┤                                        │
│  │ + lock()                │                                        │
│  │ + unlock()              │                                        │
│  │ + is_locked()           │                                        │
│  │ + get_all_locks()       │                                        │
│  └─────────────────────────┘                                        │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.2 Auto Prefetch Service Class Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                       auto_prefetch_service                          │
├─────────────────────────────────────────────────────────────────────┤
│ - database_: storage::index_database&                               │
│ - thread_pool_: std::shared_ptr<thread::thread_pool>                │
│ - config_: prefetch_service_config                                  │
│ - worker_thread_: std::thread                                       │
│ - mutex_: std::mutex                                                │
│ - cv_: std::condition_variable                                      │
│ - stop_requested_: std::atomic<bool>                                │
│ - enabled_: std::atomic<bool>                                       │
│ - request_queue_: std::queue<prefetch_request>                      │
│ - queued_patients_: std::set<std::string>                           │
├─────────────────────────────────────────────────────────────────────┤
│ <<constructor>>                                                      │
│ + auto_prefetch_service(database, config)                           │
│ + auto_prefetch_service(database, thread_pool, config)              │
│ + ~auto_prefetch_service()                                          │
├─────────────────────────────────────────────────────────────────────┤
│ <<lifecycle>>                                                        │
│ + enable(): void                                                     │
│ + start(): void                                                      │
│ + disable(wait: bool): void                                         │
│ + stop(wait: bool): void                                            │
│ + is_enabled(): bool                                                │
│ + is_running(): bool                                                │
├─────────────────────────────────────────────────────────────────────┤
│ <<operations>>                                                       │
│ + prefetch_priors(patient_id, lookback): prefetch_result            │
│ + trigger_for_worklist(items): void                                 │
│ + trigger_cycle(): void                                             │
│ + run_prefetch_cycle(): prefetch_result                             │
│ + on_worklist_query(items): void                                    │
├─────────────────────────────────────────────────────────────────────┤
│ <<statistics>>                                                       │
│ + get_last_result(): optional<prefetch_result>                      │
│ + get_cumulative_stats(): prefetch_result                           │
│ + time_until_next_cycle(): optional<seconds>                        │
│ + cycles_completed(): size_t                                        │
│ + pending_requests(): size_t                                        │
└─────────────────────────────────────────────────────────────────────┘
           △
           │ uses
           │
┌──────────┴──────────────────────────────────────────────────────────┐
│                    prefetch_service_config                           │
├─────────────────────────────────────────────────────────────────────┤
│ + enabled: bool                                                      │
│ + prefetch_interval: std::chrono::seconds                           │
│ + max_concurrent_prefetches: size_t                                 │
│ + remote_pacs: vector<remote_pacs_config>                           │
│ + criteria: prefetch_criteria                                        │
│ + retry_on_failure: bool                                            │
│ + max_retry_attempts: size_t                                        │
├─────────────────────────────────────────────────────────────────────┤
│ + is_valid(): bool                                                   │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.3 Study Lock Manager Class Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                       study_lock_manager                             │
├─────────────────────────────────────────────────────────────────────┤
│ - config_: study_lock_manager_config                                │
│ - locks_: std::map<string, lock_entry>                              │
│ - token_to_study_: std::map<string, string>                         │
│ - mutex_: std::shared_mutex                                         │
│ - stats_: lock_manager_stats                                        │
│ - next_token_id_: std::atomic<uint64_t>                             │
│ - on_lock_acquired_: lock_event_callback                            │
│ - on_lock_released_: lock_event_callback                            │
│ - on_lock_expired_: lock_event_callback                             │
├─────────────────────────────────────────────────────────────────────┤
│ <<constructor>>                                                      │
│ + study_lock_manager()                                              │
│ + study_lock_manager(config)                                        │
│ + ~study_lock_manager()                                             │
├─────────────────────────────────────────────────────────────────────┤
│ <<lock acquisition>>                                                 │
│ + lock(study_uid, reason, holder, timeout): Result<lock_token>      │
│ + lock(study_uid, type, reason, holder, timeout): Result<lock_token>│
│ + try_lock(...): Result<lock_token>                                 │
├─────────────────────────────────────────────────────────────────────┤
│ <<lock release>>                                                     │
│ + unlock(token): Result<monostate>                                  │
│ + unlock(study_uid, holder): Result<monostate>                      │
│ + force_unlock(study_uid, reason): Result<monostate>                │
│ + unlock_all_by_holder(holder): size_t                              │
├─────────────────────────────────────────────────────────────────────┤
│ <<lock status>>                                                      │
│ + is_locked(study_uid): bool                                        │
│ + is_locked(study_uid, type): bool                                  │
│ + get_lock_info(study_uid): optional<lock_info>                     │
│ + validate_token(token): bool                                       │
│ + refresh_lock(token, extension): Result<lock_token>                │
├─────────────────────────────────────────────────────────────────────┤
│ <<maintenance>>                                                      │
│ + cleanup_expired_locks(): size_t                                   │
│ + get_stats(): lock_manager_stats                                   │
│ + reset_stats(): void                                               │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 8. Sequence Diagrams

### 8.1 SEQ-WKF-001: Auto Prefetch Workflow

```
┌─────────┐    ┌─────────────────┐    ┌─────────────┐    ┌───────────┐    ┌──────────┐
│ Worklist│    │auto_prefetch_svc│    │thread_pool  │    │Remote PACS│    │ Database │
│  Query  │    │                 │    │             │    │           │    │          │
└────┬────┘    └────────┬────────┘    └──────┬──────┘    └─────┬─────┘    └────┬─────┘
     │                  │                    │                 │               │
     │ worklist_items   │                    │                 │               │
     │─────────────────>│                    │                 │               │
     │                  │                    │                 │               │
     │                  │ Extract patients   │                 │               │
     │                  │────────┐           │                 │               │
     │                  │        │           │                 │               │
     │                  │<───────┘           │                 │               │
     │                  │                    │                 │               │
     │                  │ queue_request()    │                 │               │
     │                  │────────┐           │                 │               │
     │                  │        │           │                 │               │
     │                  │<───────┘           │                 │               │
     │                  │                    │                 │               │
     │                  │ schedule_task()    │                 │               │
     │                  │───────────────────>│                 │               │
     │                  │                    │                 │               │
     │                  │                    │  C-FIND (priors)│               │
     │                  │                    │────────────────>│               │
     │                  │                    │                 │               │
     │                  │                    │  study_list     │               │
     │                  │                    │<────────────────│               │
     │                  │                    │                 │               │
     │                  │                    │ check_exists()  │               │
     │                  │                    │─────────────────────────────────>
     │                  │                    │                 │               │
     │                  │                    │ exists_result   │               │
     │                  │                    │<─────────────────────────────────
     │                  │                    │                 │               │
     │                  │                    │  C-MOVE (fetch) │               │
     │                  │                    │────────────────>│               │
     │                  │                    │                 │               │
     │                  │                    │  images         │               │
     │                  │                    │<────────────────│               │
     │                  │                    │                 │               │
     │                  │                    │ store_images()  │               │
     │                  │                    │─────────────────────────────────>
     │                  │                    │                 │               │
     │                  │ prefetch_complete  │                 │               │
     │                  │<───────────────────│                 │               │
     │                  │                    │                 │               │
     │                  │ update_stats()     │                 │               │
     │                  │────────┐           │                 │               │
     │                  │        │           │                 │               │
     │                  │<───────┘           │                 │               │
     │                  │                    │                 │               │
```

### 8.2 SEQ-WKF-002: Task Scheduler Execution

```
┌─────────┐    ┌───────────────┐    ┌─────────────┐    ┌─────────────┐
│ Timer   │    │task_scheduler │    │thread_pool  │    │ Task        │
│         │    │               │    │             │    │ Callback    │
└────┬────┘    └──────┬────────┘    └──────┬──────┘    └──────┬──────┘
     │                │                    │                  │
     │ check_interval │                    │                  │
     │───────────────>│                    │                  │
     │                │                    │                  │
     │                │ get_due_tasks()    │                  │
     │                │────────┐           │                  │
     │                │        │           │                  │
     │                │<───────┘           │                  │
     │                │                    │                  │
     │                │ sort_by_priority() │                  │
     │                │────────┐           │                  │
     │                │        │           │                  │
     │                │<───────┘           │                  │
     │                │                    │                  │
     │                │  submit_task()     │                  │
     │                │───────────────────>│                  │
     │                │                    │                  │
     │                │                    │ execute()        │
     │                │                    │─────────────────>│
     │                │                    │                  │
     │                │                    │                  │ (do work)
     │                │                    │                  │────┐
     │                │                    │                  │    │
     │                │                    │                  │<───┘
     │                │                    │                  │
     │                │                    │ result           │
     │                │                    │<─────────────────│
     │                │                    │                  │
     │                │ task_complete      │                  │
     │                │<───────────────────│                  │
     │                │                    │                  │
     │                │ record_execution() │                  │
     │                │────────┐           │                  │
     │                │        │           │                  │
     │                │<───────┘           │                  │
     │                │                    │                  │
     │                │ calc_next_run()    │                  │
     │                │────────┐           │                  │
     │                │        │           │                  │
     │                │<───────┘           │                  │
     │                │                    │                  │
     │                │ notify_callback()  │                  │
     │                │────────┐           │                  │
     │                │        │           │                  │
     │                │<───────┘           │                  │
     │                │                    │                  │
```

### 8.3 SEQ-WKF-003: Study Lock Acquisition

```
┌─────────┐    ┌──────────────────┐    ┌────────────┐
│ Client  │    │study_lock_manager│    │ Lock Entry │
│         │    │                  │    │            │
└────┬────┘    └────────┬─────────┘    └─────┬──────┘
     │                  │                    │
     │ lock(study_uid, │                    │
     │  type, reason)   │                    │
     │─────────────────>│                    │
     │                  │                    │
     │                  │ acquire_lock()     │
     │                  │────────┐           │
     │                  │        │           │
     │                  │<───────┘           │
     │                  │                    │
     │                  │ check_existing()   │
     │                  │───────────────────>│
     │                  │                    │
     │                  │ lock_status        │
     │                  │<───────────────────│
     │                  │                    │
     │                  │ [can_acquire]      │
     │                  │ generate_token()   │
     │                  │────────┐           │
     │                  │        │           │
     │                  │<───────┘           │
     │                  │                    │
     │                  │ create_entry()     │
     │                  │───────────────────>│
     │                  │                    │
     │                  │ record_stats()     │
     │                  │────────┐           │
     │                  │        │           │
     │                  │<───────┘           │
     │                  │                    │
     │                  │ notify_acquired()  │
     │                  │────────┐           │
     │                  │        │           │
     │                  │<───────┘           │
     │                  │                    │
     │  Result<token>   │                    │
     │<─────────────────│                    │
     │                  │                    │
     │ [use resource]   │                    │
     │                  │                    │
     │ unlock(token)    │                    │
     │─────────────────>│                    │
     │                  │                    │
     │                  │ validate_token()   │
     │                  │───────────────────>│
     │                  │                    │
     │                  │ valid             │
     │                  │<───────────────────│
     │                  │                    │
     │                  │ remove_entry()     │
     │                  │───────────────────>│
     │                  │                    │
     │                  │ notify_released()  │
     │                  │────────┐           │
     │                  │        │           │
     │                  │<───────┘           │
     │                  │                    │
     │  Result<ok>      │                    │
     │<─────────────────│                    │
     │                  │                    │
```

---

## 9. Traceability

### 9.1 Design to Requirements Mapping

| Design ID | Design Element | SRS ID(s) | PRD ID(s) |
|-----------|----------------|-----------|-----------|
| DES-WKF-001 | auto_prefetch_service | SRS-WKF-001 | FR-4.6 |
| DES-WKF-002 | prefetch_config | SRS-WKF-001 | FR-4.6 |
| DES-WKF-003 | task_scheduler | SRS-WKF-002 | FR-4.7 |
| DES-WKF-004 | task_scheduler_config | SRS-WKF-002 | FR-4.7 |
| DES-WKF-005 | study_lock_manager | SRS-WKF-003 | FR-4.8 |

### 9.2 Design to Implementation Mapping

| Design ID | Implementation Files |
|-----------|---------------------|
| DES-WKF-001 | `include/pacs/workflow/auto_prefetch_service.hpp`, `src/workflow/auto_prefetch_service.cpp` |
| DES-WKF-002 | `include/pacs/workflow/prefetch_config.hpp` |
| DES-WKF-003 | `include/pacs/workflow/task_scheduler.hpp`, `src/workflow/task_scheduler.cpp` |
| DES-WKF-004 | `include/pacs/workflow/task_scheduler_config.hpp` |
| DES-WKF-005 | `include/pacs/workflow/study_lock_manager.hpp`, `src/workflow/study_lock_manager.cpp` |

### 9.3 Design to Test Mapping

| Design ID | Test Files |
|-----------|-----------|
| DES-WKF-001 | `tests/workflow/auto_prefetch_service_test.cpp` |
| DES-WKF-003 | `tests/workflow/task_scheduler_test.cpp` |
| DES-WKF-005 | `tests/workflow/study_lock_manager_test.cpp` |

### 9.4 Sequence Diagram Mapping

| Sequence ID | Description | Related Design IDs |
|-------------|-------------|-------------------|
| SEQ-WKF-001 | Auto Prefetch Workflow | DES-WKF-001, DES-WKF-002 |
| SEQ-WKF-002 | Task Scheduler Execution | DES-WKF-003, DES-WKF-004 |
| SEQ-WKF-003 | Study Lock Acquisition | DES-WKF-005 |

---

## Appendix A: Usage Examples

### A.1 Auto Prefetch Service Example

```cpp
#include "pacs/workflow/auto_prefetch_service.hpp"

// Configure prefetch service
prefetch_service_config config;
config.prefetch_interval = std::chrono::minutes{5};
config.max_concurrent_prefetches = 4;
config.criteria.lookback_period = std::chrono::days{365};
config.criteria.max_studies_per_patient = 10;

// Add remote PACS
remote_pacs_config remote;
remote.ae_title = "ARCHIVE_PACS";
remote.host = "192.168.1.100";
remote.port = 11112;
config.remote_pacs.push_back(remote);

// Set callbacks
config.on_cycle_complete = [](const prefetch_result& r) {
    std::cout << "Prefetched " << r.studies_prefetched << " studies\n";
};

// Create and start service
auto_prefetch_service service{database, config};
service.start();

// Later: stop service
service.stop();
```

### A.2 Task Scheduler Example

```cpp
#include "pacs/workflow/task_scheduler.hpp"

// Configure scheduler
task_scheduler_config config;
config.max_concurrent_tasks = 4;
config.check_interval = std::chrono::seconds{60};

// Configure cleanup
config.cleanup = cleanup_config{};
config.cleanup->default_retention = std::chrono::days{365};

// Create scheduler
task_scheduler scheduler{database, storage, config};
scheduler.start();

// Schedule custom task
auto task_id = scheduler.schedule(
    "my_task",
    "Custom maintenance",
    cron_schedule::daily_at(1, 30),  // 1:30 AM
    []() -> std::optional<std::string> {
        // Do work...
        return std::nullopt;  // Success
    }
);

// List scheduled tasks
auto tasks = scheduler.list_tasks();
```

### A.3 Study Lock Manager Example

```cpp
#include "pacs/workflow/study_lock_manager.hpp"

// Configure lock manager
study_lock_manager_config config;
config.default_timeout = std::chrono::minutes{30};
config.auto_cleanup = true;

study_lock_manager manager{config};

// Acquire exclusive lock
auto lock_result = manager.lock(
    "1.2.3.4.5",
    "User modification",
    "user@example.com"
);

if (lock_result.is_ok()) {
    auto token = lock_result.value();

    // Perform modifications...

    // Release lock
    manager.unlock(token);
} else {
    // Handle lock failure
    std::cerr << "Lock failed: " << lock_result.error().message << "\n";
}

// Check if locked
if (manager.is_locked("1.2.3.4.5")) {
    auto info = manager.get_lock_info("1.2.3.4.5");
    // Handle locked study...
}
```

---

*Document generated for PACS System v0.3.0*
