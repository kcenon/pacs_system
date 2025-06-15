# PACS System Connection Pooling and Resilience Guide

## Overview

The PACS system implements advanced connection pooling and resilience patterns to ensure reliable communication with DICOM nodes and database servers in hospital environments.

## Features

- **Connection Pooling**: Reuse connections to reduce overhead
- **Automatic Retry**: Configurable retry policies with backoff
- **Circuit Breaker**: Prevent cascading failures
- **Health Checks**: Automatic validation of connections
- **Resource Management**: Automatic cleanup of idle connections

## Connection Pooling

### DICOM Connection Pool

The DICOM connection pool manages connections to remote DICOM nodes (PACS, modalities, workstations).

#### Basic Usage

```cpp
#include "common/network/dicom_connection_pool.h"

// Configure connection parameters
DicomConnection::Parameters params;
params.remoteHost = "192.168.1.100";
params.remotePort = 11112;
params.remoteAeTitle = "REMOTE_PACS";
params.localAeTitle = "MY_PACS";
params.maxPduSize = 16384;
params.timeout = std::chrono::seconds(30);

// Configure pool
ConnectionPool<DicomConnection>::Config poolConfig;
poolConfig.minSize = 2;
poolConfig.maxSize = 10;
poolConfig.maxIdleTime = 300; // 5 minutes
poolConfig.validateOnBorrow = true;

// Get or create pool
auto& poolManager = DicomConnectionPoolManager::getInstance();
auto pool = poolManager.getPool("REMOTE_PACS", params, poolConfig);

// Execute DICOM operation with automatic connection management
auto result = pool->executeWithConnection("C-STORE", 
    [](DicomConnection* conn) -> core::Result<void> {
        // Use connection for DICOM operations
        auto scu = conn->getSCU();
        // Perform C-STORE operation
        return core::Result<void>::ok();
    });
```

#### Advanced Configuration

```cpp
// Per-modality configuration
std::map<std::string, ConnectionPool<DicomConnection>::Config> modalityConfigs = {
    {"CT_SCANNER", {.minSize = 3, .maxSize = 15, .maxIdleTime = 600}},
    {"MR_SCANNER", {.minSize = 2, .maxSize = 10, .maxIdleTime = 300}},
    {"WORKSTATION", {.minSize = 1, .maxSize = 5, .maxIdleTime = 180}}
};
```

### Database Connection Pool

PostgreSQL connections are automatically pooled:

```cpp
// PostgreSQL configuration includes pooling
PostgreSQLConfig config;
config.minPoolSize = 2;
config.maxPoolSize = 10;

// Connections are automatically managed
auto result = db->executeQuery("SELECT * FROM studies WHERE patient_id = $1", 
                               {patientId});
```

## Retry Policies

### Configuration

```cpp
RetryConfig config;
config.maxAttempts = 3;
config.initialDelay = std::chrono::milliseconds(1000);
config.maxDelay = std::chrono::seconds(30);
config.strategy = RetryStrategy::ExponentialJitter;
config.backoffMultiplier = 2.0;
config.jitterFactor = 0.1;

// Add retryable error patterns
config.addRetryableError("timeout");
config.addRetryableError("connection refused");
config.addRetryableError("network unreachable");
```

### Retry Strategies

1. **Fixed**: Same delay between retries
   ```cpp
   config.strategy = RetryStrategy::Fixed;
   config.initialDelay = std::chrono::seconds(2);
   ```

2. **Exponential**: Exponentially increasing delay
   ```cpp
   config.strategy = RetryStrategy::Exponential;
   config.backoffMultiplier = 2.0; // 1s, 2s, 4s, 8s...
   ```

3. **Exponential with Jitter**: Prevents thundering herd
   ```cpp
   config.strategy = RetryStrategy::ExponentialJitter;
   config.jitterFactor = 0.1; // ±10% randomization
   ```

4. **Linear**: Linear increase
   ```cpp
   config.strategy = RetryStrategy::Linear;
   // 1s, 2s, 3s, 4s...
   ```

5. **Fibonacci**: Fibonacci sequence delays
   ```cpp
   config.strategy = RetryStrategy::Fibonacci;
   // 1s, 1s, 2s, 3s, 5s, 8s...
   ```

### Usage Example

```cpp
RetryPolicy retry(config);

auto result = retry.execute([&]() -> core::Result<DcmDataset*> {
    return performDicomQuery(queryDataset);
});

if (!result.isOk()) {
    logger::logError("Query failed after {} attempts: {}", 
                     config.maxAttempts, result.getError());
}
```

## Circuit Breaker Pattern

### Configuration

```cpp
CircuitBreaker::Config cbConfig;
cbConfig.failureThreshold = 5;     // Open after 5 failures
cbConfig.successThreshold = 2;     // Close after 2 successes
cbConfig.openDuration = std::chrono::seconds(60);
cbConfig.halfOpenTimeout = std::chrono::seconds(10);
```

### States

1. **Closed**: Normal operation, requests pass through
2. **Open**: Too many failures, requests fail immediately
3. **Half-Open**: Testing if service recovered

### Usage

```cpp
CircuitBreaker cb("RemotePACS", cbConfig);

auto result = cb.execute([&]() -> core::Result<void> {
    return sendImageToRemotePACS(image);
});

// Check circuit state
if (cb.getState() == CircuitBreaker::State::Open) {
    logger::logWarning("Circuit breaker is open for RemotePACS");
}
```

