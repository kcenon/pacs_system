# 소프트웨어 설계 명세서 (SDS) - PACS 시스템

> **버전:** 0.1.3.0
> **최종 수정일:** 2026-01-04
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

### 1.4 설계 식별자 규칙

설계 명세는 다음 ID 형식을 사용합니다:

```
DES-<모듈>-<번호>

설명:
- DES: 설계 명세 접두사
- 모듈: 모듈 식별자 (CORE, ENC, NET, SVC, STOR, INT, DB)
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

---

## 2. 설계 개요

### 2.1 아키텍처 레이어

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

| 컴포넌트 | 설계 ID | 설명 | 추적 |
|----------|---------|------|------|
| `verification_scp` | DES-SVC-001 | C-ECHO 핸들러 | SRS-SVC-001 |
| `storage_scp` | DES-SVC-002 | C-STORE 수신기 | SRS-SVC-002 |
| `storage_scu` | DES-SVC-003 | C-STORE 송신기 | SRS-SVC-003 |
| `query_scp` | DES-SVC-004 | C-FIND 핸들러 | SRS-SVC-004 |
| `retrieve_scp` | DES-SVC-005 | C-MOVE/C-GET 핸들러 | SRS-SVC-005 |
| `worklist_scp` | DES-SVC-006 | MWL C-FIND 핸들러 | SRS-SVC-006 |
| `mpps_scp` | DES-SVC-007 | N-CREATE/N-SET 핸들러 | SRS-SVC-007 |

### 4.5 스토리지 모듈 (pacs_storage)

**목적:** 영구 저장소 백엔드

| 컴포넌트 | 설계 ID | 설명 | 추적 |
|----------|---------|------|------|
| `storage_interface` | DES-STOR-001 | 추상 저장소 API | SRS-STOR-001 |
| `file_storage` | DES-STOR-002 | 파일시스템 구현 | SRS-STOR-002 |
| `index_database` | DES-STOR-003 | SQLite 인덱스 | SRS-STOR-003 |

### 4.6 통합 모듈 (pacs_integration)

**목적:** 에코시스템 컴포넌트 어댑터

| 컴포넌트 | 설계 ID | 설명 | 추적 |
|----------|---------|------|------|
| `container_adapter` | DES-INT-001 | VR-컨테이너 매핑 | IR-1 |
| `network_adapter` | DES-INT-002 | TCP/TLS via network_system | IR-2 |
| `thread_adapter` | DES-INT-003 | 작업 처리 | IR-3 |
| `logger_adapter` | DES-INT-004 | 감사 로깅 | IR-4 |
| `monitoring_adapter` | DES-INT-005 | 성능 메트릭 | IR-5 |

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

---

## 7. 품질 속성

### 7.1 성능 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| C-STORE 처리량 | ≥50 이미지/초 | 비동기 I/O, 스레드 풀 | NFR-1.1 |
| C-FIND 지연시간 | <100ms (1000 스터디) | 인덱싱된 쿼리 | NFR-1.2 |
| 동시 Association | ≥20 | 비동기 멀티플렉싱 | NFR-1.3 |
| Association당 메모리 | <10MB | 스트리밍, 풀링 | NFR-1.4 |

### 7.2 신뢰성 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| 가동률 | 99.9% | 우아한 성능 저하 | NFR-2.1 |
| 데이터 무결성 | 손실 0 | 원자적 트랜잭션 | NFR-2.2 |
| 오류 복구 | 자동 | 재시도 메커니즘 | NFR-2.3 |

### 7.3 보안 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| 전송 | TLS 1.2/1.3 | network_system TLS | NFR-3.1 |
| 접근 제어 | AE 화이트리스트 | 설정 기반 | NFR-3.2 |
| 감사 로깅 | 완전 | logger_system | NFR-3.3 |

### 7.4 유지보수성 목표

| 지표 | 목표 | 설계 솔루션 | 추적 |
|------|------|-------------|------|
| 테스트 커버리지 | ≥80% | 종합 테스트 스위트 | NFR-4.1 |
| 문서화 | 완료 | Doxygen + 가이드 | NFR-4.2 |
| 정적 분석 | 클린 | clang-tidy 통합 | NFR-4.3 |

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

---

*문서 버전: 0.1.0.0*
*작성일: 2025-11-30*
*작성자: kcenon@naver.com*
