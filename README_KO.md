# PACS System

> **언어:** [English](README.md) | **한국어**

## 개요

kcenon 에코시스템을 기반으로 외부 DICOM 라이브러리 없이 구축된 프로덕션급 C++20 PACS (Picture Archiving and Communication System) 구현체입니다. 이 프로젝트는 기존 고성능 인프라를 활용하여 DICOM 표준을 처음부터 구현합니다.

**주요 특징**:
- **외부 DICOM 라이브러리 제로**: kcenon 에코시스템을 활용한 순수 구현
- **고성능**: SIMD 가속, lock-free 큐, 비동기 I/O 활용
- **프로덕션 품질**: 종합적인 CI/CD, 새니타이저, 품질 메트릭
- **모듈식 아키텍처**: 인터페이스 기반 설계로 관심사의 명확한 분리
- **크로스 플랫폼**: Linux, macOS, Windows 지원

---

## 프로젝트 현황

**현재 단계**: 🔨 Phase 1 완료 - Core & Encoding

| 마일스톤 | 상태 | 목표 |
|----------|------|------|
| 분석 및 문서화 | ✅ 완료 | 1주차 |
| 핵심 DICOM 구조 | ✅ 완료 | 2-5주차 |
| Encoding 모듈 | ✅ 완료 | 2-5주차 |
| 네트워크 프로토콜 (PDU) | 🔄 진행중 | 6-9주차 |
| DIMSE 서비스 | 🔜 예정 | 10-13주차 |
| Storage SCP/SCU | 🔜 예정 | 14-17주차 |
| Query/Retrieve | 🔜 예정 | 18-20주차 |

### Phase 1 성과

**Core 모듈** (113개 테스트 통과):
- `dicom_tag` - DICOM 태그 표현 (Group, Element 쌍)
- `dicom_element` - 태그, VR, 값을 가진 데이터 요소
- `dicom_dataset` - 데이터 요소의 정렬된 컬렉션
- `dicom_file` - DICOM Part 10 파일 읽기/쓰기
- `dicom_dictionary` - 표준 태그 메타데이터 조회

**Encoding 모듈**:
- `vr_type` - 30개 이상의 Value Representation 타입
- `vr_info` - VR 메타데이터 및 검증 유틸리티
- `transfer_syntax` - Transfer Syntax 관리
- `implicit_vr_codec` - Implicit VR Little Endian 코덱
- `explicit_vr_codec` - Explicit VR Little Endian 코덱

---

## 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│                       PACS System                            │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Storage SCP │  │ Q/R SCP     │  │ Worklist/MPPS SCP   │  │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
│         └────────────────┼────────────────────┘             │
│  ┌───────────────────────▼───────────────────────────────┐  │
│  │                  DIMSE Message Handler                 │  │
│  └───────────────────────┬───────────────────────────────┘  │
│  ┌───────────────────────▼───────────────────────────────┐  │
│  │              PDU / Association Manager                 │  │
│  └───────────────────────┬───────────────────────────────┘  │
└──────────────────────────┼──────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
┌────────▼────────┐ ┌──────▼──────┐ ┌────────▼────────┐
│ network_system  │ │thread_system│ │container_system │
│    (TCP/TLS)    │ │(Thread Pool)│ │ (Serialization) │
└─────────────────┘ └─────────────┘ └─────────────────┘
         │                 │                 │
         └─────────────────┼─────────────────┘
                           │
                  ┌────────▼────────┐
                  │  common_system  │
                  │  (Foundation)   │
                  └─────────────────┘
