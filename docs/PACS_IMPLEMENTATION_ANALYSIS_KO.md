# PACS 시스템 구현 분석

> **언어:** [English](PACS_IMPLEMENTATION_ANALYSIS.md) | **한국어**

## 1. 개요

본 문서는 기존 kcenon 에코시스템(common_system, container_system, thread_system, logger_system, monitoring_system, network_system)을 활용하여 외부 DICOM 라이브러리 없이 PACS(Picture Archiving and Communication System)를 구현하기 위한 분석 및 전략을 제시합니다.

### 1.1 목표

- **외부 DICOM 라이브러리 무사용**: DCMTK, GDCM 등 외부 라이브러리 없이 순수 구현
- **기존 에코시스템 최대 활용**: 이미 검증된 시스템들을 기반으로 구축
- **프로덕션 레벨 품질**: CI/CD, 테스트, 보안 등 기존 시스템과 동일한 품질 표준 적용

---

## 2. 기존 시스템 분석

### 2.1 시스템 개요

| 시스템 | 주요 기능 | PACS 활용 영역 |
|--------|----------|---------------|
| **common_system** | IExecutor, Result<T>, Event Bus, Error Codes | 공통 인터페이스, 에러 처리 |
| **container_system** | 타입 안전 직렬화, SIMD 가속, Binary/JSON/XML | DICOM 데이터 구조, 직렬화 |
| **thread_system** | Thread Pool, Lock-free Queue, Hazard Pointers | 비동기 작업 처리 |
| **logger_system** | 비동기 로깅, 4.34M msg/s | 시스템 로깅, 감사 로그 |
| **monitoring_system** | 메트릭, 분산 트레이싱, Health Check | 성능 모니터링, 진단 |
| **network_system** | TCP, TLS, WebSocket, HTTP | DICOM 네트워크 통신 |

### 2.2 common_system 활용

```
common_system (Foundation Layer)
├── IExecutor Interface → DICOM 작업 비동기 실행
├── Result<T> Pattern → DICOM 오류 처리
├── Event Bus → DICOM 이벤트 알림
└── Error Code Registry → DICOM 전용 에러 코드 (-800 ~ -899 할당)
```

**주요 활용점:**
- `Result<T>` 패턴으로 DICOM 파싱/전송 에러 처리
- Event Bus로 이미지 수신, 저장 완료 등 이벤트 전파
- 에코시스템 에러 코드 체계 활용 (-800 ~ -899 범위 할당 가능)

### 2.3 container_system 활용

```
container_system (Data Layer)
├── Value Types (15종) → DICOM VR(Value Representation) 매핑
├── Binary Serialization → DICOM 파일 저장/로드
├── SIMD Acceleration → 대용량 이미지 처리 가속
└── Thread-safe Container → 동시 접근 안전
```

**DICOM VR과 Value Type 매핑:**

| DICOM VR | 설명 | container_system Type |
|----------|------|----------------------|
| CS | Code String | string |
| DS | Decimal String | string → double 변환 |
| IS | Integer String | string → int64 변환 |
| LO | Long String | string |
| PN | Person Name | string (구조체 확장) |
| DA | Date | string (YYYYMMDD) |
| TM | Time | string (HHMMSS) |
| UI | Unique Identifier | string |
| UL | Unsigned Long | uint32 |
| US | Unsigned Short | uint16 |
| OB | Other Byte | bytes |
| OW | Other Word | bytes |
| SQ | Sequence | container (nested) |
| AT | Attribute Tag | uint32 |

### 2.4 thread_system 활용

```
thread_system (Concurrency Layer)
├── Thread Pool → 다중 클라이언트 동시 처리
├── Typed Thread Pool → 우선순위 기반 작업 스케줄링
├── Lock-free Queue → 고성능 작업 큐
└── Cancellation Token → 긴 작업 취소 지원
```

**활용 시나리오:**
- C-STORE 요청 병렬 처리 (이미지 저장)
- 대용량 Query/Retrieve 비동기 처리
- 이미지 압축/변환 작업 백그라운드 처리

