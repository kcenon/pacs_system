# PACS System

Status: active (v1.0을 향해 개발 진행 중) | 최신 릴리스: [v0.1.0](https://github.com/kcenon/pacs_system/releases/tag/v0.1.0)

> **언어:** [English](README.md) | **한국어**

[![CI](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml)
[![Code Coverage](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml)
[![Documentation](https://github.com/kcenon/pacs_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/build-Doxygen.yaml)
[![SBOM Generation](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml)

## 개요

kcenon 에코시스템을 기반으로 구축된 C++20 PACS(Picture Archiving and Communication System)입니다.
DICOM 데이터 모델, 인코딩, 네트워크 프로토콜, 서비스를 이 저장소에서 직접 구현하며 DCMTK나 GDCM에
의존하지 않습니다. DCMTK는 [DCMTK 상호운용성 워크플로](.github/workflows/dcmtk-interop.yml)에서
테스트 상대로만 사용됩니다.

## 기능

- **DICOM 서비스**: Storage, Query/Retrieve, Modality Worklist, MPPS, Storage Commitment, Print Management SCP/SCU; Crow REST 서버 위의 DICOMweb (WADO-RS, STOW-RS, QIDO-RS)
- **모듈식 라이브러리**: core, encoding, network, services, storage, security와 개별적으로 링크할 수 있는 선택 컴포넌트 AI, monitoring, workflow, web, integration
- **구성 요소**: thread_system의 스레드 풀과 큐, network_system의 TCP/TLS 전송, 바이트 스왑·RLE·광도 변환·window/level용 SIMD 코드 경로 (`include/kcenon/pacs/encoding/simd/`)
- **품질 점검**: [새니타이저](.github/workflows/sanitizers.yml), [정적 분석](.github/workflows/static-analysis.yml), [퍼징](.github/workflows/fuzzing.yml)을 포함한 CI/CD
- **플랫폼**: CI가 Ubuntu (GCC, Clang), macOS, Windows (MSVC)에서 빌드하고 각각 CTest를 실행합니다 ([ci.yml](.github/workflows/ci.yml))

## 목차

- [프로젝트 상태](#프로젝트-상태)
- [DICOM 적합성](#dicom-적합성)
- [시작하기](#시작하기)
- [CLI 도구 및 예제](#cli-도구-및-예제)
- [에코시스템 의존성](#에코시스템-의존성)
- [아키텍처](#아키텍처)
- [프로젝트 구조](#프로젝트-구조)
- [성능](#성능)
- [문서](#문서)
- [코드 통계](#코드-통계)
- [규정 준수](#규정-준수)
- [기여하기](#기여하기)
- [라이선스](#라이선스)

## 프로젝트 상태

**최신 게시 패키지**: 0.1.0 — [`v0.1.0` 릴리스](https://github.com/kcenon/pacs_system/releases/tag/v0.1.0), vcpkg 포트 `kcenon-pacs-system` 0.1.0 (port-version 11)
**계획된 마일스톤**: v1.0 — 미출시 API 계약이며, v1.0.0 태그·GitHub 릴리스·레지스트리 버전은 아직 없습니다 ([#1095](https://github.com/kcenon/pacs_system/issues/1095) 및 [#1164](https://github.com/kcenon/pacs_system/issues/1164)에서 관리)
**프로젝트 단계**: 아래 표의 Phase 1-4 컴포넌트는 `v0.1.0` 태그에 포함되어 있으며, 그 이후 기본 브랜치의 작업(예: IHE XDS.b 액터, `include/kcenon/pacs/` 헤더 구조, `pacs_system::pacs_system` CMake 계약)은 미출시 상태입니다

계획된 v1.0 계약은 `include/kcenon/pacs/` 아래의 공개 헤더 표면, `pacs_system::pacs_system` 집계 CMake
타겟, 그리고 문서화된 Doxygen API를 대상으로 합니다. 이 표면은 검토를 위해 고정된 상태일 뿐 출시되지
않았으며, #1095 준비 체크리스트가 닫히고 Tier 0-4 생태계 의존성이 모두 v1.0을 게시한 뒤(#1164)에
출시됩니다. [검토 중인 헤더 목록](docs/v1.0-api-surface.md), [계획된 예외/`Result<T>` 계약](docs/v1.0-throw-policy.md),
[고정 이전 지원 중단 감사](docs/v1.0-deprecation-inventory.md), [0.x → 1.0 마이그레이션 가이드](docs/migration/0.x-to-1.0.md)를
참조하세요. [CHANGELOG](CHANGELOG.md)는 `samples/` → `examples/` 및 `examples/` → `tools/` 재배치(#1139)와
CMake 계약(#1158)을 포함한 변경 사항을 `[Unreleased]` 아래에 기록하며, v1.0.0이 태그되기 전까지
v1.0.0 섹션을 만들지 않습니다.

| 단계 | 범위 | `v0.1.0` 포함 |
|-------|-------|-------------|
| **Phase 1**: 기반 | DICOM 코어, 태그 사전, 파일 I/O (Part 10), 전송 구문 | 예 |
| **Phase 2**: 네트워크 프로토콜 | Upper Layer Protocol (PDU), Association 상태 머신, DIMSE-C, 압축 코덱 | 예 |
| **Phase 3**: 핵심 서비스 | Storage SCP/SCU, 파일 스토리지, 인덱스 데이터베이스, Query/Retrieve, 로깅, 모니터링 | 예 |
| **Phase 4**: 고급 서비스 | REST API, DICOMweb, AI 통합, 클라이언트 모듈, 클라우드 스토리지, 인쇄 관리, 보안, 워크플로우, 주석/뷰어 | 예 |

CI는 단위 테스트를 실행하고([ci.yml](.github/workflows/ci.yml)) 커버리지 보고서를
[Codecov](https://codecov.io/gh/kcenon/pacs_system)에 업로드하며, 단계별 기능 목록은 [기능](docs/FEATURES.kr.md)에 있습니다.

## DICOM 적합성

지원 SOP 클래스의 출처는 [DICOM 적합성 선언문](docs/DICOM_CONFORMANCE_STATEMENT.md) 3절입니다:
Verification; CT, MR, US, XA, XRF, X-Ray, NM, PET, RT, SR, SEG, MG, CR, SC Storage; Patient 및 Study
Root Query/Retrieve; Modality Worklist; MPPS; Storage Commitment; Basic Grayscale Print, Basic Color
Print, Printer. [DCMTK 상호운용성 워크플로](.github/workflows/dcmtk-interop.yml)는 Ubuntu와 macOS에서
DCMTK 도구를 상대로 C-ECHO, C-STORE, C-FIND, C-MOVE를 실행합니다.

### 전송 구문 지원

코덱 기반 전송 구문은 `PACS_BUILD_CODECS=ON`(기본값)과 구성 시점에 찾은 해당 라이브러리가 필요하며,
라이브러리가 없으면 CMake가 경고와 함께 그 코덱을 비활성화합니다(`cmake/dependencies.cmake`).
적합성 선언문 4절에는 그 밖의 전송 구문(Deflated Explicit VR Little Endian, HTJ2K, HEVC, JPEG XL,
Frame Deflate)이 있습니다.

| 전송 구문 | UID | 상태 |
|----------------|-----|--------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | 내장 |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | 내장 |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | 내장 |
| JPEG Baseline (Process 1) | 1.2.840.10008.1.2.4.50 | 빌드 시 libjpeg-turbo 필요 |
| JPEG Lossless (Process 14) | 1.2.840.10008.1.2.4.70 | 내장 |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | 빌드 시 OpenJPEG 필요 |
| JPEG 2000 Lossy | 1.2.840.10008.1.2.4.91 | 빌드 시 OpenJPEG 필요 |
| JPEG-LS Lossless | 1.2.840.10008.1.2.4.80 | 구현됨; 적합성 선언문에는 아직 미기재 (빌드 시 CharLS 필요) |
| JPEG-LS Near-Lossless | 1.2.840.10008.1.2.4.81 | 구현됨; 적합성 선언문에는 아직 미기재 (빌드 시 CharLS 필요) |
| RLE Lossless | 1.2.840.10008.1.2.5 | 내장 |

## 시작하기

### 사전 요구사항

- Concepts를 지원하는 C++20 컴파일러: GCC 10+ (std::format 전체 지원을 위해 GCC 13+ 권장), Clang 10+ (Clang 14+ 권장), MSVC 2022 (19.30+)
- CMake 3.20+와 Ninja (권장); kcenon 에코시스템 라이브러리는 CMake가 자동으로 다운로드합니다

| 플랫폼 | 의존성 |
|--------|--------|
| Linux (Ubuntu 24.04+) | `sudo apt install cmake ninja-build libicu-dev libsqlite3-dev libssl-dev libfmt-dev` |
| macOS | `brew install cmake ninja icu4c sqlite3 openssl@3 fmt` |
| Windows | C++ 워크로드를 포함한 Visual Studio 2022와 [vcpkg](https://vcpkg.io/): `icu`, `sqlite3`, `openssl`, `fmt` |

### 빌드

```bash
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system
cmake -S . -B build
cmake --build build
cd build && ctest --output-on-failure
```

Windows(PowerShell)에서는 `vcpkg install icu:x64-windows sqlite3:x64-windows openssl:x64-windows fmt:x64-windows`를
실행하고 구성 명령에 `-G Ninja -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"`를
추가합니다.

단위 테스트는 kcenon 에코시스템 기본값인 GoogleTest와 달리 **Catch2 v3.4.0**을 사용합니다.
[`docs/ECOSYSTEM.md`](docs/ECOSYSTEM.md#per-system-conventions)와
[#1141](https://github.com/kcenon/pacs_system/issues/1141)을 참조하세요. `(std::min)(a, b)` 같은 Windows
코딩 규칙은 [기여 가이드](docs/contributing/CONTRIBUTING.md#windows-compatibility)에 있습니다.

### 빌드 옵션

```cmake
PACS_BUILD_TESTS (ON)              # 단위 테스트 활성화
PACS_BUILD_EXAMPLES (OFF)          # CLI 도구 빌드 (tools/)
PACS_BUILD_BENCHMARKS (OFF)        # 벤치마크 활성화
PACS_BUILD_STORAGE (ON)            # 스토리지 모듈 빌드
```

### vcpkg를 통한 설치

```bash
vcpkg install kcenon-pacs-system
```

이 명령은 `v0.1.0` 소스 아카이브로 빌드되는 최신 게시 패키지 0.1.0(port-version 11)을 설치합니다.
0.1.0 패키지는 헤더를 `include/pacs/` 아래에 설치하고 `kcenon::pacs::<component>` 타겟
(`core`, `encoding`, `network`, `client`, `services`, `security`, `integration` 및 기능에 따라 추가되는
컴포넌트)을 export합니다:

```cmake
find_package(pacs_system 0.1 CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE kcenon::pacs::core)
```

> port-version 11에 포함된 `usage` 안내문은 `pacs_system::*` 타겟 이름을 나열합니다. 이 이름은 아래의
> 계획된 v1.0 계약에 속하며 0.1.0 패키지는 export하지 않습니다(레지스트리 정정은
> [kcenon/vcpkg-registry#104](https://github.com/kcenon/vcpkg-registry/issues/104)에서 추적).
> 선택 기능과 전체 패키지 기록은 [패키지 상태](docs/PACKAGE_STATE.kr.md#vcpkg-기능)를 참조하세요.

**계획된 v1.0 CMake 계약** (#1158): 집계 타겟 `pacs_system::pacs_system`이며, 아직 어떤 게시 패키지에도
포함되지 않았습니다. 이 브랜치의 소스 설치(`cmake --install`)는 이미 이 타겟을 제공하지만 패키지 버전은
0.1.0으로 보고하므로, v1.0.0이 출시되기 전까지는 `find_package(pacs_system 0.1 REQUIRED)`로 요청하세요.
소비자 예제, 컴포넌트 타겟, 레거시 별칭 `kcenon::pacs_system`은
[패키지 상태](docs/PACKAGE_STATE.kr.md#cmake-타겟-계획된-v10-계약)에 있습니다.

## CLI 도구 및 예제

- `tools/` — 도구마다 하나의 하위 디렉터리: CLI 유틸리티와 `module_example` C++20 모듈 데모 (하위 디렉터리 32개); `-DPACS_BUILD_EXAMPLES=ON`으로 빌드
- `examples/` — `examples/tutorials/` 아래의 5개 점진적 튜토리얼 (`01_hello_dicom`부터 `05_production_pacs`까지); `-DPACS_BUILD_SAMPLES=ON`으로 빌드

이 옵션 이름은 기존 CI 및 다운스트림 소비자와의 호환성을 위해 유지됩니다
([#1139](https://github.com/kcenon/pacs_system/issues/1139) 참조). 빌드 후 빠른 예제:

```bash
./build/bin/echo_scu localhost 11112                  # 연결 검증 (C-ECHO)
./build/bin/store_scu localhost 11112 image.dcm       # 파일 전송 (C-STORE)
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"
./build/bin/dcm_dump image.dcm --format json          # 파일 검사
```

도구는 파일 유틸리티(덤프, 수정, 변환, 비식별화, DICOMDIR, JSON 및 XML), 네트워크 클라이언트(echo,
store, find, move, get, print), 서버(Query/Retrieve, worklist, MPPS, `pacs_server`)로 구성됩니다.
모든 도구와 옵션은 [CLI 레퍼런스](docs/CLI_REFERENCE.kr.md)를 참조하세요.

## 에코시스템 의존성

| 시스템 | pacs_system에서의 용도 | 필수 여부 |
|--------|------------------------|-----------|
| [common_system](https://github.com/kcenon/common_system) | `Result<T>`, `IExecutor`, 이벤트 버스 | 필수 (Tier 0) |
| [container_system](https://github.com/kcenon/container_system) | DICOM 데이터 직렬화 | 필수 (Tier 1) |
| [network_system](https://github.com/kcenon/network_system) | DICOM용 TCP/TLS 전송 | 필수 (Tier 4) |
| [thread_system](https://github.com/kcenon/thread_system) | 스레드 풀과 큐 | network_system 경유 (Tier 1) |
| [logger_system](https://github.com/kcenon/logger_system) | 로깅 인프라 | network_system 경유 (Tier 2) |
| [monitoring_system](https://github.com/kcenon/monitoring_system) | 메트릭 및 추적 | 선택 (Tier 3) |
| [database_system](https://github.com/kcenon/database_system) | 구성 시점에 발견되면 스토리지 인덱스 리포지토리의 쿼리 빌더로 사용 (`PACS_WITH_DATABASE_SYSTEM`) | 선택 (Tier 3) |

의존성 그래프는 [Architecture](docs/ARCHITECTURE.md#ecosystem-dependencies)에, 버전·라이선스·안전 분류는
[IEC 62304 SOUP 등록부](docs/SOUP.md)에 있습니다.

## 아키텍처

[계층 다이어그램](docs/ARCHITECTURE.kr.md#계층-개요)은 REST/DICOMweb, AI, 워크플로우 프런트엔드에서 IHE 액터,
DICOM 서비스, 네트워크 및 보안, 코어/인코딩을 거쳐 kcenon 에코시스템 라이브러리까지 이어집니다.
`kcenon.pacs`의 C++20 모듈 인터페이스가 `src/modules/` 아래에 있지만, 현재 리비전에서는 모듈 빌드가
컴파일되지 않으며 `PACS_BUILD_MODULES=ON`으로 빌드하는 CI 작업도 없으므로 헤더 기반 인터페이스를
사용하세요([상태와 12개 파티션](docs/ARCHITECTURE.kr.md#c20-모듈-아키텍처)).

## 프로젝트 구조

공개 헤더는 `include/kcenon/pacs/<module>/` 아래에 위치합니다. 레거시 `include/pacs/...` 경로는
폐기되었으며, 모든 소비자는 `#include <kcenon/pacs/...>`를 사용해야 합니다.

```
pacs_system/
├── include/kcenon/pacs/  # 공개 헤더
├── src/                  # 소스 구현
├── tests/                # 테스트 스위트
├── examples/             # 점진적 튜토리얼 (examples/tutorials/)
├── tools/                # CLI 유틸리티, 도구마다 하나의 하위 디렉터리
├── docs/                 # 문서 (마크다운)
└── CMakeLists.txt        # 빌드 구성 (project VERSION 0.1.0)
```

모듈별 표와 파일 수준 디렉터리 트리는 [프로젝트 구조](docs/PROJECT_STRUCTURE.kr.md#공개-헤더-모듈)에 있으며,
파일·라인·테스트 수는 CI에서 `scripts/check-stats-drift.sh`가 검사하는 [코드 통계](#코드-통계)에 기록됩니다.

## 성능

[벤치마크](docs/BENCHMARKS.kr.md)에는 스위트, 목표, 실행 명령, 그리고 환경·날짜·명령·결과 파일을 갖춘 기록된
결과가 있으며, [스레드 시스템 마이그레이션 요약](docs/MIGRATION_COMPLETE.kr.md)도 참조하세요.

## 문서

| 주제 | 문서 |
|------|------|
| API | [API Reference (Doxygen)](https://kcenon.github.io/pacs_system/) (`include/kcenon/pacs/**`에서 생성); [API 참조](docs/API_REFERENCE.kr.md) |
| 설계 | [아키텍처](docs/ARCHITECTURE.kr.md), [제품 요구사항](docs/PRD.kr.md), [구현 분석](docs/PACS_IMPLEMENTATION_ANALYSIS.kr.md), [기능](docs/FEATURES.kr.md), [프로젝트 구조](docs/PROJECT_STRUCTURE.kr.md) |
| 도구 | [CLI 레퍼런스](docs/CLI_REFERENCE.kr.md) |
| 적합성 | [DICOM 적합성 선언문](docs/DICOM_CONFORMANCE_STATEMENT.md); [IHE 통합 선언문](docs/IHE_INTEGRATION_STATEMENT.md) — IHE 프로파일 자체 선언 (XDS-I.b, AIRA, PIR); Connectathon에서 검증되지 않음 |
| 업그레이드 | [0.x to 1.0 Migration Guide](docs/migration/0.x-to-1.0.md): 0.1.0 베이스라인과 계획된 1.0.0 API 고정 사이의 모든 호환성 변경 사항, 전후 스니펫, `Result<T>` 마이그레이션 설명, 자가 점검 체크리스트 포함 |
| 데이터베이스 | [Migration Guide](docs/database/MIGRATION_GUIDE.md), [Query Builder API](docs/database/API_REFERENCE.md), [Performance Guide](docs/database/PERFORMANCE_GUIDE.md), [ADR-001](docs/adr/ADR-001-database-system-integration.md), [ADR-002](docs/adr/ADR-002-pacs-storage-port-segmentation.md) (스토리지 경계 및 리포지토리 세트 계약) |
| 패키징 | [패키지 상태](docs/PACKAGE_STATE.kr.md), [Dependency Manifest](dependency-manifest.json) (네이티브, 페치, 내부, 프런트엔드 출처), [Third-Party Licenses](LICENSE-THIRD-PARTY) |
| 색인 | [문서 등록부](docs/README.md) |

## 코드 통계

`scripts/update-readme-stats.sh`가 트리에서 생성한 영문 README의 표를 옮긴 것입니다(집계 규칙은 `scripts/calculate-stats.sh`).
[Documentation Statistics 워크플로](.github/workflows/docs-stats.yml)는 영문 README의 값이 허용 범위를 벗어나거나
이 표의 값이 영문 README와 다르면 실패합니다.

<!-- STATS_START -->
<!-- source: generated by scripts/update-readme-stats.sh from scripts/calculate-stats.sh; checked by scripts/check-stats-drift.sh in the Documentation Statistics workflow (.github/workflows/docs-stats.yml) -->

| 메트릭 | 값 |
|--------|-------|
| **헤더 파일** | 291 파일 |
| **소스 파일** | 217 파일 |
| **헤더 LOC** | ~78,700 라인 |
| **소스 LOC** | ~114,700 라인 |
| **예제 LOC** | ~4,100 라인 |
| **테스트 LOC** | ~89,300 라인 |
| **전체 LOC** | ~286,900 라인 |
| **테스트 파일** | 191 파일 |
| **테스트 케이스** | 2661+ 테스트 |
| **예제 프로그램** | 5 앱 |
| **문서** | 109 마크다운 파일 |
| **CI/CD 워크플로우** | 23 워크플로우 |
| **버전** | 0.1.0 |

<!-- STATS_END -->

## 규정 준수

`pacs_system`은 의료 기관이 정보보안관리체계(ISMS)의 일부로 사용할 수 있는 기술적 기본 요소를
제공합니다. 라이브러리 자체는 인증되지 않았으며, 도입 기관이 이를 통합하고 조직적 통제(정책,
교육, 사고 대응, 업무 연속성 계획)를 제공합니다.

- [SaMD 인증 준비 체크리스트](docs/checklists/README.md) — 제품 범위·적용성 결정, 9개 기술·품질관리 검토, 증거 기록 및 제안된 출시 점검 항목(영문)
- [ISO 27799 통제 매핑](docs/compliance/iso-27799.md) — ATNA 감사 추적, 감사 로그 암호화, TLS 정책, 접근 통제, 익명화가 ISO 27799:2016 조항 7.4–7.7에 어떻게 매핑되는지
- [DICOM Conformance Statement](docs/DICOM_CONFORMANCE_STATEMENT.md) — 구현된 ISO 12052 / DICOM 2023e 서비스, SOP 클래스, 전송 구문
- [common_system / ISO Standards Overview](https://github.com/kcenon/common_system/blob/main/docs/compliance/ISO_OVERVIEW.md) — kcenon 시스템이 다루는 모든 ISO 표준의 에코시스템 수준 색인

## 기여하기

Pull Request를 제출하기 전에 [기여 가이드](docs/contributing/CONTRIBUTING.md)를 읽어 주세요. README 변경은
[README 편집 정책](docs/contributing/README_POLICY.md)을 따르며, `python3 scripts/readme_lint.py`가 이를 검사합니다.

- **프로젝트 소유자**: kcenon (kcenon@naver.com)
- **리포지토리**: https://github.com/kcenon/pacs_system
- **이슈**: https://github.com/kcenon/pacs_system/issues

## 라이선스

이 프로젝트는 BSD 3-Clause 라이선스에 따라 배포됩니다. 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.
