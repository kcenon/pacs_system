# Pipeline Design Documentation

**Version:** 1.0.0
**Date:** 2025-01-08
**Related Issue:** #517 - Implement typed_thread_pool-based I/O Pipeline for DICOM Operations

## Overview

This document describes the 6-stage I/O pipeline architecture implemented for high-throughput DICOM operations. The pipeline leverages `typed_thread_pool` from the thread_system library to achieve parallel processing across multiple stages while maintaining DICOM protocol correctness.

### Design Goals

1. **High Throughput**: Target 150,000+ C-ECHO/s and 50,000+ C-STORE/s
2. **Low Latency**: P99 latency <= 5ms for C-ECHO operations
3. **Scalability**: Support 500+ concurrent associations
4. **Maintainability**: Standard threading model without C++20 coroutines

### Why Pipeline Architecture?

Traditional single-threaded DICOM handlers suffer from head-of-line blocking:

```
Traditional: [Network I/O] → [Decode] → [Process] → [DB I/O] → [Encode] → [Send]
             ────────────────────── Sequential ──────────────────────────
             Total latency = Sum of all stages
```

The pipeline architecture allows concurrent processing:

```
Pipeline:    Stage 1  Stage 2  Stage 3  Stage 4  Stage 5  Stage 6
             [Net RX] [Decode] [DIMSE]  [DB I/O] [Encode] [Net TX]
Request 1:   ────────►────────►────────►────────►────────►────────►
Request 2:            ────────►────────►────────►────────►────────►
Request 3:                     ────────►────────►────────►────────►
             ────────────────── Concurrent ─────────────────────────
```

---

## Pipeline Architecture

### 6-Stage Design

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Pipeline Coordinator                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐   │
│  │  Stage 1        │     │  Stage 2        │     │  Stage 3        │   │
│  │  Network I/O    │────►│  PDU Decode     │────►│  DIMSE Process  │   │
│  │  (Receive)      │     │                 │     │                 │   │
│  │  4 workers      │     │  2 workers      │     │  2 workers      │   │
│  └─────────────────┘     └─────────────────┘     └─────────────────┘   │
│         │                        │                        │             │
│         │ PDU Bytes              │ Decoded PDU            │ Service Req │
│         ▼                        ▼                        ▼             │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐   │
│  │  Stage 6        │     │  Stage 5        │     │  Stage 4        │   │
│  │  Network I/O    │◄────│  Response       │◄────│  Storage/Query  │   │
│  │  (Send)         │     │  Encoding       │     │  Execution      │   │
│  │  4 workers      │     │  2 workers      │     │  8 workers      │   │
│  └─────────────────┘     └─────────────────┘     └─────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Stage Details

| Stage | Name | Workers | Purpose | Blocking |
|-------|------|---------|---------|----------|
| 1 | `network_receive` | 4 | Receive PDU bytes from TCP socket | No |
| 2 | `pdu_decode` | 2 | Decode PDU into structured data | No |
| 3 | `dimse_process` | 2 | Parse DIMSE and route to handlers | No |
| 4 | `storage_query_exec` | 8 | Execute C-STORE/C-FIND/C-GET/C-MOVE | **Yes** |
| 5 | `response_encode` | 2 | Encode response into PDU format | No |
| 6 | `network_send` | 4 | Send PDU bytes to TCP socket | No |

**Key Design Decisions:**

1. **Stage 4 has the most workers (8)** because database and file I/O are blocking operations
2. **Stages 1 & 6 share worker count (4 each)** for balanced network I/O
3. **Stages 2, 3, 5 have fewer workers (2 each)** as they are CPU-bound and fast

---

## Component Reference

### File Structure

```
include/pacs/network/pipeline/
├── pipeline_job_types.hpp        # Stage enum, job categories, job_context
├── pipeline_coordinator.hpp      # Main coordinator class
├── pipeline_adapter.hpp          # Integration with existing handlers
├── jobs/
│   ├── receive_network_io_job.hpp
│   ├── send_network_io_job.hpp
│   ├── pdu_decode_job.hpp
│   ├── dimse_process_job.hpp
│   ├── storage_query_exec_job.hpp
│   └── response_encode_job.hpp
└── metrics/
    └── pipeline_metrics.hpp      # Throughput and latency metrics

src/network/pipeline/
├── pipeline_coordinator.cpp
├── pipeline_adapter.cpp
└── jobs/
    ├── receive_network_io_job.cpp
    ├── send_network_io_job.cpp
    ├── pdu_decode_job.cpp
    ├── dimse_process_job.cpp
    ├── storage_query_exec_job.cpp
    └── response_encode_job.cpp
```

