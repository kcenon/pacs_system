# 프로젝트 구조 - PACS 시스템

> **버전:** 1.0.0
> **최종 수정일:** 2025-01-30
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
├── include/                    # 공개 헤더 파일
│   └── pacs/
│       ├── core/               # 코어 DICOM 구조
│       │   ├── dicom_element.h
│       │   ├── dicom_dataset.h
│       │   ├── dicom_file.h
│       │   ├── dicom_dictionary.h
│       │   └── dicom_tags.h
│       │
│       ├── encoding/           # VR 인코딩/디코딩
│       │   ├── vr_types.h
│       │   ├── transfer_syntax.h
│       │   ├── implicit_vr_codec.h
│       │   ├── explicit_vr_codec.h
│       │   └── byte_order.h
│       │
│       ├── network/            # 네트워크 프로토콜
│       │   ├── pdu/
│       │   │   ├── pdu_types.h
│       │   │   ├── pdu_encoder.h
│       │   │   ├── pdu_decoder.h
│       │   │   ├── associate_rq.h
│       │   │   ├── associate_ac.h
│       │   │   ├── associate_rj.h
│       │   │   ├── p_data.h
│       │   │   ├── a_release.h
│       │   │   └── a_abort.h
│       │   │
│       │   ├── dimse/
│       │   │   ├── dimse_message.h
│       │   │   ├── dimse_command.h
│       │   │   ├── c_echo.h
│       │   │   ├── c_store.h
│       │   │   ├── c_find.h
│       │   │   ├── c_move.h
│       │   │   ├── c_get.h
│       │   │   ├── n_create.h
│       │   │   └── n_set.h
│       │   │
│       │   ├── association.h
│       │   ├── presentation_context.h
│       │   └── dicom_server.h
│       │
│       ├── services/           # DICOM 서비스
│       │   ├── service_provider.h
│       │   ├── verification_scp.h
│       │   ├── verification_scu.h
│       │   ├── storage_scp.h
│       │   ├── storage_scu.h
│       │   ├── query_scp.h
│       │   ├── query_scu.h
│       │   ├── retrieve_scp.h
│       │   ├── worklist_scp.h
│       │   └── mpps_scp.h
│       │
│       ├── storage/            # 저장 백엔드
│       │   ├── storage_interface.h
│       │   ├── file_storage.h
│       │   ├── storage_config.h
│       │   └── index_database.h
│       │
│       ├── integration/        # 에코시스템 어댑터
│       │   ├── container_adapter.h
│       │   ├── network_adapter.h
│       │   ├── thread_adapter.h
│       │   ├── logger_adapter.h
│       │   └── monitoring_adapter.h
│       │
│       └── common/             # 공통 유틸리티
│           ├── error_codes.h
│           ├── uid_generator.h
│           └── config.h
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
│   │   └── CMakeLists.txt
│   │
│   ├── storage/
│   │   ├── file_storage.cpp
│   │   ├── index_database.cpp
│   │   └── CMakeLists.txt
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
│   │   └── test_storage.cpp
│   │
│   ├── integration/
│   │   ├── test_interop.cpp
│   │   └── test_conformance.cpp
│   │
│   └── fixtures/
│       ├── sample_ct.dcm
│       ├── sample_mr.dcm
│       └── sample_cr.dcm
│
├── examples/                   # 예제 애플리케이션
│   ├── CMakeLists.txt
│   ├── echo_scu/
│   │   └── main.cpp
│   ├── store_scu/
│   │   └── main.cpp
│   ├── store_scp/
│   │   └── main.cpp
│   ├── query_scu/
│   │   └── main.cpp
│   └── pacs_server/
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
| `dicom_element.h` | 데이터 요소 (태그, VR, 값) |
| `dicom_dataset.h` | 데이터 요소의 정렬된 컬렉션 |
| `dicom_file.h` | DICOM Part 10 파일 처리 |
| `dicom_dictionary.h` | 태그 딕셔너리 및 메타데이터 |
| `dicom_tags.h` | 표준 태그 상수 |

### 인코딩 모듈 (`include/pacs/encoding/`)

Value Representation 인코딩을 처리합니다:

| 파일 | 설명 |
|------|------|
| `vr_types.h` | 27개 VR 타입 정의 |
| `transfer_syntax.h` | Transfer Syntax 관리 |
| `implicit_vr_codec.h` | Implicit VR 인코딩 |
| `explicit_vr_codec.h` | Explicit VR 인코딩 |
| `byte_order.h` | 엔디언 처리 |

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
| `verification_scp/scu.h` | C-ECHO 서비스 |
| `storage_scp/scu.h` | C-STORE 서비스 |
| `query_scp/scu.h` | C-FIND 서비스 |
| `retrieve_scp.h` | C-MOVE/C-GET 서비스 |
| `worklist_scp.h` | Modality Worklist |
| `mpps_scp.h` | MPPS 서비스 |

### 저장 모듈 (`include/pacs/storage/`)

저장 백엔드 추상화:

| 파일 | 설명 |
|------|------|
| `storage_interface.h` | 추상 저장 인터페이스 |
| `file_storage.h` | 파일시스템 구현 |
| `index_database.h` | SQLite 인덱스 |
| `storage_config.h` | 저장 구성 |

### 통합 모듈 (`include/pacs/integration/`)

에코시스템 통합 어댑터:

| 파일 | 설명 |
|------|------|
| `container_adapter.h` | container_system 통합 |
| `network_adapter.h` | network_system 통합 |
| `thread_adapter.h` | thread_system 통합 |
| `logger_adapter.h` | logger_system 통합 |
| `monitoring_adapter.h` | monitoring_system 통합 |

---

## 헤더 파일

### 인클루드 경로 구조

```cpp
// 표준 인클루드 패턴
#include <pacs/core/dicom_element.h>
#include <pacs/encoding/vr_types.h>
#include <pacs/network/association.h>
#include <pacs/services/storage_scp.h>
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

#include <pacs/core/dicom_element.h>  // 자체 헤더 먼저

#include <pacs/encoding/vr_types.h>    // 내부 헤더
#include <pacs/common/error_codes.h>

#include <common/result.h>             // 에코시스템 헤더
#include <container/value.h>

#include <algorithm>                   // 표준 라이브러리
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

*문서 버전: 1.0.0*
*작성일: 2025-01-30*
*작성자: kcenon@naver.com*
