# 소프트웨어 설계 명세서 (SDS) - PACS 시스템

> **버전:** 0.2.0.0
> **최종 수정일:** 2026-02-08
> **언어:** [English](SDS.md) | **한국어**
> **상태:** 완료

---

## 문서 정보

| 항목 | 설명 |
|------|------|
| 문서 ID | PACS-SDS-001 |
| 프로젝트 | PACS 시스템 |
| 작성자 | kcenon@naver.com |
| 검토자 | - |
| 승인일 | - |

### 관련 문서

| 문서 | ID | 버전 |
|------|-----|------|
| 제품 요구사항 문서 | [PRD](PRD_KO.md) | 1.0.0 |
| 소프트웨어 요구사항 명세서 | [SRS](SRS_KO.md) | 1.0.0 |
| 아키텍처 문서 | [ARCHITECTURE](ARCHITECTURE_KO.md) | 1.0.0 |
| API 레퍼런스 | [API_REFERENCE](API_REFERENCE_KO.md) | 1.0.0 |

### 문서 구조

본 SDS는 유지보수성을 위해 여러 파일로 구성됩니다:

| 파일 | 설명 |
|------|------|
| **SDS_KO.md** (본 문서) | 개요, 설계 원칙, 모듈 요약 |
| [SDS_COMPONENTS.md](SDS_COMPONENTS_KO.md) | 상세 컴포넌트 설계 |
| [SDS_INTERFACES.md](SDS_INTERFACES_KO.md) | 인터페이스 명세 |
| [SDS_DATABASE.md](SDS_DATABASE_KO.md) | 데이터베이스 스키마 설계 |
| [SDS_SEQUENCES.md](SDS_SEQUENCES_KO.md) | 시퀀스 다이어그램 |
| [SDS_TRACEABILITY.md](SDS_TRACEABILITY_KO.md) | 요구사항 추적성 매트릭스 |
| [SDS_SECURITY.md](SDS_SECURITY.md) | 보안 모듈 설계 (RBAC, 익명화, 디지털 서명) |
| [SDS_CLOUD_STORAGE.md](SDS_CLOUD_STORAGE.md) | 클라우드 스토리지 백엔드 (S3, Azure, HSM) |
| [SDS_WORKFLOW.md](SDS_WORKFLOW.md) | 워크플로우 모듈 (Auto Prefetch, Task Scheduler, Study Lock) |
| [SDS_SERVICES_CACHE.md](SDS_SERVICES_CACHE.md) | 쿼리 캐싱, LRU, 스트리밍 쿼리 |
| [SDS_NETWORK_V2.md](SDS_NETWORK_V2.md) | Network V2 messaging_server 통합 |
| [SDS_DI.md](SDS_DI.md) | 의존성 주입 모듈 |
| [SDS_AI.md](SDS_AI.md) | AI 서비스 통합 모듈 |
| [SDS_CLIENT.md](SDS_CLIENT.md) | 클라이언트 모듈 (Job, Routing, Sync, Prefetch, Remote Node) |
| [SDS_MONITORING_COLLECTORS.md](SDS_MONITORING_COLLECTORS.md) | 모니터링 수집기 플러그인 아키텍처 |

---

## 목차