### Core Classes

#### `pipeline_coordinator`

The central orchestrator that manages all pipeline stages:

```cpp
// Create with custom configuration
pipeline_config config;
config.execution_workers = 16;  // More DB workers for heavy load
config.max_queue_depth = 5000;  // Lower backpressure threshold

auto coordinator = std::make_shared<pipeline_coordinator>(config);

// Start the pipeline
auto result = coordinator->start();
if (!result) {
    // Handle startup error
}

// Submit a job to Stage 1
auto job = std::make_unique<receive_network_io_job>(session, buffer);
coordinator->submit_to_stage(pipeline_stage::network_receive, std::move(job));

// Monitor metrics
auto& metrics = coordinator->get_metrics();
auto echo_throughput = metrics.get_throughput_per_second(job_category::echo);

// Graceful shutdown
coordinator->stop();
```

#### `pipeline_config`

Configuration options for tuning pipeline behavior:

```cpp
struct pipeline_config {
    size_t net_io_workers = 4;        // Stages 1 & 6
    size_t protocol_workers = 2;       // Stages 2 & 3
    size_t execution_workers = 8;      // Stage 4
    size_t encode_workers = 2;         // Stage 5
    size_t max_queue_depth = 10000;    // Backpressure threshold
    std::chrono::milliseconds shutdown_timeout{500};
    bool enable_metrics = true;
    std::string name_prefix = "pipeline";
};
```

#### `pipeline_job_base`

Abstract base class for all pipeline jobs:

```cpp
class pipeline_job_base {
public:
    virtual ~pipeline_job_base() = default;

    // Execute the job (called by worker thread)
    virtual auto execute(pipeline_coordinator& coordinator) -> VoidResult = 0;

    // Get job context for tracking
    virtual auto get_context() const noexcept -> const job_context& = 0;

    // Get job name for logging
    virtual auto get_name() const -> std::string = 0;
};
```

#### `job_context`

Context passed through all stages for end-to-end tracking:

```cpp
struct job_context {
    uint64_t job_id{0};           // Unique job identifier
    uint64_t session_id{0};       // Association/session ID
    uint16_t message_id{0};       // DIMSE message ID
    pipeline_stage stage;          // Current stage
    job_category category;         // echo/store/find/get/move
    uint64_t enqueue_time_ns{0};  // Timestamp for latency calculation
    uint32_t sequence_number{0};  // Ordering within session
    uint8_t priority{128};        // 0 = highest priority
};
```

---

## Worker Tuning Guidelines

### Default Configuration Rationale

The default worker counts are tuned for a typical PACS workload:

| Stage | Workers | Rationale |
|-------|---------|-----------|
| Network I/O | 4 | Non-blocking socket operations, low CPU |
| PDU Decode | 2 | Fast CPU-bound parsing |
| DIMSE Process | 2 | Lightweight routing logic |
| Storage/Query | 8 | **Blocking I/O bottleneck**, needs parallelism |
| Response Encode | 2 | Fast CPU-bound serialization |

### Tuning for Specific Workloads

#### High C-STORE Throughput (Image Archive)

```cpp
pipeline_config config;
config.execution_workers = 16;    // More DB/file I/O workers
config.net_io_workers = 8;        // Handle more concurrent uploads
config.max_queue_depth = 20000;   // Allow deeper queues
```

**When to use:** Primary archive servers receiving images from multiple modalities.

#### Low Latency C-ECHO (Health Check)

```cpp
pipeline_config config;
config.protocol_workers = 4;      // Faster decode/process
config.execution_workers = 4;     // Quick DB lookup
config.max_queue_depth = 1000;    // Tight backpressure
```

**When to use:** Servers that need fast response times for monitoring.

#### High Concurrent Connections (Multi-Tenant)

