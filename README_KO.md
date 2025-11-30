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

**현재 단계**: 📋 분석 및 계획

| 마일스톤 | 상태 | 목표 |
|----------|------|------|
| 분석 및 문서화 | ✅ 완료 | 1주차 |
| 핵심 DICOM 구조 | 🔜 예정 | 2-5주차 |
| 네트워크 프로토콜 (PDU) | 🔜 예정 | 6-9주차 |
| DIMSE 서비스 | 🔜 예정 | 10-13주차 |
| Storage SCP/SCU | 🔜 예정 | 14-17주차 |
| Query/Retrieve | 🔜 예정 | 18-20주차 |

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
├── core/                    # 핵심 DICOM 구현
│   ├── dicom_element.h      # Data Element
│   ├── dicom_dataset.h      # Data Set
│   ├── dicom_file.h         # DICOM File (Part 10)
│   └── dicom_dictionary.h   # Tag Dictionary
│
├── encoding/                # 인코딩/디코딩
│   ├── vr_types.h           # Value Representation
│   ├── transfer_syntax.h    # Transfer Syntax
│   └── codecs/              # 압축 코덱
│
├── network/                 # 네트워크 프로토콜
│   ├── pdu/                 # Protocol Data Units
│   ├── dimse/               # DIMSE Messages
│   └── association.h        # Association Manager
│
├── services/                # DICOM 서비스
│   ├── storage_scp.h        # Storage SCP
│   ├── qr_scp.h             # Query/Retrieve SCP
│   ├── worklist_scp.h       # Modality Worklist SCP
│   └── mpps_scp.h           # MPPS SCP
│
├── storage/                 # 저장소 백엔드
│   ├── storage_interface.h  # 추상 인터페이스
│   └── file_storage.h       # 파일시스템 저장소
│
├── integration/             # 에코시스템 통합
│   ├── container_adapter.h  # container_system 어댑터
│   ├── network_adapter.h    # network_system 어댑터
│   └── thread_adapter.h     # thread_system 어댑터
│
├── tests/                   # 테스트 스위트
├── examples/                # 사용 예제
├── scripts/                 # 빌드 및 유틸리티 스크립트
└── docs/                    # 문서
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

### 빌드 (준비 중)

```bash
# 저장소 클론
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system

# 의존성 설치
./scripts/dependency.sh

# 빌드
./scripts/build.sh

# 테스트 실행
./scripts/test.sh
```

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
