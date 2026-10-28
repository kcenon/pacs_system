---
doc_id: "PAC-PERF-002"
doc_title: "PACS System Performance Benchmarks"
doc_version: "1.1.0"
doc_date: "2026-10-23"
doc_status: "Released"
project: "pacs_system"
category: "PERF"
---

# PACS System Performance Benchmarks

> **SSOT**: This document is the single source of truth for **PACS System Performance Benchmarks**.

> **Language:** **English** | [한국어](BENCHMARKS.kr.md)

**Last Updated**: 2026-10-23

This document holds the benchmark suites and targets, every result recorded in the repository
with its environment, date, command, and result file, and the commands that run the benchmarks.
The README links here instead of quoting figures.

---

## Table of Contents

- [Overview](#overview)
- [Recorded Results](#recorded-results)
- [Benchmark Categories](#benchmark-categories)
- [Running Benchmarks](#running-benchmarks)

---

## Overview

| Suite | Source | Executables | What it measures |
|-------|--------|-------------|------------------|
| Thread performance | `benchmarks/thread_performance/` | `thread_performance_benchmarks` | Association setup, C-ECHO and C-STORE throughput, concurrent load, shutdown time |
| SIMD performance | `benchmarks/simd_performance/` | `simd_performance_benchmarks`, `photometric_benchmark`, `windowing_benchmark` | Byte swapping, RLE, photometric conversion, window/level |

The codec, storage, and query categories below are planned: they define targets only, and the
repository has no benchmark source for them yet.

See also [PERFORMANCE_BASELINE.md](PERFORMANCE_BASELINE.md) for the thread migration baseline
targets and [PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md) for the full report of the recorded run.

---

## Recorded Results

### Thread migration run (Issue #160)

This is the only benchmark run recorded in the repository. The values are copied from
[PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md). This table replaces the "Key Performance Metrics"
table that the README carried until #1212.

| Item | Record |
|------|--------|
| Environment | macOS 26.2 (Darwin), Apple M4 Max (ARM64), 36 GB memory, Apple Clang 17.0.0, Release build, C++20 ([Test Environment](PERFORMANCE_RESULTS.md#test-environment)) |
| Date | Not recorded reliably. The report originally stated 2024-12-05, but the benchmark sources were added on 2025-12-04 (`7c59a5d7`, PR #164), the commit that added the report (`501957b0`, PR #171) is dated 2025-12-05, and macOS 26.2 and Apple Clang 17.0.0 were not available on 2024-12-05. |
| Command | Not recorded. The report names the four `thread_performance_benchmarks` suites (association, throughput, concurrent load, shutdown); [Running Benchmarks](#running-benchmarks) lists commands that run them. |
| Result file | [PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md) (summary tables). Raw benchmark output is not recorded in the repository. |
| Setup | Client and `benchmark_server` fixture in one process over `localhost` (`benchmarks/thread_performance/benchmark_common.h`). The report attributes the high single-worker rates to in-memory benchmark operations, so these values are not network throughput between hosts. |

| Metric | Target | Recorded | Report status | Report section |
|--------|--------|----------|---------------|----------------|
| Single association (connect) | < 100 ms | 0.00134 ms | PASS | 1 |
| Connect + release round-trip | < 200 ms | 0.00267 ms | PASS | 1 |
| Connect + C-ECHO + release | < 150 ms | 468.7 ms | INVESTIGATE | 1 |
| Sequential connection rate | > 10 conn/s | 419,655 conn/s | PASS | 1 |
| C-ECHO, single connection | > 100 msg/s | 89,964 echo/s | PASS | 2 |
| C-ECHO, sustained 10 s | > 80 msg/s | 132,391 msg/s | PASS | 2 |
| C-STORE, single connection | > 20 store/s | 31,759 store/s | PASS | 2 |
| C-STORE data rate, 512x512 (512 KB) images | > 5 MB/s | 9,247 MB/s (18,494 store/s) | Not stated | 2 |
| Concurrent C-ECHO, 10 workers | > 50 ops/s | 124 echo/s | PASS | 3 |
| Concurrent C-STORE, 5 workers | > 10 ops/s | 49.6 store/s | PASS | 3 |
| Shutdown with 5 active connections | < 5000 ms | 107 ms | PASS | 4 |
| Shutdown under load (3 workers) | < 10000 ms | 110 ms | PASS | 4 |

The data-rate target comes from [PERFORMANCE_BASELINE.md](PERFORMANCE_BASELINE.md); the report
lists that row without a status. The report explains the Connect + C-ECHO + release value as
benchmark fixture timing ("Known Issues and Investigations"); no later measurement is recorded.

**Reconciliation with the former README table.** The README's "Association Latency < 1 ms" row
took the connect and round-trip values and left out the Connect + C-ECHO + release value
(468.7 ms, INVESTIGATE). Its "Graceful Shutdown 110 ms (with active connections)" row paired the
under-load value (3 workers) with the active-connections label; the active-connections value is
107 ms. Its C-ECHO, C-STORE, concurrent operations, and 512x512 data-rate rows match the report.

### SIMD benchmarks

No SIMD result with an environment and date is recorded in the repository, so
[FEATURES.md](FEATURES.md#simd-optimization) gives no SIMD speedup figure.

### Earlier reference platform

Earlier revisions of this document listed a reference platform: Intel i7-12700K @ 3.2GHz
(12 cores, 20 threads), 32GB DDR4-3200, NVMe SSD, Ubuntu 22.04 LTS, GCC 13.1 with `-O2`.
No result in the repository was recorded on it.

---

## Benchmark Categories

### DICOM Transfer Performance

Measures DICOM network protocol performance including association establishment, message
exchange, and data transfer (`benchmarks/thread_performance/`).

| Metric | Target | Description |
|--------|--------|-------------|
| Single connection | < 100 ms | TCP + A-ASSOCIATE negotiation |
| Full round-trip | < 200 ms | Connect + Release |
| Connection + C-ECHO | < 150 ms | Connect + Single echo + Release |
| Sequential rate | > 10 conn/s | Rapid sequential connections |
| C-ECHO throughput | > 100 msg/s | Verification messages (single conn) |
| C-ECHO sustained | > 80 msg/s | Sustained throughput over 10s |
| C-STORE throughput | > 20 store/s | Small images (64x64) |
| Data rate (512x512) | > 5 MB/s | Large image transfer |

### Codec Performance (Planned)

No codec benchmark source exists yet, so this table defines targets only. The SIMD
suite's `rle_simd_benchmark.cpp` measures the SIMD helper steps that the RLE codec uses (RGB
planar conversion, 16-bit plane split and merge), not RLE compression or decompression.

| Codec | Operation | Target | Notes |
|-------|-----------|--------|-------|
| JPEG | Compress | Measured per image | Lossy baseline (Process 1) |
| JPEG | Decompress | Measured per image | - |
| JPEG 2000 | Compress | Measured per image | Lossy and lossless |
| JPEG 2000 | Decompress | Measured per image | - |
| JPEG-LS | Compress | Measured per image | Near-lossless |
| JPEG-LS | Decompress | Measured per image | - |
| RLE | Compress | Measured per image | Lossless, DICOM standard RLE |
| RLE | Decompress | Measured per image | - |

### Storage Performance (Planned)

No storage benchmark source exists yet, so this table defines targets only.

| Backend | Operation | Metric | Description |
|---------|-----------|--------|-------------|
| Filesystem | Write | MB/s | Raw DICOM file write throughput |
| Filesystem | Read | MB/s | DICOM file read throughput |
| Cloud (S3) | Upload | Latency (ms) | Single object upload latency |
| Cloud (S3) | Download | Latency (ms) | Single object download latency |
| Database | Index | ops/s | Metadata indexing speed |

### Query Performance (Planned)

No query benchmark source exists yet, so this table defines targets only.

| Query Type | Target | Description |
|------------|--------|-------------|
| C-FIND (Patient) | < 50 ms | Patient-level query |
| C-FIND (Study) | < 100 ms | Study-level query |
| C-FIND (Series) | < 100 ms | Series-level query |
| QIDO-RS (paginated) | < 200 ms | DICOMweb query with pagination |

---

## Running Benchmarks

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPACS_BUILD_BENCHMARKS=ON
cmake --build build
```

Both suites need Catch2, which CMake fetches only while `PACS_BUILD_TESTS` is `ON` (the
default; see `cmake/testing.cmake` and `cmake/examples.cmake`).

### Thread performance suite

```bash
# Run all thread performance benchmarks
cmake --build build --target run_full_benchmarks

# Quick subset: association and C-ECHO throughput
cmake --build build --target run_quick_benchmarks

# JUnit output in build/benchmark_results.xml
cmake --build build --target run_benchmarks_ci

# Run one category
./build/bin/thread_performance_benchmarks "[benchmark][association]"
./build/bin/thread_performance_benchmarks "[benchmark][throughput]"
./build/bin/thread_performance_benchmarks "[benchmark][concurrent]"
./build/bin/thread_performance_benchmarks "[benchmark][shutdown]"
```

### SIMD suite

```bash
cmake --build build --target run_simd_benchmarks_full     # byte swap and RLE
cmake --build build --target run_simd_benchmarks_quick    # summary only
cmake --build build --target run_photometric_benchmark
cmake --build build --target run_windowing_benchmark
```

When you record a new result here, include the environment, the date of the run, the exact
command, and a link to the result file in the repository.