```cpp
pipeline_config config;
config.net_io_workers = 16;       // Handle many connections
config.execution_workers = 32;    // More parallel DB access
config.max_queue_depth = 50000;   // Larger queue capacity
```

**When to use:** Central PACS serving multiple departments.

#### Memory-Constrained Environment

```cpp
pipeline_config config;
config.net_io_workers = 2;
config.protocol_workers = 1;
config.execution_workers = 4;
config.encode_workers = 1;
config.max_queue_depth = 2000;
```

**When to use:** Edge devices or containerized deployments.

### Worker Count Formula

As a general guideline:

```
Total workers = net_io_workers × 2 + protocol_workers × 2 +
                execution_workers + encode_workers

Recommended: Total workers <= CPU cores × 2
Memory per worker: ~100KB stack + job-specific heap
```

### Identifying Bottlenecks

Use the metrics API to identify which stage is the bottleneck:

```cpp
auto& metrics = coordinator->get_metrics();

// Check queue depths
for (int i = 0; i < 6; ++i) {
    auto stage = static_cast<pipeline_stage>(i);
    auto depth = coordinator->get_queue_depth(stage);
    if (depth > config.max_queue_depth * 0.8) {
        // Stage is approaching backpressure
        LOG_WARN("Stage {} queue depth: {}", get_stage_name(stage), depth);
    }
}

// Check throughput per category
auto echo_tps = metrics.get_throughput_per_second(job_category::echo);
auto store_tps = metrics.get_throughput_per_second(job_category::store);
```

---

## Backpressure and Flow Control

### Backpressure Mechanism

When a stage's queue depth exceeds `max_queue_depth`:

1. **Callback invoked**: `backpressure_callback(stage, queue_depth)`
2. **New jobs may be rejected** or throttled
3. **Upstream stages slow down** naturally

```cpp
coordinator->set_backpressure_callback(
    [](pipeline_stage stage, size_t depth) {
        LOG_WARN("Backpressure active on stage {}: {} jobs queued",
                 get_stage_name(stage), depth);
        // Optionally reject new connections or send DICOM busy status
    }
);
```

### Queue Depth Thresholds

| Threshold | Action |
|-----------|--------|
| < 50% | Normal operation |
| 50-80% | Log warning, consider scaling |
| 80-100% | Backpressure callback, throttle intake |
| 100% | Reject new jobs until queue drains |

### Graceful Degradation

For production systems, implement graceful degradation:

```cpp
if (coordinator->is_backpressure_active(pipeline_stage::storage_query_exec)) {
    // Option 1: Return DICOM status 0xA700 (Out of Resources)
    // Option 2: Queue request to persistent storage for later processing
    // Option 3: Redirect to another server
}
```

---

## Metrics and Monitoring

### Available Metrics

The `pipeline_metrics` class provides atomic counters for:

| Metric | Description |
|--------|-------------|
| `jobs_submitted` | Total jobs submitted per category |
| `jobs_completed` | Total jobs completed successfully |
| `jobs_failed` | Total jobs that failed |
| `total_latency_ns` | Cumulative latency for throughput calculation |
| `stage_queue_depths` | Current queue depth per stage |

### Calculating Throughput

```cpp
auto& metrics = coordinator->get_metrics();

// Get throughput (jobs per second)
double echo_tps = metrics.get_throughput_per_second(job_category::echo);
double store_tps = metrics.get_throughput_per_second(job_category::store);

// Get average latency
auto avg_latency = metrics.get_average_latency(job_category::store);
```

### Integration with Monitoring Systems

Export metrics to Prometheus/Grafana:

```cpp
// Prometheus-style metrics
void export_prometheus_metrics(const pipeline_metrics& metrics) {
    std::cout << "# HELP pacs_pipeline_throughput_total Jobs per second\n";
    std::cout << "# TYPE pacs_pipeline_throughput_total gauge\n";
    std::cout << "pacs_pipeline_throughput_total{category=\"echo\"} "
              << metrics.get_throughput_per_second(job_category::echo) << "\n";
    std::cout << "pacs_pipeline_throughput_total{category=\"store\"} "
              << metrics.get_throughput_per_second(job_category::store) << "\n";
}
```