## Resilient Executor

Combines retry policy and circuit breaker:

```cpp
ResilientExecutor executor("ImageTransfer", retryConfig, cbConfig);

auto result = executor.execute([&]() -> core::Result<void> {
    return transferImage(sopInstanceUid);
});
```

## Configuration Examples

### Hospital PACS Configuration

```json
{
  "network": {
    "connection_pools": {
      "main_pacs": {
        "min_size": 5,
        "max_size": 20,
        "max_idle_time": 600,
        "validation_interval": 60
      },
      "modalities": {
        "min_size": 2,
        "max_size": 10,
        "max_idle_time": 300,
        "validation_interval": 30
      }
    },
    "retry": {
      "max_attempts": 3,
      "initial_delay_ms": 1000,
      "max_delay_ms": 30000,
      "strategy": "exponential_jitter"
    },
    "circuit_breaker": {
      "failure_threshold": 5,
      "success_threshold": 2,
      "open_duration_seconds": 60
    }
  }
}
```

### Per-Service Configuration

```cpp
// Critical services - more retries, longer timeouts
struct CriticalServiceConfig {
    static constexpr size_t MAX_RETRIES = 5;
    static constexpr auto TIMEOUT = std::chrono::seconds(60);
    static constexpr size_t POOL_SIZE = 10;
};

// Non-critical services - fewer retries, shorter timeouts
struct StandardServiceConfig {
    static constexpr size_t MAX_RETRIES = 2;
    static constexpr auto TIMEOUT = std::chrono::seconds(30);
    static constexpr size_t POOL_SIZE = 5;
};
```

## Monitoring and Metrics

### Pool Statistics

```cpp
auto stats = pool->getPoolStats();
logger::logInfo("Pool stats - Total: {}, Active: {}, Available: {}", 
                stats.totalSize, stats.activeSize, stats.availableSize);
```

### Circuit Breaker Monitoring

```cpp
auto cbStats = executor.getCircuitBreakerStats();
logger::logInfo("Circuit breaker - State: {}, Failures: {}, Success rate: {:.2f}%",
                cbStats.state, cbStats.failureCount,
                (cbStats.successCount * 100.0) / cbStats.totalCalls);
```

### Health Checks

```cpp
// Periodic health check
void performHealthCheck() {
    auto& poolManager = DicomConnectionPoolManager::getInstance();
    auto allStats = poolManager.getAllPoolStats();
    
    for (const auto& [name, stats] : allStats) {
        if (stats.availableSize == 0 && stats.activeSize == stats.maxSize) {
            logger::logWarning("Connection pool {} is exhausted", name);
            // Trigger alert
        }
    }
}
```

## Best Practices

1. **Pool Sizing**
   - Start with conservative sizes and monitor
   - Set max size based on remote system capacity
   - Consider peak vs average load

2. **Timeout Configuration**
   - Network timeout < Application timeout
   - Consider network latency
   - Balance responsiveness vs reliability

3. **Retry Strategy**
   - Use exponential backoff for network issues
   - Add jitter to prevent thundering herd
   - Limit retries for user-facing operations

4. **Circuit Breaker Tuning**
   - Set thresholds based on SLAs
   - Monitor and adjust based on patterns
   - Consider business impact of open circuits

5. **Resource Management**
   - Set appropriate idle timeouts
   - Enable connection validation
   - Monitor pool exhaustion

## Troubleshooting

### Connection Pool Exhaustion

Symptoms:
- Timeouts waiting for connections
- Increased response times

Solutions:
```cpp
// Increase pool size
poolConfig.maxSize = 20;

// Reduce connection hold time
// Ensure connections are returned promptly

// Add monitoring
if (stats.availableSize == 0) {
    logger::logError("Pool exhausted: {}", poolName);
}
```

### Circuit Breaker Always Open

Symptoms:
- All requests failing immediately
- Circuit breaker not closing

Solutions:
```cpp
// Manual reset for testing
circuitBreaker.reset();

// Adjust thresholds
cbConfig.failureThreshold = 10; // More tolerant
cbConfig.successThreshold = 1;  // Easier to close

// Check downstream service health
```

### Retry Storm

Symptoms:
- Excessive retries
- System overload

Solutions:
```cpp
// Add jitter
config.strategy = RetryStrategy::ExponentialJitter;
config.jitterFactor = 0.2; // 20% randomization

// Implement request coalescing
// Use circuit breaker to fail fast
```

## Integration with Monitoring Systems

### Prometheus Metrics

```cpp
// Export metrics
prometheus::Counter connection_pool_borrows{
    "pacs_connection_pool_borrows_total",
    "Total number of connection borrows"
};

prometheus::Gauge connection_pool_size{
    "pacs_connection_pool_size",
    "Current size of connection pool"
};

prometheus::Counter circuit_breaker_trips{
    "pacs_circuit_breaker_trips_total",
    "Total number of circuit breaker trips"
};
```

### Logging

```cpp
// Structured logging for analysis
logger::logInfo("connection_pool_event", {
    {"event", "borrow"},
    {"pool", poolName},
    {"available", stats.availableSize},
    {"active", stats.activeSize},
    {"wait_time_ms", waitTime.count()}
});
```