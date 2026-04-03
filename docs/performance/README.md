---
doc_id: "PAC-GUID-033"
doc_title: "PACS System Performance Documentation"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "GUID"
---

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