### 2.5 logger_system 활용

```
logger_system (Logging Layer)
├── Async Logging → 성능 저하 없는 로깅
├── Multiple Writers → 파일, 콘솔, 네트워크 동시 출력
├── Security Audit → HIPAA/GDPR 컴플라이언스
└── Rotating Files → 장기 운영 지원
```

**PACS 로깅 요구사항:**
- 환자 정보 접근 감사 로그 (HIPAA 필수)
- DICOM Association 로깅
- 이미지 저장/조회 이력

### 2.6 monitoring_system 활용

```
monitoring_system (Observability Layer)
├── Performance Monitor → 처리량, 지연시간 추적
├── Distributed Tracing → 요청 흐름 추적
├── Health Monitor → 서비스 상태 체크
└── Circuit Breaker → 장애 격리
```

**모니터링 대상:**
- DICOM Association 성공/실패율
- 이미지 저장 속도 (MB/s)
- Query 응답 시간
- 저장소 사용량

### 2.7 network_system 활용

```
network_system (Network Layer)
├── TCP Server/Client → DICOM Upper Layer Protocol
├── TLS Support → 암호화 통신 (DICOM TLS)
├── Session Management → DICOM Association 관리
└── Async I/O → 고성능 네트워크 처리
```

**DICOM 네트워크 매핑:**
- `messaging_server` → SCP (Service Class Provider)
- `messaging_client` → SCU (Service Class User)
- `messaging_session` → DICOM Association

---

## 3. DICOM 표준 요구사항

### 3.1 DICOM 프로토콜 계층

```
┌─────────────────────────────────────────┐
│        Application (SOP Classes)        │
│   Storage, Query/Retrieve, Worklist     │
├─────────────────────────────────────────┤
│         DIMSE (Message Exchange)        │
│   C-STORE, C-FIND, C-MOVE, C-GET       │
├─────────────────────────────────────────┤
│     Upper Layer (Association Mgmt)      │
│   A-ASSOCIATE, A-RELEASE, P-DATA       │
├─────────────────────────────────────────┤
│         TCP/IP (Transport)              │
│          Port 104, 11112               │
└─────────────────────────────────────────┘
```

### 3.2 필수 구현 항목

#### 3.2.1 DICOM 데이터 구조
- **Data Element**: (Group, Element, VR, Length, Value)
- **Data Set**: Data Element의 순서 집합
- **IOD (Information Object Definition)**: 표준 데이터 모델
- **SOP Class**: 서비스-객체 쌍

#### 3.2.2 DICOM 네트워크 프로토콜
- **PDU (Protocol Data Unit)**: A-ASSOCIATE-RQ/AC/RJ, P-DATA-TF, A-RELEASE-RQ/RP, A-ABORT
- **DIMSE Messages**: C-STORE, C-FIND, C-MOVE, C-GET, C-ECHO, N-CREATE, N-SET, N-GET, N-EVENT-REPORT

#### 3.2.3 Transfer Syntax
- **Implicit VR Little Endian** (필수)
- **Explicit VR Little Endian** (권장)
- **Explicit VR Big Endian** (지원)
- 압축: JPEG, JPEG 2000, RLE (선택)

### 3.3 PACS 서비스 클래스

| 서비스 | SOP Class | 역할 |
|--------|-----------|------|
| **Storage** | Storage SCP | 이미지 수신/저장 |
| **Query/Retrieve** | Q/R SCP | 이미지 검색/전송 |
| **Worklist** | MWL SCP | 검사 스케줄 조회 |
| **MPPS** | MPPS SCP | 검사 수행 상태 관리 |
| **Verification** | Verification SCP | 연결 테스트 (C-ECHO) |

---

## 4. 구현 전략

