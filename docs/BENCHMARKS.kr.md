---
doc_id: "PAC-PERF-001"
doc_title: "PACS System 성능 벤치마크"
doc_version: "1.1.0"
doc_date: "2026-10-23"
doc_status: "Released"
project: "pacs_system"
category: "PERF"
---

# PACS System 성능 벤치마크

> **SSOT**: This document is the single source of truth for **PACS System 성능 벤치마크**.

> **언어:** [English](BENCHMARKS.md) | **한국어**

**최종 업데이트**: 2026-10-23

이 문서는 벤치마크 스위트와 목표, 저장소에 기록된 모든 결과(환경, 날짜, 명령, 결과 파일 포함),
그리고 벤치마크 실행 명령을 담습니다. README는 수치를 직접 인용하지 않고 이 문서를 링크합니다.

---

## 목차

- [개요](#개요)
- [기록된 결과](#기록된-결과)
- [벤치마크 카테고리](#벤치마크-카테고리)
- [벤치마크 실행](#벤치마크-실행)

---

## 개요

| 스위트 | 소스 | 실행 파일 | 측정 대상 |
|--------|------|-----------|-----------|
| 스레드 성능 | `benchmarks/thread_performance/` | `thread_performance_benchmarks` | Association 수립, C-ECHO 및 C-STORE 처리량, 동시 부하, 종료 시간 |
| SIMD 성능 | `benchmarks/simd_performance/` | `simd_performance_benchmarks`, `photometric_benchmark`, `windowing_benchmark` | 바이트 스왑, RLE, 광도 변환, window/level |

아래의 코덱, 스토리지, 쿼리 카테고리는 계획 단계입니다. 목표만 정의하며, 저장소에는 아직 이에 대한 벤치마크 소스가 없습니다.

스레드 마이그레이션 기준 목표는 [PERFORMANCE_BASELINE.md](PERFORMANCE_BASELINE.md)를, 기록된 실행의
전체 보고서는 [PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md)를 참조하세요.

---

## 기록된 결과

### 스레드 마이그레이션 실행 (이슈 #160)

저장소에 기록된 유일한 벤치마크 실행입니다. 값은 [PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md)에서
옮겨 왔습니다. 이 표는 #1212 이전까지 README에 있던 "주요 성능 메트릭" 표를 대체합니다.

| 항목 | 기록 |
|------|------|
| 환경 | macOS 26.2 (Darwin), Apple M4 Max (ARM64), 메모리 36 GB, Apple Clang 17.0.0, Release 빌드, C++20 ([Test Environment](PERFORMANCE_RESULTS.md#test-environment)) |
| 날짜 | 신뢰할 수 있는 기록 없음. 보고서는 원래 2024-12-05로 적었지만 벤치마크 소스는 2025-12-04에 추가되었고(`7c59a5d7`, PR #164), 보고서를 추가한 커밋(`501957b0`, PR #171)의 날짜는 2025-12-05이며, macOS 26.2와 Apple Clang 17.0.0은 2024-12-05에는 출시되지 않았습니다. |
| 명령 | 기록 없음. 보고서는 `thread_performance_benchmarks`의 네 스위트(association, throughput, 동시 부하, shutdown)를 언급하며, 이를 실행하는 명령은 [벤치마크 실행](#벤치마크-실행)에 있습니다. |
| 결과 파일 | [PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md) (요약 표). 원시 벤치마크 출력은 저장소에 기록되어 있지 않습니다. |
| 구성 | 클라이언트와 `benchmark_server` 픽스처가 한 프로세스 안에서 `localhost`로 통신합니다 (`benchmarks/thread_performance/benchmark_common.h`). 보고서는 단일 워커의 높은 처리율을 메모리 내 벤치마크 연산 때문이라고 설명하므로, 이 값은 호스트 간 네트워크 처리량이 아닙니다. |

| 메트릭 | 목표 | 기록값 | 보고서 판정 | 보고서 절 |
|--------|------|--------|-------------|-----------|
| 단일 association (연결) | < 100 ms | 0.00134 ms | PASS | 1 |
| 연결 + 해제 왕복 | < 200 ms | 0.00267 ms | PASS | 1 |
| 연결 + C-ECHO + 해제 | < 150 ms | 468.7 ms | INVESTIGATE | 1 |
| 순차 연결 속도 | > 10 conn/s | 419,655 conn/s | PASS | 1 |
| C-ECHO, 단일 연결 | > 100 msg/s | 89,964 echo/s | PASS | 2 |
| C-ECHO, 10초 지속 | > 80 msg/s | 132,391 msg/s | PASS | 2 |
| C-STORE, 단일 연결 | > 20 store/s | 31,759 store/s | PASS | 2 |
| C-STORE 데이터 전송률, 512x512 (512 KB) 이미지 | > 5 MB/s | 9,247 MB/s (18,494 store/s) | 미기재 | 2 |
| 동시 C-ECHO, 워커 10개 | > 50 ops/s | 124 echo/s | PASS | 3 |
| 동시 C-STORE, 워커 5개 | > 10 ops/s | 49.6 store/s | PASS | 3 |
| 활성 연결 5개에서 종료 | < 5000 ms | 107 ms | PASS | 4 |
| 부하 중 종료 (워커 3개) | < 10000 ms | 110 ms | PASS | 4 |

데이터 전송률 목표는 [PERFORMANCE_BASELINE.md](PERFORMANCE_BASELINE.md)에서 가져왔으며, 보고서는 이
행에 판정을 적지 않았습니다. 보고서는 연결 + C-ECHO + 해제 값을 벤치마크 픽스처 타이밍 문제로
설명하며("Known Issues and Investigations"), 이후의 측정은 기록되어 있지 않습니다.

**이전 README 표와의 대조.** README의 "Association 지연 시간 < 1 ms" 행은 연결 및 왕복 값만 가져오고
연결 + C-ECHO + 해제 값(468.7 ms, INVESTIGATE)을 빠뜨렸습니다. "정상 종료 110 ms (활성 연결 포함)" 행은
부하 중 값(워커 3개)에 활성 연결이라는 설명을 붙였으며, 활성 연결 값은 107 ms입니다. C-ECHO, C-STORE,
동시 작업, 512x512 데이터 전송률 행은 보고서와 일치합니다.

### SIMD 벤치마크

환경과 날짜를 갖춘 SIMD 결과는 저장소에 기록되어 있지 않으므로,
[FEATURES.kr.md](FEATURES.kr.md#simd-최적화)는 SIMD 속도 향상 수치를 제시하지 않습니다.

### 이전 참조 플랫폼

이 문서의 이전 판은 참조 플랫폼으로 Intel i7-12700K @ 3.2GHz (12코어, 20스레드), 32GB DDR4-3200,
NVMe SSD, Ubuntu 22.04 LTS, GCC 13.1 `-O2`를 적었습니다. 저장소의 어떤 결과도 이 플랫폼에서 기록되지
않았습니다.

---

## 벤치마크 카테고리

### DICOM 전송 성능

연결 수립, 메시지 교환 및 데이터 전송을 포함한 DICOM 네트워크 프로토콜 성능을 측정합니다
(`benchmarks/thread_performance/`).

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

### 코덱 성능 (계획됨)

코덱 벤치마크 소스가 아직 없으므로 이 표는 목표만 정의합니다. SIMD 스위트의 `rle_simd_benchmark.cpp`는
RLE 코덱이 사용하는 SIMD 보조 단계(RGB 평면 변환, 16비트 평면 분할 및 병합)를 측정하며, RLE 압축이나
해제를 측정하지는 않습니다.

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

### 스토리지 성능 (계획됨)

스토리지 벤치마크 소스가 아직 없으므로 이 표는 목표만 정의합니다.

| 백엔드 | 연산 | 메트릭 | 설명 |
|-------|------|--------|------|
| 파일시스템 | 쓰기 | MB/s | 원본 DICOM 파일 쓰기 처리량 |
| 파일시스템 | 읽기 | MB/s | DICOM 파일 읽기 처리량 |
| 클라우드 (S3) | 업로드 | 지연시간 (ms) | 단일 객체 업로드 지연시간 |
| 클라우드 (S3) | 다운로드 | 지연시간 (ms) | 단일 객체 다운로드 지연시간 |
| 데이터베이스 | 인덱싱 | ops/s | 메타데이터 인덱싱 속도 |

### 쿼리 성능 (계획됨)

쿼리 벤치마크 소스가 아직 없으므로 이 표는 목표만 정의합니다.

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
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPACS_BUILD_BENCHMARKS=ON
cmake --build build
```

두 스위트 모두 Catch2가 필요하며, CMake는 `PACS_BUILD_TESTS`가 `ON`(기본값)일 때만 Catch2를 가져옵니다
(`cmake/testing.cmake`, `cmake/examples.cmake` 참조).

### 스레드 성능 스위트

```bash
# 모든 스레드 성능 벤치마크 실행
cmake --build build --target run_full_benchmarks

# 빠른 부분 집합: association 및 C-ECHO 처리량
cmake --build build --target run_quick_benchmarks

# JUnit 출력 (build/benchmark_results.xml)
cmake --build build --target run_benchmarks_ci

# 특정 카테고리 실행
./build/bin/thread_performance_benchmarks "[benchmark][association]"
./build/bin/thread_performance_benchmarks "[benchmark][throughput]"
./build/bin/thread_performance_benchmarks "[benchmark][concurrent]"
./build/bin/thread_performance_benchmarks "[benchmark][shutdown]"
```

### SIMD 스위트

```bash
cmake --build build --target run_simd_benchmarks_full     # 바이트 스왑 및 RLE
cmake --build build --target run_simd_benchmarks_quick    # 요약만
cmake --build build --target run_photometric_benchmark
cmake --build build --target run_windowing_benchmark
```

새 결과를 이 문서에 기록할 때는 환경, 실행 날짜, 정확한 명령, 그리고 저장소 안의 결과 파일 링크를 함께 적으세요.
