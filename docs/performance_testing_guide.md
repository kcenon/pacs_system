# PACS System Performance Testing Guide

## Overview

The PACS System includes a comprehensive performance testing framework designed to measure and monitor system performance across various components.

## Performance Test Framework

### Architecture

The framework provides:
- Automated performance measurement
- Statistical analysis of results
- Multiple output formats (console, JSON, CSV)
- Baseline comparison
- Concurrent execution support
- Memory and CPU profiling capabilities

### Key Components

1. **PerfTest**: Base class for all performance tests
2. **PerfTestRunner**: Executes tests and collects metrics
3. **PerfBenchmark**: Helper utilities for measurements
4. **LoadGenerator**: Tools for stress testing

## Running Performance Tests

### Basic Usage

```bash
# Run all DICOM performance tests
./dicom_perf_tests

# Run with custom iterations
./dicom_perf_tests --iterations 5000

# Run with multiple threads
./dicom_perf_tests --threads 4

# Output to JSON
./dicom_perf_tests --output json

# Output to CSV
./dicom_perf_tests --output csv
```

### Available Test Suites

1. **DICOM Performance Tests** (`dicom_perf_tests`)
   - Dataset parsing
   - Query operations
   - Connection pooling
   - Storage operations
   - Concurrent operations
   - Large dataset handling

2. **Database Performance Tests** (`db_perf_tests`)
   - INSERT operations
   - Batch operations
   - SELECT queries
   - Complex JOINs
   - UPDATE/DELETE
   - Index performance
   - Concurrent access

3. **Network Performance Tests** (`network_perf_tests`)
   - Connection pool creation
   - Borrow/return cycles
   - Concurrent access
   - Pool exhaustion
   - Retry policies
   - Circuit breaker
   - High throughput

## Writing Performance Tests

### Simple Test Example

```cpp
#include "performance_test_framework.h"

PERF_TEST(MyPerformanceTest) {
    // Setup code (not measured)
    auto data = prepareTestData();
    
    // This code is measured
    auto result = performOperation(data);
    
    if (!result.isOk()) {
        return core::Result<void>::error("Operation failed");
    }
    
    return core::Result<void>::ok();
}
```

### Test with Setup/Teardown

```cpp
class ComplexPerfTest : public pacs::perf::PerfTest {
public:
    ComplexPerfTest() : PerfTest("ComplexTest") {}
    
    core::Result<void> setup() override {
        // Initialize resources
        database_ = createTestDatabase();
        return core::Result<void>::ok();
    }
    
    core::Result<void> execute() override {
        // Measured operation
        return database_->performComplexQuery();
    }
    
    void teardown() override {
        // Cleanup
        database_.reset();
    }
    
private:
    std::unique_ptr<Database> database_;
};
```

## Performance Metrics

### Collected Metrics

Each test collects:
- **Timing**: min, max, mean, median, stddev, p95, p99
- **Throughput**: Operations per second
- **Memory** (optional): Initial, peak, final usage
- **Custom metrics**: Test-specific measurements

### Understanding Results

```
Test Name                     Iterations  Mean (ms)   P95 (ms)    P99 (ms)    Throughput    Status
----------------------------- ----------- ----------- ----------- ----------- -------------- -------
DicomDatasetParsing           1000        2.34        3.12        4.56        427.35 ops/s  PASSED
DicomQuery                    1000        0.45        0.62        0.89        2222.22 ops/s PASSED
ConnectionPoolBorrow          1000        0.12        0.18        0.25        8333.33 ops/s PASSED
```

## Configuration Options

### Test Configuration

```cpp
PerfTestConfig config;
config.iterations = 10000;          // Number of test iterations
config.warmupIterations = 1000;     // Warmup runs (not measured)
config.maxDuration = 300s;          // Maximum test duration
config.concurrentThreads = 8;       // Parallel execution threads
config.measureMemory = true;        // Enable memory profiling
config.outputFormat = "json";       // Output format
```

### Environment Variables