### 4.1 모듈 아키텍처

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
│   ├── implicit_vr.h        # Implicit VR Codec
│   └── explicit_vr.h        # Explicit VR Codec
│
├── network/                 # 네트워크 프로토콜
│   ├── pdu/                 # Protocol Data Units
│   │   ├── pdu_types.h
│   │   ├── associate_rq.h
│   │   ├── associate_ac.h
│   │   └── p_data.h
│   ├── dimse/               # DIMSE Messages
│   │   ├── dimse_message.h
│   │   ├── c_store.h
│   │   ├── c_find.h
│   │   └── c_move.h
│   └── association.h        # Association Manager
│
├── services/                # DICOM 서비스
│   ├── storage_scp.h        # Storage SCP
│   ├── storage_scu.h        # Storage SCU
│   ├── qr_scp.h             # Query/Retrieve SCP
│   ├── worklist_scp.h       # Modality Worklist SCP
│   └── mpps_scp.h           # MPPS SCP
│
├── storage/                 # 저장소 백엔드
│   ├── storage_interface.h  # 추상 인터페이스
│   ├── file_storage.h       # 파일 시스템 저장소
│   └── database_index.h     # 인덱스 DB
│
└── integration/             # 에코시스템 통합
    ├── container_adapter.h  # container_system 어댑터
    ├── network_adapter.h    # network_system 어댑터
    └── thread_adapter.h     # thread_system 어댑터
```

### 4.2 개발 로드맵

#### Phase 1: 기반 구조 (4주)
1. **DICOM 데이터 구조**
   - Data Element, VR types 구현
   - container_system value types 활용
   - 테스트: 기본 DICOM 파일 읽기/쓰기

2. **Tag Dictionary**
   - DICOM 표준 태그 정의 (PS3.6)
   - 컴파일 타임 상수로 구현

#### Phase 2: 네트워크 프로토콜 (6주)
1. **PDU Layer**
   - network_system TCP 기반 확장
   - A-ASSOCIATE, P-DATA, A-RELEASE 구현

2. **DIMSE Layer**
   - C-ECHO (Verification) 구현
   - C-STORE (Storage) 구현

#### Phase 3: 핵심 서비스 (6주)
1. **Storage SCP/SCU**
   - 이미지 수신/전송
   - 파일 시스템 저장

2. **C-FIND Query**
   - Patient/Study/Series/Image 레벨 검색

#### Phase 4: 고급 서비스 (4주)
1. **C-MOVE/C-GET**
   - 이미지 전송 요청

2. **Worklist SCP**
   - 검사 스케줄 조회

3. **MPPS SCP**
   - 검사 수행 상태 관리

### 4.3 기존 시스템 통합 설계

```
┌─────────────────────────────────────────────────────────────┐
│                       PACS System                            │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Storage SCP │  │ Q/R SCP     │  │ Worklist/MPPS SCP   │  │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
│         │                │                     │             │
│  ┌──────▼────────────────▼─────────────────────▼──────────┐ │
│  │                  DIMSE Message Handler                  │ │
│  └──────────────────────────┬──────────────────────────────┘ │
│                             │                                │
│  ┌──────────────────────────▼──────────────────────────────┐ │
│  │              PDU / Association Manager                   │ │
│  │           (extends network_system session)               │ │
│  └──────────────────────────┬──────────────────────────────┘ │
└─────────────────────────────┼───────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
┌─────────▼─────────┐ ┌───────▼───────┐ ┌─────────▼─────────┐
│  network_system   │ │ thread_system │ │ container_system  │
│  (TCP/TLS)        │ │ (Thread Pool) │ │ (Serialization)   │
└───────────────────┘ └───────────────┘ └───────────────────┘
          │                   │                   │
          └───────────────────┼───────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │   common_system   │
                    │   (Foundation)    │
                    └───────────────────┘
