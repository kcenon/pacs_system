# PACS System Performance Documentation

This directory contains performance-related documentation for PACS System.

## Documents

### Benchmarks

- [BENCHMARKS.md](../BENCHMARKS.md) - Performance benchmark definitions, targets, and how to run them
- [BENCHMARKS.kr.md](../BENCHMARKS.kr.md) - Korean version

### Performance Reports

- [PERFORMANCE_BASELINE.md](../PERFORMANCE_BASELINE.md) - Baseline measurements for thread migration
- [PERFORMANCE_RESULTS.md](../PERFORMANCE_RESULTS.md) - Latest performance test results

## Running Performance Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPACS_BUILD_TESTS=ON
cmake --build build
./build/bin/pacs_benchmarks
```

See [BENCHMARKS.md](../BENCHMARKS.md) for detailed instructions and configuration options.
