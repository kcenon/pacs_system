# SDS - Client Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-02-08
> **Status:** Complete

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-CLI-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#669](https://github.com/kcenon/pacs_system/issues/669) |

### Related Requirements

| Requirement | Description |
|-------------|-------------|
| FR-10.1 | Job Management |
| FR-10.2 | Routing Manager |
| FR-10.3 | Sync Manager |
| FR-10.4 | Remote Node Manager |
| FR-10.5 | Prefetch Manager |

> **Note:** SRS-level requirement IDs for the Client Module (SRS-CLI-xxx) are pending formal SRS update. PRD functional requirements (FR-10.x) are referenced until SRS propagation is complete.

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Job Manager](#2-job-manager)
- [3. Routing Manager](#3-routing-manager)
- [4. Sync Manager](#4-sync-manager)
- [5. Prefetch Manager](#5-prefetch-manager)
- [6. Remote Node Manager](#6-remote-node-manager)
- [7. Class Diagrams](#7-class-diagrams)
- [8. Sequence Diagrams](#8-sequence-diagrams)
- [9. Traceability](#9-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the Client module for the PACS System. The module provides client-side orchestration capabilities for distributed DICOM operations:

- **Job Management**: Centralized asynchronous DICOM operation management with priority queues
- **Routing Management**: Rule-based automatic forwarding of DICOM images
- **Synchronization**: Bidirectional data synchronization with conflict resolution
- **Prefetching**: Proactive data loading based on worklist and prior study rules
- **Remote Node Management**: Connection pooling, health monitoring, and node lifecycle

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| Job Manager | 1 header, 1 source | DES-CLI-001 |
| Routing Manager | 1 header, 1 source | DES-CLI-002 |
| Sync Manager | 1 header, 1 source | DES-CLI-003 |
| Prefetch Manager | 1 header, 1 source | DES-CLI-004 |
| Remote Node Manager | 1 header, 1 source | DES-CLI-005 |

### 1.3 Design Identifier Convention

```
DES-CLI-<NUMBER>

Where:
- DES: Design Specification prefix
- CLI: Client module identifier
- NUMBER: 3-digit sequential number (001-005)
```

### 1.4 Integration with kcenon Ecosystem

| System | Integration Point |
|--------|-------------------|
| thread_system | Worker thread pools for job execution |
| logger_system | Audit logging for all client operations |
| monitoring_system | Metrics for job, routing, sync, and prefetch operations |
| common_system | Result<T> for error handling |

### 1.5 Component Dependencies

```
┌─────────────────────────────────────────────────────────────────┐
│                    Client Module Dependencies                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────┐                                            │
│  │ Remote Node Mgr │◄──────────────────────────────────────────┐│
│  │  DES-CLI-005    │                                           ││
│  └────────┬────────┘                                           ││
│           │                                                     ││
│           ▼                                                     ││
│  ┌─────────────────┐     ┌─────────────────┐                   ││
│  │  Job Manager    │◄────│ Routing Manager  │                   ││
│  │  DES-CLI-001    │     │  DES-CLI-002     │                   ││
│  └────────┬────────┘     └─────────────────┘                   ││
│           │                                                     ││
│     ┌─────┴──────┐                                              ││
│     │            │                                              ││
│     ▼            ▼                                              ││
│  ┌──────────┐ ┌──────────────┐                                  ││
│  │Sync Mgr  │ │Prefetch Mgr  │──────────────────────────────────┘│
│  │DES-CLI-003│ │DES-CLI-004   │                                   │
│  └──────────┘ └──────────────┘                                   │
│                                                                   │
│  Storage Dependencies:                                            │
│  ├─ job_repository (DES-STOR-012)                                │
│  ├─ routing_repository (DES-STOR-014)                            │
│  ├─ sync_repository (DES-STOR-016)                               │
│  ├─ prefetch_repository (DES-STOR-015)                           │
│  └─ node_repository (DES-STOR-013)                               │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

---

## 2. Job Manager

### 2.1 DES-CLI-001: Job Manager

**Traces to:** FR-10.1 (Job Management)

**Files:**
- `include/pacs/client/job_manager.hpp`
- `src/client/job_manager.cpp`

The `job_manager` class provides centralized management of asynchronous DICOM operations with a priority queue, worker thread pool, and progress tracking. It supports multiple job types (retrieve, store, query, sync, prefetch) with configurable concurrency and automatic retry.

#### 2.1.1 Design Decisions

| Decision | Rationale | Alternatives |
|----------|-----------|--------------|
| Priority queue with FIFO within tiers | Ensures urgent jobs (e.g., STAT exams) are processed first | Simple FIFO (no priority), round-robin |
| Pimpl pattern | ABI stability, reduced compile-time dependencies | Direct member variables |
| UUID v4 for job IDs | Globally unique, no coordination needed | Sequential integers (collision risk) |
| Job persistence via repository | Recovery after crash, audit trail | In-memory only (data loss risk) |

#### 2.1.2 Configuration

```cpp
struct job_manager_config {
    size_t worker_count = 4;        ///< Number of worker threads
    size_t max_queue_size = 1000;   ///< Maximum queued jobs
    bool auto_retry = true;         ///< Enable automatic retry on failure
    std::chrono::seconds retry_delay{60};  ///< Delay between retries
    std::chrono::hours job_timeout{1};     ///< Maximum job execution time
};
```

#### 2.1.3 Class Design

```cpp
namespace pacs::client {

class job_manager {
public:
    // Construction
    explicit job_manager(
        std::shared_ptr<storage::job_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<di::ILogger> logger = nullptr);

    explicit job_manager(
        const job_manager_config& config,
        std::shared_ptr<storage::job_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<di::ILogger> logger = nullptr);

    // Job Creation
    [[nodiscard]] core::result<std::string> create_retrieve_job(/* params */);
    [[nodiscard]] core::result<std::string> create_store_job(/* params */);
    [[nodiscard]] core::result<std::string> create_query_job(/* params */);
    [[nodiscard]] core::result<std::string> create_sync_job(/* params */);
    [[nodiscard]] core::result<std::string> create_prefetch_job(/* params */);

    // Job Control
    [[nodiscard]] core::result<void> start_job(std::string_view job_id);
    [[nodiscard]] core::result<void> pause_job(std::string_view job_id);
    [[nodiscard]] core::result<void> resume_job(std::string_view job_id);
    [[nodiscard]] core::result<void> cancel_job(std::string_view job_id);
    [[nodiscard]] core::result<void> retry_job(std::string_view job_id);
    [[nodiscard]] core::result<void> delete_job(std::string_view job_id);

    // Job Queries
    [[nodiscard]] std::optional<job_record> get_job(std::string_view id) const;
    [[nodiscard]] std::vector<job_record> list_jobs(/* filters */) const;
    [[nodiscard]] std::vector<job_record> list_jobs_by_node(std::string_view node_id) const;

    // Progress Monitoring
    [[nodiscard]] std::optional<job_progress> get_progress(std::string_view id) const;
    void set_progress_callback(job_progress_callback cb);
    void set_completion_callback(job_completion_callback cb);
    [[nodiscard]] std::future<job_record> wait_for_completion(std::string_view id);

    // Worker Lifecycle
    void start_workers();
    void stop_workers();
    [[nodiscard]] bool is_running() const noexcept;

    // Statistics
    [[nodiscard]] size_t active_jobs() const;
    [[nodiscard]] size_t pending_jobs() const;
    [[nodiscard]] size_t completed_jobs_today() const;
    [[nodiscard]] size_t failed_jobs_today() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace pacs::client
```

#### 2.1.4 Thread Safety

| Operation | Thread Safety | Mechanism |
|-----------|--------------|-----------|
| Job creation | Thread-safe | `shared_mutex` on cache |
| Job control | Thread-safe | `shared_mutex` + `condition_variable` |
| Progress queries | Thread-safe | `shared_mutex` (read lock) |
| Callback registration | Thread-safe | `shared_mutex` on callbacks |

#### 2.1.5 Job Lifecycle State Machine

```
                    ┌─────────┐
           create   │ pending │
          ─────────►│         │
                    └────┬────┘
                         │ start
                         ▼
                    ┌─────────┐     pause    ┌─────────┐
                    │ running │─────────────►│ paused  │
                    │         │◄─────────────│         │
                    └────┬────┘    resume    └─────────┘
                    │    │
              ┌─────┘    └──────┐
              ▼                 ▼
        ┌───────────┐    ┌───────────┐
        │ completed │    │  failed   │
        └───────────┘    └─────┬─────┘
                               │ retry
                               ▼
                          ┌─────────┐
                          │ pending │
                          └─────────┘
```

---

## 3. Routing Manager

### 3.1 DES-CLI-002: Routing Manager

**Traces to:** FR-10.2 (Routing Manager)

**Files:**
- `include/pacs/client/routing_manager.hpp`
- `src/client/routing_manager.cpp`

The `routing_manager` class provides rule-based automatic forwarding of DICOM images to configured destinations. Rules support wildcard pattern matching on DICOM fields with priority-based evaluation.

#### 3.1.1 Design Decisions

| Decision | Rationale | Alternatives |
|----------|-----------|--------------|
| AND logic for conditions within a rule | Most natural for multi-field matching | OR logic (too broad) |
| Wildcard pattern matching (`*`, `?`) | Flexible without regex complexity | Regex (overkill), exact match (too rigid) |
| Priority-based rule ordering | Predictable evaluation order | Unordered (non-deterministic) |
| Storage SCP integration | Automatic post-store routing | Manual trigger only |

#### 3.1.2 Configuration

```cpp
struct routing_manager_config {
    bool enabled = true;                    ///< Global enable/disable
    size_t max_rules = 100;                 ///< Maximum rules allowed
    bool log_matches = true;                ///< Log rule matches
    std::chrono::seconds evaluation_timeout{5}; ///< Max evaluation time
};
```

#### 3.1.3 Class Design

```cpp
namespace pacs::client {

class routing_manager {
public:
    // Construction
    explicit routing_manager(
        std::shared_ptr<storage::routing_repository> repo,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<di::ILogger> logger = nullptr);

    // Rule CRUD
    [[nodiscard]] core::result<std::string> add_rule(const routing_rule& rule);
    [[nodiscard]] core::result<void> update_rule(const routing_rule& rule);
    [[nodiscard]] core::result<void> remove_rule(std::string_view rule_id);
    [[nodiscard]] std::optional<routing_rule> get_rule(std::string_view id) const;
    [[nodiscard]] std::vector<routing_rule> list_rules() const;
    [[nodiscard]] std::vector<routing_rule> list_enabled_rules() const;

    // Rule Evaluation
    [[nodiscard]] std::vector<routing_action> evaluate(
        const core::dicom_dataset& dataset) const;
    [[nodiscard]] core::result<void> route(
        const core::dicom_dataset& dataset);
    [[nodiscard]] core::result<void> route(
        std::string_view sop_instance_uid);

    // Storage SCP Integration
    void attach_to_storage_scp(services::storage_scp& scp);
    void detach_from_storage_scp();

    // Global Control
    void enable();
    void disable();
    [[nodiscard]] bool is_enabled() const noexcept;

    // Testing
    [[nodiscard]] std::vector<routing_action> test_rules(
        const core::dicom_dataset& dataset) const;

    // Statistics
    [[nodiscard]] routing_statistics get_statistics() const;
    void reset_statistics();

private:
    routing_manager_config config_;
    std::shared_ptr<storage::routing_repository> repo_;
    std::shared_ptr<job_manager> job_manager_;
    std::shared_ptr<di::ILogger> logger_;
    std::vector<routing_rule> rules_;
    mutable std::shared_mutex rules_mutex_;
    std::atomic<bool> enabled_{true};
};

} // namespace pacs::client
```

#### 3.1.4 Matching Fields

| DICOM Tag | Field Name | Wildcard Support |
|-----------|------------|------------------|
| (0008,0060) | Modality | Yes |
| (0008,1010) | Station AE Title | Yes |
| (0008,0080) | Institution Name | Yes |
| (0008,1040) | Department | Yes |
| (0008,0090) | Referring Physician | Yes |
| (0008,1030) | Study Description | Yes |
| (0008,103E) | Series Description | Yes |
| (0018,0015) | Body Part Examined | Yes |
| (0010,0020) | Patient ID | Yes |
| (0008,0016) | SOP Class UID | Yes |

---

## 4. Sync Manager

### 4.1 DES-CLI-003: Sync Manager

**Traces to:** FR-10.3 (Sync Manager)

**Files:**
- `include/pacs/client/sync_manager.hpp`
- `src/client/sync_manager.cpp`

The `sync_manager` class provides bidirectional synchronization of DICOM data between local PACS and remote nodes with conflict detection, resolution strategies, and scheduled sync support.

#### 4.1.1 Design Decisions

| Decision | Rationale | Alternatives |
|----------|-----------|--------------|
| Incremental sync (timestamp-based) | Efficient for large datasets | Full sync only (slow) |
| Three conflict resolution strategies | Covers common use cases | Manual-only (blocks workflow) |
| Cron-based scheduling | Standard, flexible scheduling | Fixed intervals (less flexible) |
| Async operations with futures | Non-blocking, composable | Sync-only (blocks caller) |

#### 4.1.2 Sync Directions

| Direction | Description |
|-----------|-------------|
| `pull` | Retrieve data from remote to local |
| `push` | Send data from local to remote |
| `bidirectional` | Two-way synchronization with conflict detection |

#### 4.1.3 Conflict Resolution Strategies

| Strategy | Description | Use Case |
|----------|-------------|----------|
| `prefer_local` | Local version wins | Primary PACS |
| `prefer_remote` | Remote version wins | Mirror/backup |
| `prefer_newer` | Most recently modified wins | Distributed editing |

#### 4.1.4 Class Design

```cpp
namespace pacs::client {

class sync_manager {
public:
    // Construction
    explicit sync_manager(
        std::shared_ptr<storage::sync_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<services::query_scu> query_scu,
        std::shared_ptr<di::ILogger> logger = nullptr);

    // Configuration CRUD
    [[nodiscard]] core::result<std::string> add_config(const sync_config& config);
    [[nodiscard]] core::result<void> update_config(const sync_config& config);
    [[nodiscard]] core::result<void> remove_config(std::string_view config_id);

    // Sync Operations
    [[nodiscard]] core::result<sync_result> sync_now(std::string_view config_id);
    [[nodiscard]] core::result<sync_result> full_sync(std::string_view config_id);
    [[nodiscard]] core::result<sync_result> incremental_sync(std::string_view config_id);
    [[nodiscard]] std::future<sync_result> wait_for_sync(std::string_view config_id);

    // Conflict Management
    [[nodiscard]] std::vector<sync_conflict> get_conflicts() const;
    [[nodiscard]] core::result<void> resolve_conflict(
        std::string_view study_uid, conflict_resolution resolution);
    [[nodiscard]] core::result<void> resolve_all_conflicts(
        std::string_view config_id, conflict_resolution resolution);

    // Scheduler
    void start_scheduler();
    void stop_scheduler();
    [[nodiscard]] bool is_scheduler_running() const noexcept;

    // Status and Statistics
    [[nodiscard]] bool is_syncing(std::string_view config_id) const;
    [[nodiscard]] sync_statistics get_statistics() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace pacs::client
```

---

## 5. Prefetch Manager

### 5.1 DES-CLI-004: Prefetch Manager

**Traces to:** FR-10.5 (Prefetch Manager)

**Files:**
- `include/pacs/client/prefetch_manager.hpp`
- `src/client/prefetch_manager.cpp`

The `prefetch_manager` class proactively loads DICOM data based on worklist entries, scheduled exams, or prior study rules. It integrates with the worklist service for continuous monitoring and supports cron-based scheduled prefetching.

#### 5.1.1 Design Decisions

| Decision | Rationale | Alternatives |
|----------|-----------|--------------|
| Multiple trigger types | Different clinical workflows need different triggers | Single trigger type |
| Request deduplication | Avoid redundant network traffic | No dedup (wasted resources) |
| Worklist polling | Continuous awareness of scheduled exams | Event-driven only (requires HL7) |
| Modality-aware lookback | Only fetch relevant prior studies | Fetch all priors (excessive) |

#### 5.1.2 Prefetch Trigger Types

| Trigger | Description | Use Case |
|---------|-------------|----------|
| `worklist_match` | Triggered by MWL entry | Prefetch priors for scheduled exams |
| `prior_studies` | Fetch prior studies for patient | Comparative reading |
| `scheduled_exam` | Based on scheduled procedure time | Pre-exam preparation |
| `manual` | Explicit user request | On-demand loading |

#### 5.1.3 Class Design

```cpp
namespace pacs::client {

class prefetch_manager {
public:
    // Construction
    explicit prefetch_manager(
        std::shared_ptr<storage::prefetch_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<services::worklist_scu> worklist_scu = nullptr,
        std::shared_ptr<di::ILogger> logger = nullptr);

    // Rule Management
    [[nodiscard]] core::result<std::string> add_rule(const prefetch_rule& rule);
    [[nodiscard]] core::result<void> update_rule(const prefetch_rule& rule);
    [[nodiscard]] core::result<void> remove_rule(std::string_view rule_id);
    [[nodiscard]] std::vector<prefetch_rule> list_rules() const;

    // Worklist-Driven Prefetch
    [[nodiscard]] core::result<size_t> process_worklist();
    [[nodiscard]] std::future<size_t> process_worklist_async();

    // Prior Study Prefetch
    [[nodiscard]] core::result<size_t> prefetch_priors(
        std::string_view patient_id, std::string_view modality);

    // Manual Prefetch
    [[nodiscard]] core::result<void> prefetch_study(
        std::string_view study_uid, std::string_view node_id);

    // Scheduler
    void start_scheduler();
    void stop_scheduler();
    [[nodiscard]] bool is_scheduler_running() const noexcept;

    // Worklist Monitor
    void start_worklist_monitor();
    void stop_worklist_monitor();
    [[nodiscard]] bool is_worklist_monitor_running() const noexcept;

    // Statistics
    [[nodiscard]] size_t pending_prefetches() const;
    [[nodiscard]] size_t completed_today() const;
    [[nodiscard]] size_t failed_today() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace pacs::client
```

---

## 6. Remote Node Manager

### 6.1 DES-CLI-005: Remote Node Manager

**Traces to:** FR-10.4 (Remote Node Manager)

**Files:**
- `include/pacs/client/remote_node_manager.hpp`
- `src/client/remote_node_manager.cpp`

The `remote_node_manager` class provides centralized management of remote PACS node connections with health monitoring, connection pooling, and status change notifications.

#### 6.1.1 Design Decisions

| Decision | Rationale | Alternatives |
|----------|-----------|--------------|
| Connection pooling | Reuse associations, reduce overhead | Per-request connections (slow) |
| Periodic health checks | Early detection of node failures | On-demand only (late detection) |
| C-ECHO for verification | Standard DICOM connectivity test | TCP-only (no DICOM verification) |
| Status callbacks | Real-time notification of changes | Polling (latency, overhead) |

#### 6.1.2 Node Status

| Status | Description |
|--------|-------------|
| `unknown` | Not yet verified |
| `online` | Connected and responsive |
| `offline` | Connection failed |
| `error` | Connection error with details |

#### 6.1.3 Configuration

```cpp
struct node_manager_config {
    std::chrono::seconds health_check_interval{300}; ///< 5 min default
    std::chrono::seconds connection_timeout{30};      ///< Connection timeout
    size_t max_pool_size = 5;                         ///< Max associations per node
    std::chrono::seconds pool_idle_timeout{300};      ///< Idle association timeout
};
```

#### 6.1.4 Class Design

```cpp
namespace pacs::client {

class remote_node_manager {
public:
    // Construction
    explicit remote_node_manager(
        std::shared_ptr<storage::node_repository> repo,
        node_manager_config config = {},
        std::shared_ptr<di::ILogger> logger = nullptr);

    // Node CRUD
    [[nodiscard]] core::result<std::string> add_node(const remote_node& node);
    [[nodiscard]] core::result<void> update_node(const remote_node& node);
    [[nodiscard]] core::result<void> remove_node(std::string_view node_id);
    [[nodiscard]] std::optional<remote_node> get_node(std::string_view id) const;
    [[nodiscard]] std::vector<remote_node> list_nodes() const;
    [[nodiscard]] std::vector<remote_node> list_nodes_by_status(node_status s) const;

    // Connection Verification
    [[nodiscard]] core::result<bool> verify_node(std::string_view node_id);
    [[nodiscard]] std::future<bool> verify_node_async(std::string_view node_id);
    void verify_all_nodes_async();

    // Association Pool
    [[nodiscard]] core::result<pooled_association> acquire_association(
        std::string_view node_id);
    void release_association(std::string_view node_id, pooled_association assoc);

    // Health Check Scheduler
    void start_health_check();
    void stop_health_check();
    [[nodiscard]] bool is_health_check_running() const noexcept;

    // Status Monitoring
    [[nodiscard]] node_status get_status(std::string_view node_id) const;
    void set_status_callback(node_status_callback cb);

    // Statistics
    [[nodiscard]] node_statistics get_statistics(std::string_view node_id) const;
    void reset_statistics(std::string_view node_id);

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace pacs::client
```

---

## 7. Class Diagrams

### 7.1 Client Module Class Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Client Module Classes                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌───────────────────┐                                                  │
│  │ remote_node_manager│ ←─── Foundation (no client dependencies)       │
│  │  - node_cache      │                                                 │
│  │  - connection_pool  │                                                │
│  │  - health_checker   │                                                │
│  └─────────┬──────────┘                                                 │
│            │ depends on                                                  │
│            ▼                                                             │
│  ┌───────────────────┐                                                  │
│  │   job_manager     │ ←─── Core orchestrator                          │
│  │  - priority_queue  │                                                 │
│  │  - worker_threads  │                                                │
│  │  - job_cache       │                                                 │
│  └──────┬──────┬─────┘                                                 │
│         │      │ depends on                                             │
│    ┌────┘      └────┐                                                  │
│    ▼                ▼                                                   │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                   │
│  │routing_mgr   │ │ sync_manager │ │prefetch_mgr  │                   │
│  │ - rules      │ │ - configs    │ │ - rules      │                   │
│  │ - statistics  │ │ - conflicts  │ │ - scheduler  │                   │
│  │ - scp_hook    │ │ - scheduler  │ │ - mwl_monitor│                   │
│  └──────────────┘ └──────────────┘ └──────────────┘                   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Sequence Diagrams

### 8.1 SEQ-CLI-001: Automatic Routing Flow

```
┌──────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────┐
│StorageSCP│  │RoutingMgr │  │ JobManager│  │RemoteNode │  │  Repo  │
└────┬─────┘  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘  └───┬────┘
     │              │              │              │             │
     │ post_store   │              │              │             │
     │─────────────►│              │              │             │
     │              │ evaluate()   │              │             │
     │              │──────┐       │              │             │
     │              │      │ match │              │             │
     │              │◄─────┘ rules │              │             │
     │              │              │              │             │
     │              │ create_store │              │             │
     │              │────────────►│              │             │
     │              │              │ save(job)    │             │
     │              │              │─────────────────────────►│
     │              │              │              │             │
     │              │              │ worker picks │             │
     │              │              │──────┐       │             │
     │              │              │      │       │             │
     │              │              │ acquire_assoc│             │
     │              │              │─────────────►│             │
     │              │              │              │ C-STORE     │
     │              │              │              │────►        │
     │              │              │              │             │
     │              │              │ update status│             │
     │              │              │─────────────────────────►│
     │              │              │              │             │
```

### 8.2 SEQ-CLI-002: Bidirectional Sync Flow

```
┌──────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────┐
│  Client  │  │ SyncMgr   │  │  QuerySCU │  │ JobManager│  │  Repo  │
└────┬─────┘  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘  └───┬────┘
     │              │              │              │             │
     │ sync_now()   │              │              │             │
     │─────────────►│              │              │             │
     │              │ C-FIND local │              │             │
     │              │──────┐       │              │             │
     │              │◄─────┘       │              │             │
     │              │              │              │             │
     │              │ C-FIND remote│              │             │
     │              │─────────────►│              │             │
     │              │◄─────────────│              │             │
     │              │              │              │             │
     │              │ compare()    │              │             │
     │              │──────┐       │              │             │
     │              │◄─────┘       │              │             │
     │              │              │              │             │
     │              │[if conflicts]│              │             │
     │              │ save_conflict│              │             │
     │              │──────────────────────────────────────────►│
     │              │              │              │             │
     │              │ create_sync  │              │             │
     │              │─────────────────────────────►│            │
     │              │              │              │             │
     │◄─────────────│ sync_result  │              │             │
     │              │              │              │             │
```

---

## 9. Traceability

### 9.1 Design to Requirements

| DES ID | Component | PRD Req | SRS Req (Pending) | Status |
|--------|-----------|---------|-------------------|--------|
| DES-CLI-001 | `job_manager` | FR-10.1 | SRS-CLI-001 (pending) | Implemented |
| DES-CLI-002 | `routing_manager` | FR-10.2 | SRS-CLI-002 (pending) | Implemented |
| DES-CLI-003 | `sync_manager` | FR-10.3 | SRS-CLI-003 (pending) | Implemented |
| DES-CLI-004 | `prefetch_manager` | FR-10.5 | SRS-CLI-004 (pending) | Implemented |
| DES-CLI-005 | `remote_node_manager` | FR-10.4 | SRS-CLI-005 (pending) | Implemented |

### 9.2 Design to Implementation

| DES ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-CLI-001 | `job_manager` | `include/pacs/client/job_manager.hpp` | `src/client/job_manager.cpp` |
| DES-CLI-002 | `routing_manager` | `include/pacs/client/routing_manager.hpp` | `src/client/routing_manager.cpp` |
| DES-CLI-003 | `sync_manager` | `include/pacs/client/sync_manager.hpp` | `src/client/sync_manager.cpp` |
| DES-CLI-004 | `prefetch_manager` | `include/pacs/client/prefetch_manager.hpp` | `src/client/prefetch_manager.cpp` |
| DES-CLI-005 | `remote_node_manager` | `include/pacs/client/remote_node_manager.hpp` | `src/client/remote_node_manager.cpp` |

### 9.3 Design to Test

| DES ID | Design Element | Test File |
|--------|---------------|-----------|
| DES-CLI-001 | `job_manager` | `tests/client/job_manager_test.cpp` |
| DES-CLI-002 | `routing_manager` | `tests/client/routing_manager_test.cpp` |
| DES-CLI-003 | `sync_manager` | `tests/client/sync_manager_test.cpp` |
| DES-CLI-004 | `prefetch_manager` | `tests/client/prefetch_manager_test.cpp` |
| DES-CLI-005 | `remote_node_manager` | `tests/client/remote_node_manager_test.cpp` |

---

## References

1. DICOM Standard PS3.4 (Service Class Specifications)
2. [PRD - Product Requirements Document](PRD.md)
3. [SRS - Software Requirements Specification](SRS.md)
4. [SDS_WORKFLOW.md](SDS_WORKFLOW.md) - Related workflow module
5. [SDS_WEB_API.md](SDS_WEB_API.md) - REST endpoints for client operations

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-02-08 | kcenon | Initial release: DES-CLI-001 to DES-CLI-005 |

---

*Document Version: 1.0.0*
*Created: 2026-02-08*
*Author: kcenon@naver.com*
