---
doc_id: "PAC-PROJ-004"
doc_title: "프로젝트 구조 - PACS 시스템"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "PROJ"
---

# 프로젝트 구조 - PACS 시스템

> **SSOT**: This document is the single source of truth for **프로젝트 구조 - PACS 시스템**.

> **버전:** 0.1.3.0
> **최종 수정일:** 2025-12-20
> **언어:** [English](PROJECT_STRUCTURE.md) | **한국어**

---

## 목차

- [개요](#개요)
- [공개 헤더 모듈](#공개-헤더-모듈)
- [디렉토리 레이아웃](#디렉토리-레이아웃)
- [모듈 설명](#모듈-설명)
- [헤더 파일](#헤더-파일)
- [소스 파일](#소스-파일)
- [빌드 시스템](#빌드-시스템)
- [문서화](#문서화)
- [테스트 구조](#테스트-구조)

---

## 개요

PACS 시스템은 다른 kcenon 에코시스템 프로젝트와 일관된 모듈러 디렉토리 구조를 따릅니다. 각 모듈은 명확한 책임을 가지며 인터페이스는 `include/` 디렉토리의 헤더 파일을 통해 정의됩니다.

---

## 공개 헤더 모듈

공개 헤더는 `include/kcenon/pacs/<module>/` 아래에 위치합니다. 레거시 `include/pacs/...` 경로는
폐기되었으며, 모든 소비자는 `#include <kcenon/pacs/...>`를 사용해야 합니다. 게시된 0.1.0 패키지는
이 재배치 이전 버전이므로 여전히 헤더를 `include/pacs/` 아래에 설치합니다
([패키지 상태](PACKAGE_STATE.kr.md) 참조).

| 모듈 | 위치 | 설명 |
|--------|----------|-------------|
| **Core** | `include/kcenon/pacs/core/` | DICOM 태그, 요소, 데이터셋, Part 10 파일 I/O, 태그 사전 |
| **Encoding** | `include/kcenon/pacs/encoding/` | VR 타입, 전송 구문, 압축 코덱 (JPEG, JP2K, JPLS, RLE) |
| **Network** | `include/kcenon/pacs/network/` | PDU 인코딩/디코딩, association 상태 머신, DIMSE 프로토콜 |
| **Services** | `include/kcenon/pacs/services/` | SCP/SCU 구현, SOP 클래스 레지스트리, IOD 검증기 |
| **Storage** | `include/kcenon/pacs/storage/` | 파일 스토리지, SQLite 인덱싱, 클라우드 스토리지 (S3/Azure), HSM |
| **Security** | `include/kcenon/pacs/security/` | RBAC, 익명화 (PS3.15), 디지털 서명, TLS |
| **Monitoring** | `include/kcenon/pacs/monitoring/` | 헬스 체크, Prometheus 메트릭 |
| **Web** | `include/kcenon/pacs/web/` | REST API (Crow), DICOMweb (WADO/STOW/QIDO) |
| **Client** | `include/kcenon/pacs/client/` | 작업, 라우팅, 동기, 프리페치, 원격 노드 관리 |
| **Workflow** | `include/kcenon/pacs/workflow/` | 자동 프리페치, 작업 스케줄러, 연구 잠금 관리자 |
| **AI** | `include/kcenon/pacs/ai/` | AI 서비스 커넥터, 결과 처리기 (SR/SEG) |
| **Integration** | `include/kcenon/pacs/integration/` | 에코시스템 어댑터 (container, network, thread, logger, monitoring) |
| **IHE** | `include/kcenon/pacs/ihe/` | IHE 액터 (XDS.b Document Source/Consumer, Registry Query, ATNA 감사) |
| **DI** | `include/kcenon/pacs/di/` | 서비스 구성을 위한 의존성 주입 유틸리티 |
| **Compat** | `include/kcenon/pacs/compat/` | 크로스 플랫폼 및 레거시 include 경로용 호환성 shim |

아래의 디렉토리 레이아웃과 모듈별 파일 목록은 축약본입니다. 최상위 디렉토리와 일부 파일만 보여 주며,
전체 파일 목록은 `git ls-tree -r --name-only HEAD`로 확인할 수 있습니다.

---

## 디렉토리 레이아웃

```
pacs_system/
├── README.md                   # 프로젝트 개요
├── README.kr.md                # 한국어 버전
├── CHANGELOG.md                # 릴리스 노트
├── CONTRIBUTING.md             # 기여 절차
├── LICENSE                     # BSD 3-Clause 라이선스
├── CMakeLists.txt              # 루트 CMake 구성
├── CMakePresets.json           # 구성 및 빌드 프리셋
├── vcpkg.json                  # vcpkg 매니페스트
├── Doxyfile                    # Doxygen 구성
├── .gitignore                  # Git 무시 패턴
│
├── web/                        # 웹 관리 프론트엔드
│   ├── src/                    # React TypeScript 소스
│   │   ├── api/                # API 클라이언트
│   │   ├── components/         # React 컴포넌트
│   │   ├── hooks/              # 커스텀 훅
│   │   ├── lib/                # 유틸리티
│   │   ├── pages/              # 페이지 컴포넌트
│   │   └── types/              # TypeScript 타입 정의
│   ├── package.json            # Node.js 의존성
│   ├── vite.config.ts          # Vite 설정
│   └── tailwind.config.js      # TailwindCSS 설정
│
├── include/                    # 공개 헤더 파일
│   └── kcenon/
│       └── pacs/
│           ├── core/               # 코어 DICOM 구조
│           │   ├── dicom_element.h
│           │   ├── dicom_dataset.h
│           │   ├── dicom_file.h
│           │   ├── dicom_dictionary.h
│           │   ├── dicom_tag.h
│           │   ├── dicom_tag_constants.h
│           │   └── tag_info.h
│           │
│           ├── encoding/           # VR 인코딩/디코딩
│           │   ├── vr_type.h
│           │   ├── vr_info.h
│           │   ├── transfer_syntax.h
│           │   ├── implicit_vr_codec.h
│           │   ├── explicit_vr_codec.h
│           │   ├── byte_order.h
│           │   ├── compression/        # 압축 코덱
│           │   └── simd/               # SIMD 보조 기능
│           │
│           ├── network/            # 네트워크 프로토콜
│           │   ├── pdu_types.h         # PDU 타입 정의
│           │   ├── pdu_encoder.h       # PDU 직렬화
│           │   ├── pdu_decoder.h       # PDU 역직렬화
│           │   ├── association.h       # 연결 상태 머신
│           │   ├── dicom_server.h      # 다중 연결 서버
│           │   ├── server_config.h     # 서버 구성
│           │   │
│           │   ├── detail/             # 내부 구현
│           │   │   └── accept_worker.h     # Accept 루프 (thread_base)
│           │   │
│           │   ├── v2/                 # network_system V2
│           │   │   ├── dicom_server_v2.h           # messaging_server 기반
│           │   │   └── dicom_association_handler.h # 세션별 핸들러
│           │   │
│           │   ├── pipeline/           # I/O 파이프라인
│           │   │
│           │   └── dimse/              # DIMSE 프로토콜
│           │       ├── dimse_message.h
│           │       ├── command_field.h
│           │       └── status_codes.h
│           │
│           ├── services/           # DICOM 서비스
│           │   ├── scp_service.h             # 기본 SCP 인터페이스
│           │   ├── verification_scp.h        # C-ECHO SCP
│           │   ├── storage_scp.h             # C-STORE SCP
│           │   ├── storage_scu.h             # C-STORE SCU
│           │   ├── storage_status.h          # 저장 상태 코드
│           │   ├── query_scp.h               # C-FIND SCP
│           │   ├── retrieve_scp.h            # C-MOVE/C-GET SCP
│           │   ├── worklist_scp.h            # MWL SCP
│           │   ├── mpps_scp.h                # MPPS SCP
│           │   ├── sop_class_registry.h      # 중앙 SOP 클래스 레지스트리
│           │   │
│           │   ├── sop_classes/              # SOP 클래스 정의
│           │   │   ├── us_storage.h          # US 저장 SOP 클래스
│           │   │   └── xa_storage.h          # XA/XRF 저장 SOP 클래스
│           │   │
│           │   └── validation/               # IOD 검증기
│           │       └── xa_iod_validator.h    # XA IOD 검증
│           │
│           ├── storage/            # 저장 백엔드
│           │   ├── storage_interface.h   # 추상 저장 인터페이스
│           │   ├── file_storage.h        # 파일시스템 저장소
│           │   ├── index_database.h      # SQLite 인덱스
│           │   ├── migration_runner.h    # 데이터베이스 스키마 마이그레이션
│           │   ├── migration_record.h    # 마이그레이션 이력 레코드
│           │   ├── patient_record.h      # 환자 데이터 모델
│           │   ├── study_record.h        # 스터디 데이터 모델
│           │   ├── series_record.h       # 시리즈 데이터 모델
│           │   ├── instance_record.h     # 인스턴스 데이터 모델
│           │   ├── worklist_record.h     # 워크리스트 데이터 모델
│           │   └── mpps_record.h         # MPPS 데이터 모델
│           │
│           ├── ai/                 # AI 통합
│           │   ├── ai_result_handler.h   # AI 생성 DICOM 객체 (SR, SEG, PR) 핸들러
│           │   └── ai_service_connector.h # 외부 AI 추론 서비스 커넥터
│           │
│           ├── integration/        # 에코시스템 어댑터
│           │   ├── container_adapter.h   # container_system 통합
│           │   ├── network_adapter.h     # network_system 통합
│           │   ├── thread_pool_adapter.h # thread_system 통합
│           │   ├── logger_adapter.h      # logger_system 통합
│           │   ├── monitoring_adapter.h  # monitoring_system 통합
│           │   └── dicom_session.h       # 고수준 세션 관리
│           │
│           ├── client/             # 작업, 라우팅, 동기화, 프리페치, 원격 노드
│           ├── compat/             # 호환성 shim
│           ├── di/                 # 의존성 주입
│           ├── ihe/                # IHE 액터 (xds/)
│           ├── monitoring/         # 헬스 체크, 메트릭
│           ├── security/           # RBAC, 익명화, 서명, TLS
│           ├── web/                # REST API, DICOMweb
│           └── workflow/           # 자동 프리페치, 작업 스케줄러, 스터디 잠금
│
├── src/                        # 구현 파일, 모듈마다 하나의 디렉토리
│   ├── core/
│   │   ├── dicom_element.cpp
│   │   ├── dicom_dataset.cpp
│   │   ├── dicom_file.cpp
│   │   └── dicom_dictionary.cpp
│   │
│   ├── network/
│   │   ├── dimse/
│   │   │   └── dimse_message.cpp
│   │   ├── pdu_encoder.cpp
│   │   ├── pdu_decoder.cpp
│   │   ├── association.cpp
│   │   └── dicom_server.cpp
│   │
│   ├── modules/                # C++20 모듈 인터페이스 유닛 (*.cppm)
│   ├── ai/
│   ├── client/
│   ├── encoding/
│   ├── ihe/
│   ├── integration/
│   ├── monitoring/
│   ├── security/
│   ├── services/
│   ├── storage/
│   ├── web/
│   └── workflow/
│
├── tests/                      # Catch2 테스트 스위트, 모듈마다 하나의 디렉토리
│   ├── core/
│   │   ├── dicom_element_test.cpp
│   │   ├── dicom_dataset_test.cpp
│   │   └── dicom_file_test.cpp
│   │
│   ├── integration/            # 통합 테스트
│   ├── cmake_consumer/         # find_package(pacs_system) 소비자 검사
│   ├── concurrency/            # 동시성 스트레스 테스트
│   ├── mocks/                  # 테스트 대역
│   ├── modules/                # C++20 모듈 테스트
│   ├── utilities/              # CLI 유틸리티 테스트
│   ├── ai/
│   ├── client/
│   ├── compat/
│   ├── di/
│   ├── encoding/
│   ├── ihe/
│   ├── monitoring/
│   ├── network/
│   ├── security/
│   ├── services/
│   ├── storage/
│   ├── web/
│   └── workflow/
│
├── examples/
│   └── tutorials/              # 5단계 개발자 튜토리얼
│       ├── CMakeLists.txt
│       ├── common/             # 튜토리얼 공용 유틸리티
│       ├── 01_hello_dicom/     # Level 1: DICOM 파일 기본 조작
│       ├── 02_echo_server/     # Level 2: C-ECHO SCP/SCU
│       ├── 03_storage_server/  # Level 3: C-STORE SCP/SCU
│       ├── 04_mini_pacs/       # Level 4: Query/Retrieve 포함 PACS
│       └── 05_production_pacs/ # Level 5: 프로덕션 기능
│
├── tools/                      # CLI 유틸리티와 module_example (하위 디렉토리 32개)
│   ├── echo_scu/               # C-ECHO SCU (검증)
│   │   └── main.cpp
│   ├── store_scu/              # C-STORE SCU (저장)
│   │   └── main.cpp
│   ├── find_scu/               # C-FIND SCU (조회)
│   │   └── main.cpp
│   ├── move_scu/               # C-MOVE SCU (검색)
│   │   └── main.cpp
│   ├── get_scu/                # C-GET SCU (검색)
│   │   └── main.cpp
│   ├── worklist_scu/           # Modality Worklist SCU
│   │   └── main.cpp
│   ├── mpps_scu/               # MPPS SCU
│   │   └── main.cpp
│   ├── echo_scp/               # C-ECHO SCP
│   │   └── main.cpp
│   ├── store_scp/              # C-STORE SCP
│   │   └── main.cpp
│   ├── qr_scp/                 # Query/Retrieve SCP (C-FIND, C-MOVE, C-GET)
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   ├── query_scu/              # Query SCU (레거시)
│   │   └── main.cpp
│   ├── pacs_server/            # 전체 PACS 서버
│   │   └── main.cpp
│   └── module_example/         # C++20 모듈 데모
│       └── main.cpp
│
├── benchmarks/                 # 성능 벤치마크
│   ├── thread_performance/     # Association, 처리량, 동시성, 종료
│   └── simd_performance/       # 바이트 스왑, RLE 보조 단계, 광도 변환, 윈도잉
│
├── fuzz/                       # libFuzzer 타깃 (PACS_BUILD_FUZZ_TARGETS)
│
├── scripts/                    # 유틸리티 스크립트
│   ├── calculate-stats.sh      # 코드 통계
│   ├── update-readme-stats.sh  # README 통계 블록 생성기
│   ├── check-stats-drift.sh    # 통계 drift 검사
│   ├── readme_lint.py          # README 정책 lint
│   ├── verify_release_version.py # 버전 일관성 검사
│   └── tests/                  # 스크립트 단위 테스트
│
├── docs/                       # 문서화
│   ├── README.md               # 문서 레지스트리
│   ├── PRD.md                  # 제품 요구사항
│   ├── PRD.kr.md               # 한국어 버전
│   ├── FEATURES.md             # 기능 문서
│   ├── FEATURES.kr.md          # 한국어 버전
│   ├── ARCHITECTURE.md         # 아키텍처 가이드
│   ├── ARCHITECTURE.kr.md      # 한국어 버전
│   ├── PROJECT_STRUCTURE.md    # 영어 버전
│   ├── PROJECT_STRUCTURE.kr.md # 이 파일
│   ├── API_REFERENCE.md        # API 문서
│   ├── API_REFERENCE.kr.md     # 한국어 버전
│   ├── BENCHMARKS.md           # 성능 벤치마크
│   ├── PRODUCTION_QUALITY.md   # 품질 메트릭
│   ├── CHANGELOG.md            # 버전 히스토리
│   ├── PACS_IMPLEMENTATION_ANALYSIS.md  # 구현 분석
│   ├── DICOM_CONFORMANCE_STATEMENT.md   # DICOM 적합성
│   ├── GETTING_STARTED.md      # 시작 가이드
│   │
│   ├── adr/                    # 아키텍처 결정 기록
│   ├── advanced/               # 고급 주제
│   ├── checklists/             # SaMD 인증 준비 체크리스트
│   ├── contributing/           # 기여 및 README 정책
│   ├── database/               # database_system 가이드
│   ├── integration/            # 통합 가이드
│   ├── migration/              # 마이그레이션 가이드
│   └── performance/            # 성능 문서
│
├── cmake/                      # CMake 모듈
│   ├── options.cmake
│   ├── compiler.cmake
│   ├── dependencies.cmake
│   ├── targets.cmake
│   ├── install.cmake
│   ├── testing.cmake
│   ├── examples.cmake
│   ├── warnings.cmake
│   ├── summary.cmake
│   └── pacs_system-config.cmake.in
│
├── vcpkg-ports/                # vcpkg 오버레이 포트 (kcenon-pacs-system)
│
└── .github/                    # GitHub 구성
    ├── workflows/
    │   ├── ci.yml              # CI 파이프라인
    │   ├── coverage.yml        # 코드 커버리지
    │   ├── static-analysis.yml # 정적 분석
    │   ├── integration-tests.yml # 통합 테스트 파이프라인
    │   ├── sanitizers.yml      # 메모리/UB 새니타이저
    │   ├── sbom.yml            # 소프트웨어 자재 명세서 (SBOM)
    │   └── dependency-security-scan.yml # 보안 스캔
    │
    ├── ISSUE_TEMPLATE/
    │   ├── bug_report.md
    │   └── feature_request.md
    │
    └── pull_request_template.md
```

---

## 모듈 설명

### 코어 모듈 (`include/kcenon/pacs/core/`)

기본적인 DICOM 데이터 구조를 제공합니다:

| 파일 | 설명 |
|------|------|
| `dicom_element.h` | 데이터 요소 (태그, VR, 값) |
| `dicom_dataset.h` | 데이터 요소의 정렬된 컬렉션 |
| `dicom_file.h` | DICOM Part 10 파일 처리 |
| `dicom_dictionary.h` | 태그 딕셔너리 및 메타데이터 |
| `dicom_tag_constants.h` | 표준 태그 상수 |

### 인코딩 모듈 (`include/kcenon/pacs/encoding/`)

Value Representation 인코딩을 처리합니다:

| 파일 | 설명 |
|------|------|
| `vr_type.h` | VR 타입 정의 |
| `transfer_syntax.h` | Transfer Syntax 관리 |
| `implicit_vr_codec.h` | Implicit VR 인코딩 |
| `explicit_vr_codec.h` | Explicit VR 인코딩 |
| `byte_order.h` | 엔디언 처리 |

### 네트워크 모듈 (`include/kcenon/pacs/network/`)

DICOM 네트워크 프로토콜을 구현합니다:

| 서브디렉토리 | 설명 |
|--------------|------|
| `dimse/` | DIMSE 메시지 핸들러 |
| `detail/` | Accept 루프 워커 |
| `pipeline/` | I/O 파이프라인 단계 |
| `v2/` | network_system 기반 서버 |
| 루트 | PDU 타입, 인코더, 디코더, 연결 관리 |

### 서비스 모듈 (`include/kcenon/pacs/services/`)

DICOM 서비스 구현:

| 파일 | 설명 |
|------|------|
| `verification_scp.h` | C-ECHO 서비스 |
| `storage_scp.h`, `storage_scu.h` | C-STORE 서비스 |
| `query_scp.h`, `query_scu.h` | C-FIND 서비스 |
| `retrieve_scp.h`, `retrieve_scu.h` | C-MOVE/C-GET 서비스 |
| `worklist_scp.h` | Modality Worklist |
| `mpps_scp.h` | MPPS 서비스 |
| `sop_class_registry.h` | 중앙 SOP 클래스 레지스트리 |
| `sop_classes/us_storage.h` | US 저장 SOP 클래스 |
| `sop_classes/xa_storage.h` | XA/XRF 저장 SOP 클래스 |
| `validation/xa_iod_validator.h` | XA IOD 검증 |

### 저장 모듈 (`include/kcenon/pacs/storage/`)

저장 백엔드 추상화:

| 파일 | 설명 |
|------|------|
| `storage_interface.h` | 추상 저장 인터페이스 |
| `file_storage.h` | 파일시스템 구현 |
| `index_database.h` | SQLite 인덱스 |
| `s3_storage.h`, `azure_blob_storage.h` | 클라우드 저장 백엔드 |
| `hsm_storage.h` | 계층적 저장소 관리 |

### AI 모듈 (`include/kcenon/pacs/ai/`)

AI 생성 DICOM 객체 핸들러:

| 파일 | 설명 |
|------|------|
| `ai_result_handler.h` | AI 생성 DICOM 객체 (SR, SEG, PR) 핸들러 |
| `ai_service_connector.h` | 외부 AI 추론 서비스 커넥터 |

지원:
- CAD 결과를 포함한 구조화 보고서 (SR)
- 이진/분수 세그먼트를 포함한 세그멘테이션 객체 (SEG)
- 주석 및 측정을 포함한 표시 상태 (PR)

### 통합 모듈 (`include/kcenon/pacs/integration/`)

에코시스템 통합 어댑터:

| 파일 | 설명 |
|------|------|
| `container_adapter.h` | container_system 통합 |
| `network_adapter.h` | network_system 통합 |
| `thread_pool_adapter.h` | thread_system 통합 |
| `logger_adapter.h` | logger_system 통합 |
| `monitoring_adapter.h` | monitoring_system 통합 |

나머지 모듈(`client/`, `compat/`, `di/`, `ihe/`, `monitoring/`, `security/`, `web/`, `workflow/`)은
[공개 헤더 모듈](#공개-헤더-모듈)에 요약되어 있습니다.

---

## 헤더 파일

### 인클루드 경로 구조

```cpp
// 표준 인클루드 패턴
#include <kcenon/pacs/core/dicom_element.h>
#include <kcenon/pacs/encoding/vr_type.h>
#include <kcenon/pacs/network/association.h>
#include <kcenon/pacs/services/storage_scp.h>
```

### 헤더 가드

헤더는 `#pragma once` 또는 `#ifndef PACS_<MODULE>_<NAME>_HPP` 인클루드 가드를 사용합니다:

```cpp
#pragma once

namespace kcenon::pacs::core {
    // ...
}
```

### 네임스페이스 구조

```cpp
namespace kcenon::pacs {
    namespace core { /* DICOM 데이터 구조 */ }
    namespace encoding { /* VR 인코딩 */ }
    namespace network {
        namespace dimse { /* DIMSE 메시지 */ }
        namespace pipeline { /* I/O 파이프라인 */ }
    }
    namespace services { /* DICOM 서비스 */ }
    namespace storage { /* 저장 백엔드 */ }
    namespace ai { /* AI 결과 처리 */ }
    namespace integration { /* 에코시스템 어댑터 */ }
    // 그 외: client, compat, di, ihe, monitoring, security, web, workflow
}
```

---

## 소스 파일

### 구현 규칙

- 파일당 하나의 클래스
- 헤더 파일 구조와 일치
- 해당 헤더를 먼저 인클루드

### 소스 조직

```cpp
// src/core/dicom_element.cpp

#include <kcenon/pacs/core/dicom_element.h>  // 자체 헤더 먼저

#include <kcenon/pacs/core/dicom_dataset.h>  // 그 밖의 pacs_system 헤더
#include <kcenon/pacs/core/result.h>

#include <algorithm>                         // 표준 라이브러리
#include <charconv>

namespace kcenon::pacs::core {

// 구현

} // namespace kcenon::pacs::core
```

---

## 빌드 시스템

### CMake 구조

루트 `CMakeLists.txt`는 프로젝트(`VERSION 0.1.0`, C++20, 실행 파일은 빌드 디렉토리의 `bin/`)를 선언하고
`cmake/`의 다음 모듈을 포함합니다:

| 파일 | 용도 |
|------|------|
| `options.cmake` | `PACS_BUILD_*` 및 `PACS_WITH_*` 옵션 |
| `compiler.cmake` | 컴파일러 경고와 `pacs_apply_warnings()` 함수 |
| `dependencies.cmake` | `find_package`, FetchContent, 에코시스템 의존성 |
| `targets.cmake` | 라이브러리 타겟과 소스 파일 목록 |
| `install.cmake` | 설치 규칙과 `pacs_system::pacs_system` export |
| `testing.cmake` | 테스트 실행 파일과 CTest 등록 |
| `examples.cmake` | CLI 도구 바이너리(`tools/` 아래), 벤치마크, 스모크 테스트 |
| `warnings.cmake` | 모든 PACS 타겟에 컴파일러 경고 적용 |
| `summary.cmake` | 빌드 구성 요약 출력 |

주요 옵션: `PACS_BUILD_TESTS` (ON), `PACS_BUILD_EXAMPLES` (OFF, `tools/`의 CLI 도구),
`PACS_BUILD_SAMPLES` (OFF, `examples/`의 튜토리얼), `PACS_BUILD_BENCHMARKS` (OFF),
`PACS_BUILD_STORAGE` (ON, SQLite3 필요), `PACS_BUILD_MODULES` (OFF).

### 라이브러리 타겟

| 타겟 | 빌드 조건 | 설명 |
|------|-----------|------|
| `pacs_core` | 항상 | 코어 DICOM 구조 |
| `pacs_encoding` | 항상 | VR 인코딩/디코딩 |
| `pacs_network` | 항상 | 네트워크 프로토콜 |
| `pacs_client` | 항상 | 원격 노드, 작업, 라우팅, 동기화, 프리페치 관리 |
| `pacs_services` | 항상 | DICOM 서비스 |
| `pacs_security` | 항상 | RBAC, 익명화, 디지털 서명, TLS |
| `pacs_storage` | `PACS_BUILD_STORAGE`이고 SQLite3를 찾았을 때 | 저장 백엔드 |
| `pacs_ai` | `pacs_storage`가 빌드될 때 | AI 결과 처리 및 서비스 커넥터 |
| `pacs_monitoring` | `pacs_storage`가 빌드될 때 | 헬스 체크 및 메트릭 |
| `pacs_workflow` | `pacs_storage`가 빌드될 때 | 자동 프리페치 및 작업 스케줄러 |
| `pacs_web` | Crow를 사용할 수 있을 때 | REST API 및 DICOMweb |
| `pacs_integration` | container_system, common_system, network_system을 찾았을 때 | 에코시스템 어댑터 |
| `pacs_ihe_xds` | pugixml, libcurl, OpenSSL을 찾았을 때 | IHE XDS.b Document Source 및 Consumer |
| `pacs_system_all` | 항상 | `pacs_system::pacs_system`으로 export되는 INTERFACE 타겟. `pacs_ihe_xds`를 제외하고 위에서 빌드된 모든 라이브러리를 링크 |

---

## 문서화

### 문서 타입

| 타입 | 위치 | 목적 |
|------|------|------|
| PRD | `docs/PRD.md` | 요구사항 |
| 기능 | `docs/FEATURES.md` | 기능 세부사항 |
| 아키텍처 | `docs/ARCHITECTURE.md` | 시스템 설계 |
| API 참조 | `docs/API_REFERENCE.md` | API 문서 |
| 시작 가이드 | `docs/GETTING_STARTED.md` | 사용 가이드 |
| 적합성 | `docs/DICOM_CONFORMANCE_STATEMENT.md` | DICOM 적합성 |
| 레지스트리 | `docs/README.md` | 문서 색인 |

### 언어 지원

많은 문서가 한국어 대응 문서를 갖고 있습니다:
- 영어: `DOCUMENT.md`
- 한국어: `DOCUMENT.kr.md`

`DICOM_CONFORMANCE_STATEMENT.md`처럼 `.kr.md` 파일이 없는 문서는 영어로만 제공됩니다.

---

## 테스트 구조

### 테스트 조직

테스트는 Catch2 v3.4.0을 사용하며 `tests/<module>/` 아래에 있습니다([디렉토리 레이아웃](#디렉토리-레이아웃) 참조).
`cmake/testing.cmake`는 모듈마다 하나의 실행 파일(예: `core_tests`, `encoding_tests`, `network_tests`,
`services_tests`)을 빌드 디렉토리의 `bin/`에 만들고, 각 테스트 케이스를 `catch_discover_tests`로 CTest에
등록합니다.

```
tests/
├── core/           # 코어 모듈 단위 테스트
├── encoding/       # 인코딩 모듈 단위 테스트
├── network/        # 네트워크 모듈 단위 테스트
├── services/       # 서비스 모듈 단위 테스트
├── storage/        # 저장 모듈 단위 테스트
└── integration/    # 통합 테스트
```

### 테스트 명명 규칙

```cpp
// 패턴: <컴포넌트>_test.cpp
// 예제: tests/core/dicom_element_test.cpp

TEST_CASE("dicom_element default construction", "[core][dicom_element]") {
    // ...
}

TEST_CASE("dicom_element string handling", "[core][dicom_element]") {
    // ...
}
```

### 테스트 데이터

단위 테스트는 데이터셋을 코드에서 만들고, Part 10 파일이 필요하면 임시 파일을 씁니다. 통합 테스트 도구는
`tools/integration_tests/test_data/`의 DICOM 파일을 사용합니다:

| 파일 | 설명 |
|------|------|
| `ct_minimal.dcm` | CT 이미지 샘플 |
| `mr_minimal.dcm` | MR 이미지 샘플 |
| `xa_minimal.dcm` | XA 이미지 샘플 |

---

## 문서 히스토리

| 버전 | 날짜 | 작성자 | 변경사항 |
|---------|------|--------|---------|
| 1.0.0 | 2025-11-30 | kcenon | 최초 릴리스 |
| 1.1.0 | 2025-12-04 | kcenon | SOP 클래스 및 검증 디렉토리 추가 |
| 1.2.0 | 2025-12-07 | kcenon | 추가: network/detail/ (accept_worker), network/v2/ (dicom_server_v2, dicom_association_handler) |
| 1.3.0 | 2025-12-13 | kcenon | 추가: ai/ 모듈 - AI 생성 DICOM 객체 (SR, SEG, PR) 핸들러 |
| 1.4.0 | 2025-12-20 | kcenon | 추가: qr_scp 유틸리티 - Query/Retrieve SCP (C-FIND/C-MOVE/C-GET) |

---

*문서 버전: 0.1.3.0*
*작성일: 2025-11-30*
*수정일: 2025-12-20*
*작성자: kcenon@naver.com*
