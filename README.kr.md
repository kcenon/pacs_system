# PACS System

[![CI](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml)
[![Code Coverage](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml)
[![SBOM Generation](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml)

> **언어:** [English](README.md) | **한국어**

## 개요

kcenon 에코시스템을 기반으로 외부 DICOM 라이브러리 없이 구축된 고성능 C++20 PACS (Picture Archiving and Communication System) 구현체입니다. 이 프로젝트는 기존 고성능 인프라를 활용하여 DICOM 표준을 처음부터 구현합니다.

**주요 특징**:
- **외부 DICOM 라이브러리 제로**: kcenon 에코시스템을 활용한 순수 구현
- **고성능**: SIMD 가속, lock-free 큐, 비동기 I/O 활용
- **프로덕션 품질**: 종합적인 CI/CD, 새니타이저, 품질 메트릭
- **모듈식 아키텍처**: 인터페이스 기반 설계로 관심사의 명확한 분리
- **크로스 플랫폼**: Linux, macOS, Windows 지원

---

## 목차

- [프로젝트 현황](#프로젝트-현황)
- [아키텍처](#아키텍처)
- [DICOM 적합성](#dicom-적합성)
- [시작하기](#시작하기)
- [CLI 도구 & 예제](#cli-도구--예제)
- [에코시스템 의존성](#에코시스템-의존성)
- [프로젝트 구조](#프로젝트-구조)
- [문서](#문서)
- [성능](#성능)
- [코드 통계](#코드-통계)
- [기여하기](#기여하기)
- [라이선스](#라이선스)
- [연락처](#연락처)

---

## 프로젝트 현황

**현재 단계**: ✅ Phase 4 완료 - Advanced Services & Production Hardening

| Phase | 범위 | 상태 |
|-------|------|------|
| **Phase 1**: Foundation | DICOM Core, Tag Dictionary, File I/O (Part 10), Transfer Syntax | ✅ 완료 |
| **Phase 2**: Network Protocol | Upper Layer Protocol (PDU), Association State Machine, DIMSE-C, Compression Codecs | ✅ 완료 |
| **Phase 3**: Core Services | Storage SCP/SCU, File Storage, Index Database, Query/Retrieve, Logging, Monitoring | ✅ 완료 |
| **Phase 4**: Advanced Services | REST API, DICOMweb, AI Integration, Client Module, Cloud Storage, Print Management, Security, Workflow | ✅ 완료 |

**테스트 커버리지**: 141개 이상 테스트 파일, 1,980개 이상 테스트 통과

> Phase별 상세 기능 목록은 [기능 명세](docs/FEATURES.kr.md)를 참조하세요.

---

## 아키텍처

```
┌──────────────────────────────────────────────────────────────────────┐
│                            PACS System                               │
├──────────────────────────────────────────────────────────────────────┤
│  ┌──────────┐ ┌──────────┐ ┌─────────┐ ┌──────────┐ ┌───────────┐  │
│  │ REST API │ │ DICOMweb │ │ Web UI  │ │ AI Svc   │ │ Workflow  │  │
│  │ (Crow)   │ │ WADO/STOW│ │ (React) │ │Connector │ │ Scheduler │  │
│  └────┬─────┘ └────┬─────┘ └────┬────┘ └────┬─────┘ └─────┬─────┘  │
│       └─────────────┼───────────┼───────────┼──────────────┘        │
│  ┌──────────────────▼───────────▼───────────▼────────────────────┐  │
│  │  Services (Storage/Query/Retrieve/Worklist/MPPS/Commit/Print) │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌──────────────────────────────▼────────────────────────────────┐  │
│  │  Network (PDU/Association/DIMSE) + Security (RBAC/TLS/Anon)   │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌──────────────────────────────▼────────────────────────────────┐  │
│  │  Core (Tag/Element/Dataset/File) + Encoding (VR/Codecs/SIMD)  │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌─────────────┐  ┌─────────────┴──────────┐  ┌─────────────────┐  │
│  │  Storage    │  │  Client Module         │  │  Monitoring     │  │
│  │  (DB/File/  │  │  (Job/Route/Sync/      │  │  (Health/       │  │
│  │   S3/Azure) │  │   Prefetch/RemoteNode) │  │   Metrics)      │  │
│  └──────┬──────┘  └────────────────────────┘  └─────────────────┘  │
├─────────┼────────────────────────────────────────────────────────────┤
│         │             Integration Adapters                           │
│  container │ network │ thread │ logger │ monitoring              │
├─────────┼────────────────────────────────────────────────────────────┤
│         │              kcenon Ecosystem                               │
│  common_system │ container_system │ thread_system │ network_system   │
│  logger_system │ monitoring_system │ database_system (opt)           │
└──────────────────────────────────────────────────────────────────────┘
```

---

## DICOM 적합성

### 지원 SOP 클래스

| 서비스 | SOP Class | 상태 |
|--------|-----------|------|
| **Verification** | 1.2.840.10008.1.1 | ✅ 완료 |
| **CT Storage** | 1.2.840.10008.5.1.4.1.1.2 | ✅ 완료 |
| **MR Storage** | 1.2.840.10008.5.1.4.1.1.4 | ✅ 완료 |
| **US Storage** | 1.2.840.10008.5.1.4.1.1.6.x | ✅ 완료 |
| **XA Storage** | 1.2.840.10008.5.1.4.1.1.12.x | ✅ 완료 |
| **XRF Storage** | 1.2.840.10008.5.1.4.1.1.12.2 | ✅ 완료 |
| **X-Ray Storage** | 1.2.840.10008.5.1.4.1.1.1.1 | ✅ 완료 |
| **Patient Root Q/R** | 1.2.840.10008.5.1.4.1.2.1.x | ✅ 완료 |
| **Study Root Q/R** | 1.2.840.10008.5.1.4.1.2.2.x | ✅ 완료 |
| **Modality Worklist** | 1.2.840.10008.5.1.4.31 | ✅ 완료 |
| **MPPS** | 1.2.840.10008.3.1.2.3.3 | ✅ 완료 |
| **Storage Commitment** | 1.2.840.10008.1.20.1 | ✅ 완료 |
| **NM Storage** | 1.2.840.10008.5.1.4.1.1.20 | ✅ 완료 |
| **PET Storage** | 1.2.840.10008.5.1.4.1.1.128 | ✅ 완료 |
| **RT Storage** | 1.2.840.10008.5.1.4.1.1.481.x | ✅ 완료 |
| **SR Storage** | 1.2.840.10008.5.1.4.1.1.88.x | ✅ 완료 |
| **SEG Storage** | 1.2.840.10008.5.1.4.1.1.66.4 | ✅ 완료 |
| **MG Storage** | 1.2.840.10008.5.1.4.1.1.1.2.x | ✅ 완료 |
| **CR Storage** | 1.2.840.10008.5.1.4.1.1.1 | ✅ 완료 |
| **SC Storage** | 1.2.840.10008.5.1.4.1.1.7 | ✅ 완료 |
| **Basic Grayscale Print** | 1.2.840.10008.5.1.1.9 | ✅ 완료 |
| **Basic Color Print** | 1.2.840.10008.5.1.1.18 | ✅ 완료 |
| **Printer** | 1.2.840.10008.5.1.1.16 | ✅ 완료 |

### Transfer Syntax 지원

| Transfer Syntax | UID | 상태 |
|-----------------|-----|------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | ✅ 완료 |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | ✅ 완료 |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | ✅ 완료 |
| JPEG Baseline (Process 1) | 1.2.840.10008.1.2.4.50 | ✅ 완료 |
| JPEG Lossless (Process 14) | 1.2.840.10008.1.2.4.70 | ✅ 완료 |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | ✅ 완료 |
| JPEG 2000 Lossy | 1.2.840.10008.1.2.4.91 | ✅ 완료 |
| JPEG-LS Lossless | 1.2.840.10008.1.2.4.80 | ✅ 완료 |
| JPEG-LS Near-Lossless | 1.2.840.10008.1.2.4.81 | ✅ 완료 |
| RLE Lossless | 1.2.840.10008.1.2.5 | ✅ 완료 |

> 전체 DICOM Conformance Statement는 [DICOM Conformance Statement](docs/DICOM_CONFORMANCE_STATEMENT.md)를 참조하세요.

---

## 시작하기

### 사전 요구사항

**모든 플랫폼:**
- C++20 Concepts를 지원하는 호환 컴파일러:
  - GCC 10+ (std::format 전체 지원을 위해 GCC 13+ 권장)
  - Clang 10+ (Clang 14+ 권장)
  - MSVC 2022 (19.30+)
- CMake 3.20+
- Ninja (권장 빌드 시스템)
- kcenon 에코시스템 라이브러리 (CMake가 자동 다운로드)

**Linux (Ubuntu 24.04+):**
```bash
sudo apt install cmake ninja-build libsqlite3-dev libssl-dev libfmt-dev
```

**macOS:**
```bash
brew install cmake ninja sqlite3 openssl@3 fmt
```

**Windows:**
- Visual Studio 2022 (C++ 워크로드 포함)
- [vcpkg](https://vcpkg.io/) 패키지 관리
- 의존성: `sqlite3`, `openssl`, `fmt`, `gtest`

### 빌드

#### Linux/macOS

```bash
# 저장소 클론
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system

# 설정 및 빌드
cmake -S . -B build
cmake --build build

# 테스트 실행
cd build && ctest --output-on-failure
```

#### Windows

```powershell
# 사전 요구사항: Visual Studio 2022, vcpkg, CMake 3.20+

# vcpkg로 의존성 설치
vcpkg install sqlite3:x64-windows openssl:x64-windows fmt:x64-windows gtest:x64-windows

# 저장소 클론
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system

# vcpkg 툴체인으로 설정
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

# 빌드
cmake --build build

# 테스트 실행
cd build
ctest --output-on-failure
```

**테스트 결과**: 1,980개 이상 테스트 통과 (141개 이상 테스트 파일 - Core, Encoding, Network, Services, Storage, Security, Web, Workflow, Client, AI, Monitoring, Integration)

### 빌드 옵션

```cmake
PACS_BUILD_TESTS (ON)              # 유닛 테스트 활성화
PACS_BUILD_EXAMPLES (OFF)          # 예제 빌드 활성화
PACS_BUILD_BENCHMARKS (OFF)        # 벤치마크 활성화
PACS_BUILD_STORAGE (ON)            # Storage 모듈 빌드
```

### Windows 개발 노트

Windows 호환성을 위해 `std::min`과 `std::max` 호출 시 괄호로 감싸서 Windows.h 매크로와의 충돌을 방지하세요:

```cpp
// 올바른 방법: 모든 플랫폼에서 작동
size_t result = (std::min)(a, b);
auto value = (std::max)(x, y);

// 잘못된 방법: Windows MSVC에서 실패 (error C2589)
size_t result = std::min(a, b);
auto value = std::max(x, y);
```

---

## CLI 도구 & 예제

프로젝트에는 32개의 예제 애플리케이션이 포함되어 있습니다. 빌드 방법:

```bash
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build
```

### 도구 요약

| 카테고리 | 도구 | 설명 |
|----------|------|------|
| **파일 유틸리티** | `dcm_dump` | DICOM 파일 메타데이터 검사 |
| | `dcm_info` | DICOM 파일 요약 정보 표시 |
| | `dcm_modify` | DICOM 태그 수정 (dcmtk 호환) |
| | `dcm_conv` | Transfer Syntax 변환 |
| | `dcm_anonymize` | DICOM 파일 비식별화 (PS3.15) |
| | `dcm_dir` | DICOMDIR 생성/관리 (PS3.10) |
| | `dcm_to_json` | DICOM → JSON 변환 (PS3.18) |
| | `json_to_dcm` | JSON → DICOM 변환 (PS3.18) |
| | `dcm_to_xml` | DICOM → XML 변환 (PS3.19) |
| | `xml_to_dcm` | XML → DICOM 변환 (PS3.19) |
| | `img_to_dcm` | JPEG 이미지 → DICOM 변환 |
| | `dcm_extract` | 픽셀 데이터를 이미지 형식으로 추출 |
| | `db_browser` | PACS 인덱스 데이터베이스 브라우저 |
| **네트워크** | `echo_scu` / `echo_scp` | DICOM 연결 검증 |
| | `secure_dicom` | TLS 보안 DICOM Echo SCU/SCP |
| | `store_scu` / `store_scp` | DICOM Storage (C-STORE) |
| | `query_scu` | 쿼리 클라이언트 (C-FIND) |
| | `find_scu` | dcmtk 호환 C-FIND SCU |
| | `retrieve_scu` | 검색 클라이언트 (C-MOVE/C-GET) |
| | `move_scu` | dcmtk 호환 C-MOVE SCU |
| | `get_scu` | dcmtk 호환 C-GET SCU |
| | `print_scu` | Print Management 클라이언트 (PS3.4 Annex H) |
| **서버** | `qr_scp` | Query/Retrieve SCP (C-FIND/C-MOVE/C-GET) |
| | `worklist_scu` / `worklist_scp` | Modality Worklist (MWL) |
| | `mpps_scu` / `mpps_scp` | Modality Performed Procedure Step |
| | `pacs_server` | 설정 기반 전체 PACS 서버 |
| **테스트** | `integration_tests` | End-to-end 워크플로우 테스트 |

### 빠른 예제

**DICOM 연결 확인:**
```bash
./build/bin/echo_scu localhost 11112
```

**DICOM 파일 전송:**
```bash
./build/bin/store_scu localhost 11112 image.dcm
```

**환자 이름으로 Study 검색:**
```bash
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"
```

**DICOM 파일 검사:**
```bash
./build/bin/dcm_dump image.dcm --format json
```

> 모든 옵션을 포함한 전체 CLI 문서는 [CLI 레퍼런스](docs/CLI_REFERENCE.kr.md)를 참조하세요.

---

## 에코시스템 의존성

이 프로젝트는 다음 kcenon 에코시스템 컴포넌트를 활용합니다:

| 시스템 | 용도 | 주요 기능 |
|--------|------|----------|
| **common_system** | 기반 인터페이스 | IExecutor, Result<T>, Event Bus |
| **container_system** | 데이터 직렬화 | 타입 안전 값, SIMD 가속 |
| **thread_system** | 동시성 | 스레드 풀, lock-free 큐 |
| **logger_system** | 로깅 | 비동기 로깅, 4.34M msg/s |
| **monitoring_system** | 관측성 | 메트릭, 분산 트레이싱 |
| **network_system** | 네트워크 I/O | TCP/TLS, 비동기 작업 |
| **database_system** | 데이터베이스 추상화 | SQL 인젝션 방지, 다중 DB 지원 (선택적) |

---

## 프로젝트 구조

| 모듈 | 위치 | 설명 |
|------|------|------|
| **Core** | `include/pacs/core/` | DICOM 태그, 요소, 데이터셋, Part 10 파일 I/O, 태그 사전 |
| **Encoding** | `include/pacs/encoding/` | VR 타입, Transfer Syntax, 압축 코덱 (JPEG, JP2K, JPLS, RLE) |
| **Network** | `include/pacs/network/` | PDU 인코딩/디코딩, Association 상태 머신, DIMSE 프로토콜 |
| **Services** | `include/pacs/services/` | SCP/SCU 구현, SOP 클래스 레지스트리, IOD 검증기 |
| **Storage** | `include/pacs/storage/` | 파일 스토리지, SQLite 인덱싱, 클라우드 스토리지 (S3/Azure), HSM |
| **Security** | `include/pacs/security/` | RBAC, 익명화 (PS3.15), 디지털 서명, TLS |
| **Monitoring** | `include/pacs/monitoring/` | Health Check, Prometheus 메트릭 |
| **Web** | `include/pacs/web/` | REST API (Crow), DICOMweb (WADO/STOW/QIDO) |
| **Client** | `include/pacs/client/` | Job, 라우팅, 동기화, 프리페치, 리모트 노드 관리 |
| **Workflow** | `include/pacs/workflow/` | 자동 프리페치, 태스크 스케줄러, Study 잠금 관리자 |
| **AI** | `include/pacs/ai/` | AI 서비스 커넥터, 결과 핸들러 (SR/SEG) |
| **Integration** | `include/pacs/integration/` | 에코시스템 어댑터 (container, network, thread, logger, monitoring) |

```
pacs_system/
├── include/pacs/         # 공개 헤더 (222개 파일)
├── src/                  # 소스 구현 (148개 파일)
├── tests/                # 테스트 스위트 (141개 이상 파일, 1,980개 이상 테스트)
├── examples/             # 예제 애플리케이션 (32개)
├── docs/                 # 문서 (57개 파일)
└── CMakeLists.txt        # 빌드 설정 (v0.2.0)
```

> 전체 파일 수준 디렉토리 트리는 [프로젝트 구조](docs/PROJECT_STRUCTURE.kr.md)를 참조하세요.

---

## 문서

- 📋 [구현 분석](docs/PACS_IMPLEMENTATION_ANALYSIS.kr.md) - 상세 구현 전략
- 📋 [요구사항 정의서](docs/PRD.kr.md) - 제품 요구사항 문서
- 🏗️ [아키텍처 가이드](docs/ARCHITECTURE.kr.md) - 시스템 아키텍처
- ⚡ [기능 명세](docs/FEATURES.kr.md) - 기능 상세 설명
- 📁 [프로젝트 구조](docs/PROJECT_STRUCTURE.kr.md) - 디렉토리 구조
- 🔧 [API 레퍼런스](docs/API_REFERENCE.kr.md) - API 문서
- 🖥️ [CLI 레퍼런스](docs/CLI_REFERENCE.kr.md) - CLI 도구 문서
- 📄 [DICOM Conformance Statement](docs/DICOM_CONFORMANCE_STATEMENT.md) - DICOM 적합성
- 🚀 [마이그레이션 완료](docs/MIGRATION_COMPLETE.kr.md) - 스레드 시스템 마이그레이션 요약

**데이터베이스 통합:**
- 🗄️ [마이그레이션 가이드](docs/database/MIGRATION_GUIDE.md) - database_system 통합 가이드
- 📚 [API 레퍼런스 (Database)](docs/database/API_REFERENCE.md) - Query Builder API 문서
- 🏛️ [ADR-001](docs/adr/ADR-001-database-system-integration.md) - 아키텍처 결정 기록
- 🏛️ [ADR-002](docs/adr/ADR-002-pacs-storage-port-segmentation.md) - PACS 스토리지 경계와 저장소 세트 계약
- ⚡ [성능 가이드](docs/database/PERFORMANCE_GUIDE.md) - 데이터베이스 최적화 팁
- 📦 [Dependency Manifest](dependency-manifest.json) - 내부/네이티브/FetchContent/npm 입력의 기준 provenance
- ⚖️ [Third-Party Licenses](LICENSE-THIRD-PARTY) - 제품 배포용 서드파티 라이선스 인벤토리

---

## 성능

PACS 시스템은 고성능 동시 처리를 위해 `thread_system` 라이브러리를 활용합니다.
스레드 시스템 마이그레이션(Epic #153)이 성공적으로 완료되어, 모든 직접 `std::thread` 사용이 jthread 지원 및 취소 토큰을 포함한 모던 C++20 추상화로 대체되었습니다.
상세 벤치마크 결과는 [PERFORMANCE_RESULTS.md](docs/PERFORMANCE_RESULTS.md), 마이그레이션 요약은 [MIGRATION_COMPLETE.kr.md](docs/MIGRATION_COMPLETE.kr.md)를 참조하세요.

### 주요 성능 지표

| 지표 | 결과 |
|------|------|
| **Association 지연** | < 1 ms |
| **C-ECHO 처리량** | 89,964 msg/s |
| **C-STORE 처리량** | 31,759 store/s |
| **동시 작업** | 124 ops/s (10 workers) |
| **정상 종료** | 110 ms (활성 연결 포함) |
| **데이터 전송률 (512x512)** | 9,247 MB/s |

### 벤치마크 실행

```bash
# 벤치마크 포함 빌드
cmake -B build -DPACS_BUILD_BENCHMARKS=ON
cmake --build build

# 전체 벤치마크 실행
cmake --build build --target run_full_benchmarks

# 특정 벤치마크 카테고리 실행
./build/bin/thread_performance_benchmarks "[benchmark][association]"
./build/bin/thread_performance_benchmarks "[benchmark][throughput]"
./build/bin/thread_performance_benchmarks "[benchmark][concurrent]"
./build/bin/thread_performance_benchmarks "[benchmark][shutdown]"
```

---

## 코드 통계

<!-- STATS_START -->

| 항목 | 값 |
|------|-----|
| **헤더 파일** | 222개 |
| **소스 파일** | 148개 |
| **헤더 LOC** | 61,462줄 |
| **소스 LOC** | 85,141줄 |
| **예제 LOC** | 34,752줄 |
| **테스트 LOC** | 62,241줄 |
| **총 LOC** | 251,242줄 |
| **테스트 파일** | 141개 이상 |
| **테스트 케이스** | 1,980개 이상 |
| **예제 프로그램** | 32개 |
| **문서** | 57개 마크다운 |
| **CI/CD 워크플로우** | 10개 |
| **버전** | 0.2.0 |
| **최종 업데이트** | 2026-02-20 |

> **측정일**: 2026-02-20. LOC는 `find <dir> -name "*.cpp" -exec cat {} + | wc -l`로 측정, `build-ci/` 제외.

<!-- STATS_END -->

---

## 기여하기

기여를 환영합니다! Pull Request를 제출하기 전에 기여 가이드라인을 읽어주세요.

---

## 라이선스

이 프로젝트는 BSD 3-Clause License로 라이선스됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

---

## 연락처

- **프로젝트 소유자**: kcenon (kcenon@naver.com)
- **저장소**: https://github.com/kcenon/pacs_system
- **이슈**: https://github.com/kcenon/pacs_system/issues

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
