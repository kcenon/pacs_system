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

> **SSOT**: This document is the single source of truth for **PACS System Performance Documentation**.

This directory contains performance-related documentation for PACS System.

## Documents

### Benchmarks

- [BENCHMARKS.md](../BENCHMARKS.md) - Benchmark suites, targets, recorded results with environment and date, and how to run them
- [BENCHMARKS.kr.md](../BENCHMARKS.kr.md) - Korean version

### Performance Reports

- [PERFORMANCE_BASELINE.md](../PERFORMANCE_BASELINE.md) - Baseline measurements for thread migration
- [PERFORMANCE_RESULTS.md](../PERFORMANCE_RESULTS.md) - Latest performance test results

## Running Performance Tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPACS_BUILD_BENCHMARKS=ON
cmake --build build
cmake --build build --target run_full_benchmarks
```

See [BENCHMARKS.md](../BENCHMARKS.md#running-benchmarks) for the individual suites and targets.