```bash
# Set log level
export PACS_LOG_LEVEL=DEBUG

# Set test data directory
export PACS_TEST_DATA=/path/to/test/data

# Enable detailed timing
export PACS_PERF_DETAILED=1
```

## Performance Baselines

### Setting Baselines

```cpp
// Load baseline results
std::vector<PerfTestResult> baseline = loadBaselineFromFile("baseline.json");
runner.setBaseline(baseline);

// Results will show comparison
// vs Baseline: +15.2%  (15.2% improvement)
```

### Baseline File Format

```json
{
  "results": [
    {
      "name": "DicomDatasetParsing",
      "timing": {
        "mean": 2.5,
        "p95": 3.5,
        "p99": 5.0
      },
      "throughput": 400.0
    }
  ]
}
```

## Load Testing

### Constant Load

```cpp
LoadGenerator::constantLoad(
    []() { 
        // Operation to test
        performDicomQuery();
    },
    100,    // Requests per second
    60s     // Duration
);
```

### Ramp-up Load

```cpp
LoadGenerator::rampUpLoad(
    []() { 
        // Operation to test
        storeImage();
    },
    10,     // Start RPS
    1000,   // End RPS
    300s    // Ramp duration
);
```

## Integration with CI/CD

### CMake Integration

```cmake
# Run performance tests
add_custom_target(perf_tests
    COMMAND dicom_perf_tests --output perf_results.json
    COMMAND db_perf_tests --output db_perf_results.json
    COMMAND network_perf_tests --output net_perf_results.json
    DEPENDS dicom_perf_tests db_perf_tests network_perf_tests
)
```

### GitHub Actions Example

```yaml
- name: Run Performance Tests
  run: |
    mkdir -p perf_results
    ./build/tests/performance/dicom_perf_tests --output perf_results/dicom.json
    ./build/tests/performance/db_perf_tests --output perf_results/db.json
    
- name: Compare with Baseline
  run: |
    python scripts/compare_perf.py perf_results baseline.json
    
- name: Upload Results
  uses: actions/upload-artifact@v2
  with:
    name: performance-results
    path: perf_results/
```

## Performance Optimization Tips

### 1. Connection Pooling

```cpp
// Configure appropriate pool sizes
ConnectionPool<DicomConnection>::Config config;
config.minSize = 10;      // Based on average load
config.maxSize = 50;      // Based on peak load
config.maxIdleTime = 300; // 5 minutes
```

### 2. Database Optimization

```cpp
// Use prepared statements
auto stmt = db->prepare("SELECT * FROM studies WHERE patient_id = ?");

// Enable query result caching
db->enableQueryCache(true);

// Use appropriate indexes
db->createIndex("idx_study_date", "studies", {"study_date"});
```

### 3. Memory Management

```cpp
// Pre-allocate buffers
std::vector<uint8_t> buffer;
buffer.reserve(1024 * 1024); // 1MB

// Use object pools for frequently created objects
ObjectPool<DcmDataset> datasetPool(100);
```

## Troubleshooting

### High Variance in Results

- Increase warmup iterations
- Check for background processes
- Disable CPU frequency scaling
- Run tests on dedicated hardware

### Memory Leaks

Enable memory profiling:
```cpp
config.measureMemory = true;
```

Check for increasing memory usage across iterations.

### Inconsistent Results

- Use process affinity to pin to specific CPU cores
- Disable hyperthreading for consistent results
- Run multiple test runs and average results

## Best Practices

1. **Isolate Tests**: Each test should be independent
2. **Realistic Data**: Use production-like test data
3. **Multiple Runs**: Average results from multiple runs
4. **Monitor System**: Check CPU, memory, I/O during tests
5. **Version Control**: Track performance over time
6. **Document Changes**: Note configuration/environment changes

## Reporting Issues

When reporting performance issues:

1. Include full test output
2. System specifications
3. Configuration used
4. Baseline comparison (if available)
5. Steps to reproduce

Example:
```
System: Ubuntu 22.04, 16 cores, 32GB RAM
Config: 1000 iterations, 4 threads
Issue: ConnectionPoolBorrow degraded 30% vs baseline
```