```

---

## 5. 기술적 도전 과제

### 5.1 DICOM 복잡성

| 도전 과제 | 해결 방안 |
|-----------|----------|
| 다양한 VR 타입 (34종) | container_system value types 확장 |
| 중첩 시퀀스 (SQ) | 재귀적 container 구조 활용 |
| Transfer Syntax 변환 | 코덱 모듈 분리 |
| 대용량 이미지 (수 GB) | 스트리밍 처리, 메모리 매핑 |

### 5.2 네트워크 프로토콜

| 도전 과제 | 해결 방안 |
|-----------|----------|
| PDU 파싱/생성 | 바이트 스트림 파서 구현 |
| Association 상태 관리 | FSM (Finite State Machine) |
| 다중 프레젠테이션 컨텍스트 | 협상 테이블 관리 |
| 동시 다중 Association | thread_system 스레드 풀 |

### 5.3 성능 요구사항

| 항목 | 목표 | 전략 |
|------|------|------|
| 이미지 저장 | 100 MB/s | 비동기 I/O, 버퍼링 |
| 동시 연결 | 100+ | Connection pooling |
| Query 응답 | <100ms | 인덱스 DB, 캐싱 |
| 메모리 사용 | <2 GB | 스트리밍, 청크 처리 |

---

## 6. 장단점 분석

### 6.1 외부 라이브러리 미사용의 장점

| 장점 | 설명 |
|------|------|
| **완전한 제어** | 모든 코드를 직접 관리, 커스터마이징 자유 |
| **라이선스 자유** | GPL/LGPL 라이브러리 의존성 제거 |
| **에코시스템 통합** | 기존 시스템과 완벽한 통합 |
| **경량화** | 필요한 기능만 구현 |
| **학습 가치** | DICOM 표준 심층 이해 |

### 6.2 외부 라이브러리 미사용의 단점

| 단점 | 설명 | 완화 전략 |
|------|------|----------|
| **개발 시간** | DCMTK 사용 대비 3-4배 소요 | 단계적 개발, MVP 우선 |
| **표준 준수** | 버그, 누락 위험 | 철저한 테스트, Conformance 검증 |
| **이미지 압축** | JPEG/JPEG2000 구현 복잡 | 초기엔 비압축만 지원 |
| **유지보수** | 표준 변경 시 직접 대응 | 모듈화, 문서화 |

### 6.3 DCMTK와 비교

| 항목 | DCMTK | 자체 구현 |
|------|-------|----------|
| 개발 시간 | 짧음 | 길음 (20주+) |
| 표준 준수 | 검증됨 | 직접 검증 필요 |
| 라이선스 | BSD (수정 배포 조건) | 자유 |
| 에코시스템 통합 | 어댑터 필요 | 네이티브 |
| 커스터마이징 | 제한적 | 완전 자유 |
| 성능 최적화 | 범용 | 특화 가능 |

---

## 7. 권장 사항

### 7.1 MVP (Minimum Viable Product) 범위

**1단계 MVP (12주):**
1. ✅ DICOM 파일 읽기/쓰기 (Part 10)
2. ✅ C-ECHO (연결 테스트)
3. ✅ C-STORE SCP (이미지 수신)
4. ✅ 기본 파일 저장소
5. ✅ 기본 로깅/모니터링

**2단계 확장 (8주):**
1. ✅ C-FIND (Query)
2. ✅ C-MOVE (Retrieve)
3. ✅ 인덱스 DB
4. ✅ 웹 뷰어 연동 (WADO)

### 7.2 대안적 접근

외부 라이브러리를 정말 피하고 싶다면, 다음 절충안도 고려할 수 있습니다:

1. **헤더 온리 라이브러리만 사용**
   - dcmdata 없이 DICOM 파싱만 사용

2. **최소 기능 라이브러리**
   - 특정 SOP 클래스만 지원하는 경량 구현

3. **하이브리드 접근**
   - 네트워크 프로토콜은 자체 구현
   - 이미지 압축은 별도 라이브러리 (libjpeg, openjpeg)

---

## 8. 결론

기존 kcenon 에코시스템은 PACS 구현에 필요한 대부분의 인프라를 제공합니다:

- **network_system**: DICOM 네트워크 기반
- **container_system**: DICOM 데이터 직렬화
- **thread_system**: 비동기 작업 처리
- **logger_system**: 감사 로깅
- **monitoring_system**: 운영 모니터링
- **common_system**: 공통 패턴 및 에러 처리

외부 DICOM 라이브러리 없이 구현하는 것은 **기술적으로 가능**하며, 완전한 제어와 에코시스템 통합이라는 장점이 있습니다. 그러나 **개발 시간이 상당히 길어지며**, 표준 준수를 위한 철저한 테스트가 필요합니다.

**권장 접근법:**
1. MVP 범위로 시작하여 핵심 기능 우선 구현
2. Conformance Statement 테스트로 표준 준수 검증
3. 점진적으로 기능 확장
4. 이미지 압축은 필요 시 외부 라이브러리 도입 고려

---

## 부록 A: DICOM 태그 예시

```cpp
// DICOM 표준 태그 정의 (PS3.6)
namespace dicom::tags {
    // Patient Module
    constexpr Tag PatientName{0x0010, 0x0010};        // PN
    constexpr Tag PatientID{0x0010, 0x0020};          // LO
    constexpr Tag PatientBirthDate{0x0010, 0x0030};   // DA
    constexpr Tag PatientSex{0x0010, 0x0040};         // CS

