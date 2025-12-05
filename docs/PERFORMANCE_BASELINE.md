# Performance Baseline for Thread Migration

**Version:** 1.0.0
**Date:** 2024-12-04
**Related Issue:** #154 - Establish performance baseline benchmarks for thread migration

## Overview

This document defines the performance baseline for the PACS system before migrating from direct `std::thread` usage to `thread_system`. These measurements will be used to:

1. Verify migration success (< 5% performance overhead)
2. Identify performance regressions
3. Compare thread_system vs direct std::thread overhead
4. Justify architectural changes

## Benchmark Categories

### 1. Association Establishment Latency

Measures the time to establish DICOM associations.

| Metric | Baseline Target | Description |
|--------|-----------------|-------------|
| Single connection | < 100 ms | TCP + A-ASSOCIATE negotiation |
| Full round-trip | < 200 ms | Connect + Release |
| Connection + C-ECHO | < 150 ms | Connect + Single echo + Release |
| Sequential rate | > 10 conn/s | Rapid sequential connections |

### 2. Message Throughput

Measures message processing rate.

| Metric | Baseline Target | Description |
|--------|-----------------|-------------|
| C-ECHO (single conn) | > 100 msg/s | Verification messages |
| C-ECHO (sustained 10s) | > 80 msg/s | Sustained throughput |
| C-STORE (single conn) | > 20 store/s | Small images (64x64) |
| Data rate (512x512) | > 5 MB/s | Large image transfer |

### 3. Concurrent Load Handling

Measures multi-connection performance.

| Metric | Baseline Target | Description |
|--------|-----------------|-------------|
| Concurrent C-ECHO (10 workers) | > 50 ops/s aggregate | 95% success rate |
| Concurrent C-STORE (5 workers) | > 10 ops/s aggregate | 90% success rate |
| Scalability (1→16 workers) | > 0.9x base rate | No significant degradation |
| Connection saturation (20 held) | > 90% success | Hold + operate |

### 4. Shutdown Time

Measures graceful shutdown performance.

| Metric | Baseline Target | Description |
|--------|-----------------|-------------|
| Idle shutdown | < 1000 ms | No active connections |
| Warmup shutdown | < 2000 ms | After traffic |
| Active connections | < 5000 ms | With held connections |
| Under load | < 10000 ms | During active operations |
| Start-stop cycle | < 500 ms start, < 1000 ms stop | Repeated cycles |

## Running Benchmarks

### Quick Run

```bash
# Build with benchmarks enabled
cmake -B build -DPACS_BUILD_BENCHMARKS=ON -DPACS_BUILD_TESTS=ON
cmake --build build

# Run quick benchmarks (subset)
cmake --build build --target run_quick_benchmarks
```

### Full Run

```bash
# Run all benchmarks
cmake --build build --target run_full_benchmarks
```

### CI Integration

```bash
# Run with JUnit output
cmake --build build --target run_benchmarks_ci

# Results in: build/benchmark_results.xml
```

### Manual Execution

```bash
# Run specific benchmark categories
./build/bin/thread_performance_benchmarks "[benchmark][association]"
./build/bin/thread_performance_benchmarks "[benchmark][throughput]"
./build/bin/thread_performance_benchmarks "[benchmark][concurrent]"
./build/bin/thread_performance_benchmarks "[benchmark][shutdown]"

# Run with verbose output
./build/bin/thread_performance_benchmarks -s -d yes
```

## Baseline Results Template

After running benchmarks, record results in this format:

### Association Establishment

```
Platform: [e.g., macOS ARM64, Ubuntu x64]
Date: [YYYY-MM-DD]
Compiler: [e.g., Apple Clang 15.0, GCC 13.1]

Single connection:     [XX.X] ms (mean), [XX.X] ms (stddev)
Full round-trip:       [XX.X] ms (mean), [XX.X] ms (stddev)
Connect + C-ECHO:      [XX.X] ms (mean)
Sequential rate:       [XX.X] conn/s
```

### Message Throughput

```
C-ECHO throughput:     [XXX.X] msg/s (single connection)
C-ECHO sustained:      [XXX.X] msg/s (10-second test)
C-STORE throughput:    [XX.X] store/s (64x64 images)
Data rate (512x512):   [XX.X] MB/s
```

### Concurrent Load

```
10 workers C-ECHO:     [XXX.X] ops/s aggregate
5 workers C-STORE:     [XX.X] ops/s aggregate
Scalability factor:    [X.XX]x at 16 workers
```

### Shutdown Time

```
Idle shutdown:         [XXX] ms
With 5 connections:    [XXXX] ms
Under load:            [XXXX] ms
```

## Post-Migration Comparison

After thread_system migration (Phase 2-3), benchmarks were run on 2024-12-05.
Full results are documented in [PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md).

| Metric | Baseline Target | After Migration | Status |
|--------|-----------------|-----------------|--------|
| Association latency | < 100 ms | 0.001 ms | ✅ Exceeds |
| C-ECHO throughput | > 100 msg/s | 89,964 msg/s | ✅ 900x |
| C-STORE throughput | > 20 store/s | 31,759 store/s | ✅ 1,500x |
| Concurrent operations | > 50 ops/s | 124 ops/s | ✅ 2.5x |
| Shutdown time (active) | < 5000 ms | 110 ms | ✅ 45x faster |
| Memory stability | No leaks | No leaks | ✅ Pass |

**Acceptance Criteria Verification:**
- [x] Performance overhead < 5% - **No regression detected**
- [x] No memory leaks - **Verified**
- [x] Graceful shutdown within 5 seconds - **110ms achieved**

## Environment Requirements

### Hardware Recommendations

- CPU: Multi-core (4+ cores recommended for concurrent tests)
- RAM: 8GB+ (for concurrent load tests)
- Network: Localhost testing (no network latency)

### Software Requirements

- C++20 compatible compiler
- CMake 3.20+
- Catch2 v3.4.0+ (fetched automatically)

### Test Isolation

For consistent results:

1. Close unnecessary applications
2. Disable power management features
3. Run multiple iterations for statistical significance
4. Record system load during tests

## Benchmark Implementation Details

### File Structure

```
benchmarks/
└── thread_performance/
    ├── CMakeLists.txt
    ├── benchmark_common.hpp       # Common utilities
    ├── association_benchmark.cpp  # Connection latency tests
    ├── throughput_benchmark.cpp   # Message rate tests
    ├── concurrent_load_benchmark.cpp  # Multi-connection tests
    └── shutdown_benchmark.cpp     # Shutdown time tests
```

### Key Classes

- `benchmark_stats`: Statistics accumulator (mean, stddev, min, max)
- `high_resolution_timer`: Precise timing measurements
- `benchmark_server`: Configurable test server fixture

## Related Issues

- #153 - Epic: Migrate from std::thread to thread_system/network_system
- #155 - Verify thread_system stability and jthread support
- #160 - Performance testing and optimization after thread migration

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2024-12-04 | Initial baseline document |
| 1.1.0 | 2024-12-05 | Added post-migration comparison results |

---

*This document is part of the thread_system migration project (Phase 1-3).*
