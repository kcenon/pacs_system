# SDS - Monitoring Collectors Module

> **Version:** 2.1.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-05
> **Status:** Complete

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-MON-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#490](https://github.com/kcenon/pacs_system/issues/490), [#481](https://github.com/kcenon/pacs_system/issues/481), [#468](https://github.com/kcenon/pacs_system/issues/468), [#310](https://github.com/kcenon/pacs_system/issues/310) |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Collector Plugin Interface](#2-collector-plugin-interface)
- [3. Association Collector (DES-MON-004)](#3-association-collector-des-mon-004)
- [4. Service Collector (DES-MON-005)](#4-service-collector-des-mon-005)
- [5. Storage Collector (DES-MON-006)](#5-storage-collector-des-mon-006)
- [6. CRTP-Based Unified Collector (DES-MON-007)](#6-crtp-based-unified-collector-des-mon-007)
- [7. Metrics Catalog](#7-metrics-catalog)
- [8. Integration with pacs_metrics](#8-integration-with-pacs_metrics)
- [9. Usage Examples](#9-usage-examples)
- [10. Traceability](#10-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the Monitoring Collectors plugin architecture for the PACS system. The collectors provide specialized metrics extraction from the centralized `pacs_metrics` system.

The module provides:

- **Collector Plugin Pattern**: Standardized interface for metrics collection
- **Association Collector**: DICOM association lifecycle metrics
- **Service Collector**: DIMSE operation metrics for all service types
- **Storage Collector**: Data transfer and object pool metrics

### 1.2 Scope

| Component | File | Design ID |
|-----------|------|-----------|
| Association Collector | `collectors/dicom_association_collector.hpp` | DES-MON-004 |
| Service Collector | `collectors/dicom_service_collector.hpp` | DES-MON-005 |
| Storage Collector | `collectors/dicom_storage_collector.hpp` | DES-MON-006 |
| CRTP Collector Base | `collectors/dicom_collector_base.hpp` | DES-MON-007a |
| Unified Metrics Collector | `collectors/dicom_metrics_collector.hpp` | DES-MON-007b |

### 1.3 Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Monitoring Architecture                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │                         pacs_metrics                                │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │ │
│  │  │  operation   │  │    data      │  │ association  │              │ │
│  │  │  counters    │  │  transfer    │  │  counters    │              │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘              │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │ │
│  │  │   element    │  │   dataset    │  │  pdu_buffer  │              │ │
│  │  │    pool      │  │    pool      │  │    pool      │              │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘              │ │
│  └─────────────────────────────┬──────────────────────────────────────┘ │
│                                │                                         │
│                                │ collect()                               │
│                                ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │                        PACS Collectors                              │ │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  │ │
│  │  │   Association    │  │     Service      │  │     Storage      │  │ │
│  │  │   DES-MON-004    │  │   DES-MON-005    │  │   DES-MON-006    │  │ │
│  │  │                  │  │                  │  │                  │  │ │
│  │  │ • active count   │  │ • DIMSE ops      │  │ • bytes sent     │  │ │
│  │  │ • peak active    │  │ • durations      │  │ • bytes received │  │ │
│  │  │ • total/rejected │  │ • success/fail   │  │ • pool metrics   │  │ │
│  │  │ • success rate   │  │ • 11 operations  │  │ • throughput     │  │ │
│  │  └──────────────────┘  └──────────────────┘  └──────────────────┘  │ │
│  │                                                                    │ │
│  │  ┌──────────────────────────────────────────────────────────────┐  │ │
│  │  │                 Unified CRTP Collector                        │  │ │
│  │  │                    DES-MON-007                                │  │ │
│  │  │                                                                │  │ │
│  │  │  • CRTP base template (zero-overhead polymorphism)            │  │ │
│  │  │  • All metrics in single collection pass                      │  │ │
│  │  │  • Configurable metric categories                             │  │ │
│  │  │  • Snapshot support for efficient access                      │  │ │
│  │  └──────────────────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Collector Plugin Interface

### 2.1 Common Interface Pattern

All collectors implement a consistent interface pattern:

```cpp
namespace pacs::monitoring {

class collector_interface {
public:
    // Lifecycle
    [[nodiscard]] bool initialize(
        const std::unordered_map<std::string, std::string>& config);

    // Collection
    [[nodiscard]] std::vector<metric_type> collect();

    // Metadata
    [[nodiscard]] std::string get_name() const;
    [[nodiscard]] std::vector<std::string> get_metric_types() const;

    // Health
    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] std::unordered_map<std::string, double> get_statistics() const;

    // Configuration
    void set_ae_title(std::string ae_title);
    [[nodiscard]] std::string get_ae_title() const;
};

} // namespace pacs::monitoring
```

### 2.2 Metric Structure

Each collector uses a specialized metric structure:

```cpp
struct base_metric {
    std::string name;                                    // Metric name
    double value;                                        // Numeric value
    std::string type;                                    // "gauge" or "counter"
    std::chrono::system_clock::time_point timestamp;    // Collection time
    std::unordered_map<std::string, std::string> labels; // Metric labels
};
```

### 2.3 Thread Safety

All collectors are designed with thread safety in mind:

- All public methods are thread-safe
- Internal state protected by mutex where necessary
- Statistics counters use atomic operations
- Non-copyable, non-movable to prevent accidental sharing

---

## 3. Association Collector (DES-MON-004)

### 3.1 Overview

**Design ID:** DES-MON-004

**Traces to:** SRS-INT-006 (monitoring_system Integration)

**File:** `include/pacs/monitoring/collectors/dicom_association_collector.hpp`

The Association Collector gathers metrics related to DICOM association lifecycle:
- Active association count
- Total associations established
- Association success/failure rates
- Peak active associations

### 3.2 Interface

```cpp
namespace pacs::monitoring {

struct association_metric {
    std::string name;
    double value;
    std::string type;  // "gauge" or "counter"
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;
};

class dicom_association_collector {
public:
    explicit dicom_association_collector(std::string ae_title = "PACS_SCP");
    ~dicom_association_collector() = default;

    // Non-copyable, non-movable
    dicom_association_collector(const dicom_association_collector&) = delete;
    dicom_association_collector& operator=(const dicom_association_collector&) = delete;

    // Collector interface
    [[nodiscard]] bool initialize(
        const std::unordered_map<std::string, std::string>& config);
    [[nodiscard]] std::vector<association_metric> collect();
    [[nodiscard]] std::string get_name() const;
    [[nodiscard]] std::vector<std::string> get_metric_types() const;
    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] std::unordered_map<std::string, double> get_statistics() const;

    // Configuration
    void set_ae_title(std::string ae_title);
    [[nodiscard]] std::string get_ae_title() const;
};

} // namespace pacs::monitoring
```

### 3.3 Exposed Metrics

| Metric Name | Type | Labels | Description |
|-------------|------|--------|-------------|
| `dicom_associations_active` | gauge | ae | Current active associations |
| `dicom_associations_peak_active` | gauge | ae | Maximum concurrent associations observed |
| `dicom_associations_total` | counter | ae | Total associations established |
| `dicom_associations_rejected_total` | counter | ae | Total associations rejected |
| `dicom_associations_aborted_total` | counter | ae | Total associations aborted |
| `dicom_associations_success_rate` | gauge | ae | Success rate (0.0 to 1.0) |

### 3.4 Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                Association Collector Data Flow                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  pacs_metrics::global_metrics()                                     │
│           │                                                          │
│           ▼                                                          │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                 association_counters                          │   │
│  │  • total_established  ─────────────► associations_total       │   │
│  │  • total_rejected     ─────────────► associations_rejected    │   │
│  │  • total_aborted      ─────────────► associations_aborted     │   │
│  │  • current_active     ─────────────► associations_active      │   │
│  │  • peak_active        ─────────────► associations_peak_active │   │
│  └──────────────────────────────────────────────────────────────┘   │
│           │                                                          │
│           │ calculate                                                │
│           ▼                                                          │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  success_rate = total / (total + rejected)                    │   │
│  │               ──────────────────► associations_success_rate   │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 4. Service Collector (DES-MON-005)

### 4.1 Overview

**Design ID:** DES-MON-005

**Traces to:** SRS-INT-006 (monitoring_system Integration)

**File:** `include/pacs/monitoring/collectors/dicom_service_collector.hpp`

The Service Collector gathers metrics for all DIMSE operations:

| Category | Operations |
|----------|------------|
| Composite Services | C-ECHO, C-STORE, C-FIND, C-MOVE, C-GET |
| Normalized Services | N-CREATE, N-SET, N-GET, N-ACTION, N-EVENT, N-DELETE |

### 4.2 Interface

```cpp
namespace pacs::monitoring {

struct service_metric {
    std::string name;
    double value;
    std::string type;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;
};

class dicom_service_collector {
public:
    explicit dicom_service_collector(std::string ae_title = "PACS_SCP");
    ~dicom_service_collector() = default;

    // Collector interface
    [[nodiscard]] bool initialize(
        const std::unordered_map<std::string, std::string>& config);
    [[nodiscard]] std::vector<service_metric> collect();
    [[nodiscard]] std::string get_name() const;
    [[nodiscard]] std::vector<std::string> get_metric_types() const;
    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] std::unordered_map<std::string, double> get_statistics() const;

    // Configuration
    void set_ae_title(std::string ae_title);
    [[nodiscard]] std::string get_ae_title() const;
    void set_operation_enabled(dimse_operation op, bool enabled);
    [[nodiscard]] bool is_operation_enabled(dimse_operation op) const;
};

} // namespace pacs::monitoring
```

### 4.3 Exposed Metrics (Per Operation)

For each DIMSE operation (e.g., c_echo, c_store, c_find, c_move, c_get, n_create, n_set, n_get, n_action, n_event, n_delete):

| Metric Name Pattern | Type | Labels | Description |
|---------------------|------|--------|-------------|
| `dicom_<op>_requests_total` | counter | ae, operation | Total requests for operation |
| `dicom_<op>_success_total` | counter | ae, operation | Successful operations |
| `dicom_<op>_failure_total` | counter | ae, operation | Failed operations |
| `dicom_<op>_duration_seconds_avg` | gauge | ae, operation | Average duration in seconds |
| `dicom_<op>_duration_seconds_min` | gauge | ae, operation | Minimum duration observed |
| `dicom_<op>_duration_seconds_max` | gauge | ae, operation | Maximum duration observed |
| `dicom_<op>_duration_seconds_sum` | counter | ae, operation | Total cumulative duration |

### 4.4 Complete Metric List

```
# Composite Services (C-xxx)
dicom_c_echo_requests_total
dicom_c_echo_success_total
dicom_c_echo_failure_total
dicom_c_echo_duration_seconds_avg
dicom_c_echo_duration_seconds_min
dicom_c_echo_duration_seconds_max
dicom_c_echo_duration_seconds_sum

dicom_c_store_requests_total
dicom_c_store_success_total
dicom_c_store_failure_total
dicom_c_store_duration_seconds_avg
dicom_c_store_duration_seconds_min
dicom_c_store_duration_seconds_max
dicom_c_store_duration_seconds_sum

dicom_c_find_requests_total
dicom_c_find_success_total
dicom_c_find_failure_total
dicom_c_find_duration_seconds_avg
dicom_c_find_duration_seconds_min
dicom_c_find_duration_seconds_max
dicom_c_find_duration_seconds_sum

dicom_c_move_requests_total
dicom_c_move_success_total
dicom_c_move_failure_total
dicom_c_move_duration_seconds_avg
dicom_c_move_duration_seconds_min
dicom_c_move_duration_seconds_max
dicom_c_move_duration_seconds_sum

dicom_c_get_requests_total
dicom_c_get_success_total
dicom_c_get_failure_total
dicom_c_get_duration_seconds_avg
dicom_c_get_duration_seconds_min
dicom_c_get_duration_seconds_max
dicom_c_get_duration_seconds_sum

# Normalized Services (N-xxx)
dicom_n_create_requests_total
dicom_n_create_success_total
dicom_n_create_failure_total
dicom_n_create_duration_seconds_avg
dicom_n_create_duration_seconds_min
dicom_n_create_duration_seconds_max
dicom_n_create_duration_seconds_sum

dicom_n_set_requests_total
dicom_n_set_success_total
dicom_n_set_failure_total
dicom_n_set_duration_seconds_avg
dicom_n_set_duration_seconds_min
dicom_n_set_duration_seconds_max
dicom_n_set_duration_seconds_sum

dicom_n_get_requests_total
dicom_n_get_success_total
dicom_n_get_failure_total
dicom_n_get_duration_seconds_avg
dicom_n_get_duration_seconds_min
dicom_n_get_duration_seconds_max
dicom_n_get_duration_seconds_sum

dicom_n_action_requests_total
dicom_n_action_success_total
dicom_n_action_failure_total
dicom_n_action_duration_seconds_avg
dicom_n_action_duration_seconds_min
dicom_n_action_duration_seconds_max
dicom_n_action_duration_seconds_sum

dicom_n_event_requests_total
dicom_n_event_success_total
dicom_n_event_failure_total
dicom_n_event_duration_seconds_avg
dicom_n_event_duration_seconds_min
dicom_n_event_duration_seconds_max
dicom_n_event_duration_seconds_sum

dicom_n_delete_requests_total
dicom_n_delete_success_total
dicom_n_delete_failure_total
dicom_n_delete_duration_seconds_avg
dicom_n_delete_duration_seconds_min
dicom_n_delete_duration_seconds_max
dicom_n_delete_duration_seconds_sum
```

### 4.5 Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                  Service Collector Data Flow                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  pacs_metrics::global_metrics()                                     │
│           │                                                          │
│           │ get_counter(dimse_operation)                            │
│           ▼                                                          │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │               operation_counter (per DIMSE op)                │   │
│  │  • success_count    ──────────────► <op>_success_total        │   │
│  │  • failure_count    ──────────────► <op>_failure_total        │   │
│  │  • total_duration_us ─────────────► <op>_duration_seconds_sum │   │
│  │  • min_duration_us  ──────────────► <op>_duration_seconds_min │   │
│  │  • max_duration_us  ──────────────► <op>_duration_seconds_max │   │
│  └──────────────────────────────────────────────────────────────┘   │
│           │                                                          │
│           │ calculate                                                │
│           ▼                                                          │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  requests_total = success_count + failure_count               │   │
│  │  duration_avg = total_duration_us / requests_total            │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 5. Storage Collector (DES-MON-006)

### 5.1 Overview

**Design ID:** DES-MON-006

**Traces to:** SRS-INT-006 (monitoring_system Integration)

**File:** `include/pacs/monitoring/collectors/dicom_storage_collector.hpp`

The Storage Collector gathers metrics related to:
- Data transfer volumes (bytes sent/received)
- Image storage/retrieval counts
- Object pool statistics (element, dataset, PDU buffer pools)
- Transfer throughput rates

### 5.2 Interface

```cpp
namespace pacs::monitoring {

struct storage_metric {
    std::string name;
    double value;
    std::string type;
    std::string unit;  // "bytes", "count", "bytes_per_second", "ratio"
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;
};

class dicom_storage_collector {
public:
    explicit dicom_storage_collector(std::string ae_title = "PACS_SCP");
    ~dicom_storage_collector() = default;

    // Collector interface
    [[nodiscard]] bool initialize(
        const std::unordered_map<std::string, std::string>& config);
    [[nodiscard]] std::vector<storage_metric> collect();
    [[nodiscard]] std::string get_name() const;
    [[nodiscard]] std::vector<std::string> get_metric_types() const;
    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] std::unordered_map<std::string, double> get_statistics() const;

    // Configuration
    void set_ae_title(std::string ae_title);
    [[nodiscard]] std::string get_ae_title() const;
    void set_pool_metrics_enabled(bool enabled);
    [[nodiscard]] bool is_pool_metrics_enabled() const;
};

} // namespace pacs::monitoring
```

### 5.3 Exposed Metrics

#### 5.3.1 Transfer Metrics

| Metric Name | Type | Unit | Labels | Description |
|-------------|------|------|--------|-------------|
| `dicom_bytes_sent_total` | counter | bytes | ae | Total bytes sent |
| `dicom_bytes_received_total` | counter | bytes | ae | Total bytes received |
| `dicom_images_stored_total` | counter | count | ae | Total images stored |
| `dicom_images_retrieved_total` | counter | count | ae | Total images retrieved |
| `dicom_bytes_sent_rate` | gauge | bytes_per_second | ae | Current send throughput |
| `dicom_bytes_received_rate` | gauge | bytes_per_second | ae | Current receive throughput |

#### 5.3.2 Pool Metrics

| Metric Name | Type | Unit | Labels | Description |
|-------------|------|------|--------|-------------|
| `dicom_element_pool_acquisitions_total` | counter | count | ae | Element pool total acquisitions |
| `dicom_element_pool_hit_ratio` | gauge | ratio | ae | Element pool hit ratio (0.0-1.0) |
| `dicom_element_pool_size` | gauge | count | ae | Current element pool size |
| `dicom_dataset_pool_acquisitions_total` | counter | count | ae | Dataset pool total acquisitions |
| `dicom_dataset_pool_hit_ratio` | gauge | ratio | ae | Dataset pool hit ratio (0.0-1.0) |
| `dicom_dataset_pool_size` | gauge | count | ae | Current dataset pool size |
| `dicom_pdu_buffer_pool_acquisitions_total` | counter | count | ae | PDU buffer pool total acquisitions |
| `dicom_pdu_buffer_pool_hit_ratio` | gauge | ratio | ae | PDU buffer pool hit ratio (0.0-1.0) |
| `dicom_pdu_buffer_pool_size` | gauge | count | ae | Current PDU buffer pool size |

### 5.4 Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                  Storage Collector Data Flow                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  pacs_metrics::global_metrics()                                     │
│           │                                                          │
│           ├─── transfer() ──────────────────────────────────────┐   │
│           │    │                                                 │   │
│           │    ▼                                                 │   │
│           │   data_transfer_metrics                              │   │
│           │   • bytes_sent      ──────► bytes_sent_total         │   │
│           │   • bytes_received  ──────► bytes_received_total     │   │
│           │   • images_stored   ──────► images_stored_total      │   │
│           │   • images_retrieved ─────► images_retrieved_total   │   │
│           │                                                      │   │
│           │   [Rate calculation over time]                       │   │
│           │   • Δbytes_sent/Δt  ──────► bytes_sent_rate          │   │
│           │   • Δbytes_recv/Δt  ──────► bytes_received_rate      │   │
│           │                                                      │   │
│           ├─── element_pool() ──────────────────────────────────┤   │
│           ├─── dataset_pool() ──────────────────────────────────┤   │
│           └─── pdu_buffer_pool() ───────────────────────────────┤   │
│                │                                                 │   │
│                ▼                                                 │   │
│               pool_counters (per pool)                           │   │
│               • total_acquisitions ──► <pool>_acquisitions_total │   │
│               • hit_ratio()        ──► <pool>_hit_ratio          │   │
│               • current_pool_size  ──► <pool>_size               │   │
│                                                                  │   │
└──────────────────────────────────────────────────────────────────┴───┘
```

---

## 6. CRTP-Based Unified Collector (DES-MON-007)

### 6.1 Overview

**Design ID:** DES-MON-007

**Traces to:** SRS-INT-006 (monitoring_system Integration), Issue #490

**Files:**
- `include/pacs/monitoring/collectors/dicom_collector_base.hpp` (DES-MON-007a)
- `include/pacs/monitoring/collectors/dicom_metrics_collector.hpp` (DES-MON-007b)

The CRTP-based unified collector provides:
- **Zero-overhead polymorphism** using Curiously Recurring Template Pattern (CRTP)
- **Single-pass collection** of all DICOM metrics for efficiency
- **Configurable categories** to enable/disable specific metric types
- **Snapshot support** for efficient point-in-time metric access

### 6.2 CRTP Base Class

```cpp
namespace pacs::monitoring {

template <typename Derived>
class dicom_collector_base {
public:
    // Collector Plugin Interface
    bool initialize(const config_map& config);
    std::vector<dicom_metric> collect();
    std::string get_name() const;
    std::vector<std::string> get_metric_types() const;
    bool is_healthy() const;
    stats_map get_statistics() const;

    // Configuration
    bool is_enabled() const;
    std::size_t get_collection_count() const;
    std::size_t get_collection_errors() const;
    std::string get_ae_title() const;
    void set_ae_title(std::string ae_title);

protected:
    // Create metric with standard tags
    dicom_metric create_base_metric(
        const std::string& name,
        double value,
        const std::string& type,
        const std::unordered_map<std::string, std::string>& extra_tags = {}) const;

    // Derived class must implement:
    // - static constexpr const char* collector_name
    // - bool do_initialize(const config_map& config)
    // - std::vector<dicom_metric> do_collect()
    // - bool is_available() const
    // - std::vector<std::string> do_get_metric_types() const
    // - void do_add_statistics(stats_map& stats) const
};

} // namespace pacs::monitoring
```

### 6.3 Unified Metrics Collector

```cpp
namespace pacs::monitoring {

struct dicom_metrics_snapshot {
    // Association metrics
    std::uint64_t total_associations;
    std::uint64_t active_associations;
    std::uint64_t failed_associations;
    std::uint64_t peak_active_associations;

    // Transfer metrics
    std::uint64_t images_sent;
    std::uint64_t images_received;
    std::uint64_t bytes_sent;
    std::uint64_t bytes_received;

    // Storage metrics
    std::uint64_t store_operations;
    std::uint64_t successful_stores;
    std::uint64_t failed_stores;
    double avg_store_latency_ms;

    // Query metrics
    std::uint64_t query_operations;
    std::uint64_t successful_queries;
    std::uint64_t failed_queries;
    double avg_query_latency_ms;

    // Timestamp
    std::chrono::system_clock::time_point timestamp;
};

class dicom_metrics_collector
    : public dicom_collector_base<dicom_metrics_collector> {
public:
    static constexpr const char* collector_name = "dicom_metrics_collector";

    explicit dicom_metrics_collector(std::string ae_title = "PACS_SCP");

    // CRTP Interface Implementation
    bool do_initialize(const config_map& config);
    std::vector<dicom_metric> do_collect();
    bool is_available() const;
    std::vector<std::string> do_get_metric_types() const;
    void do_add_statistics(stats_map& stats) const;

    // Snapshot access
    dicom_metrics_snapshot get_snapshot() const;
    dicom_metrics_snapshot get_last_snapshot() const;

    // Configuration
    void set_collect_associations(bool enabled);
    void set_collect_transfers(bool enabled);
    void set_collect_storage(bool enabled);
    void set_collect_queries(bool enabled);
    void set_collect_pools(bool enabled);
};

} // namespace pacs::monitoring
```

### 6.4 Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable/disable collector |
| `ae_title` | string | "PACS_SCP" | Application Entity title for labels |
| `collect_associations` | bool | true | Collect association metrics |
| `collect_transfers` | bool | true | Collect transfer metrics |
| `collect_storage` | bool | true | Collect storage metrics |
| `collect_queries` | bool | true | Collect query metrics |
| `collect_pools` | bool | true | Collect pool metrics |

### 6.5 Benefits

| Aspect | Traditional Collectors | CRTP Collector |
|--------|----------------------|----------------|
| Virtual function overhead | Present | Eliminated |
| Collection passes | Multiple (3) | Single |
| Metric deduplication | Manual | Automatic |
| Snapshot support | No | Yes |
| Compile-time optimization | Limited | Full inlining |

### 6.6 Performance Targets

Based on monitoring_system benchmarks:
- **Metric collection:** < 100ns per operation
- **Memory overhead:** < 1MB for counters
- **CPU overhead:** < 1% at 1000 ops/sec

### 6.7 Integration with pacs_monitor

```cpp
// Access via pacs_monitor
auto& collector = pacs_monitor::global_monitor().unified_collector();

// Get metrics
auto metrics = collector.collect();

// Get snapshot for quick access
auto snapshot = pacs_monitor::global_monitor().get_unified_snapshot();
std::cout << "Active associations: " << snapshot.active_associations << "\n";
```

---

## 7. Metrics Catalog

### 7.1 Complete Metrics Reference

```yaml
# PACS Monitoring Collectors Metrics Catalog
# Format: Prometheus-compatible naming convention

# ─────────────────────────────────────────────────────────────────────
# Association Metrics (DES-MON-004)
# ─────────────────────────────────────────────────────────────────────

- name: dicom_associations_active
  type: gauge
  help: Number of currently active DICOM associations
  labels: [ae]

- name: dicom_associations_peak_active
  type: gauge
  help: Maximum concurrent associations observed since startup
  labels: [ae]

- name: dicom_associations_total
  type: counter
  help: Total DICOM associations established since startup
  labels: [ae]

- name: dicom_associations_rejected_total
  type: counter
  help: Total DICOM associations rejected
  labels: [ae]

- name: dicom_associations_aborted_total
  type: counter
  help: Total DICOM associations aborted
  labels: [ae]

- name: dicom_associations_success_rate
  type: gauge
  help: Association success rate (0.0 to 1.0)
  labels: [ae]

# ─────────────────────────────────────────────────────────────────────
# Service Metrics (DES-MON-005) - Per DIMSE Operation
# ─────────────────────────────────────────────────────────────────────

# Pattern for each operation: c_echo, c_store, c_find, c_move, c_get,
#                             n_create, n_set, n_get, n_action, n_event, n_delete

- name: dicom_<operation>_requests_total
  type: counter
  help: Total requests for DIMSE operation
  labels: [ae, operation]

- name: dicom_<operation>_success_total
  type: counter
  help: Successful DIMSE operations
  labels: [ae, operation]

- name: dicom_<operation>_failure_total
  type: counter
  help: Failed DIMSE operations
  labels: [ae, operation]

- name: dicom_<operation>_duration_seconds_avg
  type: gauge
  help: Average operation duration in seconds
  labels: [ae, operation]

- name: dicom_<operation>_duration_seconds_min
  type: gauge
  help: Minimum operation duration observed
  labels: [ae, operation]

- name: dicom_<operation>_duration_seconds_max
  type: gauge
  help: Maximum operation duration observed
  labels: [ae, operation]

- name: dicom_<operation>_duration_seconds_sum
  type: counter
  help: Total cumulative duration for rate calculation
  labels: [ae, operation]

# ─────────────────────────────────────────────────────────────────────
# Storage Metrics (DES-MON-006)
# ─────────────────────────────────────────────────────────────────────

# Transfer Metrics
- name: dicom_bytes_sent_total
  type: counter
  unit: bytes
  help: Total bytes sent over network
  labels: [ae]

- name: dicom_bytes_received_total
  type: counter
  unit: bytes
  help: Total bytes received from network
  labels: [ae]

- name: dicom_images_stored_total
  type: counter
  unit: count
  help: Total DICOM images stored
  labels: [ae]

- name: dicom_images_retrieved_total
  type: counter
  unit: count
  help: Total DICOM images retrieved
  labels: [ae]

- name: dicom_bytes_sent_rate
  type: gauge
  unit: bytes_per_second
  help: Current send throughput
  labels: [ae]

- name: dicom_bytes_received_rate
  type: gauge
  unit: bytes_per_second
  help: Current receive throughput
  labels: [ae]

# Pool Metrics
- name: dicom_element_pool_acquisitions_total
  type: counter
  help: Total element pool acquisitions
  labels: [ae]

- name: dicom_element_pool_hit_ratio
  type: gauge
  help: Element pool cache hit ratio (0.0-1.0)
  labels: [ae]

- name: dicom_element_pool_size
  type: gauge
  help: Current element pool size
  labels: [ae]

- name: dicom_dataset_pool_acquisitions_total
  type: counter
  help: Total dataset pool acquisitions
  labels: [ae]

- name: dicom_dataset_pool_hit_ratio
  type: gauge
  help: Dataset pool cache hit ratio (0.0-1.0)
  labels: [ae]

- name: dicom_dataset_pool_size
  type: gauge
  help: Current dataset pool size
  labels: [ae]

- name: dicom_pdu_buffer_pool_acquisitions_total
  type: counter
  help: Total PDU buffer pool acquisitions
  labels: [ae]

- name: dicom_pdu_buffer_pool_hit_ratio
  type: gauge
  help: PDU buffer pool cache hit ratio (0.0-1.0)
  labels: [ae]

- name: dicom_pdu_buffer_pool_size
  type: gauge
  help: Current PDU buffer pool size
  labels: [ae]
```

### 7.2 Metrics Summary

| Category | Count | Collector |
|----------|-------|-----------|
| Association Metrics | 6 | DES-MON-004 |
| Service Metrics | 77 (7 per operation × 11 operations) | DES-MON-005 |
| Transfer Metrics | 6 | DES-MON-006 |
| Pool Metrics | 9 | DES-MON-006 |
| CRTP Unified Metrics | All above + snapshots | DES-MON-007 |
| **Total** | **98+** | |

---

## 8. Integration with pacs_metrics

### 8.1 Central Metrics Repository

All collectors read from the centralized `pacs_metrics` singleton:

```cpp
// Collectors access metrics via:
const auto& metrics = pacs_metrics::global_metrics();

// Association metrics
const auto& assoc = metrics.associations();

// Transfer metrics
const auto& transfer = metrics.transfer();

// Operation counters
const auto& counter = metrics.get_counter(dimse_operation::c_store);

// Pool metrics
const auto& element_pool = metrics.element_pool();
const auto& dataset_pool = metrics.dataset_pool();
const auto& pdu_pool = metrics.pdu_buffer_pool();
```

### 8.2 Collector Registration

Collectors can be registered with monitoring_system:

```cpp
#include <pacs/monitoring/collectors/dicom_association_collector.hpp>
#include <pacs/monitoring/collectors/dicom_service_collector.hpp>
#include <pacs/monitoring/collectors/dicom_storage_collector.hpp>

void register_pacs_collectors(const std::string& ae_title) {
    // Create collectors
    auto assoc_collector = std::make_unique<dicom_association_collector>(ae_title);
    auto service_collector = std::make_unique<dicom_service_collector>(ae_title);
    auto storage_collector = std::make_unique<dicom_storage_collector>(ae_title);

    // Initialize
    assoc_collector->initialize({{"ae_title", ae_title}});
    service_collector->initialize({{"ae_title", ae_title}});
    storage_collector->initialize({
        {"ae_title", ae_title},
        {"collect_pool_metrics", "true"}
    });

    // Register with monitoring system
    // monitoring_registry.register_collector(std::move(assoc_collector));
    // monitoring_registry.register_collector(std::move(service_collector));
    // monitoring_registry.register_collector(std::move(storage_collector));
}
```

---

## 9. Usage Examples

### 9.1 Basic Collection

```cpp
#include <pacs/monitoring/collectors/dicom_association_collector.hpp>
#include <iostream>

int main() {
    // Create and initialize collector
    pacs::monitoring::dicom_association_collector collector("PACS_SCP");
    collector.initialize({});

    // Collect metrics
    auto metrics = collector.collect();

    // Process metrics
    for (const auto& m : metrics) {
        std::cout << m.name << " = " << m.value
                  << " [" << m.type << "]" << std::endl;
    }

    return 0;
}
```

### 9.2 Prometheus Export Format

```cpp
std::string export_prometheus_format(
    const std::vector<association_metric>& metrics) {

    std::ostringstream oss;

    for (const auto& m : metrics) {
        // HELP line
        oss << "# HELP " << m.name << " DICOM metric\n";
        // TYPE line
        oss << "# TYPE " << m.name << " " << m.type << "\n";
        // Metric line with labels
        oss << m.name << "{";
        bool first = true;
        for (const auto& [key, value] : m.labels) {
            if (!first) oss << ",";
            oss << key << "=\"" << value << "\"";
            first = false;
        }
        oss << "} " << m.value << "\n";
    }

    return oss.str();
}
```

### 9.3 Grafana Dashboard Queries

```promql
# Active Associations
sum(dicom_associations_active{ae="PACS_SCP"})

# C-STORE Throughput (requests/sec)
rate(dicom_c_store_requests_total{ae="PACS_SCP"}[5m])

# C-STORE Success Rate
dicom_c_store_success_total / dicom_c_store_requests_total

# Average Query Latency
dicom_c_find_duration_seconds_avg{ae="PACS_SCP"}

# Data Transfer Rate
dicom_bytes_received_rate{ae="PACS_SCP"}

# Pool Hit Ratio
dicom_element_pool_hit_ratio{ae="PACS_SCP"}
```

---

## 10. Traceability

### 10.1 Requirements to Design

| SRS ID | Design ID | Component | Implementation |
|--------|-----------|-----------|----------------|
| SRS-INT-006 | DES-MON-004 | Association Collector | `dicom_association_collector` class |
| SRS-INT-006 | DES-MON-005 | Service Collector | `dicom_service_collector` class |
| SRS-INT-006 | DES-MON-006 | Storage Collector | `dicom_storage_collector` class |
| SRS-INT-006 | DES-MON-007 | CRTP Unified Collector | `dicom_metrics_collector` class |

### 10.2 Design to Implementation

| Design ID | Component | Header File | Source |
|-----------|-----------|-------------|--------|
| DES-MON-004 | `dicom_association_collector` | `include/pacs/monitoring/collectors/dicom_association_collector.hpp` | (header-only) |
| DES-MON-005 | `dicom_service_collector` | `include/pacs/monitoring/collectors/dicom_service_collector.hpp` | (header-only) |
| DES-MON-006 | `dicom_storage_collector` | `include/pacs/monitoring/collectors/dicom_storage_collector.hpp` | (header-only) |
| DES-MON-007a | `dicom_collector_base` | `include/pacs/monitoring/collectors/dicom_collector_base.hpp` | (header-only) |
| DES-MON-007b | `dicom_metrics_collector` | `include/pacs/monitoring/collectors/dicom_metrics_collector.hpp` | (header-only) |

### 10.3 Related Design Elements

| Design ID | Component | Relationship |
|-----------|-----------|--------------|
| DES-MON-002 | `pacs_metrics` | Data source for all collectors |
| DES-INT-005 | `monitoring_adapter` | Integration with monitoring_system |
| DES-MON-003 | `health_checker` | System health monitoring |

---

*Document Version: 2.1.0*
*Created: 2026-01-04*
*Updated: 2026-01-05*
*Author: kcenon@naver.com*
*Change Log: Added CRTP-based unified collector (DES-MON-007) - Issue #490*