```

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

---

## 프로젝트 구조

```
pacs_system/
├── include/pacs/
│   ├── core/                    # 핵심 DICOM 구현 (✅ 완료)
│   │   ├── dicom_tag.hpp        # 태그 표현 (Group, Element)
│   │   ├── dicom_tag_constants.hpp # 표준 태그 상수
│   │   ├── dicom_element.hpp    # Data Element
│   │   ├── dicom_dataset.hpp    # Data Set
│   │   ├── dicom_file.hpp       # DICOM File (Part 10)
│   │   ├── dicom_dictionary.hpp # Tag Dictionary
│   │   └── tag_info.hpp         # 태그 메타데이터
│   │
│   ├── encoding/                # 인코딩/디코딩 (✅ 완료)
│   │   ├── vr_type.hpp          # Value Representation 열거형
│   │   ├── vr_info.hpp          # VR 메타데이터 및 유틸리티
│   │   ├── transfer_syntax.hpp  # Transfer Syntax
│   │   ├── byte_order.hpp       # 바이트 순서 처리
│   │   ├── implicit_vr_codec.hpp # Implicit VR 코덱
│   │   └── explicit_vr_codec.hpp # Explicit VR 코덱
│   │
│   └── network/                 # 네트워크 프로토콜 (🔄 진행중)
│       ├── pdu_types.hpp        # PDU 타입 정의
│       └── pdu_encoder.hpp      # PDU 인코더
│
├── src/                         # 소스 파일
│   ├── core/                    # Core 구현
│   ├── encoding/                # Encoding 구현
│   └── network/                 # Network 구현
│
├── tests/                       # 테스트 스위트 (113개 테스트)
│   ├── core/                    # Core 모듈 테스트
│   ├── encoding/                # Encoding 모듈 테스트
│   └── network/                 # Network 모듈 테스트
│
├── examples/                    # 샘플 애플리케이션
│   ├── dcm_dump/                # DICOM 파일 검사 유틸리티
│   ├── echo_scu/                # DICOM 연결 테스트 클라이언트
│   ├── echo_scp/                # DICOM 연결 테스트 서버
│   ├── store_scu/               # DICOM 이미지 전송 클라이언트
│   ├── store_scp/               # DICOM 이미지 수신 서버
│   └── pacs_server/             # 완전한 PACS 서버
│
├── docs/                        # 문서
└── CMakeLists.txt               # 빌드 설정
```

---

## 샘플 애플리케이션

프로젝트에는 DICOM 기능을 보여주는 여러 샘플 애플리케이션이 포함되어 있습니다:

### dcm_dump - DICOM 파일 검사기

DICOM 파일 내용을 검사하는 명령줄 유틸리티:

```bash
# 기본 덤프
./build/bin/dcm_dump image.dcm

# 특정 태그만 표시
./build/bin/dcm_dump image.dcm --tags PatientName,PatientID,StudyDate

# 픽셀 데이터 정보 표시
./build/bin/dcm_dump image.dcm --pixel-info

# JSON 출력
./build/bin/dcm_dump image.dcm --format json

# 디렉토리 재귀 처리
./build/bin/dcm_dump ./dicom_folder/ --recursive --summary
```

### echo_scu / echo_scp - 연결 테스트

DICOM 네트워크 연결 테스트:

```bash
# 에코 서버 시작
./build/bin/echo_scp 11112 MY_PACS

# 연결 테스트
./build/bin/echo_scu localhost 11112 MY_PACS
```

### store_scu / store_scp - 이미지 전송

DICOM 이미지 전송:

```bash
# 스토리지 서버 시작
./build/bin/store_scp 11113 STORAGE_SCP --storage ./dicom_storage

# DICOM 파일 전송
./build/bin/store_scu localhost 11113 STORAGE_SCP image.dcm
```

---

## 문서

- 📋 [구현 분석](docs/PACS_IMPLEMENTATION_ANALYSIS_KO.md) - 상세 구현 전략
- 📋 [요구사항 정의서](docs/PRD_KO.md) - 제품 요구사항 문서
- 🏗️ [아키텍처 가이드](docs/ARCHITECTURE_KO.md) - 시스템 아키텍처
- ⚡ [기능 명세](docs/FEATURES_KO.md) - 기능 상세 설명
- 📁 [프로젝트 구조](docs/PROJECT_STRUCTURE_KO.md) - 디렉토리 구조
- 🔧 [API 레퍼런스](docs/API_REFERENCE_KO.md) - API 문서

---

## DICOM 적합성

### 계획된 SOP 클래스

| 서비스 | SOP Class | 우선순위 |
|--------|-----------|----------|
| **Verification** | 1.2.840.10008.1.1 | MVP |
| **CT Storage** | 1.2.840.10008.5.1.4.1.1.2 | MVP |
| **MR Storage** | 1.2.840.10008.5.1.4.1.1.4 | MVP |
| **X-Ray Storage** | 1.2.840.10008.5.1.4.1.1.1.1 | MVP |
| **Patient Root Q/R** | 1.2.840.10008.5.1.4.1.2.1.x | Phase 2 |
| **Study Root Q/R** | 1.2.840.10008.5.1.4.1.2.2.x | Phase 2 |
| **Modality Worklist** | 1.2.840.10008.5.1.4.31 | Phase 2 |
| **MPPS** | 1.2.840.10008.3.1.2.3.3 | Phase 2 |

### Transfer Syntax 지원

| Transfer Syntax | UID | 우선순위 |
|-----------------|-----|----------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | 필수 |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | MVP |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | 선택 |
| JPEG Baseline | 1.2.840.10008.1.2.4.50 | 향후 |
| JPEG 2000 | 1.2.840.10008.1.2.4.90 | 향후 |

---

## 시작하기

### 사전 요구사항

- C++20 호환 컴파일러 (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20+
- kcenon 에코시스템 라이브러리

### 빌드

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

**테스트 결과**: 113개 테스트 통과 (Core: 57, Encoding: 41, Network: 15)

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
