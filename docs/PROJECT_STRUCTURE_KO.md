# 프로젝트 구조 - PACS 시스템

> **버전:** 0.1.3.0
> **최종 수정일:** 2025-12-20
> **언어:** [English](PROJECT_STRUCTURE.md) | **한국어**

---

## 목차

- [개요](#개요)
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

## 디렉토리 레이아웃

```
pacs_system/
├── README.md                   # 프로젝트 개요
├── README_KO.md                # 한국어 버전
├── LICENSE                     # BSD 3-Clause 라이선스
├── CMakeLists.txt              # 루트 CMake 구성
├── .gitignore                  # Git 무시 패턴
├── .clang-format               # 코드 포매팅 규칙
├── .clang-tidy                 # 정적 분석 규칙
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
│   └── pacs/
│       ├── core/               # 코어 DICOM 구조
│       │   ├── dicom_element.hpp
│       │   ├── dicom_dataset.hpp
│       │   ├── dicom_file.hpp
│       │   ├── dicom_dictionary.hpp
│       │   ├── dicom_tag.hpp
│       │   ├── dicom_tag_constants.hpp
│       │   └── tag_info.hpp
│       │
│       ├── encoding/           # VR 인코딩/디코딩
│       │   ├── vr_type.hpp
│       │   ├── vr_info.hpp
│       │   ├── transfer_syntax.hpp
│       │   ├── implicit_vr_codec.hpp
│       │   ├── explicit_vr_codec.hpp
│       │   └── byte_order.hpp
│       │
│       ├── network/            # 네트워크 프로토콜
│       │   ├── pdu_types.hpp       # PDU 타입 정의
│       │   ├── pdu_encoder.hpp     # PDU 직렬화
│       │   ├── pdu_decoder.hpp     # PDU 역직렬화
│       │   ├── association.hpp     # 연결 상태 머신
│       │   ├── dicom_server.hpp    # 다중 연결 서버
│       │   ├── server_config.hpp   # 서버 구성
│       │   │
│       │   ├── detail/             # 내부 구현
│       │   │   └── accept_worker.hpp   # Accept 루프 (thread_base) [NEW v1.1.0]
│       │   │
│       │   ├── v2/                 # network_system V2 [NEW v1.1.0]
│       │   │   ├── dicom_server_v2.hpp           # messaging_server 기반
│       │   │   └── dicom_association_handler.hpp # 세션별 핸들러
│       │   │
│       │   └── dimse/              # DIMSE 프로토콜
│       │       ├── dimse_message.hpp
│       │       ├── command_field.hpp
│       │       └── status_codes.hpp
│       │
│       ├── services/           # DICOM 서비스
│       │   ├── scp_service.hpp           # 기본 SCP 인터페이스
│       │   ├── verification_scp.hpp      # C-ECHO SCP
│       │   ├── storage_scp.hpp           # C-STORE SCP
│       │   ├── storage_scu.hpp           # C-STORE SCU
│       │   ├── storage_status.hpp        # 저장 상태 코드
│       │   ├── query_scp.hpp             # C-FIND SCP
│       │   ├── retrieve_scp.hpp          # C-MOVE/C-GET SCP
│       │   ├── worklist_scp.hpp          # MWL SCP
│       │   ├── mpps_scp.hpp              # MPPS SCP
│       │   ├── sop_class_registry.hpp    # 중앙 SOP 클래스 레지스트리
│       │   │
│       │   ├── sop_classes/              # SOP 클래스 정의
│       │   │   ├── us_storage.hpp        # US 저장 SOP 클래스
│       │   │   └── xa_storage.hpp        # XA/XRF 저장 SOP 클래스
│       │   │
│       │   └── validation/               # IOD 검증기
│       │       └── xa_iod_validator.hpp  # XA IOD 검증
│       │
│       ├── storage/            # 저장 백엔드
│       │   ├── storage_interface.hpp # 추상 저장 인터페이스
│       │   ├── file_storage.hpp      # 파일시스템 저장소
│       │   ├── index_database.hpp    # SQLite 인덱스 (~2,900줄)
│       │   ├── migration_runner.hpp  # 데이터베이스 스키마 마이그레이션
│       │   ├── migration_record.hpp  # 마이그레이션 이력 레코드
│       │   ├── patient_record.hpp    # 환자 데이터 모델
│       │   ├── study_record.hpp      # 스터디 데이터 모델
│       │   ├── series_record.hpp     # 시리즈 데이터 모델
│       │   ├── instance_record.hpp   # 인스턴스 데이터 모델
│       │   ├── worklist_record.hpp   # 워크리스트 데이터 모델
│       │   └── mpps_record.hpp       # MPPS 데이터 모델
│       │
│       ├── ai/                 # AI 결과 처리 [NEW v1.3.0]
│       │   └── ai_result_handler.hpp # AI 생성 DICOM 객체 (SR, SEG, PR) 핸들러
│       │
│       └── integration/        # 에코시스템 어댑터
│           ├── container_adapter.hpp # container_system 통합
│           ├── network_adapter.hpp   # network_system 통합
│           ├── thread_adapter.hpp    # thread_system 통합
│           ├── logger_adapter.hpp    # logger_system 통합
│           ├── monitoring_adapter.hpp # monitoring_system 통합
│           └── dicom_session.hpp     # 고수준 세션 관리
│
├── src/                        # 구현 파일
│   ├── core/
│   │   ├── dicom_element.cpp
│   │   ├── dicom_dataset.cpp
│   │   ├── dicom_file.cpp
│   │   ├── dicom_dictionary.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── encoding/
│   │   ├── vr_types.cpp
│   │   ├── transfer_syntax.cpp
│   │   ├── implicit_vr_codec.cpp
│   │   ├── explicit_vr_codec.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── network/
│   │   ├── pdu/
│   │   │   ├── pdu_encoder.cpp
│   │   │   ├── pdu_decoder.cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── dimse/
│   │   │   ├── dimse_message.cpp
│   │   │   ├── c_echo.cpp
│   │   │   ├── c_store.cpp
│   │   │   ├── c_find.cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── association.cpp
│   │   ├── dicom_server.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── services/
│   │   ├── verification_scp.cpp
│   │   ├── storage_scp.cpp
│   │   ├── query_scp.cpp
│   │   ├── sop_class_registry.cpp
│   │   ├── sop_classes/
│   │   │   ├── us_storage.cpp
│   │   │   └── xa_storage.cpp
│   │   ├── validation/
│   │   │   └── xa_iod_validator.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── storage/
│   │   ├── file_storage.cpp
│   │   ├── index_database.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── ai/                     # AI 결과 처리 [NEW v1.3.0]
│   │   └── ai_result_handler.cpp
│   │
│   └── integration/
│       ├── container_adapter.cpp
│       ├── network_adapter.cpp
│       └── CMakeLists.txt
│
├── tests/                      # 테스트 스위트
│   ├── CMakeLists.txt
│   ├── core/
│   │   ├── test_dicom_element.cpp
│   │   ├── test_dicom_dataset.cpp
│   │   └── test_dicom_file.cpp
│   │
│   ├── encoding/
│   │   ├── test_vr_types.cpp
│   │   └── test_transfer_syntax.cpp
│   │
│   ├── network/
│   │   ├── test_pdu.cpp
│   │   ├── test_dimse.cpp
│   │   └── test_association.cpp
│   │
│   ├── services/
│   │   ├── test_verification.cpp
│   │   ├── test_storage.cpp
│   │   └── xa_storage_test.cpp
│   │
│   ├── integration/
│   │   ├── test_interop.cpp
│   │   └── test_conformance.cpp
│   │
│   ├── ai/                     # AI 모듈 테스트 [NEW v1.3.0]
│   │   └── ai_result_handler_test.cpp
│   │
│   └── fixtures/
│       ├── sample_ct.dcm
│       ├── sample_mr.dcm
│       └── sample_cr.dcm
│
├── examples/                   # 예제 애플리케이션 및 CLI 유틸리티
│   ├── CMakeLists.txt
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
│   │   ├── main.cpp
│   │   ├── worklist_query_builder.hpp
│   │   └── worklist_result_formatter.hpp
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
│   └── pacs_server/            # 전체 PACS 서버
│       └── main.cpp
│
├── benchmarks/                 # 성능 벤치마크
│   ├── CMakeLists.txt
│   ├── README.md
│   ├── bench_serialization.cpp
│   ├── bench_storage.cpp
│   └── bench_network.cpp
│
├── scripts/                    # 유틸리티 스크립트
│   ├── build.sh                # Unix 빌드 스크립트
│   ├── build.bat               # Windows 빌드 스크립트
│   ├── build.ps1               # PowerShell 빌드 스크립트
│   ├── test.sh                 # 테스트 러너
│   ├── test.bat                # Windows 테스트 러너
│   ├── dependency.sh           # 의존성 설치기
│   ├── dependency.bat          # Windows 의존성 설치기
│   ├── clean.sh                # 빌드 아티팩트 정리
│   └── format.sh               # 코드 포매터
│
├── docs/                       # 문서화
│   ├── README.md               # 문서 인덱스
│   ├── PRD.md                  # 제품 요구사항
│   ├── PRD_KO.md               # 한국어 버전
│   ├── FEATURES.md             # 기능 문서
│   ├── FEATURES_KO.md          # 한국어 버전
│   ├── ARCHITECTURE.md         # 아키텍처 가이드
│   ├── ARCHITECTURE_KO.md      # 한국어 버전
│   ├── PROJECT_STRUCTURE.md    # 이 파일
│   ├── PROJECT_STRUCTURE_KO.md # 한국어 버전
│   ├── API_REFERENCE.md        # API 문서
│   ├── API_REFERENCE_KO.md     # 한국어 버전
│   ├── BENCHMARKS.md           # 성능 벤치마크
│   ├── PRODUCTION_QUALITY.md   # 품질 메트릭
│   ├── CHANGELOG.md            # 버전 히스토리
│   ├── PACS_IMPLEMENTATION_ANALYSIS.md  # 구현 분석
│   │
│   ├── guides/                 # 사용자 가이드
│   │   ├── QUICK_START.md
│   │   ├── BUILD_GUIDE.md
│   │   ├── INTEGRATION.md
│   │   ├── TROUBLESHOOTING.md
│   │   └── FAQ.md
│   │
│   ├── conformance/            # DICOM 적합성
│   │   ├── CONFORMANCE_STATEMENT.md
│   │   └── SOP_CLASSES.md
│   │
│   └── advanced/               # 고급 주제
│       ├── CUSTOM_SOP.md
│       ├── TRANSFER_SYNTAX.md
│       └── PRIVATE_TAGS.md
│
├── cmake/                      # CMake 모듈
│   ├── FindCommonSystem.cmake
│   ├── FindContainerSystem.cmake
│   ├── FindNetworkSystem.cmake
│   ├── FindThreadSystem.cmake
│   ├── FindLoggerSystem.cmake
│   ├── FindMonitoringSystem.cmake
│   ├── CompilerOptions.cmake
│   └── Testing.cmake
│
└── .github/                    # GitHub 구성
    ├── workflows/
    │   ├── ci.yml              # CI 파이프라인
    │   ├── coverage.yml        # 코드 커버리지
    │   ├── static-analysis.yml # 정적 분석
    │   └── build-Doxygen.yaml  # API 문서
    │
    ├── ISSUE_TEMPLATE/
    │   ├── bug_report.md
    │   └── feature_request.md
    │
    └── PULL_REQUEST_TEMPLATE.md
```

---

## 모듈 설명

### 코어 모듈 (`include/pacs/core/`)

기본적인 DICOM 데이터 구조를 제공합니다:

| 파일 | 설명 |
|------|------|
| `dicom_element.hpp` | 데이터 요소 (태그, VR, 값) |
| `dicom_dataset.hpp` | 데이터 요소의 정렬된 컬렉션 |
| `dicom_file.hpp` | DICOM Part 10 파일 처리 |
| `dicom_dictionary.hpp` | 태그 딕셔너리 및 메타데이터 |
| `dicom_tag.hpp` | 표준 태그 상수 |

### 인코딩 모듈 (`include/pacs/encoding/`)

Value Representation 인코딩을 처리합니다:

| 파일 | 설명 |
|------|------|
| `vr_type.hpp` | 34개 VR 타입 정의 |
| `transfer_syntax.hpp` | Transfer Syntax 관리 |
| `implicit_vr_codec.hpp` | Implicit VR 인코딩 |
| `explicit_vr_codec.hpp` | Explicit VR 인코딩 |
| `byte_order.hpp` | 엔디언 처리 |

### 네트워크 모듈 (`include/pacs/network/`)

DICOM 네트워크 프로토콜을 구현합니다:

| 서브디렉토리 | 설명 |
|--------------|------|
| `pdu/` | Protocol Data Unit 타입 |
| `dimse/` | DIMSE 메시지 핸들러 |
| 루트 | 연결 관리 |

### 서비스 모듈 (`include/pacs/services/`)

DICOM 서비스 구현:

| 파일 | 설명 |
|------|------|
| `verification_scp/scu.hpp` | C-ECHO 서비스 |
| `storage_scp/scu.hpp` | C-STORE 서비스 |
| `query_scp/scu.hpp` | C-FIND 서비스 |
| `retrieve_scp.hpp` | C-MOVE/C-GET 서비스 |
| `worklist_scp.hpp` | Modality Worklist |
| `mpps_scp.hpp` | MPPS 서비스 |
| `sop_class_registry.hpp` | 중앙 SOP 클래스 레지스트리 |
| `sop_classes/us_storage.hpp` | US 저장 SOP 클래스 |
| `sop_classes/xa_storage.hpp` | XA/XRF 저장 SOP 클래스 |
| `validation/xa_iod_validator.hpp` | XA IOD 검증 |

### 저장 모듈 (`include/pacs/storage/`)

저장 백엔드 추상화:

| 파일 | 설명 |
|------|------|
| `storage_interface.hpp` | 추상 저장 인터페이스 |
| `file_storage.hpp` | 파일시스템 구현 |
| `index_database.hpp` | SQLite 인덱스 |
| `storage_config.hpp` | 저장 구성 |

### AI 모듈 (`include/pacs/ai/`) [NEW v1.3.0]

AI 생성 DICOM 객체 핸들러:

| 파일 | 설명 |
|------|------|
| `ai_result_handler.hpp` | AI 생성 DICOM 객체 (SR, SEG, PR) 핸들러 |

지원:
- CAD 결과를 포함한 구조화 보고서 (SR)
- 이진/분수 세그먼트를 포함한 세그멘테이션 객체 (SEG)
- 주석 및 측정을 포함한 표시 상태 (PR)

### 통합 모듈 (`include/pacs/integration/`)

에코시스템 통합 어댑터:

| 파일 | 설명 |
|------|------|
| `container_adapter.hpp` | container_system 통합 |
| `network_adapter.hpp` | network_system 통합 |
| `thread_adapter.hpp` | thread_system 통합 |
| `logger_adapter.hpp` | logger_system 통합 |
| `monitoring_adapter.hpp` | monitoring_system 통합 |

---

## 헤더 파일

### 인클루드 경로 구조

```cpp
// 표준 인클루드 패턴
#include <pacs/core/dicom_element.hpp>
#include <pacs/encoding/vr_type.hpp>
#include <pacs/network/association.hpp>
#include <pacs/services/storage_scp.hpp>
```

### 헤더 가드

모든 헤더는 인클루드 가드에 `#pragma once`를 사용합니다:

```cpp
#pragma once

namespace pacs::core {
    // ...
}
```

### 네임스페이스 구조

```cpp
namespace pacs {
    namespace core { /* DICOM 데이터 구조 */ }
    namespace encoding { /* VR 인코딩 */ }
    namespace network {
        namespace pdu { /* PDU 타입 */ }
        namespace dimse { /* DIMSE 메시지 */ }
    }
    namespace services { /* DICOM 서비스 */ }
    namespace storage { /* 저장 백엔드 */ }
    namespace ai { /* AI 결과 처리 [NEW v1.3.0] */ }
    namespace integration { /* 에코시스템 어댑터 */ }
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
// dicom_element.cpp

#include <pacs/core/dicom_element.hpp>  // 자체 헤더 먼저

#include <pacs/encoding/vr_type.hpp>    // 내부 헤더
#include <pacs/core/dicom_tag.hpp>

#include <common/result.h>              // 에코시스템 헤더
#include <container/value.h>

#include <algorithm>                    // 표준 라이브러리
#include <vector>

namespace pacs::core {

// 구현

} // namespace pacs::core
```

---

## 빌드 시스템

### CMake 구조

```cmake
# 루트 CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(pacs_system VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 옵션
option(BUILD_TESTING "테스트 빌드" ON)
option(BUILD_EXAMPLES "예제 빌드" ON)
option(BUILD_BENCHMARKS "벤치마크 빌드" OFF)
option(BUILD_DOCS "문서 빌드" OFF)

# 에코시스템 의존성 찾기
find_package(CommonSystem REQUIRED)
find_package(ContainerSystem REQUIRED)
find_package(NetworkSystem REQUIRED)
find_package(ThreadSystem REQUIRED)
find_package(LoggerSystem REQUIRED)
find_package(MonitoringSystem REQUIRED)

# 서브디렉토리 추가
add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
```

### 라이브러리 타겟

| 타겟 | 타입 | 설명 |
|------|------|------|
| `pacs_core` | STATIC | 코어 DICOM 구조 |
| `pacs_encoding` | STATIC | VR 인코딩/디코딩 |
| `pacs_network` | STATIC | 네트워크 프로토콜 |
| `pacs_services` | STATIC | DICOM 서비스 |
| `pacs_storage` | STATIC | 저장 백엔드 |
| `pacs_ai` | STATIC | AI 결과 처리 [NEW v1.3.0] |
| `pacs_system` | INTERFACE | 올인원 타겟 |

---

## 문서화

### 문서 타입

| 타입 | 위치 | 목적 |
|------|------|------|
| PRD | `docs/PRD.md` | 요구사항 |
| 기능 | `docs/FEATURES.md` | 기능 세부사항 |
| 아키텍처 | `docs/ARCHITECTURE.md` | 시스템 설계 |
| API 참조 | `docs/API_REFERENCE.md` | API 문서 |
| 가이드 | `docs/guides/` | 사용자 가이드 |
| 적합성 | `docs/conformance/` | DICOM 적합성 |

### 언어 지원

모든 문서는 영어와 한국어로 제공됩니다:
- 영어: `DOCUMENT.md`
- 한국어: `DOCUMENT_KO.md`

---

## 테스트 구조

### 테스트 조직

```
tests/
├── core/           # 코어 모듈 단위 테스트
├── encoding/       # 인코딩 모듈 단위 테스트
├── network/        # 네트워크 모듈 단위 테스트
├── services/       # 서비스 모듈 단위 테스트
├── integration/    # 통합 테스트
└── fixtures/       # 테스트 데이터 파일
```

### 테스트 명명 규칙

```cpp
// 패턴: test_<모듈>_<기능>.cpp
// 예제: test_dicom_element.cpp

TEST(DicomElement, CreateWithTag) {
    // ...
}

TEST(DicomElement, SetValue) {
    // ...
}
```

### 테스트 데이터

`tests/fixtures/`에 저장된 테스트 DICOM 파일:

| 파일 | 설명 |
|------|------|
| `sample_ct.dcm` | CT 이미지 샘플 |
| `sample_mr.dcm` | MR 이미지 샘플 |
| `sample_cr.dcm` | CR 이미지 샘플 |

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