- [1. 소개](#1-소개)
- [2. 설계 개요](#2-설계-개요)
- [3. 설계 원칙](#3-설계-원칙)
- [4. 모듈 요약](#4-모듈-요약)
- [5. 설계 제약사항](#5-설계-제약사항)
- [6. 설계 결정사항](#6-설계-결정사항)
- [7. 품질 속성](#7-품질-속성)

---

## 1. 소개

### 1.1 목적

본 소프트웨어 설계 명세서(SDS)는 PACS(의료영상저장전송시스템) 시스템의 상세 소프트웨어 설계를 기술합니다. SRS에 정의된 요구사항을 직접 구현 가능한 종합적인 소프트웨어 아키텍처 및 컴포넌트 설계로 변환합니다.

### 1.2 범위

본 문서는 다음을 포함합니다:

- 모든 소프트웨어 모듈의 상세 설계
- 컴포넌트 인터페이스 및 상호작용
- 데이터 구조 및 데이터베이스 스키마
- 주요 작업에 대한 시퀀스 다이어그램
- 설계 패턴 및 규약
- 요구사항에 대한 추적성

### 1.3 정의 및 약어

| 용어 | 정의 |
|------|------|
| SDS | 소프트웨어 설계 명세서 (Software Design Specification) |
| PRD | 제품 요구사항 문서 (Product Requirements Document) |
| SRS | 소프트웨어 요구사항 명세서 (Software Requirements Specification) |
| DICOM | 의료디지털영상통신 (Digital Imaging and Communications in Medicine) |
| PDU | 프로토콜 데이터 유닛 (Protocol Data Unit) |
| DIMSE | DICOM 메시지 서비스 요소 (DICOM Message Service Element) |
| VR | 값 표현 (Value Representation) |
| SOP | 서비스-객체 쌍 (Service-Object Pair) |
| SCU | 서비스 클래스 사용자 (Service Class User) |
| SCP | 서비스 클래스 제공자 (Service Class Provider) |
| UID | 고유 식별자 (Unique Identifier) |
| RAII | 자원 획득은 초기화이다 (Resource Acquisition Is Initialization) |

### 1.4 설계 식별자 규칙

설계 명세는 다음 ID 형식을 사용합니다:

```
DES-<모듈>-<번호>

설명:
- DES: 설계 명세 접두사
- 모듈: 모듈 식별자 (CORE, ENC, NET, SVC, STOR, INT, DB, SEC)
- 번호: 3자리 순차 번호
```

**모듈 식별자:**

| 모듈 ID | 모듈명 | 설명 |
|---------|--------|------|
| CORE | 코어 모듈 | DICOM 데이터 구조 |
| ENC | 인코딩 모듈 | VR 인코딩/디코딩 |
| NET | 네트워크 모듈 | DICOM 네트워크 프로토콜 |
| SVC | 서비스 모듈 | DICOM 서비스 구현 |
| STOR | 스토리지 모듈 | 저장소 백엔드 |
| INT | 인터페이스 모듈 | 에코시스템 통합 |
| DB | 데이터베이스 모듈 | 인덱스 데이터베이스 설계 |
| SEC | 보안 모듈 | RBAC, 익명화, 디지털 서명 |
| CACHE | 캐시 모듈 | 쿼리 캐싱 및 스트리밍 |
| AI | AI 모듈 | AI 서비스 통합 |
| CLI | 클라이언트 모듈 | 클라이언트측 오케스트레이션 |
| DI | DI 모듈 | 의존성 주입 |
| MON | 모니터링 모듈 | 메트릭 수집기 |

---

## 2. 설계 개요

### 2.1 시스템 컨텍스트

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              외부 시스템                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐ │
│  │   장비류      │  │    뷰어      │  │     RIS      │  │  기타 PACS      │ │
│  │  (CT, MR..)  │  │  (워크스테이션) │  │   시스템     │  │    서버         │ │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └────────┬─────────┘ │
│         │                 │                 │                    │          │
│         │    DICOM        │    DICOM        │    HL7/DICOM       │  DICOM   │
│         │    C-STORE      │    C-FIND       │    Worklist        │  C-MOVE  │
│         ▼                 ▼                 ▼                    ▼          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                           ┌─────────────────────┐                           │
│                           │                     │                           │
│                           │    PACS 시스템       │                           │
│                           │                     │                           │
│                           │  ┌───────────────┐  │                           │
│                           │  │  Storage SCP  │  │                           │
│                           │  │  Query SCP    │  │                           │
│                           │  │  Worklist SCP │  │                           │
│                           │  │  MPPS SCP     │  │                           │
│                           │  └───────────────┘  │                           │
│                           │                     │                           │
│                           └──────────┬──────────┘                           │
│                                      │                                      │
│                                      ▼                                      │
│                           ┌─────────────────────┐                           │
│                           │   저장소 백엔드       │                           │
│                           │  (파일 + 인덱스 DB)  │                           │
│                           └─────────────────────┘                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 아키텍처 레이어

PACS 시스템은 계층 아키텍처를 따릅니다:

```
┌─────────────────────────────────────────────────────────────────┐
│                   레이어 6: 애플리케이션                          │
│            (PACS 서버, CLI 도구, 예제)                           │
│                                                                  │
│  추적: FR-3.x (서비스), NFR-1 (성능)                             │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    레이어 5: 서비스                               │
│       (Storage SCP/SCU, Query SCP/SCU, Worklist, MPPS)          │
│                                                                  │
│  추적: SRS-SVC-xxx (서비스 요구사항)                             │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    레이어 4: 프로토콜                             │
│              (DIMSE 메시지, Association 관리자)                   │
│                                                                  │
│  추적: SRS-NET-xxx (네트워크 요구사항)                           │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    레이어 3: 네트워크                             │
│                    (PDU 인코더/디코더)                           │
│                                                                  │
│  추적: FR-2.1, FR-2.2 (PDU 타입, 상태 머신)                      │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    레이어 2: 코어                                 │
│         (DICOM Element, Dataset, File, Dictionary)               │
│                                                                  │
│  추적: SRS-CORE-xxx (코어 요구사항)                              │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    레이어 1: 인코딩                               │
│            (VR 타입, Transfer Syntax, 코덱)                      │
│                                                                  │
│  추적: FR-1.2 (VR 타입), FR-1.4 (Transfer Syntax)                │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    레이어 0: 통합                                 │
│     (container_adapter, network_adapter, thread_adapter)         │
│                                                                  │
│  추적: IR-1.x ~ IR-5.x (통합 요구사항)                           │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                  에코시스템 기반                                  │
│  (common_system, container_system, network_system, thread_system)│
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 모듈 의존성

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           모듈 의존성                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                    ┌───────────────┐                                        │
│                    │   services    │                                        │
│                    │               │                                        │
│                    │ • storage_scp │                                        │
│                    │ • query_scp   │                                        │
│                    │ • worklist    │                                        │
│                    │ • mpps        │                                        │
│                    └───────┬───────┘                                        │
│                            │                                                │
│              ┌─────────────┼─────────────┐                                  │
│              │             │             │                                  │
│              ▼             ▼             ▼                                  │
│       ┌──────────┐  ┌──────────┐  ┌──────────┐                             │
│       │ network  │  │ storage  │  │   core   │                             │
│       │          │  │          │  │          │                             │
│       │ • pdu    │  │ • file   │  │ • element│                             │
│       │ • dimse  │  │ • index  │  │ • dataset│                             │
│       │ • assoc  │  │          │  │ • file   │                             │
│       └────┬─────┘  └────┬─────┘  └────┬─────┘                             │
│            │             │             │                                    │
│            └─────────────┼─────────────┘                                    │
│                          │                                                  │
│                          ▼                                                  │
│                   ┌──────────┐                                              │
│                   │ encoding │                                              │
│                   │          │                                              │
│                   │ • vr     │                                              │
│                   │ • ts     │                                              │
│                   │ • codec  │                                              │
│                   └────┬─────┘                                              │
│                        │                                                    │
│                        ▼                                                    │
│                 ┌─────────────┐                                             │
│                 │ integration │                                             │
│                 │             │                                             │
│                 │ • container │                                             │
│                 │ • network   │                                             │
│                 │ • thread    │                                             │
│                 └─────────────┘                                             │
│                        │                                                    │
│                        ▼                                                    │
│         ┌──────────────────────────────┐                                    │
│         │     에코시스템 라이브러리       │                                    │
│         │  common | container | network│                                    │
│         │  thread | logger | monitoring│                                    │
│         └──────────────────────────────┘                                    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 설계 원칙

### 3.1 에코시스템 우선 설계

**원칙:** 처음부터 구현하기보다 기존 kcenon 에코시스템 컴포넌트를 활용합니다.

| 에코시스템 컴포넌트 | PACS 사용처 | 설계 근거 |
|---------------------|------------|-----------|
| `common_system::Result<T>` | 오류 처리 | 일관된 오류 전파 |
| `container_system::value` | VR 값 저장 | 타입 안전 값 처리 |
| `network_system::messaging_server` | DICOM 서버 | 검증된 비동기 I/O |
| `thread_system::thread_pool` | 워커 풀 | Lock-free 작업 처리 |
| `logger_system` | 감사 로깅 | HIPAA 준수 |

**추적:** IR-1 ~ IR-5 (통합 요구사항)

### 3.2 외부 DICOM 의존성 없음

**원칙:** 에코시스템 컴포넌트만 사용하여 모든 DICOM 기능을 구현합니다.

**근거:**
- GPL/LGPL 라이선스 우려 없음
- 완전한 구현 제어
- 에코시스템 통합에 최적화
- 완전한 투명성 및 감사 가능성

**추적:** PRD 설계 철학 §3

### 3.3 DICOM 표준 준수

**원칙:** DICOM PS3.x 명세를 정확히 따릅니다.

| 표준 | 준수 영역 |
|------|-----------|
| PS3.5 | 데이터 구조 및 인코딩 |
| PS3.6 | 데이터 사전 |
| PS3.7 | 메시지 교환 |
| PS3.8 | 네트워크 통신 |

**추적:** PRD 설계 철학 §2

### 3.4 프로덕션 등급 품질

**원칙:** 에코시스템 수준의 품질 표준을 유지합니다.

| 품질 지표 | 목표 | 검증 방법 |
|-----------|------|-----------|
| RAII 등급 | A | 코드 리뷰, 정적 분석 |
| 메모리 누수 | 0 | AddressSanitizer |
| 데이터 경쟁 | 0 | ThreadSanitizer |
| 테스트 커버리지 | ≥80% | 커버리지 리포트 |
| API 문서화 | 완료 | Doxygen |

**추적:** NFR-2 (신뢰성), NFR-4 (유지보수성)

---

## 4. 모듈 요약

### 4.1 코어 모듈 (pacs_core)

**목적:** 기본 DICOM 데이터 구조

| 컴포넌트 | 설계 ID | 설명 | 추적 |
|----------|---------|------|------|
| `dicom_tag` | DES-CORE-001 | 태그 표현 (Group, Element) | SRS-CORE-001 |
| `dicom_element` | DES-CORE-002 | 데이터 요소 (Tag, VR, Value) | SRS-CORE-002 |
| `dicom_dataset` | DES-CORE-003 | 정렬된 요소 컬렉션 | SRS-CORE-003 |
| `dicom_file` | DES-CORE-004 | Part 10 파일 처리 | SRS-CORE-004 |
| `dicom_dictionary` | DES-CORE-005 | 태그 메타데이터 조회 | SRS-CORE-005 |

### 4.2 인코딩 모듈 (pacs_encoding)

**목적:** VR 인코딩 및 Transfer Syntax 처리

| 컴포넌트 | 설계 ID | 설명 | 추적 |
|----------|---------|------|------|
| `vr_type` | DES-ENC-001 | 34개 VR 타입 열거형 | SRS-CORE-006 |
| `vr_info` | DES-ENC-002 | VR 메타데이터 유틸리티 | SRS-CORE-006 |
| `transfer_syntax` | DES-ENC-003 | Transfer Syntax 처리 | SRS-CORE-007 |
| `implicit_vr_codec` | DES-ENC-004 | Implicit VR 인코더/디코더 | SRS-CORE-008 |
| `explicit_vr_codec` | DES-ENC-005 | Explicit VR 인코더/디코더 | SRS-CORE-008 |

### 4.3 네트워크 모듈 (pacs_network)

**목적:** DICOM 네트워크 프로토콜 구현

| 컴포넌트 | 설계 ID | 설명 | 추적 |
|----------|---------|------|------|
| `pdu_encoder` | DES-NET-001 | PDU 직렬화 | SRS-NET-001 |
| `pdu_decoder` | DES-NET-002 | PDU 역직렬화 | SRS-NET-001 |
| `dimse_message` | DES-NET-003 | DIMSE 메시지 처리 | SRS-NET-002 |
| `association` | DES-NET-004 | Association 상태 머신 | SRS-NET-003 |
| `dicom_server` | DES-NET-005 | 다중 Association 서버 | SRS-NET-004 |

### 4.4 서비스 모듈 (pacs_services)

**목적:** DICOM 서비스 구현

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `verification_scp` | DES-SVC-001 | C-ECHO 핸들러 | SRS-SVC-001 | ✅ |
| `storage_scp` | DES-SVC-002 | C-STORE 수신기 | SRS-SVC-002 | ✅ |
| `storage_scu` | DES-SVC-003 | C-STORE 송신기 | SRS-SVC-003 | ✅ |
| `query_scp` | DES-SVC-004 | C-FIND 핸들러 | SRS-SVC-004 | ✅ |
| `retrieve_scp` | DES-SVC-005 | C-MOVE/C-GET 핸들러 | SRS-SVC-005 | ✅ |
| `worklist_scp` | DES-SVC-006 | MWL C-FIND 핸들러 | SRS-SVC-006 | ✅ |
| `mpps_scp` | DES-SVC-007 | N-CREATE/N-SET 핸들러 | SRS-SVC-007 | ✅ |
| `dimse_n_encoder` | DES-SVC-008 | N-GET/N-ACTION/N-EVENT/N-DELETE | SRS-SVC-008 | ✅ |
| `ultrasound_storage` | DES-SVC-009 | US/US-MF SOP 클래스 | SRS-SVC-009 | ✅ |
| `xa_storage` | DES-SVC-010 | XA/XRF SOP 클래스 | SRS-SVC-010 | ✅ |

### 4.5 스토리지 모듈 (pacs_storage)

**목적:** 영구 저장소 백엔드

| 컴포넌트 | 설계 ID | 설명 | 추적 |
|----------|---------|------|------|
| `storage_interface` | DES-STOR-001 | 추상 저장소 API | SRS-STOR-001 |
| `file_storage` | DES-STOR-002 | 파일시스템 구현 | SRS-STOR-002 |
| `index_database` | DES-STOR-003 | SQLite 인덱스 | SRS-STOR-003 |

### 4.6 통합 모듈 (pacs_integration)

**목적:** 에코시스템 컴포넌트 어댑터

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `container_adapter` | DES-INT-001 | VR-컨테이너 매핑 | IR-1 | ✅ |
| `network_adapter` | DES-INT-002 | TCP/TLS via network_system | IR-2 | ✅ |
| `thread_adapter` | DES-INT-003 | 작업 처리, 스레드 풀 | IR-3 | ✅ |
| `accept_worker` | DES-INT-003a | TCP 소켓 bind/listen/accept (thread_base) | IR-3 | ✅ (v1.2.0) |
| `logger_adapter` | DES-INT-004 | 감사 로깅 | IR-4 | ✅ |
| `monitoring_adapter` | DES-INT-005 | 성능 메트릭 | IR-5 | ✅ |

### 4.7 Network V2 모듈 (pacs_network_v2) - 선택사항

**목적:** network_system 기반 DICOM 서버 구현

**주요 컴포넌트:**

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `dicom_server_v2` | DES-NET-006 | messaging_server 기반 DICOM 서버 | SRS-INT-003 | ✅ |
| `dicom_association_handler` | DES-NET-007 | 세션별 PDU 프레이밍 및 디스패칭 | SRS-INT-003 | ✅ |

### 4.8 워크플로우 모듈 (pacs_workflow)

**목적:** 워크플로우 자동화 서비스

**참조:** [SDS_WORKFLOW.md](SDS_WORKFLOW.md)

**주요 컴포넌트:**

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `auto_prefetch_service` | DES-WKF-001 | 이전 스터디 자동 프리페치 | SRS-WKF-001 | ✅ |
| `prefetch_config` | DES-WKF-002 | 프리페치 설정 | SRS-WKF-001 | ✅ |
| `task_scheduler` | DES-WKF-003 | 예약 작업 실행 | SRS-WKF-002 | ✅ |
| `task_scheduler_config` | DES-WKF-004 | 스케줄러 설정 | SRS-WKF-002 | ✅ |
| `study_lock_manager` | DES-WKF-005 | 동시 접근 제어 | SRS-WKF-001 | ✅ |

### 4.9 서비스 캐시 모듈 (pacs_services_cache)

**목적:** 성능 최적화를 위한 쿼리 캐싱 및 스트리밍

**참조:** [SDS_SERVICES_CACHE.md](SDS_SERVICES_CACHE.md)

**주요 컴포넌트:**

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `query_cache` | DES-CACHE-001 | TTL 기반 결과 캐싱 | SRS-PERF-003 | ✅ |
| `simple_lru_cache` | DES-CACHE-002 | LRU 교체 정책 | SRS-PERF-005 | ✅ |
| `streaming_query_handler` | DES-CACHE-003 | 메모리 효율적 쿼리 처리 | SRS-SVC-004 | ✅ |
| `parallel_query_executor` | DES-CACHE-004 | 멀티스레드 쿼리 실행 | SRS-PERF-002 | ✅ |
| `database_cursor` | DES-CACHE-005 | 데이터베이스 커서 추상화 | SRS-STOR-002 | ✅ |
| `query_result_stream` | DES-CACHE-006 | 이터레이터 기반 결과 스트리밍 | SRS-SVC-004 | ✅ |

### 4.10 AI 서비스 모듈 (pacs_ai)

**목적:** 외부 AI/ML 서비스 통합

**참조:** [SDS_AI.md](SDS_AI.md)

**주요 컴포넌트:**

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `ai_service_connector` | DES-AI-001 | AI 서비스용 REST 클라이언트 | SRS-AI-001 | ✅ |
| `ai_result_handler` | DES-AI-002 | DICOM SR/SC/SEG 출력 | SRS-AI-001 | ✅ |

### 4.11 의존성 주입 모듈 (pacs_di)

**목적:** 서비스 추상화 및 테스트 지원

**참조:** [SDS_DI.md](SDS_DI.md)

**주요 컴포넌트:**

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `service_interfaces` | DES-DI-001 | 추상 서비스 인터페이스 | SRS-MAINT-005 | ✅ |
| `service_registration` | DES-DI-002 | 서비스 팩토리 및 레지스트리 | SRS-MAINT-005 | ✅ |
| `ilogger` | DES-DI-003 | 로거 인터페이스 추상화 | SRS-INT-005 | ✅ |
| `test_support` | DES-DI-004 | Mock 주입 유틸리티 | SRS-MAINT-001 | ✅ |

### 4.12 모니터링 수집기 모듈 (pacs_monitoring)

**목적:** monitoring_system을 위한 PACS 전용 메트릭 수집

**참조:** [SDS_MONITORING_COLLECTORS.md](SDS_MONITORING_COLLECTORS.md)

**주요 컴포넌트:**

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `dicom_association_collector` | DES-MON-001 | 연결 및 상태 메트릭 | SRS-INT-006 | ✅ |
| `dicom_storage_collector` | DES-MON-002 | 파일 시스템 및 데이터베이스 통계 | SRS-INT-006 | ✅ |
| `dicom_service_collector` | DES-MON-003 | DIMSE 작업 카운터 | SRS-INT-006 | ✅ |

### 4.13 클라이언트 모듈 (pacs_client)

**목적:** 분산 DICOM 작업을 위한 클라이언트측 오케스트레이션

**참조:** [SDS_CLIENT.md](SDS_CLIENT.md)

**주요 컴포넌트:**

| 컴포넌트 | 설계 ID | 설명 | 추적 | 상태 |
|----------|---------|------|------|------|
| `job_manager` | DES-CLI-001 | 우선순위 및 워커를 지원하는 비동기 작업 큐 | FR-10.1 | ✅ |
| `routing_manager` | DES-CLI-002 | 규칙 기반 자동 전달 | FR-10.2 | ✅ |
| `sync_manager` | DES-CLI-003 | 충돌 해결을 지원하는 양방향 동기화 | FR-10.3 | ✅ |
| `prefetch_manager` | DES-CLI-004 | 사전 데이터 로딩 (워크리스트, 이전 스터디) | FR-10.5 | ✅ |
| `remote_node_manager` | DES-CLI-005 | 연결 풀링 및 상태 모니터링 | FR-10.4 | ✅ |

---

## 5. 설계 제약사항

### 5.1 기술 제약사항

| 제약사항 | 설명 | 영향 |
|----------|------|------|
| C++20 필수 | concepts, ranges, coroutines 사용 | 최소 컴파일러 버전 |
| 외부 DICOM 금지 | DCMTK, GDCM 사용 금지 | 전체 내부 구현 |
| 에코시스템 버전 | 특정 에코시스템 라이브러리 버전 | 의존성 관리 |
| DICOM 준수 | PS3.x 준수 필수 | 구현 복잡성 |

### 5.2 리소스 제약사항

| 리소스 | 제약사항 | 설계 결정 |
|--------|----------|-----------|
| 메모리 | 대용량 이미지 (최대 2GB) | 스트리밍 처리, 메모리 매핑 |
| 스레드 | 제한된 스레드 수 | 설정 가능한 스레드 풀 |
| 저장소 | 수백만 개의 파일 가능 | 계층적 디렉토리 구조 |
| 네트워크 | 다중 동시 Association | 비동기 I/O, 연결 풀링 |

### 5.3 인터페이스 제약사항

| 인터페이스 | 제약사항 | 출처 |
|-----------|----------|------|
| DICOM 포트 | 기본 11112, 설정 가능 | DICOM 표준 |
| PDU 크기 | 기본 최대 131072 바이트 | PS3.8 |
| AE Title | 최대 16자 | PS3.5 |
| UID | 최대 64자 | PS3.5 |

---

## 6. 설계 결정사항

### 6.1 주요 설계 결정

| 결정 ID | 결정 | 근거 | 고려된 대안 |
|---------|------|------|-------------|
| DD-001 | Dataset에 `std::map` 사용 | DICOM이 요구하는 태그 순서 | `std::unordered_map` (빠르지만 순서 없음) |
| DD-002 | 인덱스 DB에 SQLite 사용 | 내장형, ACID, 설정 불필요 | PostgreSQL (과도함), 커스텀 (위험) |
| DD-003 | 계층적 파일 저장소 | 자연스러운 DICOM 계층구조 | Flat (UID 충돌 위험) |
| DD-004 | 오류 처리에 Result<T> | 에코시스템 일관성 | 예외 (성능 오버헤드) |
| DD-005 | ASIO 통한 비동기 I/O | 검증된 확장성 | select/poll (이식성 낮음) |
| DD-006 | DIMSE용 스레드 풀 | I/O와 처리 분리 | Association별 스레드 (리소스 과다) |

### 6.2 기술 선택

| 기술 | 버전 | 용도 |
|------|------|------|
| C++ | 20 | 구현 언어 |
| CMake | 3.20+ | 빌드 시스템 |
| SQLite | 3.36+ | 인덱스 데이터베이스 |
| ASIO | (network_system 경유) | 비동기 네트워킹 |
| Google Test | 1.11+ | 단위 테스트 |
| Google Benchmark | 1.6+ | 성능 테스트 |

---

## 7. 품질 속성

### 7.1 성능 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| C-STORE 처리량 | ≥50 이미지/초 | 비동기 I/O, 스레드 풀 | NFR-1.1 |
| C-FIND 지연시간 | <100ms (1000 스터디) | 인덱싱된 쿼리 | NFR-1.2 |
| 동시 Association | ≥100 | 비동기 멀티플렉싱 | NFR-1.3 |
| Association당 메모리 | <10MB | 스트리밍, 풀링 | NFR-1.4 |

### 7.2 신뢰성 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| 가동률 | 99.9% | 우아한 성능 저하 | NFR-2.1 |
| 데이터 무결성 | 손실 0 | 원자적 트랜잭션 | NFR-2.2 |
| 오류 복구 | 자동 | 재시도 메커니즘 | NFR-2.3 |
| 연결 복구 | 자동 | Association 재설정 | NFR-2.4 |
| 트랜잭션 안전성 | ACID | 원자적 파일 쓰기 (임시 + 이름 변경) | NFR-2.5 |

### 7.3 확장성 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| 수평 확장 | 지원 | 다중 인스턴스 | NFR-3.1 |
| 이미지 용량 | ≥100만 스터디 | 계층적 저장소 | NFR-3.2 |
| 처리량 확장 | ≥80% 효율 | 스레드 풀 확장 | NFR-3.3 |
| 큐 용량 | ≥10K 작업 | 비동기 작업 큐 | NFR-3.4 |

### 7.4 보안 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| 전송 | TLS 1.2/1.3 | network_system TLS | NFR-4.1 |
| 접근 제어 | AE 화이트리스트 | 설정 기반 | NFR-4.2 |
| 감사 로깅 | 완전 | logger_system | NFR-4.3 |
| 입력 검증 | 100% | PDU/DIMSE 검증 레이어 | NFR-4.4 |
| 메모리 안전성 | 누수 0 | RAII, AddressSanitizer CI | NFR-4.5 |

### 7.5 유지보수성 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| 테스트 커버리지 | ≥80% | 종합 테스트 스위트 | NFR-5.1 |
| 문서화 | 완료 | Doxygen + 가이드 | NFR-5.2 |
| 정적 분석 | 클린 | clang-tidy 통합 | NFR-5.3 |
| 스레드 안전성 | 검증됨 | shared_mutex, ThreadSanitizer CI | NFR-5.4 |
| 모듈화 설계 | 낮은 결합도 | 인터페이스 기반 DI (pacs_di) | NFR-5.5 |

---

## 참조

1. DICOM 표준 PS3.1-PS3.20
2. [PRD - 제품 요구사항 문서](PRD_KO.md)
3. [SRS - 소프트웨어 요구사항 명세서](SRS_KO.md)
4. [ARCHITECTURE - 아키텍처 문서](ARCHITECTURE_KO.md)
5. [API_REFERENCE - API 레퍼런스](API_REFERENCE_KO.md)
6. kcenon 에코시스템 문서

---

## 문서 이력

| 버전 | 날짜 | 작성자 | 변경 사항 |
|------|------|--------|-----------|
| 1.0.0 | 2025-11-30 | kcenon | 최초 릴리스 |
| 1.1.0 | 2025-12-04 | kcenon | 컴포넌트 상태 업데이트, Transfer Syntax 상세 추가 |
| 1.2.0 | 2025-12-07 | kcenon | 추가: DES-SVC-008~010 (DIMSE-N, Ultrasound, XA), DES-INT-003a (accept_worker), DES-NET-006~007 (Network V2); thread_adapter 설계 업데이트 (thread_system 마이그레이션) |
| 1.3.0 | 2026-01-02 | kcenon | accept_worker 업데이트: 플레이스홀더를 대체하는 TCP 소켓 bind/listen/accept 구현 |
| 1.4.0 | 2026-01-04 | kcenon | 추가: 캐시 모듈 (DES-CACHE-001~006), AI 모듈 (DES-AI-001~002), DI 모듈 (DES-DI-001~004), 모니터링 모듈 (DES-MON-001~003); 모듈 ID 추가: CACHE, AI, DI, MON |
| 2.0.0 | 2026-02-08 | kcenon | 추가: 클라이언트 모듈 (DES-CLI-001~005) SDS_CLIENT.md 포함; CLI 모듈 ID 추가; SDS_WEB_API.md에 10개 신규 엔드포인트 추가 (DES-WEB-013~022); SDS_TRACEABILITY.md에 28개 신규 DES 항목 추가; 11개 스토리지 리포지토리 추가 (DES-STOR-010~020); DB 모니터링 추가 (DES-MON-007) |

---

*문서 버전: 0.2.0.0*
*작성일: 2025-11-30*
*최종 수정일: 2026-02-08*
*작성자: kcenon@naver.com*