    // Study Module
    constexpr Tag StudyInstanceUID{0x0020, 0x000D};   // UI
    constexpr Tag StudyDate{0x0008, 0x0020};          // DA
    constexpr Tag StudyTime{0x0008, 0x0030};          // TM
    constexpr Tag AccessionNumber{0x0008, 0x0050};    // SH

    // Series Module
    constexpr Tag SeriesInstanceUID{0x0020, 0x000E};  // UI
    constexpr Tag Modality{0x0008, 0x0060};           // CS
    constexpr Tag SeriesNumber{0x0020, 0x0011};       // IS

    // Image Module
    constexpr Tag SOPInstanceUID{0x0008, 0x0018};     // UI
    constexpr Tag SOPClassUID{0x0008, 0x0016};        // UI
    constexpr Tag InstanceNumber{0x0020, 0x0013};     // IS
    constexpr Tag Rows{0x0028, 0x0010};               // US
    constexpr Tag Columns{0x0028, 0x0011};            // US
    constexpr Tag PixelData{0x7FE0, 0x0010};          // OW/OB
}
```

## 부록 B: 에러 코드 할당

```cpp
// common_system 에러 코드 체계 확장
// pacs_system: -800 ~ -899

namespace pacs::error_codes {
    // DICOM Parsing Errors (-800 ~ -819)
    constexpr int INVALID_DICOM_FILE = -800;
    constexpr int INVALID_VR = -801;
    constexpr int MISSING_REQUIRED_TAG = -802;
    constexpr int INVALID_TRANSFER_SYNTAX = -803;

    // Association Errors (-820 ~ -839)
    constexpr int ASSOCIATION_REJECTED = -820;
    constexpr int ASSOCIATION_ABORTED = -821;
    constexpr int NO_PRESENTATION_CONTEXT = -822;
    constexpr int INVALID_PDU = -823;

    // DIMSE Errors (-840 ~ -859)
    constexpr int DIMSE_FAILURE = -840;
    constexpr int DIMSE_TIMEOUT = -841;
    constexpr int DIMSE_INVALID_RESPONSE = -842;

    // Storage Errors (-860 ~ -879)
    constexpr int STORAGE_FAILED = -860;
    constexpr int DUPLICATE_SOP_INSTANCE = -861;
    constexpr int INVALID_SOP_CLASS = -862;

    // Query Errors (-880 ~ -899)
    constexpr int QUERY_FAILED = -880;
    constexpr int NO_MATCHES_FOUND = -881;
    constexpr int TOO_MANY_MATCHES = -882;
}
```

---

*문서 버전: 0.1.0.0*
*작성일: 2025-11-30*
*작성자: kcenon@naver.com*
