# SDS - 요구사항 추적성 매트릭스

> **버전:** 1.0.0
> **상위 문서:** [SDS_KO.md](SDS_KO.md)
> **최종 수정일:** 2025-11-30

---

## 목차

- [1. 개요](#1-개요)
- [2. PRD에서 SRS 추적성](#2-prd에서-srs-추적성)
- [3. SRS에서 SDS 추적성](#3-srs에서-sds-추적성)
- [4. 완전한 추적성 체인](#4-완전한-추적성-체인)
- [5. 커버리지 분석](#5-커버리지-분석)

---

## 1. 개요

### 1.1 목적

이 문서는 제품 요구사항(PRD)에서 소프트웨어 요구사항(SRS)을 거쳐 설계 명세(SDS)까지의 완전한 추적성을 제공합니다. 이를 통해 다음을 보장합니다:

- 모든 요구사항이 설계에 반영됨
- 요구사항 없는 설계 요소가 존재하지 않음
- 테스트 계획이 설계 요소를 참조할 수 있음
- 요구사항 변경에 대한 영향 분석 가능

### 1.2 추적성 체인

```
┌─────────┐       ┌─────────┐       ┌─────────┐       ┌─────────┐
│   PRD   │──────►│   SRS   │──────►│   SDS   │──────►│ 테스트  │
│         │       │         │       │         │       │         │
│  FR-x   │       │SRS-xxx  │       │DES-xxx  │       │ TC-xxx  │
│  NFR-x  │       │         │       │SEQ-xxx  │       │         │
│  IR-x   │       │         │       │DES-DB-x │       │         │
└─────────┘       └─────────┘       └─────────┘       └─────────┘
```

### 1.3 요구사항 ID 규칙

| 문서 | 형식 | 예시 |
|------|------|------|
| PRD 기능 | FR-X.Y | FR-1.1, FR-3.2 |
| PRD 비기능 | NFR-X | NFR-1, NFR-3 |
| PRD 통합 | IR-X | IR-1, IR-3 |
| SRS | SRS-MOD-XXX | SRS-CORE-001 |
| SDS 컴포넌트 | DES-MOD-XXX | DES-CORE-001 |
| SDS 시퀀스 | SEQ-XXX | SEQ-004 |
| SDS 데이터베이스 | DES-DB-XXX | DES-DB-001 |

---

## 2. PRD에서 SRS 추적성

### 2.1 기능 요구사항

| PRD ID | PRD 설명 | SRS ID(s) | 커버리지 |
|--------|---------|-----------|----------|
| **FR-1.1** | DICOM 데이터 엘리먼트 (Tag, VR, Length, Value) | SRS-CORE-001, SRS-CORE-002 | 완전 |
| **FR-1.2** | PS3.5 기준 27가지 VR 타입 | SRS-CORE-006 | 완전 |
| **FR-1.3** | DICOM Part 10 파일 형식 | SRS-CORE-004 | 완전 |
| **FR-1.4** | 전송 구문 지원 | SRS-CORE-007, SRS-CORE-008 | 완전 |
| **FR-1.5** | 데이터 사전 (PS3.6) | SRS-CORE-005 | 완전 |
| **FR-2.1** | PDU 타입 (A-ASSOCIATE, P-DATA 등) | SRS-NET-001 | 완전 |
| **FR-2.2** | Association 상태 머신 (8개 상태) | SRS-NET-003 | 완전 |
| **FR-2.3** | DIMSE 메시지 타입 | SRS-NET-002 | 완전 |
| **FR-3.1** | 저장 서비스 (C-STORE) | SRS-SVC-002, SRS-SVC-003 | 완전 |
| **FR-3.2** | 조회/검색 서비스 (C-FIND, C-MOVE) | SRS-SVC-004, SRS-SVC-005 | 완전 |
| **FR-3.3** | Modality Worklist (MWL) | SRS-SVC-006 | 완전 |
| **FR-3.4** | MPPS 서비스 (N-CREATE, N-SET) | SRS-SVC-007 | 완전 |
| **FR-4.1** | 계층적 파일 저장 | SRS-STOR-002 | 완전 |
| **FR-4.2** | 인덱스 데이터베이스 | SRS-STOR-003 | 완전 |

### 2.2 비기능 요구사항

| PRD ID | PRD 설명 | SRS ID(s) | 커버리지 |
|--------|---------|-----------|----------|
| **NFR-1** | 성능 (처리량, 지연시간) | SRS-PERF-001 ~ SRS-PERF-004 | 완전 |
| **NFR-2** | 신뢰성 (가동률, 데이터 무결성) | SRS-REL-001 ~ SRS-REL-003 | 완전 |
| **NFR-3** | 보안 (TLS, 접근 제어, 감사) | SRS-SEC-001 ~ SRS-SEC-003 | 완전 |
| **NFR-4** | 유지보수성 (테스트 커버리지, 문서화) | SRS-MAINT-001 ~ SRS-MAINT-003 | 완전 |

### 2.3 통합 요구사항

| PRD ID | PRD 설명 | SRS ID(s) | 커버리지 |
|--------|---------|-----------|----------|
| **IR-1** | container_system 통합 | SRS-INT-001 | 완전 |
| **IR-2** | network_system 통합 | SRS-INT-002 | 완전 |
| **IR-3** | thread_system 통합 | SRS-INT-003 | 완전 |
| **IR-4** | logger_system 통합 | SRS-INT-004 | 완전 |
| **IR-5** | monitoring_system 통합 | SRS-INT-005 | 완전 |

---

## 3. SRS에서 SDS 추적성

### 3.1 Core 모듈 요구사항

| SRS ID | SRS 설명 | SDS ID(s) | 설계 요소 |
|--------|---------|-----------|----------|
| **SRS-CORE-001** | dicom_tag 구현 | DES-CORE-001 | `dicom_tag` 클래스 |
| **SRS-CORE-002** | dicom_element 구현 | DES-CORE-002 | `dicom_element` 클래스 |
| **SRS-CORE-003** | dicom_dataset 구현 | DES-CORE-003 | `dicom_dataset` 클래스 |
| **SRS-CORE-004** | dicom_file 읽기/쓰기 | DES-CORE-004 | `dicom_file` 클래스 |
| **SRS-CORE-005** | dicom_dictionary 조회 | DES-CORE-005 | `dicom_dictionary` 클래스 |
| **SRS-CORE-006** | VR 타입 처리 | DES-ENC-001, DES-ENC-002 | `vr_type`, `vr_info` |
| **SRS-CORE-007** | 전송 구문 처리 | DES-ENC-003 | `transfer_syntax` 클래스 |
| **SRS-CORE-008** | VR 인코딩/디코딩 | DES-ENC-004, DES-ENC-005 | 코덱 |

### 3.2 Network 모듈 요구사항

| SRS ID | SRS 설명 | SDS ID(s) | 설계 요소 |
|--------|---------|-----------|----------|
| **SRS-NET-001** | PDU 인코딩/디코딩 | DES-NET-001, DES-NET-002 | `pdu_encoder`, `pdu_decoder` |
| **SRS-NET-002** | DIMSE 메시지 처리 | DES-NET-003 | `dimse_message` 클래스 |
| **SRS-NET-003** | Association 관리 | DES-NET-004 | `association` 클래스 |
| **SRS-NET-004** | DICOM 서버 | DES-NET-005 | `dicom_server` 클래스 |

### 3.3 Service 모듈 요구사항

| SRS ID | SRS 설명 | SDS ID(s) | 설계 요소 |
|--------|---------|-----------|----------|
| **SRS-SVC-001** | Verification SCP | DES-SVC-001 | `verification_scp` 클래스 |
| **SRS-SVC-002** | Storage SCP | DES-SVC-002 | `storage_scp` 클래스 |
| **SRS-SVC-003** | Storage SCU | DES-SVC-003 | `storage_scu` 클래스 |
| **SRS-SVC-004** | Query SCP (C-FIND) | DES-SVC-004 | `query_scp` 클래스 |
| **SRS-SVC-005** | Retrieve SCP (C-MOVE/C-GET) | DES-SVC-005 | `retrieve_scp` 클래스 |
| **SRS-SVC-006** | Worklist SCP | DES-SVC-006 | `worklist_scp` 클래스 |
| **SRS-SVC-007** | MPPS SCP | DES-SVC-007 | `mpps_scp` 클래스 |

### 3.4 Storage 모듈 요구사항

| SRS ID | SRS 설명 | SDS ID(s) | 설계 요소 |
|--------|---------|-----------|----------|
| **SRS-STOR-001** | 저장소 인터페이스 | DES-STOR-001 | `storage_interface` |
| **SRS-STOR-002** | 파일 저장소 구현 | DES-STOR-002 | `file_storage` 클래스 |
| **SRS-STOR-003** | 인덱스 데이터베이스 | DES-STOR-003, DES-DB-001 ~ DES-DB-007 | `index_database`, 테이블 |

### 3.5 Integration 모듈 요구사항

| SRS ID | SRS 설명 | SDS ID(s) | 설계 요소 |
|--------|---------|-----------|----------|
| **SRS-INT-001** | container_system 어댑터 | DES-INT-001 | `container_adapter` |
| **SRS-INT-002** | network_system 어댑터 | DES-INT-002 | `network_adapter` |
| **SRS-INT-003** | thread_system 어댑터 | DES-INT-003 | `thread_adapter` |
| **SRS-INT-004** | logger_system 어댑터 | DES-INT-004 | `logger_adapter` |
| **SRS-INT-005** | monitoring_system 어댑터 | DES-INT-005 | `monitoring_adapter` |

### 3.6 시퀀스 다이어그램 매핑

| SRS ID | 시나리오 | SEQ ID(s) | 다이어그램 이름 |
|--------|---------|-----------|---------------|
| SRS-NET-003 | Association 수립 | SEQ-001, SEQ-002, SEQ-003 | Association 흐름 |
| SRS-SVC-002 | C-STORE 수신 | SEQ-004, SEQ-005 | 저장 흐름 |
| SRS-SVC-004 | C-FIND 조회 | SEQ-006, SEQ-007 | 조회 흐름 |
| SRS-SVC-005 | C-MOVE 검색 | SEQ-008 | 검색 흐름 |
| SRS-SVC-006 | MWL 조회 | SEQ-009 | 워크리스트 흐름 |
| SRS-SVC-007 | MPPS 상태 | SEQ-010, SEQ-011 | MPPS 흐름 |
| SRS-REL-003 | 오류 복구 | SEQ-012, SEQ-013, SEQ-014 | 오류 처리 |

---

## 4. 완전한 추적성 체인

### 4.1 DICOM 핵심 데이터 처리

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     DICOM 핵심 데이터 처리 추적                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  FR-1.1 ──────────► SRS-CORE-001 ────────► DES-CORE-001 (dicom_tag)        │
│  (데이터 엘리먼트)  SRS-CORE-002 ────────► DES-CORE-002 (dicom_element)    │
│                                                                              │
│  FR-1.2 ──────────► SRS-CORE-006 ────────► DES-ENC-001 (vr_type)           │
│  (VR 타입)                        ────────► DES-ENC-002 (vr_info)          │
│                                                                              │
│  FR-1.3 ──────────► SRS-CORE-004 ────────► DES-CORE-004 (dicom_file)       │
│  (Part 10 파일)                                                              │
│                                                                              │
│  FR-1.4 ──────────► SRS-CORE-007 ────────► DES-ENC-003 (transfer_syntax)   │
│  (전송 구문)       SRS-CORE-008 ────────► DES-ENC-004 (implicit_vr_codec) │
│                                    ────────► DES-ENC-005 (explicit_vr_codec) │
│                                                                              │
│  FR-1.5 ──────────► SRS-CORE-005 ────────► DES-CORE-005 (dicom_dictionary) │
│  (데이터 사전)                                                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 네트워크 프로토콜

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       네트워크 프로토콜 추적                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  FR-2.1 ──────────► SRS-NET-001 ─────────► DES-NET-001 (pdu_encoder)       │
│  (PDU 타입)                       ─────────► DES-NET-002 (pdu_decoder)      │
│                                                                              │
│  FR-2.2 ──────────► SRS-NET-003 ─────────► DES-NET-004 (association)        │
│  (상태 머신)                      ─────────► SEQ-001 (수립)                  │
│                                   ─────────► SEQ-002 (거부)                  │
│                                   ─────────► SEQ-003 (해제)                  │
│                                                                              │
│  FR-2.3 ──────────► SRS-NET-002 ─────────► DES-NET-003 (dimse_message)      │
│  (DIMSE 메시지)                                                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.3 DICOM 서비스

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         DICOM 서비스 추적                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  FR-3.1 ──────────► SRS-SVC-002 ─────────► DES-SVC-002 (storage_scp)       │
│  (저장 서비스)     SRS-SVC-003 ─────────► DES-SVC-003 (storage_scu)       │
│                                  ─────────► SEQ-004 (C-STORE 성공)         │
│                                  ─────────► SEQ-005 (중복 처리)            │
│                                                                              │
│  FR-3.2 ──────────► SRS-SVC-004 ─────────► DES-SVC-004 (query_scp)         │
│  (조회/검색)       SRS-SVC-005 ─────────► DES-SVC-005 (retrieve_scp)      │
│                                  ─────────► SEQ-006 (Patient Root 조회)   │
│                                  ─────────► SEQ-007 (C-FIND 취소)         │
│                                  ─────────► SEQ-008 (C-MOVE 검색)         │
│                                                                              │
│  FR-3.3 ──────────► SRS-SVC-006 ─────────► DES-SVC-006 (worklist_scp)      │
│  (워크리스트)                    ─────────► SEQ-009 (MWL 조회)             │
│                                  ─────────► DES-DB-006 (worklist 테이블)  │
│                                                                              │
│  FR-3.4 ──────────► SRS-SVC-007 ─────────► DES-SVC-007 (mpps_scp)          │
│  (MPPS)                          ─────────► SEQ-010 (MPPS 진행 중)         │
│                                  ─────────► SEQ-011 (MPPS 완료)           │
│                                  ─────────► DES-DB-005 (mpps 테이블)      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.4 저장 백엔드

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        저장 백엔드 추적                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  FR-4.1 ──────────► SRS-STOR-001 ────────► DES-STOR-001 (storage_interface)│
│  (파일 저장)       SRS-STOR-002 ────────► DES-STOR-002 (file_storage)     │
│                                                                              │
│  FR-4.2 ──────────► SRS-STOR-003 ────────► DES-STOR-003 (index_database)   │
│  (인덱스 DB)                     ────────► DES-DB-001 (patients 테이블)   │
│                                  ────────► DES-DB-002 (studies 테이블)    │
│                                  ────────► DES-DB-003 (series 테이블)     │
│                                  ────────► DES-DB-004 (instances 테이블)  │
│                                  ────────► DES-DB-Q001 ~ DES-DB-Q006      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.5 통합

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           통합 추적                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  IR-1 ────────────► SRS-INT-001 ─────────► DES-INT-001 (container_adapter) │
│  (container_system)              VR ↔ value 매핑                            │
│                                                                              │
│  IR-2 ────────────► SRS-INT-002 ─────────► DES-INT-002 (network_adapter)   │
│  (network_system)                TCP/TLS 처리                               │
│                                                                              │
│  IR-3 ────────────► SRS-INT-003 ─────────► DES-INT-003 (thread_adapter)    │
│  (thread_system)                 작업 큐, 스레드 풀                         │
│                                                                              │
│  IR-4 ────────────► SRS-INT-004 ─────────► DES-INT-004 (logger_adapter)    │
│  (logger_system)                 감사 로깅                                  │
│                                                                              │
│  IR-5 ────────────► SRS-INT-005 ─────────► DES-INT-005 (monitoring_adapter)│
│  (monitoring_system)             성능 메트릭                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.6 비기능 요구사항

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      비기능 요구사항 추적                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  NFR-1 ───────────► SRS-PERF-001 ────────► 스레드 풀 설계                   │
│  (성능)            SRS-PERF-002 ────────► 비동기 I/O 설계                  │
│                    SRS-PERF-003 ────────► 인덱스 최적화                    │
│                    SRS-PERF-004 ────────► 메모리 풀링                      │
│                                                                              │
│  NFR-2 ───────────► SRS-REL-001 ─────────► 원자적 트랜잭션                  │
│  (신뢰성)          SRS-REL-002 ─────────► RAII 자원 관리                   │
│                    SRS-REL-003 ─────────► SEQ-012 ~ SEQ-014 (오류)         │
│                                                                              │
│  NFR-3 ───────────► SRS-SEC-001 ─────────► network_adapter의 TLS           │
│  (보안)            SRS-SEC-002 ─────────► AE Title 화이트리스트            │
│                    SRS-SEC-003 ─────────► logger_adapter 감사              │
│                                                                              │
│  NFR-4 ───────────► SRS-MAINT-001 ───────► 테스트 커버리지 목표            │
│  (유지보수성)      SRS-MAINT-002 ───────► API 문서화                       │
│                    SRS-MAINT-003 ───────► 정적 분석                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. 커버리지 분석

### 5.1 요구사항 커버리지 요약

| 카테고리 | PRD 총계 | SRS 추적 | SDS 추적 | 커버리지 |
|----------|---------|---------|---------|---------|
| 기능 (FR) | 14 | 14 | 14 | 100% |
| 비기능 (NFR) | 4 | 4 | 4 | 100% |
| 통합 (IR) | 5 | 5 | 5 | 100% |
| **총계** | **23** | **23** | **23** | **100%** |

### 5.2 SRS에서 SDS 커버리지

| 모듈 | SRS 개수 | SDS 개수 | 시퀀스 | DB 테이블 | 커버리지 |
|------|---------|---------|--------|----------|---------|
| Core | 8 | 8 | - | - | 100% |
| Network | 4 | 5 | 3 | - | 100% |
| Services | 7 | 7 | 8 | 2 | 100% |
| Storage | 3 | 3 | - | 4 | 100% |
| Integration | 5 | 5 | - | - | 100% |
| Performance | 4 | 해당 없음 | - | - | 반영됨 |
| Reliability | 3 | 해당 없음 | 3 | - | 반영됨 |
| Security | 3 | 해당 없음 | - | - | 반영됨 |
| Maintainability | 3 | 해당 없음 | - | - | 반영됨 |

### 5.3 고아 분석

**고아 요구사항 (설계 없음):** 없음

**고아 설계 (요구사항 없음):** 없음

### 5.4 추적성 격차

| 격차 ID | 설명 | 상태 | 해결 방안 |
|---------|-----|------|----------|
| - | 식별된 항목 없음 | - | - |

### 5.5 영향 분석 템플릿

요구사항이 변경될 때 이 체크리스트를 사용하세요:

```
┌─────────────────────────────────────────────────────────────────┐
│             요구사항 변경 영향 분석                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  변경된 요구사항: ________________________________________      │
│                                                                  │
│  1. 영향 받는 SRS 항목 식별:                                    │
│     □ SRS-CORE-xxx                                              │
│     □ SRS-NET-xxx                                               │
│     □ SRS-SVC-xxx                                               │
│     □ SRS-STOR-xxx                                              │
│     □ SRS-INT-xxx                                               │
│                                                                  │
│  2. 영향 받는 SDS 요소 식별:                                    │
│     □ DES-xxx (컴포넌트 설계)                                   │
│     □ SEQ-xxx (시퀀스 다이어그램)                               │
│     □ DES-DB-xxx (데이터베이스 테이블)                          │
│                                                                  │
│  3. 영향 받는 코드 식별:                                        │
│     □ include/pacs/xxx                                          │
│     □ src/xxx                                                   │
│                                                                  │
│  4. 영향 받는 테스트 식별:                                      │
│     □ tests/xxx                                                 │
│                                                                  │
│  5. 작업량 추정:                                                 │
│     □ 설계 변경:    ___ 시간                                    │
│     □ 구현:         ___ 시간                                    │
│     □ 테스팅:       ___ 시간                                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 부록 A: 요구사항 ID 빠른 참조

### PRD 요구사항

| ID | 설명 |
|----|-----|
| FR-1.1 | DICOM 데이터 엘리먼트 |
| FR-1.2 | VR 타입 |
| FR-1.3 | Part 10 파일 형식 |
| FR-1.4 | 전송 구문 |
| FR-1.5 | 데이터 사전 |
| FR-2.1 | PDU 타입 |
| FR-2.2 | Association 상태 머신 |
| FR-2.3 | DIMSE 메시지 |
| FR-3.1 | 저장 서비스 |
| FR-3.2 | 조회/검색 서비스 |
| FR-3.3 | Modality Worklist |
| FR-3.4 | MPPS 서비스 |
| FR-4.1 | 파일 저장 |
| FR-4.2 | 인덱스 데이터베이스 |
| NFR-1 | 성능 |
| NFR-2 | 신뢰성 |
| NFR-3 | 보안 |
| NFR-4 | 유지보수성 |
| IR-1 | container_system |
| IR-2 | network_system |
| IR-3 | thread_system |
| IR-4 | logger_system |
| IR-5 | monitoring_system |

### SDS 설계 요소

| ID | 요소 |
|----|-----|
| DES-CORE-001 | dicom_tag |
| DES-CORE-002 | dicom_element |
| DES-CORE-003 | dicom_dataset |
| DES-CORE-004 | dicom_file |
| DES-CORE-005 | dicom_dictionary |
| DES-ENC-001 | vr_type |
| DES-ENC-002 | vr_info |
| DES-ENC-003 | transfer_syntax |
| DES-ENC-004 | implicit_vr_codec |
| DES-ENC-005 | explicit_vr_codec |
| DES-NET-001 | pdu_encoder |
| DES-NET-002 | pdu_decoder |
| DES-NET-003 | dimse_message |
| DES-NET-004 | association |
| DES-NET-005 | dicom_server |
| DES-SVC-001 | verification_scp |
| DES-SVC-002 | storage_scp |
| DES-SVC-003 | storage_scu |
| DES-SVC-004 | query_scp |
| DES-SVC-005 | retrieve_scp |
| DES-SVC-006 | worklist_scp |
| DES-SVC-007 | mpps_scp |
| DES-STOR-001 | storage_interface |
| DES-STOR-002 | file_storage |
| DES-STOR-003 | index_database |
| DES-INT-001 | container_adapter |
| DES-INT-002 | network_adapter |
| DES-INT-003 | thread_adapter |
| DES-INT-004 | logger_adapter |
| DES-INT-005 | monitoring_adapter |
| DES-DB-001 | patients 테이블 |
| DES-DB-002 | studies 테이블 |
| DES-DB-003 | series 테이블 |
| DES-DB-004 | instances 테이블 |
| DES-DB-005 | mpps 테이블 |
| DES-DB-006 | worklist 테이블 |
| DES-DB-007 | schema_version 테이블 |

---

*문서 버전: 1.0.0*
*작성일: 2025-11-30*
*작성자: kcenon@naver.com*
