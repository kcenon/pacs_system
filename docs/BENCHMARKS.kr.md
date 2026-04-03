---
doc_id: "PAC-PERF-001"
doc_title: "PACS System 성능 벤치마크"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "PERF"
---

# PACS System 성능 벤치마크

> **SSOT**: This document is the single source of truth for **PACS System 성능 벤치마크**.

> **언어:** [English](BENCHMARKS.md) | **한국어**

**최종 업데이트**: 2026-03-18

이 문서는 PACS System에 대한 포괄적인 성능 메트릭, 벤치마킹 방법론 및 결과를 제공합니다.

---

## 목차

- [개요](#개요)
- [벤치마크 카테고리](#벤치마크-카테고리)
- [DICOM 전송 성능](#dicom-전송-성능)
- [코덱 성능](#코덱-성능)
- [스토리지 성능](#스토리지-성능)
- [쿼리 성능](#쿼리-성능)
- [벤치마크 실행](#벤치마크-실행)

---

## 개요

PACS System은 DICOM 작업 처리량, 코덱 처리, 스토리지 I/O 및 쿼리 응답 시간을 다루는 성능 벤치마크를 제공합니다.

**현재 상태**:
- 기준 목표가 정의된 연결 및 전송 벤치마크
- 압축/해제 처리량을 위한 코덱 벤치마크
- 파일시스템 및 클라우드 백엔드를 위한 스토리지 벤치마크

스레드 마이그레이션 기준 측정에 대해서는 [PERFORMANCE_BASELINE.md](PERFORMANCE_BASELINE.md)도 참조하세요.

---

## 벤치마크 카테고리

### DICOM 전송 성능

연결 수립, 메시지 교환 및 데이터 전송을 포함한 DICOM 네트워크 프로토콜 성능을 측정합니다.

| 메트릭 | 목표 | 설명 |
|-------|------|------|
| 단일 연결 | < 100 ms | TCP + A-ASSOCIATE 협상 |
| 전체 왕복 | < 200 ms | 연결 + 해제 |
| 연결 + C-ECHO | < 150 ms | 연결 + 단일 에코 + 해제 |
| 순차 속도 | > 10 conn/s | 빠른 순차 연결 |
| C-ECHO 처리량 | > 100 msg/s | 검증 메시지 (단일 연결) |
| C-ECHO 지속 | > 80 msg/s | 10초 동안 지속 처리량 |
| C-STORE 처리량 | > 20 store/s | 소형 이미지 (64x64) |
| 데이터 전송률 (512x512) | > 5 MB/s | 대형 이미지 전송 |

### 코덱 성능

지원되는 전송 구문의 압축 및 해제 처리량을 측정합니다.

| 코덱 | 연산 | 목표 | 비고 |
|------|------|------|------|
| JPEG | 압축 | 이미지당 측정 | 손실 기본 (Process 1) |
| JPEG | 해제 | 이미지당 측정 | - |
| JPEG 2000 | 압축 | 이미지당 측정 | 손실 및 무손실 |
| JPEG 2000 | 해제 | 이미지당 측정 | - |
| JPEG-LS | 압축 | 이미지당 측정 | 준 무손실 |
| JPEG-LS | 해제 | 이미지당 측정 | - |
| RLE | 압축 | 이미지당 측정 | 무손실, DICOM 표준 RLE |
| RLE | 해제 | 이미지당 측정 | - |

### 스토리지 성능

스토리지 백엔드 I/O 처리량을 측정합니다.

| 백엔드 | 연산 | 메트릭 | 설명 |
|-------|------|--------|------|
| 파일시스템 | 쓰기 | MB/s | 원본 DICOM 파일 쓰기 처리량 |
| 파일시스템 | 읽기 | MB/s | DICOM 파일 읽기 처리량 |
| 클라우드 (S3) | 업로드 | 지연시간 (ms) | 단일 객체 업로드 지연시간 |
| 클라우드 (S3) | 다운로드 | 지연시간 (ms) | 단일 객체 다운로드 지연시간 |
| 데이터베이스 | 인덱싱 | ops/s | 메타데이터 인덱싱 속도 |

### 쿼리 성능

DICOM 쿼리/검색 및 DICOMweb 쿼리 성능을 측정합니다.

| 쿼리 유형 | 목표 | 설명 |
|----------|------|------|
| C-FIND (환자) | < 50 ms | 환자 수준 쿼리 |
| C-FIND (스터디) | < 100 ms | 스터디 수준 쿼리 |
| C-FIND (시리즈) | < 100 ms | 시리즈 수준 쿼리 |
| QIDO-RS (페이지네이션) | < 200 ms | 페이지네이션이 있는 DICOMweb 쿼리 |

---

## 벤치마크 실행

### 빌드

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPACS_BUILD_TESTS=ON
cmake --build build
```

### 실행

```bash
# 모든 벤치마크 실행
./build/bin/pacs_benchmarks

# 특정 벤치마크 카테고리 실행
./build/bin/pacs_benchmarks --filter=Association
./build/bin/pacs_benchmarks --filter=Codec
./build/bin/pacs_benchmarks --filter=Storage
```

### 설정

벤치마크 동작은 환경 변수를 통해 설정할 수 있습니다:

| 변수 | 기본값 | 설명 |
|-----|-------|------|
| `PACS_BENCH_ITERATIONS` | 100 | 벤치마크당 반복 횟수 |
| `PACS_BENCH_WARMUP` | 10 | 워밍업 반복 횟수 |
| `PACS_BENCH_OUTPUT` | stdout | 출력 형식 (stdout, json, csv) |

---

## 결과

벤치마크 결과는 하드웨어 구성 및 네트워크 환경에 따라 다릅니다. 최신 문서화된 결과는 `PERFORMANCE_RESULTS.md`를 참조하세요.

**참조 플랫폼**:
- **CPU**: Intel i7-12700K @ 3.2GHz (12코어, 20스레드)
- **RAM**: 32GB DDR4-3200
- **스토리지**: NVMe SSD
- **OS**: Ubuntu 22.04 LTS
- **컴파일러**: GCC 13.1, -O2