---

## Integration Guide

### Migration from Traditional Handler

The `pipeline_adapter` bridges the pipeline with existing `dicom_association_handler`:

```cpp
// Before: Direct handler invocation
handler->handle_c_store(request, response);

// After: Pipeline-based processing
auto adapter = std::make_shared<pipeline_adapter>(coordinator, handler);
adapter->submit_c_store(session_id, request);
// Response delivered via callback
```

### Session Lifecycle

1. **Association establishment**: Create session context
2. **Request processing**: Jobs flow through pipeline stages
3. **Association release**: Flush pending jobs, clean up

```cpp
// On A-ASSOCIATE
auto session_ctx = create_session_context(association);
adapter->register_session(session_ctx);

// On DIMSE request
adapter->submit_request(session_ctx.id, pdu);

// On A-RELEASE
adapter->unregister_session(session_ctx.id);
coordinator->flush_session(session_ctx.id);  // Wait for pending jobs
```

---

## Performance Targets

### Current vs. Target

| Metric | Current (Single-threaded) | Target (Pipeline) |
|--------|--------------------------|-------------------|
| C-ECHO throughput | 89,964 msg/s | 150,000+ msg/s |
| C-STORE throughput | 31,759 store/s | 50,000+ store/s |
| Concurrent associations | 100+ | 500+ |
| P99 latency (C-ECHO) | N/A | <= 5ms |
| Graceful shutdown | N/A | < 500ms |

### Theoretical Improvement

```
Single-threaded (sequential):
  Latency = Network(1ms) + Decode(2ms) + Process(1ms) +
            DB(10ms) + Encode(2ms) + Send(1ms) = 17ms
  Throughput = 1000/17 ≈ 59 ops/sec

Pipeline (8 execution workers):
  Bottleneck stage = Stage 4 (DB I/O) = 10ms
  Throughput = 8 workers × (1000/10) = 800 ops/sec

Improvement = 800/59 ≈ 13.5x
```

---

## Testing

### Unit Tests

```bash
# Run pipeline unit tests
ctest --test-dir build -R "pipeline" --output-on-failure
```

Test coverage includes:
- Pipeline coordinator lifecycle (start/stop)
- Job submission and routing
- Backpressure handling
- Metrics collection
- Concurrent job processing

### Integration Tests

```bash
# Run integration tests
ctest --test-dir build -R "pipeline_integration" --output-on-failure
```

Integration tests verify:
- End-to-end DICOM flow through all stages
- Graceful shutdown with pending jobs
- Multi-session concurrent processing

---

## Troubleshooting

### Common Issues

#### High Queue Depth on Stage 4

**Symptom:** Stage 4 queue grows while others remain low.

**Cause:** Database or file I/O is the bottleneck.

**Solution:**
```cpp
config.execution_workers = 16;  // Increase DB workers
// Or optimize database queries/indices
```

#### Jobs Stuck in Pipeline

**Symptom:** Jobs submitted but never complete.

**Cause:** Worker thread deadlock or infinite loop in job.

**Solution:**
```bash
# Enable debug logging
export PACS_LOG_LEVEL=DEBUG
# Check for thread starvation
top -H -p $(pgrep pacs_server)
```

#### High P99 Latency

**Symptom:** Occasional slow responses despite good average latency.

**Cause:** Queue depth spikes or GC pauses.

**Solution:**
```cpp
config.max_queue_depth = 1000;  // Lower threshold
config.enable_metrics = true;    // Monitor queue depths
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2025-01-08 | Initial pipeline design documentation |

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - Overall system architecture
- [PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md) - Benchmark results
- [THREAD_POOL_MIGRATION.md](THREAD_POOL_MIGRATION.md) - Thread system migration

## Related Issues

- #517 - Implement typed_thread_pool-based I/O Pipeline (Parent Epic)
- #518 - Phase 1: Pipeline Infrastructure
- #519 - Phase 2: Network I/O Jobs
- #520 - Phase 3: Protocol Handling Jobs
- #521 - Phase 4: Execution Jobs
- #522 - Phase 5: Response Jobs
- #523 - Phase 6: Integration
- #525 - Phase 8: Documentation (This document)
