---
doc_id: "PAC-API-002-SECURITY"
doc_title: "API Reference - Security, Monitoring, DI & Modules Support"
doc_version: "0.1.5.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "API"
---

# API Reference - Security, Monitoring, DI & Modules Support

> **Part of**: [API Reference Index](API_REFERENCE.md)
> **Covers**: Security module (RBAC, access control, audit trail REST endpoints, anonymization), monitoring collectors and health checks, dependency injection services, C++20 module support, document history

> **Version:** 0.1.5.0
> **Last Updated:** 2025-12-13
> **Language:** **English**

---

## Table of Contents

- [Monitoring Module](#monitoring-module)
- [Security Module](#security-module)
- [DI Module](#di-module)
- [C++20 Module Support](#c20-module-support)
- [Document History](#document-history)

---

## Monitoring Module

### `pacs::monitoring::pacs_metrics`

Thread-safe metrics collection for DICOM operations with atomic counters and timing data.

```cpp
#include <pacs/monitoring/pacs_metrics.hpp>

namespace pacs::monitoring {

// DIMSE operation types
enum class dimse_operation {
    c_echo,    // C-ECHO (Verification)
    c_store,   // C-STORE (Storage)
    c_find,    // C-FIND (Query)
    c_move,    // C-MOVE (Retrieve)
    c_get,     // C-GET (Retrieve)
    n_create,  // N-CREATE (MPPS)
    n_set,     // N-SET (MPPS)
    n_get,     // N-GET
    n_action,  // N-ACTION
    n_event,   // N-EVENT-REPORT
    n_delete   // N-DELETE
};

// Atomic counter for operation statistics
struct operation_counter {
    std::atomic<uint64_t> success_count{0};
    std::atomic<uint64_t> failure_count{0};
    std::atomic<uint64_t> total_duration_us{0};
    std::atomic<uint64_t> min_duration_us{UINT64_MAX};
    std::atomic<uint64_t> max_duration_us{0};

    uint64_t total_count() const noexcept;
    uint64_t average_duration_us() const noexcept;
    void record_success(std::chrono::microseconds duration) noexcept;
    void record_failure(std::chrono::microseconds duration) noexcept;
    void reset() noexcept;
};

// Data transfer tracking
struct data_transfer_metrics {
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> images_stored{0};
    std::atomic<uint64_t> images_retrieved{0};

    void add_bytes_sent(uint64_t bytes) noexcept;
    void add_bytes_received(uint64_t bytes) noexcept;
    void increment_images_stored() noexcept;
    void increment_images_retrieved() noexcept;
    void reset() noexcept;
};

// Association lifecycle tracking
struct association_counters {
    std::atomic<uint64_t> total_established{0};
    std::atomic<uint64_t> total_rejected{0};
    std::atomic<uint64_t> total_aborted{0};
    std::atomic<uint32_t> current_active{0};
    std::atomic<uint32_t> peak_active{0};

    void record_established() noexcept;
    void record_released() noexcept;
    void record_rejected() noexcept;
    void record_aborted() noexcept;
    void reset() noexcept;
};

class pacs_metrics {
public:
    // Singleton access
    static pacs_metrics& global_metrics() noexcept;

    // DIMSE operation recording
    void record_store(bool success, std::chrono::microseconds duration,
                      uint64_t bytes_stored = 0) noexcept;
    void record_query(bool success, std::chrono::microseconds duration,
                      uint32_t matches = 0) noexcept;
    void record_echo(bool success, std::chrono::microseconds duration) noexcept;
    void record_move(bool success, std::chrono::microseconds duration,
                     uint32_t images_moved = 0) noexcept;
    void record_get(bool success, std::chrono::microseconds duration,
                    uint32_t images_retrieved = 0, uint64_t bytes = 0) noexcept;
    void record_operation(dimse_operation op, bool success,
                          std::chrono::microseconds duration) noexcept;

    // Data transfer recording
    void record_bytes_sent(uint64_t bytes) noexcept;
    void record_bytes_received(uint64_t bytes) noexcept;

    // Association recording
    void record_association_established() noexcept;
    void record_association_released() noexcept;
    void record_association_rejected() noexcept;
    void record_association_aborted() noexcept;

    // Metric access
    const operation_counter& get_counter(dimse_operation op) const noexcept;
    const data_transfer_metrics& transfer() const noexcept;
    const association_counters& associations() const noexcept;

    // Export
    std::string to_json() const;
    std::string to_prometheus(std::string_view prefix = "pacs") const;

    // Reset all metrics
    void reset() noexcept;
};

} // namespace pacs::monitoring
```

**Usage Example:**

```cpp
#include <pacs/monitoring/pacs_metrics.hpp>
using namespace pacs::monitoring;

// Get global metrics instance
auto& metrics = pacs_metrics::global_metrics();

// Record a C-STORE operation
auto start = std::chrono::steady_clock::now();
// ... perform C-STORE ...
auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
metrics.record_store(true, duration, 1024 * 1024);  // 1MB image

// Export for monitoring systems
std::string json = metrics.to_json();           // For REST API
std::string prom = metrics.to_prometheus();      // For Prometheus
```

---

### `pacs::monitoring::dicom_association_collector`

Collector for DICOM association lifecycle metrics. Provides integration with monitoring systems via a plugin-compatible interface.

```cpp
#include <pacs/monitoring/collectors/dicom_association_collector.hpp>

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
    // Construction
    explicit dicom_association_collector(std::string ae_title = "PACS_SCP");
    ~dicom_association_collector() = default;

    // Collector Plugin Interface
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

**Collected Metrics:**

| Metric Name | Type | Description |
|-------------|------|-------------|
| `dicom_associations_active` | gauge | Currently active associations |
| `dicom_associations_peak_active` | gauge | Peak concurrent associations |
| `dicom_associations_established_total` | counter | Total established associations |
| `dicom_associations_rejected_total` | counter | Total rejected associations |
| `dicom_associations_aborted_total` | counter | Total aborted associations |
| `dicom_associations_success_ratio` | gauge | Success rate (0.0-1.0) |

---

### `pacs::monitoring::dicom_service_collector`

Collector for DICOM DIMSE service operation metrics. Tracks all 11 DIMSE operations with counts, timing, and success/failure statistics.

```cpp
#include <pacs/monitoring/collectors/dicom_service_collector.hpp>

namespace pacs::monitoring {

struct service_metric {
    std::string name;
    double value;
    std::string type;  // "gauge" or "counter"
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;
};

class dicom_service_collector {
public:
    // Construction
    explicit dicom_service_collector(std::string ae_title = "PACS_SCP");
    ~dicom_service_collector() = default;

    // Collector Plugin Interface
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

**Collected Metrics (per DIMSE operation):**

| Metric Name Pattern | Type | Description |
|---------------------|------|-------------|
| `dicom_{op}_requests_total` | counter | Total requests for operation |
| `dicom_{op}_success_total` | counter | Successful operations |
| `dicom_{op}_failure_total` | counter | Failed operations |
| `dicom_{op}_duration_seconds_avg` | gauge | Average duration |
| `dicom_{op}_duration_seconds_min` | gauge | Minimum duration |
| `dicom_{op}_duration_seconds_max` | gauge | Maximum duration |
| `dicom_{op}_duration_seconds_sum` | counter | Cumulative duration |

Where `{op}` is one of: `c_echo`, `c_store`, `c_find`, `c_move`, `c_get`, `n_create`, `n_set`, `n_get`, `n_action`, `n_event`, `n_delete`.

---

### `pacs::monitoring::dicom_storage_collector`

Collector for DICOM storage and data transfer metrics. Includes object pool statistics for memory management monitoring.

```cpp
#include <pacs/monitoring/collectors/dicom_storage_collector.hpp>

namespace pacs::monitoring {

struct storage_metric {
    std::string name;
    double value;
    std::string type;  // "gauge" or "counter"
    std::string unit;  // "bytes", "count", etc.
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;
};

class dicom_storage_collector {
public:
    // Construction
    explicit dicom_storage_collector(std::string ae_title = "PACS_SCP");
    ~dicom_storage_collector() = default;

    // Collector Plugin Interface
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

**Collected Metrics:**

| Metric Name | Type | Unit | Description |
|-------------|------|------|-------------|
| `dicom_bytes_sent_total` | counter | bytes | Total bytes sent |
| `dicom_bytes_received_total` | counter | bytes | Total bytes received |
| `dicom_images_stored_total` | counter | count | Images stored |
| `dicom_images_retrieved_total` | counter | count | Images retrieved |
| `dicom_bytes_sent_rate` | gauge | bytes/s | Send throughput |
| `dicom_bytes_received_rate` | gauge | bytes/s | Receive throughput |
| `dicom_{pool}_pool_acquisitions_total` | counter | count | Pool acquisitions |
| `dicom_{pool}_pool_hit_ratio` | gauge | ratio | Pool hit ratio |
| `dicom_{pool}_pool_size` | gauge | count | Current pool size |

Where `{pool}` is one of: `element`, `dataset`, `pdu_buffer`.

---

### `pacs::monitoring::pacs_monitor`

Unified monitoring class integrating all DICOM metric collectors with health check support and Prometheus export.

```cpp
#include <pacs/monitoring/pacs_monitor.hpp>

namespace pacs::monitoring {

// Health check result for a single component
struct health_check_result {
    std::string name;
    bool healthy;
    std::string message;
    std::chrono::milliseconds latency{0};
    std::chrono::system_clock::time_point timestamp;
};

// Aggregated snapshot of all metrics
struct metrics_snapshot {
    std::vector<association_metric> association_metrics;
    std::vector<service_metric> service_metrics;
    std::vector<storage_metric> storage_metrics;
    std::unordered_map<std::string, double> custom_metrics;
    std::chrono::system_clock::time_point timestamp;
};

class pacs_monitor {
public:
    // Singleton access (Meyer's singleton)
    static pacs_monitor& global_monitor() noexcept;

    // Initialization
    [[nodiscard]] bool initialize(
        const std::unordered_map<std::string, std::string>& config = {});
    [[nodiscard]] bool is_initialized() const noexcept;

    // Custom metrics
    void record_metric(std::string_view name, double value);
    [[nodiscard]] std::optional<double> get_metric(std::string_view name) const;

    // Metric collection
    [[nodiscard]] metrics_snapshot get_metrics();

    // Health checks
    void register_health_check(std::string_view component,
                               std::function<bool()> check);
    void unregister_health_check(std::string_view component);
    [[nodiscard]] health_check_result check_health(std::string_view component);
    [[nodiscard]] std::vector<health_check_result> check_all_health();
    [[nodiscard]] bool is_healthy() const;

    // Export formats
    [[nodiscard]] std::string to_prometheus() const;
    [[nodiscard]] std::string to_json() const;

    // Configuration
    void set_ae_title(std::string ae_title);
    [[nodiscard]] std::string get_ae_title() const;

    // Lifecycle
    void reset();

private:
    pacs_monitor() = default;  // Singleton
};

} // namespace pacs::monitoring
```

**Usage Example:**

{% raw %}
```cpp
#include <pacs/monitoring/pacs_monitor.hpp>
using namespace pacs::monitoring;

// Initialize the global monitor
auto& monitor = pacs_monitor::global_monitor();
monitor.initialize({{"ae_title", "MY_PACS"}});

// Register custom health checks
monitor.register_health_check("database", []() {
    return db_connection.is_alive();
});
monitor.register_health_check("storage", []() {
    return storage.free_space_gb() > 10;
});

// Record custom metrics
monitor.record_metric("active_studies", 42.0);

// Get all metrics snapshot
auto snapshot = monitor.get_metrics();
for (const auto& m : snapshot.association_metrics) {
    std::cout << m.name << ": " << m.value << "\n";
}

// Export for Prometheus
std::string prometheus_output = monitor.to_prometheus();
// Example output:
// # HELP dicom_associations_active Currently active associations
// # TYPE dicom_associations_active gauge
// dicom_associations_active{ae="MY_PACS"} 5

// Check health status
auto health = monitor.check_all_health();
for (const auto& h : health) {
    std::cout << h.name << ": " << (h.healthy ? "OK" : "FAIL") << "\n";
}
```
{% endraw %}

---
## Security Module

### `pacs::security::access_control_manager`

Manages Users, Roles, and Permission enforcement.

```cpp
#include <pacs/security/access_control_manager.hpp>

namespace pacs::security {

class access_control_manager {
public:
    // Setup
    void set_storage(std::shared_ptr<security_storage_interface> storage);

    // User Management
    VoidResult create_user(const User& user);
    Result<User> get_user(std::string_view id);
    VoidResult assign_role(std::string_view user_id, Role role);

    // Permission Check
    bool check_permission(const User& user, ResourceType resource, uint32_t action_mask) const;
    bool has_role(const User& user, Role role) const;
};

} // namespace pacs::security
```

### REST API Endpoints

**Base Path**: `/api/v1/security`

#### Create User
*   **Method**: `POST`
*   **Path**: `/users`
*   **Body**:
    ```json
    {
      "id": "u123",
      "username": "jdoe"
    }
    ```
*   **Responses**:
    *   `201 Created`: User created.
    *   `400 Bad Request`: Invalid input or user exists.
    *   `401 Unauthorized`: Missing or invalid authentication.

#### Assign Role
*   **Method**: `POST`
*   **Path**: `/users/<id>/roles`
*   **Body**:
    ```json
    {
      "role": "Radiologist"
    }
    ```
*   **Role Values**: `Viewer`, `Technologist`, `Radiologist`, `Administrator`.
*   **Responses**:
    *   `200 OK`: Role assigned.
    *   `400 Bad Request`: Invalid role.
    *   `404 Not Found`: User not found.
    *   `401 Unauthorized`: Missing or invalid authentication.

### Patient REST API Endpoints

**Base Path**: `/api/v1/patients`

#### List Patients
*   **Method**: `GET`
*   **Path**: `/`
*   **Query Parameters**:
    *   `patient_id`: Filter by patient ID (supports `*` wildcard)
    *   `patient_name`: Filter by patient name (supports `*` wildcard)
    *   `birth_date`: Filter by birth date (YYYYMMDD)
    *   `birth_date_from`: Birth date range start
    *   `birth_date_to`: Birth date range end
    *   `sex`: Filter by sex (M, F, O)
    *   `limit`: Maximum results (default: 20, max: 100)
    *   `offset`: Pagination offset
*   **Responses**:
    *   `200 OK`: Returns paginated patient list with total count
    *   `503 Service Unavailable`: Database not configured

#### Get Patient Details
*   **Method**: `GET`
*   **Path**: `/<patient_id>`
*   **Responses**:
    *   `200 OK`: Returns patient details
    *   `404 Not Found`: Patient not found
    *   `503 Service Unavailable`: Database not configured

#### Get Patient's Studies
*   **Method**: `GET`
*   **Path**: `/<patient_id>/studies`
*   **Responses**:
    *   `200 OK`: Returns list of studies for the patient
    *   `404 Not Found`: Patient not found
    *   `503 Service Unavailable`: Database not configured

### Study REST API Endpoints

**Base Path**: `/api/v1/studies`

#### List Studies
*   **Method**: `GET`
*   **Path**: `/`
*   **Query Parameters**:
    *   `patient_id`: Filter by patient ID
    *   `patient_name`: Filter by patient name (supports `*` wildcard)
    *   `study_uid`: Filter by Study Instance UID
    *   `study_id`: Filter by Study ID
    *   `study_date`: Filter by study date (YYYYMMDD)
    *   `study_date_from`: Study date range start
    *   `study_date_to`: Study date range end
    *   `accession_number`: Filter by accession number
    *   `modality`: Filter by modality (CT, MR, etc.)
    *   `referring_physician`: Filter by referring physician
    *   `study_description`: Filter by study description
    *   `limit`: Maximum results (default: 20, max: 100)
    *   `offset`: Pagination offset
*   **Responses**:
    *   `200 OK`: Returns paginated study list with total count
    *   `503 Service Unavailable`: Database not configured

#### Get Study Details
*   **Method**: `GET`
*   **Path**: `/<study_uid>`
*   **Responses**:
    *   `200 OK`: Returns study details
    *   `404 Not Found`: Study not found
    *   `503 Service Unavailable`: Database not configured

#### Get Study's Series
*   **Method**: `GET`
*   **Path**: `/<study_uid>/series`
*   **Responses**:
    *   `200 OK`: Returns list of series for the study
    *   `404 Not Found`: Study not found
    *   `503 Service Unavailable`: Database not configured

#### Get Study's Instances
*   **Method**: `GET`
*   **Path**: `/<study_uid>/instances`
*   **Responses**:
    *   `200 OK`: Returns list of all instances in the study
    *   `404 Not Found`: Study not found
    *   `503 Service Unavailable`: Database not configured

#### Delete Study
*   **Method**: `DELETE`
*   **Path**: `/<study_uid>`
*   **Responses**:
    *   `200 OK`: Study deleted successfully
    *   `404 Not Found`: Study not found
    *   `500 Internal Server Error`: Delete operation failed
    *   `503 Service Unavailable`: Database not configured

### Series REST API Endpoints

**Base Path**: `/api/v1/series`

#### Get Series Details
*   **Method**: `GET`
*   **Path**: `/<series_uid>`
*   **Responses**:
    *   `200 OK`: Returns series details
    *   `404 Not Found`: Series not found
    *   `503 Service Unavailable`: Database not configured

#### Get Series Instances
*   **Method**: `GET`
*   **Path**: `/<series_uid>/instances`
*   **Responses**:
    *   `200 OK`: Returns list of instances in the series
    *   `404 Not Found`: Series not found
    *   `503 Service Unavailable`: Database not configured

### Worklist REST API Endpoints

**Base Path**: `/api/v1/worklist`

#### List Worklist Items
*   **Method**: `GET`
*   **Path**: `/`
*   **Query Parameters**:
    *   `limit` (optional): Maximum number of results (default: 20, max: 100)
    *   `offset` (optional): Offset for pagination
    *   `station_ae` (optional): Filter by station AE title
    *   `modality` (optional): Filter by modality
    *   `scheduled_date_from` (optional): Start of date range
    *   `scheduled_date_to` (optional): End of date range
    *   `patient_id` (optional): Filter by patient ID
    *   `patient_name` (optional): Filter by patient name (supports wildcards)
    *   `accession_no` (optional): Filter by accession number
    *   `step_id` (optional): Filter by step ID
    *   `include_all_status` (optional): Include all statuses if "true"
*   **Responses**:
    *   `200 OK`: Returns paginated list of worklist items
    *   `503 Service Unavailable`: Database not configured

#### Create Worklist Item
*   **Method**: `POST`
*   **Path**: `/`
*   **Request Body**: JSON object with worklist item fields
*   **Required Fields**: `step_id`, `patient_id`, `modality`, `scheduled_datetime`
*   **Responses**:
    *   `201 Created`: Returns created worklist item
    *   `400 Bad Request`: Missing required fields
    *   `500 Internal Server Error`: Create operation failed
    *   `503 Service Unavailable`: Database not configured

#### Get Worklist Item
*   **Method**: `GET`
*   **Path**: `/<id>`
*   **Responses**:
    *   `200 OK`: Returns worklist item details
    *   `404 Not Found`: Worklist item not found
    *   `503 Service Unavailable`: Database not configured

#### Update Worklist Item
*   **Method**: `PUT`
*   **Path**: `/<id>`
*   **Request Body**: JSON object with `step_status` field
*   **Valid Statuses**: `SCHEDULED`, `STARTED`, `COMPLETED`
*   **Responses**:
    *   `200 OK`: Returns updated worklist item
    *   `400 Bad Request`: Invalid status value
    *   `404 Not Found`: Worklist item not found
    *   `500 Internal Server Error`: Update operation failed
    *   `503 Service Unavailable`: Database not configured

#### Delete Worklist Item
*   **Method**: `DELETE`
*   **Path**: `/<id>`
*   **Responses**:
    *   `200 OK`: Worklist item deleted successfully
    *   `404 Not Found`: Worklist item not found
    *   `500 Internal Server Error`: Delete operation failed
    *   `503 Service Unavailable`: Database not configured

### Audit Log REST API Endpoints

**Base Path**: `/api/v1/audit`

#### List Audit Logs
*   **Method**: `GET`
*   **Path**: `/logs`
*   **Query Parameters**:
    *   `limit` (optional): Maximum number of results (default: 20, max: 100)
    *   `offset` (optional): Offset for pagination
    *   `event_type` (optional): Filter by event type (e.g., C_STORE, C_FIND)
    *   `outcome` (optional): Filter by outcome (SUCCESS, FAILURE, WARNING)
    *   `user_id` (optional): Filter by user/AE title
    *   `source_ae` (optional): Filter by source AE title
    *   `patient_id` (optional): Filter by patient ID
    *   `study_uid` (optional): Filter by study UID
    *   `date_from` (optional): Start of date range
    *   `date_to` (optional): End of date range
    *   `format` (optional): Export format ("csv" for CSV, default JSON)
*   **Responses**:
    *   `200 OK`: Returns paginated list of audit log entries
    *   `503 Service Unavailable`: Database not configured

#### Get Audit Log Entry
*   **Method**: `GET`
*   **Path**: `/logs/<id>`
*   **Responses**:
    *   `200 OK`: Returns audit log entry details
    *   `404 Not Found`: Audit log entry not found
    *   `503 Service Unavailable`: Database not configured

#### Export Audit Logs
*   **Method**: `GET`
*   **Path**: `/export`
*   **Query Parameters**: Same as List Audit Logs (without pagination)
    *   `format` (optional): Export format ("csv" or "json", default: "json")
*   **Responses**:
    *   `200 OK`: Returns audit logs as downloadable file
    *   `503 Service Unavailable`: Database not configured

### Association REST API Endpoints

**Base Path**: `/api/v1/associations`

#### List Active Associations
*   **Method**: `GET`
*   **Path**: `/active`
*   **Responses**:
    *   `200 OK`: Returns list of active DICOM associations
*   **Note**: Requires integration with DICOM server for real-time data

#### Get Association Details
*   **Method**: `GET`
*   **Path**: `/<id>`
*   **Responses**:
    *   `200 OK`: Returns association details
    *   `404 Not Found`: Association not found
*   **Note**: Requires integration with DICOM server for real-time data

#### Terminate Association
*   **Method**: `DELETE`
*   **Path**: `/<id>`
*   **Responses**:
    *   `200 OK`: Association terminated successfully
    *   `400 Bad Request`: Invalid association ID
    *   `501 Not Implemented`: Requires DICOM server integration
*   **Note**: Requires integration with DICOM server

---
## DI Module

The DI (Dependency Injection) module provides ServiceContainer-based dependency injection for PACS services.

### `pacs::di::IDicomStorage`

Alias for `pacs::storage::storage_interface`. Used for service registration and resolution.

### `pacs::di::IDicomNetwork`

Interface for DICOM network operations.

```cpp
#include <pacs/di/service_interfaces.hpp>

namespace pacs::di {

class IDicomNetwork {
public:
    virtual ~IDicomNetwork() = default;

    // Create a DICOM server
    [[nodiscard]] virtual std::unique_ptr<network::dicom_server> create_server(
        const network::server_config& config,
        const integration::tls_config& tls_cfg = {}) = 0;

    // Connect to a remote DICOM peer
    [[nodiscard]] virtual integration::Result<integration::network_adapter::session_ptr>
        connect(const integration::connection_config& config) = 0;

    [[nodiscard]] virtual integration::Result<integration::network_adapter::session_ptr>
        connect(const std::string& host, uint16_t port,
                std::chrono::milliseconds timeout = std::chrono::seconds{30}) = 0;
};

} // namespace pacs::di
```

### `pacs::di::ILogger`

Abstract logger interface for dependency injection. Enables testable logging by allowing mock implementations.

```cpp
#include <pacs/di/ilogger.hpp>

namespace pacs::di {

class ILogger {
public:
    virtual ~ILogger() = default;

    // Log level methods
    virtual void trace(std::string_view message) = 0;
    virtual void debug(std::string_view message) = 0;
    virtual void info(std::string_view message) = 0;
    virtual void warn(std::string_view message) = 0;
    virtual void error(std::string_view message) = 0;
    virtual void fatal(std::string_view message) = 0;

    // Check if a log level is enabled
    [[nodiscard]] virtual bool is_enabled(integration::log_level level) const noexcept = 0;

    // Formatted logging convenience methods (template)
    template <typename... Args>
    void trace_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);
    template <typename... Args>
    void debug_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);
    template <typename... Args>
    void info_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);
    template <typename... Args>
    void warn_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);
    template <typename... Args>
    void error_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);
    template <typename... Args>
    void fatal_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);
};

// No-op logger implementation (default when no logger is provided)
class NullLogger final : public ILogger;

// Default implementation that delegates to logger_adapter
class LoggerService final : public ILogger;

// Get a shared null logger instance (singleton)
[[nodiscard]] std::shared_ptr<ILogger> null_logger();

} // namespace pacs::di
```

**Usage in Services**:

All SCP/SCU services support optional logger injection:

```cpp
#include <pacs/services/storage_scp.hpp>
#include <pacs/di/ilogger.hpp>

// Default construction uses null_logger (silent)
storage_scp scp;

// Inject a custom logger
auto logger = std::make_shared<pacs::di::LoggerService>();
storage_scp scp_with_logging(logger);

// Change logger at runtime
scp.set_logger(another_logger);

// Access current logger
auto& current = scp.logger();
current->info("Storage SCP ready");
```

### `pacs::di::register_services`

Register all PACS services with a ServiceContainer.

```cpp
#include <pacs/di/service_registration.hpp>

// Create container and register services
kcenon::common::di::service_container container;
auto result = pacs::di::register_services(container);

// Resolve services
auto storage = container.resolve<pacs::di::IDicomStorage>().value();
auto network = container.resolve<pacs::di::IDicomNetwork>().value();
```

### Configuration

```cpp
pacs::di::registration_config config;
config.storage_path = "/path/to/storage";  // Custom storage path
config.enable_network = true;              // Enable network services
config.enable_logger = true;               // Enable logger services (default: true)
config.use_singletons = true;              // Use singleton lifetime

auto result = pacs::di::register_services(container, config);
```

### Logger Registration

Register a custom logger implementation:

```cpp
#include <pacs/di/service_registration.hpp>

// Register a custom logger factory
auto result = pacs::di::register_logger<MyCustomLogger>(
    container,
    [](IServiceContainer&) { return std::make_shared<MyCustomLogger>(); },
    service_lifetime::singleton
);

// Or register a pre-created logger instance
auto my_logger = std::make_shared<pacs::di::LoggerService>();
auto result = pacs::di::register_logger_instance(container, my_logger);

// Resolve and use
auto logger = container.resolve<pacs::di::ILogger>().value();
logger->info("Logger resolved from DI container");
```

### `pacs::di::create_container`

Convenience function to create a configured container.

```cpp
auto container = pacs::di::create_container();
// Container has all services registered with default configuration
```

### Test Support

The module provides mock implementations for unit testing.

```cpp
#include <pacs/di/test_support.hpp>

// Create a test container with mock services
auto container = pacs::di::test::create_test_container();

// Or use the builder for more control
auto mock_storage = std::make_shared<pacs::di::test::MockStorage>();
auto container = pacs::di::test::TestContainerBuilder()
    .with_storage(mock_storage)
    .with_mock_network()
    .build();

// Use in tests
auto storage = container->resolve<pacs::di::IDicomStorage>().value();
storage->store(dataset);  // Uses mock implementation

// Verify mock interactions
EXPECT_EQ(mock_storage->store_count(), 1);
```

---
## C++20 Module Support

> **Status:** Experimental. CI builds the module target with Clang 18 on Ubuntu 24.04; see
> [C++20 Module Architecture](ARCHITECTURE.md#c20-module-architecture) for what the CI job proves.
> **Requirements:** CMake 3.28+, Ninja, and a compiler that CMake supports for modules
> (Clang 16+/GCC 14+/MSVC 2022 17.4+); only Clang 18 is tested

### Overview

pacs_system provides C++20 module support as an alternative to the traditional header-based approach.
The primary module `kcenon.pacs` re-exports the names that the public headers declare, in the same
`kcenon::pacs::` namespaces. Modules offer:
- Faster compilation of consumers, since the partitions are compiled once
- Better encapsulation of internal APIs
- Cleaner dependency graph
- Improved IDE responsiveness

### Module Structure

```
kcenon.pacs
├── :core                 # DICOM core types (dicom_tag, dicom_dataset, tags::, events, Result<T>)
├── :encoding             # VR types, transfer syntaxes, compression codecs, SIMD feature checks
├── :storage              # Storage backends, index database, records (needs pacs_storage)
├── :security             # RBAC, certificates and signatures, anonymization
├── :network              # Association, PDU types and codecs, dicom_server, v2 server
├── :network_dimse        # DIMSE messages, command fields, status checks, factories
├── :services             # SCP/SCU implementations, SOP class registry
├── :services_cache       # Query cache; cursors and streaming with database_system
├── :services_validation  # IOD validators
├── :workflow             # Task scheduling and prefetching (needs pacs_workflow)
├── :web                  # REST API and DICOMweb helpers (needs pacs_web)
├── :integration          # Ecosystem adapters (needs pacs_integration)
├── :ai                   # AI/ML service integration (needs pacs_ai)
├── :monitoring           # Health checks and metrics (needs pacs_monitoring)
└── :di                   # Dependency injection utilities
```

A partition marked "needs `pacs_<name>`" is empty when that library is not built.

### Building with Modules

```bash
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++-18 -DPACS_BUILD_MODULES=ON
cmake --build build --target pacs_system_modules
```

### Usage

```cpp
#include <iostream>

// Import the entire pacs module
import kcenon.pacs;

int main() {
    // Use core types
    kcenon::pacs::core::dicom_dataset ds;
    ds.set_string(kcenon::pacs::core::tags::patient_name,
                  kcenon::pacs::encoding::vr_type::PN, "DOE^JOHN");

    // Use the Result pattern
    auto result = kcenon::pacs::ok(42);
    if (result.is_ok()) {
        std::cout << result.value() << std::endl;
    }

    return 0;
}
```

Partitions are internal to the module: `import kcenon.pacs:core;` is not valid outside
`kcenon.pacs`, so consumers always write `import kcenon.pacs;`. Namespace-scope `constexpr`
constants without `inline` (for example `kcenon::pacs::error_codes` and the DIMSE `status_*`
codes) cannot be exported; include their headers next to the import to use them.

### Partition Details

| Partition | Exported namespaces | Main types |
|-----------|--------------------|------------|
| `:core` | `kcenon::pacs`, `kcenon::pacs::core`, `kcenon::pacs::core::tags`, `kcenon::pacs::events` | `dicom_tag`, `dicom_element`, `dicom_dataset`, `dicom_file`, `dicom_dictionary`, `Result<T>` |
| `:encoding` | `kcenon::pacs::encoding`, `::compression`, `::simd` | `vr_type`, `transfer_syntax`, `explicit_vr_codec`, `codec_factory`, `jpeg_baseline_codec` |
| `:storage` | `kcenon::pacs::storage` | `file_storage`, `s3_storage`, `azure_blob_storage`, `hsm_storage`, `index_database` |
| `:security` | `kcenon::pacs::security` | `access_control_manager`, `User`, `Role`, `certificate`, `digital_signature`, `anonymizer` |
| `:network` | `kcenon::pacs::network`, `::v2` | `association`, `pdu_encoder`, `pdu_decoder`, `server_config`, `dicom_server`, `dicom_server_v2` |
| `:network_dimse` | `kcenon::pacs::network::dimse` | `dimse_message`, `command_field`, `status_code`, `make_c_echo_rq` and the other factories |
| `:services` | `kcenon::pacs::services` | `storage_scp`, `query_scp`, `retrieve_scp`, `storage_scu`, `query_scu`, `sop_class_registry` |
| `:services_cache` | `kcenon::pacs::services::cache`, `kcenon::pacs::services` | `query_cache`, `simple_lru_cache`; `database_cursor`, `query_result_stream` with database_system |
| `:services_validation` | `kcenon::pacs::services::validation` | `us_iod_validator`, `dx_iod_validator`, `ct_iod_validator`, `validation_result` |
| `:workflow` | `kcenon::pacs::workflow` | `task_scheduler`, `auto_prefetch_service`, `study_lock_manager` |
| `:web` | `kcenon::pacs::web`, `::dicomweb` | `rest_server`, `rest_server_config`, `multipart_builder` |
| `:integration` | `kcenon::pacs::integration` | `logger_adapter`, `thread_pool_adapter`, `network_adapter`, `dicom_session` |
| `:ai` | `kcenon::pacs::ai` | `ai_service_connector`, `ai_result_handler`, `assessment_manager` |
| `:monitoring` | `kcenon::pacs::monitoring` | `pacs_monitor`, `health_checker`, `pacs_metrics`, `dicom_metrics_collector` |
| `:di` | `kcenon::pacs::di`, `::test` | `ILogger`, `IDicomStorage`, `register_services`, `TestContainerBuilder` |

The types are documented with their headers in the other parts of this API reference; the
partitions add no API of their own apart from `kcenon::pacs::module_version`.

### Testing and Examples

**Module Tests:**

Run module import tests to verify module functionality:

```bash
# Build with tests and modules
cmake -G Ninja -DPACS_BUILD_MODULES=ON -DPACS_BUILD_TESTS=ON ..
cmake --build . --target module_tests

# Run module tests
ctest -L modules
```

**Module Example:**

The `module_example` demonstrates C++20 module usage:

```bash
# Build with examples and modules
cmake -G Ninja -DPACS_BUILD_MODULES=ON -DPACS_BUILD_EXAMPLES=ON ..
cmake --build . --target module_example

# Run example
./bin/module_example [optional_dicom_file]
```

### CMake Integration

The module target is defined in the pacs_system build tree and is not installed, so
`find_package(pacs_system)` does not provide it. Add pacs_system to your build (for example with
`add_subdirectory` or `FetchContent`) and link the alias:

```cmake
set(PACS_BUILD_MODULES ON)
add_subdirectory(pacs_system)

# Link to module target
target_link_libraries(my_app PRIVATE kcenon::pacs_modules)
set_target_properties(my_app PROPERTIES CXX_SCAN_FOR_MODULES ON)

# Or link specific components (header-based)
target_link_libraries(my_app PRIVATE
    pacs_core
    pacs_encoding
    pacs_network
)
```
## Document History

| Version | Date       | Changes                                      |
|---------|------------|----------------------------------------------|
| 1.0.0   | 2025-11-30 | Initial API Reference document               |
| 1.1.0   | 2025-12-05 | Added thread_system integration APIs         |
| 1.2.0   | 2025-12-07 | Added Network V2 Module (dicom_server_v2, dicom_association_handler) |
| 1.3.0   | 2025-12-07 | Added Monitoring Module (pacs_metrics for DIMSE operation tracking) |
| 1.4.0   | 2025-12-07 | Added DX Modality Module (dx_storage, dx_iod_validator) |
| 1.5.0   | 2025-12-08 | Added MG Modality Module (mg_storage, mg_iod_validator) |
| 1.6.0   | 2025-12-09 | Added Web Module (rest_server foundation)    |
| 1.7.0   | 2025-12-11 | Added Patient, Study, Series REST API endpoints |
| 1.8.0   | 2025-12-12 | Added DICOMweb (WADO-RS) API endpoints and utilities |
| 1.9.0   | 2025-12-13 | Added STOW-RS (Store Over the Web) API endpoints and multipart parser |
| 1.10.0  | 2025-12-13 | Added QIDO-RS (Query based on ID for DICOM Objects) API endpoints |
| 1.11.0  | 2025-12-13 | Added WADO-RS Frame retrieval and Rendered image endpoints |


