# SDS - Monitoring Collectors Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-04
> **Status:** Initial

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-MON-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#481](https://github.com/kcenon/pacs_system/issues/481), [#468](https://github.com/kcenon/pacs_system/issues/468) |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Collector Interface](#2-collector-interface)
- [3. Association Collector](#3-association-collector)
- [4. Storage Collector](#4-storage-collector)
- [5. Service Collector](#5-service-collector)
- [6. Metrics Catalog](#6-metrics-catalog)
- [7. Traceability](#7-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the Monitoring Collectors plugin architecture. The module provides:

- **Collector Interface**: Plugin pattern for metrics collection
- **DICOM Association Metrics**: Connection and state tracking
- **Storage Metrics**: File system and database statistics
- **Service Metrics**: DIMSE operation counters and latencies

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| Association Collector | 1 header | DES-MON-001 |
| Storage Collector | 1 header | DES-MON-002 |
| Service Collector | 1 header | DES-MON-003 |

### 1.3 Integration with monitoring_system

```
┌─────────────────────────────────────────────────────────────────┐
│                    Monitoring Architecture                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                    monitoring_system                        │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │ │
│  │  │   Registry   │  │   Exporter   │  │   Alerting   │      │ │
│  │  └──────┬───────┘  └──────────────┘  └──────────────┘      │ │
│  └─────────┼──────────────────────────────────────────────────┘ │
│            │ register_collector()                                │
│            │                                                     │
│  ┌─────────▼──────────────────────────────────────────────────┐ │
│  │                    PACS Collectors                          │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │ │
│  │  │ Association  │  │   Storage    │  │   Service    │      │ │
│  │  │  DES-MON-001 │  │  DES-MON-002 │  │  DES-MON-003 │      │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘      │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Collector Interface

### 2.1 Base Collector Pattern

```cpp
namespace pacs::monitoring {

// Metric types
enum class metric_type {
    counter,    // Monotonically increasing value
    gauge,      // Current value (can go up/down)
    histogram,  // Distribution of values
    summary     // Percentiles
};

struct metric_value {
    std::string name;
    metric_type type;
    double value;
    std::map<std::string, std::string> labels;
    std::chrono::system_clock::time_point timestamp;
};

// Collector interface (matches monitoring_system pattern)
class collector_interface {
public:
    virtual ~collector_interface() = default;

    [[nodiscard]] virtual auto collect() -> std::vector<metric_value> = 0;
    [[nodiscard]] virtual auto name() const -> std::string = 0;
    [[nodiscard]] virtual auto description() const -> std::string = 0;
};

// Registration helper
void register_pacs_collectors(kcenon::monitoring::metrics_registry& registry);

} // namespace pacs::monitoring
```

---

## 3. Association Collector

### 3.1 DES-MON-001: Association Collector

**Traces to:** SRS-INT-006 (monitoring_system Integration)

**File:** `include/pacs/monitoring/collectors/dicom_association_collector.hpp`

```cpp
namespace pacs::monitoring::collectors {

class dicom_association_collector : public collector_interface {
public:
    explicit dicom_association_collector(
        const network::dicom_server& server);

    [[nodiscard]] auto collect() -> std::vector<metric_value> override;
    [[nodiscard]] auto name() const -> std::string override;
    [[nodiscard]] auto description() const -> std::string override;

private:
    const network::dicom_server& server_;
};

} // namespace pacs::monitoring::collectors
```

**Exposed Metrics:**

| Metric Name | Type | Labels | Description |
|-------------|------|--------|-------------|
| `pacs_associations_active` | gauge | ae_title | Current active associations |
| `pacs_associations_total` | counter | ae_title, result | Total associations (accepted/rejected) |
| `pacs_association_duration_seconds` | histogram | ae_title | Association duration |
| `pacs_pdu_received_total` | counter | pdu_type | PDUs received by type |
| `pacs_pdu_sent_total` | counter | pdu_type | PDUs sent by type |
| `pacs_pdu_bytes_received` | counter | - | Total bytes received |
| `pacs_pdu_bytes_sent` | counter | - | Total bytes sent |

---

## 4. Storage Collector

### 4.1 DES-MON-002: Storage Collector

**Traces to:** SRS-INT-006 (monitoring_system Integration)

**File:** `include/pacs/monitoring/collectors/dicom_storage_collector.hpp`

```cpp
namespace pacs::monitoring::collectors {

class dicom_storage_collector : public collector_interface {
public:
    explicit dicom_storage_collector(
        const storage::storage_interface& storage,
        const storage::index_database& database);

    [[nodiscard]] auto collect() -> std::vector<metric_value> override;
    [[nodiscard]] auto name() const -> std::string override;
    [[nodiscard]] auto description() const -> std::string override;

private:
    const storage::storage_interface& storage_;
    const storage::index_database& database_;
};

} // namespace pacs::monitoring::collectors
```

**Exposed Metrics:**

| Metric Name | Type | Labels | Description |
|-------------|------|--------|-------------|
| `pacs_storage_patients_total` | gauge | - | Total patients in system |
| `pacs_storage_studies_total` | gauge | - | Total studies stored |
| `pacs_storage_series_total` | gauge | - | Total series stored |
| `pacs_storage_instances_total` | gauge | - | Total instances stored |
| `pacs_storage_bytes_total` | gauge | - | Total storage bytes used |
| `pacs_storage_bytes_available` | gauge | - | Available storage bytes |
| `pacs_database_queries_total` | counter | query_type | Database queries executed |
| `pacs_database_query_duration_seconds` | histogram | query_type | Query latency |

---

## 5. Service Collector

### 5.1 DES-MON-003: Service Collector

**Traces to:** SRS-INT-006 (monitoring_system Integration)

**File:** `include/pacs/monitoring/collectors/dicom_service_collector.hpp`

```cpp
namespace pacs::monitoring::collectors {

class dicom_service_collector : public collector_interface {
public:
    explicit dicom_service_collector(
        const services::service_registry& registry);

    [[nodiscard]] auto collect() -> std::vector<metric_value> override;
    [[nodiscard]] auto name() const -> std::string override;
    [[nodiscard]] auto description() const -> std::string override;

private:
    const services::service_registry& registry_;
};

} // namespace pacs::monitoring::collectors
```

**Exposed Metrics:**

| Metric Name | Type | Labels | Description |
|-------------|------|--------|-------------|
| `pacs_dimse_requests_total` | counter | command, status | DIMSE requests by command |
| `pacs_dimse_duration_seconds` | histogram | command | DIMSE operation latency |
| `pacs_cstore_received_total` | counter | sop_class, status | C-STORE operations |
| `pacs_cstore_bytes_total` | counter | sop_class | C-STORE data volume |
| `pacs_cfind_queries_total` | counter | level, status | C-FIND queries |
| `pacs_cfind_matches_total` | counter | level | C-FIND matching results |
| `pacs_cmove_transfers_total` | counter | status | C-MOVE transfers |
| `pacs_cmove_instances_total` | counter | - | Instances transferred via C-MOVE |
| `pacs_echo_requests_total` | counter | status | C-ECHO verifications |

---

## 6. Metrics Catalog

### 6.1 Complete Metrics Reference

```yaml
# PACS Metrics Catalog
# Format: Prometheus exposition format compatible

# Association Metrics
- name: pacs_associations_active
  type: gauge
  help: Number of currently active DICOM associations
  labels: [ae_title]

- name: pacs_associations_total
  type: counter
  help: Total DICOM associations since server start
  labels: [ae_title, result]

# Storage Metrics
- name: pacs_storage_instances_total
  type: gauge
  help: Total DICOM instances stored
  labels: []

- name: pacs_storage_bytes_total
  type: gauge
  help: Total storage space used in bytes
  labels: []

# Service Metrics
- name: pacs_dimse_requests_total
  type: counter
  help: Total DIMSE requests processed
  labels: [command, status]

- name: pacs_dimse_duration_seconds
  type: histogram
  help: DIMSE operation duration in seconds
  labels: [command]
  buckets: [0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1, 5, 10]
```

### 6.2 Grafana Dashboard Example

```json
{
  "panels": [
    {
      "title": "Active Associations",
      "type": "gauge",
      "targets": [{"expr": "sum(pacs_associations_active)"}]
    },
    {
      "title": "C-STORE Throughput",
      "type": "graph",
      "targets": [{"expr": "rate(pacs_cstore_bytes_total[5m])"}]
    },
    {
      "title": "Query Latency (P95)",
      "type": "stat",
      "targets": [{"expr": "histogram_quantile(0.95, pacs_dimse_duration_seconds{command='C-FIND'})"}]
    }
  ]
}
```

---

## 7. Traceability

### 7.1 Requirements to Design

| SRS ID | Design ID(s) | Implementation |
|--------|--------------|----------------|
| SRS-INT-006 | DES-MON-001 | Association metrics collector |
| SRS-INT-006 | DES-MON-002 | Storage metrics collector |
| SRS-INT-006 | DES-MON-003 | Service metrics collector |

---

*Document Version: 1.0.0*
*Created: 2026-01-04*
*Author: kcenon@naver.com*
