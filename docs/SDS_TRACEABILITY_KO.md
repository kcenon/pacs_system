# SDS - 요구사항 추적성 매트릭스

> **버전:** 0.1.3.2
> **상위 문서:** [SDS_KO.md](SDS_KO.md)
> **최종 수정일:** 2026-02-07

---

## 목차

- [1. 개요](#1-개요)
- [2. PRD에서 SRS 추적성](#2-prd에서-srs-추적성)
- [3. SRS에서 SDS 추적성](#3-srs에서-sds-추적성)
- [4. 완전한 추적성 체인](#4-완전한-추적성-체인)
- [5. SDS에서 구현 추적성](#5-sds에서-구현-추적성)
- [6. SDS에서 테스트 추적성](#6-sds에서-테스트-추적성)
- [7. 커버리지 분석](#7-커버리지-분석)

---

## 1. 개요

### 1.1 목적

이 문서는 제품 요구사항(PRD)에서 소프트웨어 요구사항(SRS)을 거쳐 설계 명세(SDS)까지의 완전한 추적성을 제공합니다. 이를 통해 다음을 보장합니다:

- 모든 요구사항이 설계에 반영됨
- 요구사항 없는 설계 요소가 존재하지 않음
- 테스트 계획이 설계 요소를 참조할 수 있음
- 요구사항 변경에 대한 영향 분석 가능

### 1.2 추적성 체인 (V-모델)

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              V-모델 추적성                                        │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  요구사항 단계                                        검증/확인 단계              │
│  ────────────                                        ──────────────              │
│                                                                                  │
│  ┌─────────┐                                              ┌─────────────────┐   │
│  │   PRD   │◄────────────────────────────────────────────►│   인수 테스트    │   │
│  │  FR-x   │                  Validation                  │   (Validation)  │   │
│  │  NFR-x  │                                              └─────────────────┘   │
│  │  IR-x   │                                                       ▲            │
│  └────┬────┘                                                       │            │
│       │                                                            │            │
│       ▼                                                            │            │
│  ┌─────────┐                                              ┌─────────────────┐   │
│  │   SRS   │◄────────────────────────────────────────────►│  시스템 테스트   │   │
│  │SRS-xxx  │                  Validation                  │   (Validation)  │   │
│  └────┬────┘                                              └─────────────────┘   │
│       │                                                            ▲            │
│       ▼                                                            │            │
│  ┌─────────┐                                              ┌─────────────────┐   │
│  │   SDS   │◄────────────────────────────────────────────►│   통합 테스트    │   │
│  │DES-xxx  │                 Verification                 │ (Verification)  │   │
│  │SEQ-xxx  │                                              └─────────────────┘   │
│  └────┬────┘                                                       ▲            │
│       │                                                            │            │
│       ▼                                                            │            │
│  ┌─────────┐                                              ┌─────────────────┐   │
│  │  코드   │◄────────────────────────────────────────────►│   단위 테스트    │   │
│  │ .hpp    │                 Verification                 │ (Verification)  │   │
│  │ .cpp    │                                              └─────────────────┘   │
│  └─────────┘                                                                    │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘

범례:
  - Validation: "올바른 제품을 만들고 있는가?" (SRS 요구사항 충족 확인)
  - Verification: "제품을 올바르게 만들고 있는가?" (SDS 설계대로 구현 확인)
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
| **FR-1.2** | PS3.5 기준 34가지 VR 타입 | SRS-CORE-006 | 완전 |
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
| **IR-1** | container_system 통합 | SRS-INT-002 | 완전 |
| **IR-2** | network_system 통합 | SRS-INT-003 | 완전 |
| **IR-3** | thread_system 통합 | SRS-INT-004 | 완전 |
| **IR-4** | logger_system 통합 | SRS-INT-005 | 완전 |
| **IR-5** | monitoring_system 통합 | SRS-INT-006 | 완전 |
| **IR-6** | ITK/VTK 통합 | SRS-INT-007 | 완전 |
| **IR-7** | Crow REST Framework 통합 | SRS-INT-008 | 완전 |
| **IR-8** | AWS SDK 통합 | SRS-INT-009 | 완전 |
| **IR-9** | Azure SDK 통합 | SRS-INT-010 | 완전 |

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
| **SRS-INT-001** | common_system IExecutor 어댑터 | DES-INT-009 | `executor_adapter` |
| **SRS-INT-002** | container_system 어댑터 | DES-INT-001 | `container_adapter` |
| **SRS-INT-003** | network_system 어댑터 | DES-INT-002 | `network_adapter` |
| **SRS-INT-004** | thread_system 어댑터 | DES-INT-003 | `thread_adapter` |
| **SRS-INT-005** | logger_system 어댑터 | DES-INT-004 | `logger_adapter` |
| **SRS-INT-006** | monitoring_system 어댑터 | DES-INT-005 | `monitoring_adapter` |
| **SRS-INT-007** | ITK/VTK 통합 | DES-INT-008 | `itk_adapter` |
| **SRS-INT-008** | Crow REST Framework | DES-WEB-001 | `rest_server` |
| **SRS-INT-009** | AWS SDK 통합 | DES-STOR-005 | `s3_storage` |
| **SRS-INT-010** | Azure SDK 통합 | DES-STOR-006 | `azure_blob_storage` |

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
│  (PRD IR 없음) ──► SRS-INT-001 ─────────► DES-INT-009 (executor_adapter)   │
│  (common_system)                 IExecutor, value mapping                   │
│                                                                              │
│  IR-1 ────────────► SRS-INT-002 ─────────► DES-INT-001 (container_adapter) │
│  (container_system)              의존성 주입                                │
│                                                                              │
│  IR-2 ────────────► SRS-INT-003 ─────────► DES-INT-002 (network_adapter)   │
│  (network_system)                TCP/TLS 처리                               │
│                                                                              │
│  IR-3 ────────────► SRS-INT-004 ─────────► DES-INT-003 (thread_adapter)    │
│  (thread_system)                 작업 큐, 스레드 풀                         │
│                                                                              │
│  IR-4 ────────────► SRS-INT-005 ─────────► DES-INT-004 (logger_adapter)    │
│  (logger_system)                 감사 로깅                                  │
│                                                                              │
│  IR-5 ────────────► SRS-INT-006 ─────────► DES-INT-005 (monitoring_adapter)│
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

## 5. SDS에서 구현 추적성

이 섹션은 설계 요소와 해당 소스 코드 파일 간의 매핑을 제공합니다.

### 5.1 Core 모듈 구현

| SDS ID | 설계 요소 | 헤더 파일 | 소스 파일 |
|--------|---------|-----------|----------|
| DES-CORE-001 | `dicom_tag` | `include/pacs/core/dicom_tag.hpp` | `src/core/dicom_tag.cpp` |
| DES-CORE-002 | `dicom_element` | `include/pacs/core/dicom_element.hpp` | `src/core/dicom_element.cpp` |
| DES-CORE-003 | `dicom_dataset` | `include/pacs/core/dicom_dataset.hpp` | `src/core/dicom_dataset.cpp` |
| DES-CORE-004 | `dicom_file` | `include/pacs/core/dicom_file.hpp` | `src/core/dicom_file.cpp` |
| DES-CORE-005 | `dicom_dictionary` | `include/pacs/core/dicom_dictionary.hpp` | `src/core/dicom_dictionary.cpp`, `src/core/standard_tags_data.cpp` |
| - | `tag_info` | `include/pacs/core/tag_info.hpp` | `src/core/tag_info.cpp` |
| - | `dicom_tag_constants` | `include/pacs/core/dicom_tag_constants.hpp` | (헤더 전용) |

### 5.2 Encoding 모듈 구현

| SDS ID | 설계 요소 | 헤더 파일 | 소스 파일 |
|--------|---------|-----------|----------|
| DES-ENC-001 | `vr_type` | `include/pacs/encoding/vr_type.hpp` | (헤더 전용, enum) |
| DES-ENC-002 | `vr_info` | `include/pacs/encoding/vr_info.hpp` | `src/encoding/vr_info.cpp` |
| DES-ENC-003 | `transfer_syntax` | `include/pacs/encoding/transfer_syntax.hpp` | `src/encoding/transfer_syntax.cpp` |
| DES-ENC-004 | `implicit_vr_codec` | `include/pacs/encoding/implicit_vr_codec.hpp` | `src/encoding/implicit_vr_codec.cpp` |
| DES-ENC-005 | `explicit_vr_codec` | `include/pacs/encoding/explicit_vr_codec.hpp` | `src/encoding/explicit_vr_codec.cpp` |
| - | `byte_order` | `include/pacs/encoding/byte_order.hpp` | (헤더 전용) |

### 5.3 Network 모듈 구현

| SDS ID | 설계 요소 | 헤더 파일 | 소스 파일 |
|--------|---------|-----------|----------|
| DES-NET-001 | `pdu_encoder` | `include/pacs/network/pdu_encoder.hpp` | `src/network/pdu_encoder.cpp` |
| DES-NET-002 | `pdu_decoder` | `include/pacs/network/pdu_decoder.hpp` | `src/network/pdu_decoder.cpp` |
| DES-NET-003 | `dimse_message` | `include/pacs/network/dimse/dimse_message.hpp` | `src/network/dimse/dimse_message.cpp` |
| DES-NET-004 | `association` | `include/pacs/network/association.hpp` | `src/network/association.cpp` |
| DES-NET-005 | `dicom_server` | `include/pacs/network/dicom_server.hpp` | `src/network/dicom_server.cpp` |
| - | `pdu_types` | `include/pacs/network/pdu_types.hpp` | (헤더 전용) |
| - | `server_config` | `include/pacs/network/server_config.hpp` | (헤더 전용) |
| - | `command_field` | `include/pacs/network/dimse/command_field.hpp` | (헤더 전용) |
| - | `status_codes` | `include/pacs/network/dimse/status_codes.hpp` | (헤더 전용) |

### 5.4 Services 모듈 구현

| SDS ID | 설계 요소 | 헤더 파일 | 소스 파일 |
|--------|---------|-----------|----------|
| DES-SVC-001 | `verification_scp` | `include/pacs/services/verification_scp.hpp` | `src/services/verification_scp.cpp` |
| DES-SVC-002 | `storage_scp` | `include/pacs/services/storage_scp.hpp` | `src/services/storage_scp.cpp` |
| DES-SVC-003 | `storage_scu` | `include/pacs/services/storage_scu.hpp` | `src/services/storage_scu.cpp` |
| DES-SVC-004 | `query_scp` | `include/pacs/services/query_scp.hpp` | `src/services/query_scp.cpp` |
| DES-SVC-005 | `retrieve_scp` | `include/pacs/services/retrieve_scp.hpp` | `src/services/retrieve_scp.cpp` |
| DES-SVC-006 | `worklist_scp` | `include/pacs/services/worklist_scp.hpp` | `src/services/worklist_scp.cpp` |
| DES-SVC-007 | `mpps_scp` | `include/pacs/services/mpps_scp.hpp` | `src/services/mpps_scp.cpp` |
| DES-SVC-008 | `sop_class_registry` | `include/pacs/services/sop_class_registry.hpp` | `src/services/sop_class_registry.cpp` |
| DES-SVC-009 | XA Storage SOP Classes | `include/pacs/services/sop_classes/xa_storage.hpp` | `src/services/sop_classes/xa_storage.cpp` |
| DES-SVC-010 | `xa_iod_validator` | `include/pacs/services/validation/xa_iod_validator.hpp` | `src/services/validation/xa_iod_validator.cpp` |
| - | `scp_service` | `include/pacs/services/scp_service.hpp` | (헤더 전용, 인터페이스) |
| - | `storage_status` | `include/pacs/services/storage_status.hpp` | (헤더 전용) |
| - | US Storage SOP Classes | `include/pacs/services/sop_classes/us_storage.hpp` | (헤더 전용) |

### 5.5 Storage 모듈 구현

| SDS ID | 설계 요소 | 헤더 파일 | 소스 파일 |
|--------|---------|-----------|----------|
| DES-STOR-001 | `storage_interface` | `include/pacs/storage/storage_interface.hpp` | `src/storage/storage_interface.cpp` |
| DES-STOR-002 | `file_storage` | `include/pacs/storage/file_storage.hpp` | `src/storage/file_storage.cpp` |
| DES-STOR-003 | `index_database` | `include/pacs/storage/index_database.hpp` | `src/storage/index_database.cpp` |
| - | `migration_runner` | `include/pacs/storage/migration_runner.hpp` | `src/storage/migration_runner.cpp` |
| - | `patient_record` | `include/pacs/storage/patient_record.hpp` | (헤더 전용, struct) |
| - | `study_record` | `include/pacs/storage/study_record.hpp` | (헤더 전용, struct) |
| - | `series_record` | `include/pacs/storage/series_record.hpp` | (헤더 전용, struct) |
| - | `instance_record` | `include/pacs/storage/instance_record.hpp` | (헤더 전용, struct) |
| - | `worklist_record` | `include/pacs/storage/worklist_record.hpp` | (헤더 전용, struct) |
| - | `mpps_record` | `include/pacs/storage/mpps_record.hpp` | (헤더 전용, struct) |
| - | `migration_record` | `include/pacs/storage/migration_record.hpp` | (헤더 전용, struct) |

### 5.6 Integration 모듈 구현

| SDS ID | 설계 요소 | 헤더 파일 | 소스 파일 |
|--------|---------|-----------|----------|
| DES-INT-001 | `container_adapter` | `include/pacs/integration/container_adapter.hpp` | `src/integration/container_adapter.cpp` |
| DES-INT-002 | `network_adapter` | `include/pacs/integration/network_adapter.hpp` | `src/integration/network_adapter.cpp` |
| DES-INT-003 | `thread_adapter` | `include/pacs/integration/thread_adapter.hpp` | `src/integration/thread_adapter.cpp` |
| DES-INT-004 | `logger_adapter` | `include/pacs/integration/logger_adapter.hpp` | `src/integration/logger_adapter.cpp` |
| DES-INT-005 | `monitoring_adapter` | `include/pacs/integration/monitoring_adapter.hpp` | `src/integration/monitoring_adapter.cpp` |
| - | `dicom_session` | `include/pacs/integration/dicom_session.hpp` | `src/integration/dicom_session.cpp` |

### 5.7 구현 요약

| 모듈 | 헤더 | 소스 | 헤더 전용 | 전체 파일 |
|------|------|------|----------|----------|
| Core | 7 | 7 | 1 | 14 |
| Encoding | 6 | 4 | 2 | 10 |
| Network | 9 | 5 | 4 | 14 |
| Services | 9 | 7 | 2 | 16 |
| Storage | 11 | 4 | 7 | 15 |
| Integration | 6 | 6 | 0 | 12 |
| **총계** | **48** | **33** | **16** | **81** |

---

## 6. SDS에서 테스트 추적성 (Verification)

이 섹션은 **Verification** 목적으로 설계 요소와 해당 테스트 파일 간의 매핑을 제공합니다.

> **Verification과 Validation 참고:**
> - **Verification** (이 섹션): 코드가 SDS 설계 명세와 일치하는지 확인하는 테스트
>   - 단위 테스트 → 개별 컴포넌트 설계 (DES-xxx)
>   - 통합 테스트 → 모듈 상호작용 설계 (SEQ-xxx)
> - **Validation** (별도): 구현이 SRS 요구사항을 충족하는지 확인하는 테스트
>   - 시스템 테스트 → SRS 기능 요구사항
>   - 인수 테스트 → PRD 사용자 요구사항

### 6.1 Core 모듈 테스트

| SDS ID | 설계 요소 | 테스트 파일 | 테스트 수 |
|--------|---------|-----------|----------|
| DES-CORE-001 | `dicom_tag` | `tests/core/dicom_tag_test.cpp` | 12 |
| DES-CORE-002 | `dicom_element` | `tests/core/dicom_element_test.cpp` | 15 |
| DES-CORE-003 | `dicom_dataset` | `tests/core/dicom_dataset_test.cpp` | 18 |
| DES-CORE-004 | `dicom_file` | `tests/core/dicom_file_test.cpp` | 8 |
| DES-CORE-005 | `dicom_dictionary` | `tests/core/dicom_dictionary_test.cpp` | 6 |
| - | `tag_info` | `tests/core/tag_info_test.cpp` | 4 |

### 6.2 Encoding 모듈 테스트

| SDS ID | 설계 요소 | 테스트 파일 | 테스트 수 |
|--------|---------|-----------|----------|
| DES-ENC-001 | `vr_type` | `tests/encoding/vr_type_test.cpp` | 10 |
| DES-ENC-002 | `vr_info` | `tests/encoding/vr_info_test.cpp` | 8 |
| DES-ENC-003 | `transfer_syntax` | `tests/encoding/transfer_syntax_test.cpp` | 7 |
| DES-ENC-004 | `implicit_vr_codec` | `tests/encoding/implicit_vr_codec_test.cpp` | 9 |
| DES-ENC-005 | `explicit_vr_codec` | `tests/encoding/explicit_vr_codec_test.cpp` | 9 |

### 6.3 Network 모듈 테스트

| SDS ID | 설계 요소 | 테스트 파일 | 테스트 수 |
|--------|---------|-----------|----------|
| DES-NET-001 | `pdu_encoder` | `tests/network/pdu_encoder_test.cpp` | 7 |
| DES-NET-002 | `pdu_decoder` | `tests/network/pdu_decoder_test.cpp` | 7 |
| DES-NET-003 | `dimse_message` | `tests/network/dimse/dimse_message_test.cpp` | 5 |
| DES-NET-004 | `association` | `tests/network/association_test.cpp` | 8 |
| DES-NET-005 | `dicom_server` | `tests/network/dicom_server_test.cpp` | 4 |

### 6.4 Services 모듈 테스트

| SDS ID | 설계 요소 | 테스트 파일 | 테스트 수 |
|--------|---------|-----------|----------|
| DES-SVC-001 | `verification_scp` | `tests/services/verification_scp_test.cpp` | - |
| DES-SVC-002 | `storage_scp` | `tests/services/storage_scp_test.cpp` | - |
| DES-SVC-003 | `storage_scu` | `tests/services/storage_scu_test.cpp` | - |
| DES-SVC-004 | `query_scp` | `tests/services/query_scp_test.cpp` | - |
| DES-SVC-005 | `retrieve_scp` | `tests/services/retrieve_scp_test.cpp` | - |
| DES-SVC-006 | `worklist_scp` | `tests/services/worklist_scp_test.cpp` | - |
| DES-SVC-007 | `mpps_scp` | `tests/services/mpps_scp_test.cpp` | - |
| DES-SVC-009 | XA Storage | `tests/services/xa_storage_test.cpp` | 15 |

### 6.5 Storage 모듈 테스트

| SDS ID | 설계 요소 | 테스트 파일 | 테스트 수 |
|--------|---------|-----------|----------|
| DES-STOR-001 | `storage_interface` | `tests/storage/storage_interface_test.cpp` | - |
| DES-STOR-002 | `file_storage` | `tests/storage/file_storage_test.cpp` | - |
| DES-STOR-003 | `index_database` | `tests/storage/index_database_test.cpp` | - |
| - | `migration_runner` | `tests/storage/migration_runner_test.cpp` | - |
| DES-DB-005 | `mpps` 레코드 | `tests/storage/mpps_test.cpp` | - |
| DES-DB-006 | `worklist` 레코드 | `tests/storage/worklist_test.cpp` | - |

### 6.6 Integration 모듈 테스트

| SDS ID | 설계 요소 | 테스트 파일 | 테스트 수 |
|--------|---------|-----------|----------|
| DES-INT-001 | `container_adapter` | `tests/integration/container_adapter_test.cpp` | - |
| DES-INT-002 | `network_adapter` | `tests/integration/network_adapter_test.cpp` | - |
| DES-INT-003 | `thread_adapter` | `tests/integration/thread_adapter_test.cpp` | - |
| DES-INT-004 | `logger_adapter` | `tests/integration/logger_adapter_test.cpp` | - |
| DES-INT-005 | `monitoring_adapter` | `tests/integration/monitoring_adapter_test.cpp` | - |

### 6.7 Verification 테스트 커버리지 요약

| 모듈 | 테스트 파일 | 커버된 설계 요소 | Verification 커버리지 |
|------|-----------|----------------|---------------------|
| Core | 6 | 6/6 | 100% |
| Encoding | 5 | 5/5 | 100% |
| Network | 5 | 5/5 | 100% |
| Services | 7 | 7/7 | 100% |
| Storage | 6 | 6/6 | 100% |
| Integration | 5 | 5/5 | 100% |
| **총계** | **34** | **34/34** | **100%** |

### 6.8 Validation 추적성 (별도 문서)

**Validation** (SRS → 시스템 테스트)은 별도의 [VALIDATION_REPORT_KO.md](VALIDATION_REPORT_KO.md)에서 문서화됩니다.

| 문서 | 목적 | 추적성 |
|------|------|--------|
| **VERIFICATION_REPORT_KO.md** | 코드가 SDS와 일치하는지 확인 | SDS ↔ 단위/통합 테스트 |
| **VALIDATION_REPORT_KO.md** | 구현이 SRS를 충족하는지 확인 | SRS ↔ 시스템/인수 테스트 |

> **참고:** 이 문서(SDS_TRACEABILITY_KO.md)는 **Verification** 추적성에만 집중합니다.

---

## 7. 커버리지 분석

### 7.1 요구사항 커버리지 요약

| 카테고리 | PRD 총계 | SRS 추적 | SDS 추적 | 커버리지 |
|----------|---------|---------|---------|---------|
| 기능 (FR) | 14 | 14 | 14 | 100% |
| 비기능 (NFR) | 4 | 4 | 4 | 100% |
| 통합 (IR) | 5 | 5 | 5 | 100% |
| **총계** | **23** | **23** | **23** | **100%** |

### 7.2 SRS에서 SDS 커버리지

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

### 7.3 고아 분석

**고아 요구사항 (설계 없음):** 없음

**고아 설계 (요구사항 없음):** 없음

### 7.4 추적성 격차

| 격차 ID | 설명 | 상태 | 해결 방안 |
|---------|-----|------|----------|
| - | 식별된 항목 없음 | - | - |

### 7.5 영향 분석 템플릿

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
| DES-SVC-008 | sop_class_registry |
| DES-SVC-009 | XA Storage SOP Classes |
| DES-SVC-010 | xa_iod_validator |
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

## 부록 B: 개정 이력

| 버전 | 일자 | 작성자 | 변경사항 |
|------|------|--------|----------|
| 1.0.0 | 2025-11-30 | kcenon@naver.com | 초기 추적성 매트릭스 |
| 1.1.0 | 2025-12-01 | kcenon@naver.com | SDS에서 구현 및 테스트 추적성 추가 (섹션 5, 6) |
| 1.2.0 | 2025-12-01 | kcenon@naver.com | V-모델 다이어그램에 Verification/Validation 구분 수정 |
| 1.3.0 | 2025-12-01 | kcenon@naver.com | Verification에만 집중하도록 범위 조정; Validation은 별도 VALIDATION_REPORT_KO.md로 이동 |
| 1.4.0 | 2025-12-01 | kcenon@naver.com | DES-SVC-008 ~ DES-SVC-010 추가 (SOP Class Registry, XA Storage, IOD Validator) |

---

*문서 버전: 0.1.0.0*
*최종 수정일: 2025-12-01*
*작성자: kcenon@naver.com*
