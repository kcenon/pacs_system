# PACS System

[![CI](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml)
[![Code Coverage](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml)
[![codecov](https://codecov.io/gh/kcenon/pacs_system/branch/main/graph/badge.svg)](https://codecov.io/gh/kcenon/pacs_system)
[![SBOM Generation](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml)
[![License](https://img.shields.io/github/license/kcenon/pacs_system)](https://github.com/kcenon/pacs_system/blob/main/LICENSE)

> **언어:** [English](README.md) | **한국어**

외부 DICOM 라이브러리 없이 kcenon 에코시스템을 기반으로 DICOM 표준을 처음부터 구현한 현대적인 C++20 PACS 시스템입니다.

## 목차

- [개요](#개요)
- [주요 기능](#주요-기능)
- [요구사항](#요구사항)
- [빠른 시작](#빠른-시작)
- [설치](#설치)
- [아키텍처](#아키텍처)
- [핵심 개념](#핵심-개념)
- [API 개요](#api-개요)
- [예제](#예제)
- [성능](#성능)
- [생태계 통합](#생태계-통합)
- [기여하기](#기여하기)
- [라이선스](#라이선스)

---

## 개요

PACS System은 kcenon 에코시스템의 고성능 인프라를 활용하여 DICOM 표준을 순수 구현한 C++20 PACS (Picture Archiving and Communication System)입니다.

**핵심 가치**:
- **외부 DICOM 라이브러리 제로**: kcenon 에코시스템을 활용한 순수 구현
- **고성능**: SIMD 가속, lock-free 큐, 비동기 I/O 활용
- **프로덕션 품질**: 종합적인 CI/CD, 새니타이저, 품질 메트릭
- **모듈식 아키텍처**: 인터페이스 기반 설계로 관심사의 명확한 분리
- **크로스 플랫폼**: Linux, macOS, Windows 지원

**현재 단계**: Phase 4 완료 - 고급 서비스 및 프로덕션 강화

| 단계 | 범위 | 상태 |
|------|------|------|
| **Phase 1**: 기반 | DICOM 코어, 태그 사전, 파일 I/O (Part 10), 전송 구문 | 완료 |
| **Phase 2**: 네트워크 | Upper Layer Protocol (PDU), Association 상태 머신, DIMSE-C, 압축 코덱 | 완료 |
| **Phase 3**: 핵심 서비스 | Storage SCP/SCU, 파일 스토리지, 인덱스 DB, Query/Retrieve, 로깅, 모니터링 | 완료 |
| **Phase 4**: 고급 서비스 | REST API, DICOMweb, AI 통합, 클라이언트 모듈, 클라우드 스토리지, 인쇄 관리, 보안, 워크플로우 | 완료 |

**테스트 커버리지**: 141+ 테스트 파일에서 1,980+ 테스트 통과

---

## 주요 기능

| 기능 | 설명 | 상태 |
|------|------|------|
| **DICOM 코어** | 태그, 요소, 데이터셋, Part 10 파일 I/O | 안정 |
| **전송 구문** | Implicit/Explicit VR, JPEG, JPEG 2000, JPEG-LS, RLE, HTJ2K | 안정 |
| **네트워크 프로토콜** | PDU, Association 상태 머신, DIMSE | 안정 |
| **Storage SCP/SCU** | C-STORE 서비스 | 안정 |
| **Query/Retrieve** | Patient/Study Root Q/R | 안정 |
| **Modality Worklist** | MWL SCP/SCU | 안정 |
| **REST API** | Crow 기반 RESTful API | 안정 |
| **DICOMweb** | WADO-RS, STOW-RS, QIDO-RS | 안정 |
| **AI 통합** | AI 서비스 커넥터, 결과 처리 (SR/SEG) | 안정 |
| **클라우드 스토리지** | S3/Azure 클라우드 백엔드 | 안정 |
| **보안** | RBAC, 익명화 (PS3.15), 디지털 서명, TLS | 안정 |
| **워크플로우** | 자동 프리페치, 작업 스케줄러, 연구 잠금 | 안정 |

---

## 요구사항

### 컴파일러 매트릭스

| 컴파일러 | 최소 버전 | 비고 |
|----------|----------|------|
| GCC | 13+ | network_system 전이 의존성 |
| Clang | 17+ | network_system 전이 의존성 |
| Apple Clang | 14+ | std::jthread 제한 (자동 fallback) |
| MSVC | 2022+ | std::min/std::max 괄호화 필요 |

### 빌드 도구 및 의존성

| 의존성 | 버전 | 필수 | 설명 |
|--------|------|------|------|
| CMake | 3.20+ | 예 | 빌드 시스템 |
| ICU | latest | 예 | 문자셋 인코딩 |
| SQLite3 | 3.45.1+ | 예 | 스토리지 인덱싱 |
| [common_system](https://github.com/kcenon/common_system) | latest | 예 | Result<T>, IExecutor |
| [container_system](https://github.com/kcenon/container_system) | latest | 예 | DICOM 직렬화 |
| [network_system](https://github.com/kcenon/network_system) | latest | 예 | TCP/TLS, 비동기 I/O |

### 선택적 코덱 의존성

| 코덱 | 의존성 | 목적 |
|------|--------|------|
| JPEG | libjpeg-turbo | JPEG Baseline/Lossless |
| JPEG 2000 | OpenJPEG | JPEG 2000 압축 |
| JPEG-LS | CharLS | JPEG-LS 압축 |
| HTJ2K | OpenJPH | High-Throughput JPEG 2000 |

---

## 빠른 시작

```bash
# 빌드
cmake -S . -B build
cmake --build build

# 테스트 실행
cd build && ctest --output-on-failure --timeout 120

# CLI 도구 실행
./build/bin/pacs_store --help
./build/bin/pacs_query --help
```

---

## 설치

### 소스에서 빌드

```bash
# 빌드 스크립트
./scripts/build.sh --debug --tests

# 또는 CMake 직접
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### CMake 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `PACS_BUILD_TESTS` | ON | Catch2 단위 테스트 |
| `PACS_BUILD_EXAMPLES` | OFF | 32개 예제 프로그램 |
| `PACS_BUILD_STORAGE` | ON | SQLite3 스토리지 |
| `PACS_BUILD_CODECS` | ON | 압축 코덱 |
| `PACS_WITH_OPENSSL` | ON | TLS/SSL 지원 |
| `PACS_WITH_REST_API` | ON | REST API (Crow) |
| `PACS_WITH_AWS_SDK` | OFF | AWS S3 클라우드 스토리지 |
| `PACS_WITH_AZURE_SDK` | OFF | Azure 클라우드 스토리지 |

---

## 아키텍처

### 12개 라이브러리 계층 구조

```
+------------------------------------------------------------------+
|                          PACS System                              |
+------------------------------------------------------------------+
| REST API | DICOMweb | Web UI  | AI Svc    | Workflow              |
| (Crow)   | WADO/STOW| (React) | Connector | Scheduler             |
+------------------------------------------------------------------+
|  Services (Storage/Query/Retrieve/Worklist/MPPS/Commit/Print)    |
+------------------------------------------------------------------+
|  Network (PDU/Association/DIMSE) + Security (RBAC/TLS/Anon)      |
+------------------------------------------------------------------+
|  Core (Tag/Element/Dataset/File) + Encoding (VR/Codecs/SIMD)     |
+------------------------------------------------------------------+
|  kcenon Ecosystem (common + container + network + thread + logger)|
+------------------------------------------------------------------+
```

### 라이브러리 타겟

| 라이브러리 | 설명 |
|-----------|------|
| `pacs_core` | 태그, 요소, 데이터셋, Part 10 파일 I/O, 사전 |
| `pacs_encoding` | VR 타입, 전송 구문, 압축 코덱 |
| `pacs_security` | RBAC, 익명화, 디지털 서명, TLS |
| `pacs_network` | PDU, Association 상태 머신, DIMSE, 파이프라인 |
| `pacs_client` | 작업/라우팅/동기/프리페치/원격 노드 관리 |
| `pacs_services` | SCP/SCU 구현, SOP 클래스 레지스트리, IOD 검증기 |
| `pacs_storage` | 파일 스토리지, SQLite 인덱싱, 클라우드 (S3/Azure), HSM |
| `pacs_ai` | AI 서비스 커넥터, 결과 처리 (SR/SEG) |
| `pacs_monitoring` | 헬스 체크, Prometheus 메트릭 |
| `pacs_workflow` | 자동 프리페치, 작업 스케줄러, 연구 잠금 |
| `pacs_web` | REST API (Crow), DICOMweb (WADO/STOW/QIDO) |
| `pacs_integration` | kcenon 에코시스템 어댑터 |

---

## 핵심 개념

### DICOM 적합성

#### 지원 SOP 클래스

| 카테고리 | SOP 클래스 |
|----------|-----------|
| 검증 | C-ECHO (Verification) |
| 스토리지 | CT, MR, US, XA, NM, PET, RT, SR, SEG, MG, CR, SC |
| Query/Retrieve | Patient Root, Study Root |
| 워크플로우 | Modality Worklist, MPPS, Storage Commitment |
| 인쇄 | Print Management |
| 기타 | UPS, PIR, XDS-I.b |

#### 지원 전송 구문

| 구문 | 설명 |
|------|------|
| Implicit VR Little Endian | 기본 전송 구문 |
| Explicit VR Little Endian | 명시적 VR |
| JPEG Baseline / Lossless | 손실/무손실 JPEG |
| JPEG 2000 | 웨이블릿 기반 압축 |
| JPEG-LS | 근무손실 JPEG |
| RLE | 런 길이 인코딩 |
| HTJ2K | 고처리량 JPEG 2000 |
| HEVC | H.265 비디오 코딩 |

### Association 상태 머신

전체 DICOM Upper Layer association 관리를 구현합니다. PDU 인코딩/디코딩, A-ASSOCIATE, A-RELEASE, A-ABORT 처리를 포함합니다.

### CLI 도구

| 도구 | 설명 |
|------|------|
| `pacs_store` | DICOM 파일 전송 (C-STORE SCU) |
| `pacs_query` | DICOM 쿼리 (C-FIND SCU) |
| `pacs_retrieve` | DICOM 검색 (C-MOVE/C-GET SCU) |
| `pacs_echo` | 연결 검증 (C-ECHO SCU) |

---

## API 개요

| API | 라이브러리 | 설명 |
|-----|-----------|------|
| `dicom_dataset` | pacs_core | DICOM 데이터셋 조작 |
| `dicom_file` | pacs_core | Part 10 파일 읽기/쓰기 |
| `association` | pacs_network | DICOM Association 관리 |
| `storage_scp` | pacs_services | Storage SCP 서비스 |
| `query_scu` | pacs_services | Query SCU 서비스 |
| `rest_server` | pacs_web | REST API 서버 |
| `wado_handler` | pacs_web | WADO-RS 핸들러 |

---

## 예제

| 예제 | 난이도 | 설명 |
|------|--------|------|
| echo_scu | 초급 | C-ECHO 연결 테스트 |
| store_scu | 초급 | DICOM 파일 전송 |
| query_retrieve | 중급 | Query/Retrieve 워크플로우 |
| dicomweb_client | 중급 | DICOMweb API 사용 |
| full_pacs_server | 고급 | 전체 PACS 서버 구성 |

---

## 성능

### 테스트 메트릭

| 메트릭 | 값 |
|--------|------|
| **테스트 수** | 1,980+ |
| **테스트 파일** | 141+ |
| **통과율** | 100% |

### 품질 메트릭

- 다중 플랫폼 CI/CD (Ubuntu GCC/Clang, macOS, Windows MSVC)
- ThreadSanitizer / AddressSanitizer / UBSanitizer 클린
- DCMTK 상호운용성 테스트
- CVE 스캔, SBOM 생성
- 정적 분석, 퍼징

---

## 생태계 통합

### 의존성 계층

```
common_system      (Tier 0) [필수] -- Result<T>, IExecutor
container_system   (Tier 1) [필수] -- DICOM 직렬화
thread_system      (Tier 1) [network_system 경유]
logger_system      (Tier 2) [network_system 경유]
monitoring_system  (Tier 2) [선택, network_system 경유]
database_system    (Tier 3) [선택]
network_system     (Tier 4) [필수] -- TCP/TLS, 비동기 I/O
pacs_system        (Tier 5) <-- 이 프로젝트
```

### 통합 어댑터

| 어댑터 | 에코시스템 컴포넌트 | 목적 |
|--------|-------------------|------|
| network_adapter | network_system | TCP/TLS 전송 |
| container_adapter | container_system | 데이터 직렬화 |
| logger_adapter | logger_system | 로깅 |
| monitoring_adapter | monitoring_system | 메트릭/헬스 체크 |
| database_adapter | database_system | 인덱스 DB |

### 플랫폼 지원

| 플랫폼 | 컴파일러 | 상태 |
|--------|----------|------|
| **Linux** | GCC 13+, Clang 17+ | 완전 지원 |
| **macOS** | Apple Clang 14+ | 완전 지원 |
| **Windows** | MSVC 2022+ | 완전 지원 |

---

## 기여하기

기여를 환영합니다! 자세한 내용은 [기여 가이드](docs/contributing/CONTRIBUTING.md)를 참조하세요.

1. 리포지토리 포크
2. 기능 브랜치 생성
3. 테스트와 함께 변경 사항 작성
4. 로컬에서 테스트 실행
5. Pull Request 열기

---

## 라이선스

이 프로젝트는 BSD 3-Clause 라이선스에 따라 배포됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
