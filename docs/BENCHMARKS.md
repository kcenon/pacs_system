# PACS System Performance Benchmarks

> **Language:** **English** | [한국어](BENCHMARKS.kr.md)

**Last Updated**: 2026-03-18

This document provides comprehensive performance metrics, benchmarking methodologies, and results for the PACS System.

---

## Table of Contents

- [Overview](#overview)
- [Benchmark Categories](#benchmark-categories)
- [DICOM Transfer Performance](#dicom-transfer-performance)
- [Codec Performance](#codec-performance)
- [Storage Performance](#storage-performance)
- [Query Performance](#query-performance)
- [Running Benchmarks](#running-benchmarks)

---

## Overview

The PACS System provides performance benchmarks covering DICOM operations throughput, codec processing, storage I/O, and query response times.

**Current Status**:
- Association and transfer benchmarks defined with baseline targets
- Codec benchmarks for compression/decompression throughput
- Storage benchmarks for filesystem and cloud backends

See also [PERFORMANCE_BASELINE.md](PERFORMANCE_BASELINE.md) for thread migration baseline measurements.

---

## Benchmark Categories

### DICOM Transfer Performance

Measures DICOM network protocol performance including association establishment, message exchange, and data transfer.

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

### Codec Performance

Measures compression and decompression throughput for supported transfer syntaxes.

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

### Storage Performance

Measures storage backend I/O throughput.

| Backend | Operation | Metric | Description |
|---------|-----------|--------|-------------|
| Filesystem | Write | MB/s | Raw DICOM file write throughput |
| Filesystem | Read | MB/s | DICOM file read throughput |
| Cloud (S3) | Upload | Latency (ms) | Single object upload latency |
| Cloud (S3) | Download | Latency (ms) | Single object download latency |
| Database | Index | ops/s | Metadata indexing speed |

### Query Performance

Measures DICOM query/retrieve and DICOMweb query performance.

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
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPACS_BUILD_TESTS=ON
cmake --build build
```

### Execute

```bash
# Run all benchmarks
./build/bin/pacs_benchmarks

# Run specific benchmark category
./build/bin/pacs_benchmarks --filter=Association
./build/bin/pacs_benchmarks --filter=Codec
./build/bin/pacs_benchmarks --filter=Storage
```

### Configuration

Benchmark behavior can be configured via environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `PACS_BENCH_ITERATIONS` | 100 | Number of iterations per benchmark |
| `PACS_BENCH_WARMUP` | 10 | Warmup iterations |
| `PACS_BENCH_OUTPUT` | stdout | Output format (stdout, json, csv) |

---

## Results

Benchmark results vary by hardware configuration and network environment. See `PERFORMANCE_RESULTS.md` for the latest documented results.

**Reference Platform**:
- **CPU**: Intel i7-12700K @ 3.2GHz (12 cores, 20 threads)
- **RAM**: 32GB DDR4-3200
- **Storage**: NVMe SSD
- **OS**: Ubuntu 22.04 LTS
- **Compiler**: GCC 13.1, -O2
