# PACS System

[![CI](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml)
[![Code Coverage](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml)
[![SBOM Generation](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml)

> **언어:** [English](README.md) | **한국어**

## 개요

kcenon 에코시스템을 기반으로 외부 DICOM 라이브러리 없이 구축된 고성능 C++20 PACS (Picture Archiving and Communication System) 구현체입니다. 이 프로젝트는 기존 고성능 인프라를 활용하여 DICOM 표준을 처음부터 구현합니다.

**주요 특징**:
- **외부 DICOM 라이브러리 제로**: kcenon 에코시스템을 활용한 순수 구현
- **고성능**: SIMD 가속, lock-free 큐, 비동기 I/O 활용
- **프로덕션 품질**: 종합적인 CI/CD, 새니타이저, 품질 메트릭
- **모듈식 아키텍처**: 인터페이스 기반 설계로 관심사의 명확한 분리
- **크로스 플랫폼**: Linux, macOS, Windows 지원

---

## 프로젝트 현황

**현재 단계**: ✅ Phase 4 완료 - Advanced Services & Production Hardening

| Phase | 범위 | 상태 |
|-------|------|------|
| **Phase 1**: Foundation | DICOM Core, Tag Dictionary, File I/O (Part 10), Transfer Syntax | ✅ 완료 |
| **Phase 2**: Network Protocol | Upper Layer Protocol (PDU), Association State Machine, DIMSE-C, Compression Codecs | ✅ 완료 |
| **Phase 3**: Core Services | Storage SCP/SCU, File Storage, Index Database, Query/Retrieve, Logging, Monitoring | ✅ 완료 |
| **Phase 4**: Advanced Services | REST API, DICOMweb, AI Integration, Client Module, Cloud Storage, Print Management, Security, Workflow | ✅ 완료 |
| **Phase 5**: Enterprise Features | VTK Integration, FHIR, Clustering, Connection Pooling | 🔜 예정 |

**테스트 커버리지**: 141개 이상 테스트 파일, 1,980개 이상 테스트 통과

### Phase 1 성과 (완료)

**Core 모듈** (57개 테스트):
- `dicom_tag` - DICOM 태그 표현 (Group, Element 쌍)
- `dicom_element` - 태그, VR, 값을 가진 데이터 요소
- `dicom_dataset` - 데이터 요소의 정렬된 컬렉션
- `dicom_file` - DICOM Part 10 파일 읽기/쓰기
- `dicom_dictionary` - 표준 태그 메타데이터 조회 (298개 주요 태그)

**Encoding 모듈** (41개 테스트):
- `vr_type` - 34개 DICOM Value Representation 타입 전체
- `vr_info` - VR 메타데이터 및 검증 유틸리티
- `transfer_syntax` - Transfer Syntax 관리
- `implicit_vr_codec` - Implicit VR Little Endian 코덱
- `explicit_vr_codec` - Explicit VR Little Endian 코덱

**Storage 모듈**:
- `storage_interface` - 추상 스토리지 백엔드 인터페이스
- `file_storage` - 파일시스템 기반 계층적 스토리지
- `index_database` - SQLite3 데이터베이스 인덱싱 (~2,900줄)
- `migration_runner` - 데이터베이스 스키마 마이그레이션
- Patient/Study/Series/Instance/Worklist/MPPS 레코드 관리

**Integration 어댑터**:
- `container_adapter` - container_system을 통한 직렬화
- `network_adapter` - network_system을 통한 TCP/TLS
- `thread_adapter` - thread_system을 통한 동시성
- `logger_adapter` - logger_system을 통한 감사 로깅
- `monitoring_adapter` - monitoring_system을 통한 메트릭/트레이싱
- `dicom_session` - 상위 레벨 세션 관리

### Phase 2 성과 (완료)

**Network 모듈** (15개 테스트):
- `pdu_types` - PDU 타입 정의 (A-ASSOCIATE, P-DATA 등)
- `pdu_encoder/decoder` - 바이너리 PDU 인코딩/디코딩 (~1,500줄)
- `association` - Association 상태 머신 (~1,300줄)
- `dicom_server` - DICOM 연결용 TCP 서버
- `dimse_message` - DIMSE 메시지 처리 (~600줄)

**Services 모듈** (7개 테스트 파일):
- `verification_scp` - C-ECHO 서비스 (ping/pong)
- `storage_scp/scu` - C-STORE 서비스 (저장/전송)
- `query_scp/scu` - C-FIND 서비스 (검색)
- `retrieve_scp/scu` - C-MOVE/C-GET 서비스 (조회)
- `worklist_scp/scu` - Modality Worklist 서비스 (MWL)
- `mpps_scp/scu` - Modality Performed Procedure Step
- `storage_commitment_scp/scu` - Storage Commitment Push Model (N-ACTION/N-EVENT-REPORT, PS3.4 Annex J)
- `print_scp/scu` - Print Management 서비스 (Film Session/Box, Image Box, Printer, PS3.4 Annex H)
- `sop_class_registry` - 47+ Storage SOP Classes (CT, MR, US, XA, DX, MG, NM, PET, RT, SR, SEG, CR, SC 등)

**Compression Codecs**:
- JPEG Baseline (DCT) - libjpeg-turbo 기반
- JPEG Lossless (Process 14)
- JPEG 2000 (Lossless & Lossy) - OpenJPEG 기반
- JPEG-LS (Lossless & Near-Lossless) - CharLS 기반
- RLE Lossless - 순수 C++ 구현
- SIMD 최적화 RLE 인코딩

### Phase 3 성과 (완료)

**Services 모듈**:
- `parallel_query_executor` - 병렬 배치 쿼리 실행 (타임아웃 지원)
- IOD 검증기 (US, XA 등 모달리티별 검증)
- 쿼리 캐싱 (`query_cache`, `simple_lru_cache`)

**Storage 모듈**:
- `file_storage` - 계층적 파일시스템 스토리지 (Study/Series/Instance)
- `index_database` - SQLite3 기반 메타데이터 인덱싱 (WAL 모드)
- `migration_runner` - 데이터베이스 스키마 마이그레이션 (V1-V8)
- `commitment_repository` - Storage Commitment 추적 (V8 마이그레이션)

**Encoding 확장**:
- DICOM Character Set Registry와 ISO 2022 파서
- CJK (중국어/일본어/한국어) 문자 셋 디코딩
- ISO-2022-JP 상태 유지 인코딩/디코딩

### Phase 4 성과 (완료)

**REST API & DICOMweb**:
- REST API 서버 (Crow 기반) - 19개 엔드포인트 모듈
- DICOMweb: WADO-RS, STOW-RS, QIDO-RS (PS3.18 적합)
- React/TypeScript 웹 프론트엔드 (대시보드, 환자, 워크리스트, 감사, 설정)

**보안**:
- RBAC 접근 제어 (Viewer, Technologist, Radiologist, Admin 역할)
- DICOM 익명화 엔진 (PS3.15: Basic, HIPAA Safe Harbor, GDPR 프로파일)
- OpenSSL 디지털 서명 (X.509 인증서)
- TLS 1.2/1.3 상호 인증

**워크플로우 & 클라이언트**:
- Auto Prior Study Prefetch (설정 가능한 기준)
- Task Scheduler (간격 기반, cron 유사, 일회성 스케줄링)
- Study Lock Manager (배타적/공유/마이그레이션 잠금)
- Client 모듈: Job, Routing, Sync, Prefetch, Remote Node 매니저

**AI 통합**:
- 외부 추론 엔드포인트를 위한 AI 서비스 커넥터
- AI 결과 핸들러 (SR, SEG, PR DICOM 객체 처리)

**Cloud Storage**:
- S3 클라우드 스토리지 (Mock 기본 + `PACS_WITH_AWS_SDK`로 AWS SDK 활성화)
- Azure Blob 스토리지 (Mock 기본 + `PACS_WITH_AZURE_SDK`로 Azure SDK 활성화)
- HSM (Hierarchical Storage Management) - Hot/Warm/Cold 3단계

**Print Management** (PS3.4 Annex H):
- `print_scp` - Print Management SCP (Film Session, Film Box, Image Box, Printer SOP 클래스)
- `print_scu` - Print Management SCU (N-CREATE/N-SET/N-GET/N-ACTION/N-DELETE 작업)
- Basic Grayscale 및 Color Print Meta SOP Class 지원
- `print_scu` 예제 애플리케이션 (전체 프린트 워크플로우)

**모니터링**:
- Health Check (/health, /ready, /live 엔드포인트)
- Prometheus 호환 메트릭 내보내기
- DICOM 메트릭 수집기 (association, DIMSE, storage 메트릭)

---

## 아키텍처

```
┌──────────────────────────────────────────────────────────────────────┐
│                            PACS System                               │
├──────────────────────────────────────────────────────────────────────┤
│  ┌──────────┐ ┌──────────┐ ┌─────────┐ ┌──────────┐ ┌───────────┐  │
│  │ REST API │ │ DICOMweb │ │ Web UI  │ │ AI Svc   │ │ Workflow  │  │
│  │ (Crow)   │ │ WADO/STOW│ │ (React) │ │Connector │ │ Scheduler │  │
│  └────┬─────┘ └────┬─────┘ └────┬────┘ └────┬─────┘ └─────┬─────┘  │
│       └─────────────┼───────────┼───────────┼──────────────┘        │
│  ┌──────────────────▼───────────▼───────────▼────────────────────┐  │
│  │  Services (Storage/Query/Retrieve/Worklist/MPPS/Commit/Print) │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌──────────────────────────────▼────────────────────────────────┐  │
│  │  Network (PDU/Association/DIMSE) + Security (RBAC/TLS/Anon)   │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌──────────────────────────────▼────────────────────────────────┐  │
│  │  Core (Tag/Element/Dataset/File) + Encoding (VR/Codecs/SIMD)  │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌─────────────┐  ┌─────────────┴──────────┐  ┌─────────────────┐  │
│  │  Storage    │  │  Client Module         │  │  Monitoring     │  │
│  │  (DB/File/  │  │  (Job/Route/Sync/      │  │  (Health/       │  │
│  │   S3/Azure) │  │   Prefetch/RemoteNode) │  │   Metrics)      │  │
│  └──────┬──────┘  └────────────────────────┘  └─────────────────┘  │
├─────────┼────────────────────────────────────────────────────────────┤
│         │             Integration Adapters                           │
│  container │ network │ thread │ logger │ monitoring │ ITK (opt)     │
├─────────┼────────────────────────────────────────────────────────────┤
│         │              kcenon Ecosystem                               │
│  common_system │ container_system │ thread_system │ network_system   │
│  logger_system │ monitoring_system │ database_system (opt)           │
└──────────────────────────────────────────────────────────────────────┘
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
│   ├── network/                 # 네트워크 프로토콜 (✅ 완료)
│   │   ├── pdu_types.hpp        # PDU 타입 정의
│   │   ├── pdu_encoder.hpp      # PDU 인코더
│   │   ├── pdu_decoder.hpp      # PDU 디코더
│   │   ├── association.hpp      # Association 관리
│   │   ├── dicom_server.hpp     # TCP 서버
│   │   └── dimse/               # DIMSE 프로토콜
│   │       ├── dimse_message.hpp
│   │       ├── command_field.hpp
│   │       └── status_codes.hpp
│   │
│   ├── services/                # DICOM 서비스 (✅ 완료)
│   │   ├── scp_service.hpp      # 기본 SCP 인터페이스
│   │   ├── verification_scp.hpp # C-ECHO SCP
│   │   ├── storage_scp.hpp      # C-STORE SCP
│   │   ├── storage_scu.hpp      # C-STORE SCU
│   │   ├── query_scp.hpp        # C-FIND SCP
│   │   ├── retrieve_scp.hpp     # C-MOVE/GET SCP
│   │   ├── worklist_scp.hpp     # MWL SCP
│   │   ├── mpps_scp.hpp         # MPPS SCP
│   │   ├── print_scp.hpp        # Print Management SCP (PS3.4 Annex H)
│   │   └── print_scu.hpp        # Print Management SCU
│   │
│   ├── storage/                 # 스토리지 백엔드 (✅ 완료)
│   │   ├── storage_interface.hpp # 추상 인터페이스
│   │   ├── file_storage.hpp     # 파일시스템 스토리지
│   │   ├── index_database.hpp   # SQLite3 인덱싱
│   │   ├── patient_record.hpp   # Patient 데이터 모델
│   │   ├── study_record.hpp     # Study 데이터 모델
│   │   ├── series_record.hpp    # Series 데이터 모델
│   │   ├── instance_record.hpp  # Instance 데이터 모델
│   │   ├── worklist_record.hpp  # Worklist 데이터 모델
│   │   └── mpps_record.hpp      # MPPS 데이터 모델
│   │
│   ├── monitoring/              # 헬스 모니터링 (✅ 완료)
│   │   ├── health_status.hpp    # 헬스 상태 구조체
│   │   ├── health_checker.hpp   # 헬스 체크 서비스
│   │   └── health_json.hpp      # JSON 직렬화
│   │
│   └── integration/             # 에코시스템 어댑터 (✅ 완료)
│       ├── container_adapter.hpp # container_system 연동
│       ├── network_adapter.hpp  # network_system 연동
│       ├── thread_adapter.hpp   # thread_system 연동
│       ├── logger_adapter.hpp   # logger_system 연동
│       ├── monitoring_adapter.hpp # monitoring_system 연동
│       └── dicom_session.hpp    # 상위 레벨 세션
│
├── src/                         # 소스 파일 (~48,500줄)
│   ├── core/                    # Core 구현 (7개 파일)
│   ├── encoding/                # Encoding 구현 (4개 파일)
│   ├── network/                 # Network 구현 (8개 파일)
│   ├── services/                # Services 구현 (7개 파일)
│   ├── storage/                 # Storage 구현 (4개 파일)
│   ├── monitoring/              # Health check 구현 (1개 파일)
│   └── integration/             # Adapter 구현 (6개 파일)
│
├── tests/                       # 테스트 스위트 (102개 파일, 170개 이상 테스트)
│   ├── core/                    # Core 모듈 테스트 (6개 파일)
│   ├── encoding/                # Encoding 모듈 테스트 (5개 파일)
│   ├── network/                 # Network 모듈 테스트 (5개 파일)
│   ├── services/                # Services 테스트 (7개 파일)
│   ├── storage/                 # Storage 테스트 (6개 파일)
│   ├── monitoring/              # Health check 테스트 (3개 파일, 50 테스트)
│   └── integration/             # Adapter 테스트 (5개 파일)
│
├── examples/                    # 예제 애플리케이션 (32개)
│   ├── dcm_dump/                # DICOM 파일 검사 유틸리티
│   ├── dcm_info/                # DICOM 파일 요약 유틸리티
│   ├── dcm_conv/                # Transfer Syntax 변환 유틸리티
│   ├── dcm_modify/              # DICOM 태그 수정 유틸리티
│   ├── dcm_anonymize/           # DICOM 비식별화 유틸리티 (PS3.15)
│   ├── dcm_dir/                 # DICOMDIR 생성/관리 유틸리티 (PS3.10)
│   ├── dcm_to_json/             # DICOM → JSON 변환 유틸리티 (PS3.18)
│   ├── json_to_dcm/             # JSON → DICOM 변환 유틸리티 (PS3.18)
│   ├── dcm_to_xml/              # DICOM → XML 변환 유틸리티 (PS3.19)
│   ├── xml_to_dcm/              # XML → DICOM 변환 유틸리티 (PS3.19)
│   ├── db_browser/              # PACS 인덱스 데이터베이스 브라우저
│   ├── echo_scp/                # DICOM Echo SCP 서버
│   ├── echo_scu/                # DICOM Echo SCU 클라이언트
│   ├── secure_dicom/            # TLS 보안 DICOM Echo SCU/SCP
│   ├── store_scp/               # DICOM Storage SCP 서버
│   ├── store_scu/               # DICOM Storage SCU 클라이언트
│   ├── qr_scp/                  # Query/Retrieve SCP (C-FIND/C-MOVE/C-GET 서버)
│   ├── query_scu/               # DICOM Query SCU 클라이언트 (C-FIND)
│   ├── retrieve_scu/            # DICOM Retrieve SCU 클라이언트 (C-MOVE/C-GET)
│   ├── worklist_scu/            # Modality Worklist 조회 클라이언트 (MWL C-FIND)
│   ├── worklist_scp/            # Modality Worklist SCP 서버 (MWL C-FIND)
│   ├── mpps_scu/                # MPPS N-CREATE/N-SET 클라이언트
│   ├── mpps_scp/                # MPPS N-CREATE/N-SET 서버
│   ├── print_scu/               # Print Management 클라이언트 (Film Session/Box 워크플로우)
│   ├── pacs_server/             # 전체 PACS 서버 예제
│   └── integration_tests/       # 통합 테스트 스위트
│
├── docs/                        # 문서 (57개 파일)
└── CMakeLists.txt               # 빌드 설정 (v0.2.0)
```

---

## 문서

- 📋 [구현 분석](docs/PACS_IMPLEMENTATION_ANALYSIS_KO.md) - 상세 구현 전략
- 📋 [요구사항 정의서](docs/PRD_KO.md) - 제품 요구사항 문서
- 🏗️ [아키텍처 가이드](docs/ARCHITECTURE_KO.md) - 시스템 아키텍처
- ⚡ [기능 명세](docs/FEATURES_KO.md) - 기능 상세 설명
- 📁 [프로젝트 구조](docs/PROJECT_STRUCTURE_KO.md) - 디렉토리 구조
- 🔧 [API 레퍼런스](docs/API_REFERENCE_KO.md) - API 문서
- 🚀 [마이그레이션 완료](docs/MIGRATION_COMPLETE_KO.md) - 스레드 시스템 마이그레이션 요약

---

## DICOM 적합성

### 지원 SOP 클래스

| 서비스 | SOP Class | 상태 |
|--------|-----------|------|
| **Verification** | 1.2.840.10008.1.1 | ✅ 완료 |
| **CT Storage** | 1.2.840.10008.5.1.4.1.1.2 | ✅ 완료 |
| **MR Storage** | 1.2.840.10008.5.1.4.1.1.4 | ✅ 완료 |
| **US Storage** | 1.2.840.10008.5.1.4.1.1.6.x | ✅ 완료 |
| **XA Storage** | 1.2.840.10008.5.1.4.1.1.12.x | ✅ 완료 |
| **XRF Storage** | 1.2.840.10008.5.1.4.1.1.12.2 | ✅ 완료 |
| **X-Ray Storage** | 1.2.840.10008.5.1.4.1.1.1.1 | ✅ 완료 |
| **Patient Root Q/R** | 1.2.840.10008.5.1.4.1.2.1.x | ✅ 완료 |
| **Study Root Q/R** | 1.2.840.10008.5.1.4.1.2.2.x | ✅ 완료 |
| **Modality Worklist** | 1.2.840.10008.5.1.4.31 | ✅ 완료 |
| **MPPS** | 1.2.840.10008.3.1.2.3.3 | ✅ 완료 |
| **Storage Commitment** | 1.2.840.10008.1.20.1 | ✅ 완료 |
| **NM Storage** | 1.2.840.10008.5.1.4.1.1.20 | ✅ 완료 |
| **PET Storage** | 1.2.840.10008.5.1.4.1.1.128 | ✅ 완료 |
| **RT Storage** | 1.2.840.10008.5.1.4.1.1.481.x | ✅ 완료 |
| **SR Storage** | 1.2.840.10008.5.1.4.1.1.88.x | ✅ 완료 |
| **SEG Storage** | 1.2.840.10008.5.1.4.1.1.66.4 | ✅ 완료 |
| **MG Storage** | 1.2.840.10008.5.1.4.1.1.1.2.x | ✅ 완료 |
| **CR Storage** | 1.2.840.10008.5.1.4.1.1.1 | ✅ 완료 |
| **SC Storage** | 1.2.840.10008.5.1.4.1.1.7 | ✅ 완료 |
| **Basic Grayscale Print** | 1.2.840.10008.5.1.1.9 | ✅ 완료 |
| **Basic Color Print** | 1.2.840.10008.5.1.1.18 | ✅ 완료 |
| **Printer** | 1.2.840.10008.5.1.1.16 | ✅ 완료 |

### Transfer Syntax 지원

| Transfer Syntax | UID | 상태 |
|-----------------|-----|------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | ✅ 완료 |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | ✅ 완료 |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | ✅ 완료 |
| JPEG Baseline (Process 1) | 1.2.840.10008.1.2.4.50 | ✅ 완료 |
| JPEG Lossless (Process 14) | 1.2.840.10008.1.2.4.70 | ✅ 완료 |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | ✅ 완료 |
| JPEG 2000 Lossy | 1.2.840.10008.1.2.4.91 | ✅ 완료 |
| JPEG-LS Lossless | 1.2.840.10008.1.2.4.80 | ✅ 완료 |
| JPEG-LS Near-Lossless | 1.2.840.10008.1.2.4.81 | ✅ 완료 |
| RLE Lossless | 1.2.840.10008.1.2.5 | ✅ 완료 |

---

## 시작하기

### 사전 요구사항

- C++20 Concepts를 지원하는 호환 컴파일러:
  - GCC 10+ (std::format 전체 지원을 위해 GCC 13+ 권장)
  - Clang 10+ (Clang 14+ 권장)
  - MSVC 2022 (19.30+)
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

**테스트 결과**: 1,980개 이상 테스트 통과 (141개 이상 테스트 파일 - Core, Encoding, Network, Services, Storage, Security, Web, Workflow, Client, AI, Monitoring, Integration)

### 빌드 옵션

```cmake
PACS_BUILD_TESTS (ON)              # 유닛 테스트 활성화
PACS_BUILD_EXAMPLES (OFF)          # 예제 빌드 활성화
PACS_BUILD_BENCHMARKS (OFF)        # 벤치마크 활성화
PACS_BUILD_STORAGE (ON)            # Storage 모듈 빌드
```

### Windows 개발 노트

Windows 호환성을 위해 `std::min`과 `std::max` 호출 시 괄호로 감싸서 Windows.h 매크로와의 충돌을 방지하세요:

```cpp
// 올바른 방법: 모든 플랫폼에서 작동
size_t result = (std::min)(a, b);
auto value = (std::max)(x, y);

// 잘못된 방법: Windows MSVC에서 실패 (error C2589)
size_t result = std::min(a, b);
auto value = std::max(x, y);
```

---

## 예제

### 예제 빌드

```bash
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build
```

### DCM Dump (파일 검사 유틸리티)

```bash
# DICOM 파일 메타데이터 출력
./build/bin/dcm_dump image.dcm

# 특정 태그만 필터링
./build/bin/dcm_dump image.dcm --tags PatientName,PatientID,Modality

# 픽셀 데이터 정보 표시
./build/bin/dcm_dump image.dcm --pixel-info

# JSON 형식 출력 (연동용)
./build/bin/dcm_dump image.dcm --format json

# 디렉토리 재귀 스캔 및 요약
./build/bin/dcm_dump ./dicom_folder/ --recursive --summary
```

### DCM Info (파일 요약 유틸리티)

```bash
# DICOM 파일 요약 정보 표시
./build/bin/dcm_info image.dcm

# 상세 모드 (모든 필드 표시)
./build/bin/dcm_info image.dcm --verbose

# JSON 출력 (스크립트 연동용)
./build/bin/dcm_info image.dcm --format json

# 여러 파일 빠른 확인
./build/bin/dcm_info ./dicom_folder/ -r -q
```

### DCM Modify (태그 수정 유틸리티)

dcmtk 호환 DICOM 태그 수정 유틸리티. 숫자 태그 형식 `(GGGG,EEEE)`과 키워드 형식 모두 지원.

```bash
# 태그 삽입 (존재하지 않으면 생성) - 두 가지 태그 형식 지원
./build/bin/dcm_modify -i "(0010,0010)=Anonymous" patient.dcm
./build/bin/dcm_modify -i PatientName=Anonymous -o modified.dcm patient.dcm

# 기존 태그 수정 (존재하지 않으면 오류)
./build/bin/dcm_modify -m "(0010,0020)=NEW_ID" patient.dcm

# 태그 삭제
./build/bin/dcm_modify -e "(0010,1000)" patient.dcm
./build/bin/dcm_modify -e OtherPatientIDs patient.dcm

# 모든 매칭 태그 삭제 (시퀀스 내 포함)
./build/bin/dcm_modify -ea "(0010,1001)" patient.dcm

# 모든 private 태그 삭제
./build/bin/dcm_modify -ep patient.dcm

# UID 재생성
./build/bin/dcm_modify -gst -gse -gin -o anonymized.dcm patient.dcm

# 스크립트 파일로 일괄 수정
./build/bin/dcm_modify --script modify.txt *.dcm

# 제자리 수정 (.bak 백업 생성)
./build/bin/dcm_modify -i PatientID=NEW_ID patient.dcm

# 백업 없이 제자리 수정 (주의!)
./build/bin/dcm_modify -i PatientID=NEW_ID -nb patient.dcm

# 디렉토리 재귀 처리
./build/bin/dcm_modify -i PatientName=Anonymous -r ./dicom_folder/ -o ./output/
```

스크립트 파일 형식 (`modify.txt`):
```
# 주석은 #으로 시작
i (0010,0010)=Anonymous     # 태그 삽입/수정
m (0008,0050)=ACC001        # 기존 태그 수정
e (0010,1000)               # 태그 삭제
ea (0010,1001)              # 모든 매칭 태그 삭제
```

### DCM Conv (Transfer Syntax 변환기)

```bash
# Explicit VR Little Endian으로 변환 (기본값)
./build/bin/dcm_conv image.dcm converted.dcm --explicit

# Implicit VR Little Endian으로 변환
./build/bin/dcm_conv image.dcm output.dcm --implicit

# JPEG Baseline으로 변환 (품질 설정 포함)
./build/bin/dcm_conv image.dcm compressed.dcm --jpeg-baseline -q 85

# 디렉토리 재귀 변환 및 검증
./build/bin/dcm_conv ./input_dir/ ./output_dir/ --recursive --verify

# 지원되는 Transfer Syntax 목록 표시
./build/bin/dcm_conv --list-syntaxes

# Transfer Syntax UID 직접 지정
./build/bin/dcm_conv image.dcm output.dcm -t 1.2.840.10008.1.2.4.50
```

### DCM Anonymize (비식별화 유틸리티)

DICOM PS3.15 보안 프로파일을 준수하는 DICOM 비식별화 유틸리티입니다.

```bash
# 기본 익명화 (직접 식별자 제거)
./build/bin/dcm_anonymize patient.dcm anonymous.dcm

# HIPAA Safe Harbor 규정 준수 (18개 식별자 제거)
./build/bin/dcm_anonymize --profile hipaa_safe_harbor patient.dcm output.dcm

# GDPR 규정 준수 가명처리
./build/bin/dcm_anonymize --profile gdpr_compliant patient.dcm output.dcm

# 특정 태그 유지
./build/bin/dcm_anonymize -k PatientSex -k PatientAge patient.dcm output.dcm

# 사용자 정의 값으로 태그 대체
./build/bin/dcm_anonymize -r "InstitutionName=연구병원" patient.dcm output.dcm

# 새 환자 식별자 설정
./build/bin/dcm_anonymize --patient-id "STUDY001_001" --patient-name "Anonymous" patient.dcm

# UID 매핑 파일로 일관된 익명화 (동일 검사 파일들 간)
./build/bin/dcm_anonymize -m mapping.json patient.dcm output.dcm

# 종단 연구를 위한 날짜 이동
./build/bin/dcm_anonymize --profile retain_longitudinal --date-offset -30 patient.dcm

# 디렉토리 재귀 일괄 처리
./build/bin/dcm_anonymize --recursive -o anonymized/ ./originals/

# 미리보기 모드 (변경 사항만 표시)
./build/bin/dcm_anonymize --dry-run --verbose patient.dcm

# 익명화 완료 검증
./build/bin/dcm_anonymize --verify patient.dcm anonymous.dcm
```

사용 가능한 익명화 프로파일:
- `basic` - 직접 환자 식별자 제거 (기본값)
- `clean_pixel` - 픽셀 데이터에서 번인된 주석 제거
- `clean_descriptions` - PHI가 포함될 수 있는 자유 텍스트 필드 정리
- `retain_longitudinal` - 날짜 이동으로 시간적 관계 유지
- `retain_patient_characteristics` - 인구통계 정보 유지 (성별, 나이, 키, 체중)
- `hipaa_safe_harbor` - 완전한 HIPAA 18개 식별자 제거
- `gdpr_compliant` - GDPR 가명처리 요구사항

### DCM Dir (DICOMDIR 생성/관리 유틸리티)

DICOM PS3.10 표준에 따라 DICOM 미디어 저장용 DICOMDIR 파일을 생성하고 관리합니다.

```bash
# 디렉토리에서 DICOMDIR 생성
./build/bin/dcm_dir create ./patient_data/

# 사용자 정의 출력 경로 및 파일셋 ID로 생성
./build/bin/dcm_dir create -o DICOMDIR --file-set-id "STUDY001" ./patient_data/

# 상세 출력으로 생성
./build/bin/dcm_dir create -v ./patient_data/

# DICOMDIR 내용을 트리 형식으로 표시
./build/bin/dcm_dir list DICOMDIR

# 상세 정보와 함께 표시
./build/bin/dcm_dir list -l DICOMDIR

# 파일 목록만 표시 (플랫 형식)
./build/bin/dcm_dir list --flat DICOMDIR

# DICOMDIR 구조 검증
./build/bin/dcm_dir verify DICOMDIR

# 참조 파일 존재 확인
./build/bin/dcm_dir verify --check-files DICOMDIR

# 일관성 검사 (중복 SOP Instance UID 감지)
./build/bin/dcm_dir verify --check-consistency DICOMDIR

# 새 파일을 추가하여 DICOMDIR 업데이트
./build/bin/dcm_dir update -a ./new_study/ DICOMDIR
```

DICOMDIR 구조는 DICOM 계층을 따릅니다:
- PATIENT → STUDY → SERIES → IMAGE (계층적 레코드 구조)
- 참조 파일 ID는 ISO 9660 호환성을 위해 백슬래시 구분자 사용
- 표준 레코드 유형 지원: PATIENT, STUDY, SERIES, IMAGE

### DCM to JSON (DICOM PS3.18 JSON 변환기)

DICOM 파일을 DICOM PS3.18 JSON 표준 형식으로 변환합니다.

```bash
# DICOM을 JSON으로 변환 (표준출력)
./build/bin/dcm_to_json image.dcm

# 파일로 출력 (정렬된 형식)
./build/bin/dcm_to_json image.dcm output.json --pretty

# 압축 출력 (서식 없음)
./build/bin/dcm_to_json image.dcm output.json --compact

# 바이너리 데이터를 Base64로 포함
./build/bin/dcm_to_json image.dcm output.json --bulk-data inline

# 바이너리 데이터를 별도 파일로 저장하고 URI로 참조
./build/bin/dcm_to_json image.dcm output.json --bulk-data uri --bulk-data-dir ./bulk/

# 픽셀 데이터 제외
./build/bin/dcm_to_json image.dcm output.json --no-pixel

# 특정 태그만 필터링
./build/bin/dcm_to_json image.dcm -t 0010,0010 -t 0010,0020

# 디렉토리 재귀 처리
./build/bin/dcm_to_json ./dicom_folder/ --recursive --no-pixel
```

출력 형식 (DICOM PS3.18):
```json
{
  "00100010": {
    "vr": "PN",
    "Value": [{"Alphabetic": "DOE^JOHN"}]
  },
  "00100020": {
    "vr": "LO",
    "Value": ["12345678"]
  }
}
```

### JSON to DCM (JSON → DICOM 변환기)

DICOM PS3.18 형식의 JSON 파일을 DICOM 파일로 변환합니다.

```bash
# JSON을 DICOM으로 변환
./build/bin/json_to_dcm metadata.json output.dcm

# 템플릿 DICOM 파일 사용 (픽셀 데이터 및 누락 태그 복사)
./build/bin/json_to_dcm metadata.json output.dcm --template original.dcm

# Transfer Syntax 지정
./build/bin/json_to_dcm metadata.json output.dcm -t 1.2.840.10008.1.2.1

# BulkDataURI 해석을 위한 디렉토리 지정
./build/bin/json_to_dcm metadata.json output.dcm --bulk-data-dir ./bulk/
```

### DCM to XML (DICOM → XML 변환)

DICOM 파일을 DICOM Native XML 형식(PS3.19)으로 변환합니다.

```bash
# DICOM을 XML로 변환 (stdout)
./build/bin/dcm_to_xml image.dcm

# 포맷팅된 출력
./build/bin/dcm_to_xml image.dcm output.xml --pretty

# 바이너리 데이터를 Base64로 포함
./build/bin/dcm_to_xml image.dcm output.xml --bulk-data inline

# 바이너리 데이터를 별도 파일로 저장하고 URI로 참조
./build/bin/dcm_to_xml image.dcm output.xml --bulk-data uri --bulk-data-dir ./bulk/

# 픽셀 데이터 제외
./build/bin/dcm_to_xml image.dcm output.xml --no-pixel

# 특정 태그만 필터링
./build/bin/dcm_to_xml image.dcm -t 0010,0010 -t 0010,0020

# 디렉토리 재귀 처리
./build/bin/dcm_to_xml ./dicom_folder/ --recursive --no-pixel
```

출력 형식 (DICOM Native XML PS3.19):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<NativeDicomModel xmlns="http://dicom.nema.org/PS3.19/models/NativeDICOM">
  <DicomAttribute tag="00100010" vr="PN" keyword="PatientName">
    <PersonName number="1">
      <Alphabetic>
        <FamilyName>DOE</FamilyName>
        <GivenName>JOHN</GivenName>
      </Alphabetic>
    </PersonName>
  </DicomAttribute>
</NativeDicomModel>
```

### XML to DCM (XML → DICOM 변환)

XML 파일(DICOM Native XML PS3.19 형식)을 DICOM 형식으로 변환합니다.

```bash
# XML을 DICOM으로 변환
./build/bin/xml_to_dcm metadata.xml output.dcm

# 템플릿 DICOM 사용 (픽셀 데이터 및 누락된 태그 복사)
./build/bin/xml_to_dcm metadata.xml output.dcm --template original.dcm

# Transfer Syntax 지정
./build/bin/xml_to_dcm metadata.xml output.dcm -t 1.2.840.10008.1.2.1

# BulkData URI 해석을 위한 디렉토리 지정
./build/bin/xml_to_dcm metadata.xml output.dcm --bulk-data-dir ./bulk/
```

### DB Browser (데이터베이스 뷰어)

```bash
# 전체 환자 목록 조회
./build/bin/db_browser pacs.db patients

# 특정 환자의 스터디 목록 조회
./build/bin/db_browser pacs.db studies --patient-id "12345"

# 날짜 범위로 스터디 필터링
./build/bin/db_browser pacs.db studies --from 20240101 --to 20241231

# 특정 스터디의 시리즈 목록 조회
./build/bin/db_browser pacs.db series --study-uid "1.2.3.4.5"

# 데이터베이스 통계 표시
./build/bin/db_browser pacs.db stats

# 데이터베이스 유지보수
./build/bin/db_browser pacs.db vacuum
./build/bin/db_browser pacs.db verify
```

### Echo SCP (검증 서버)

```bash
# Echo SCP 실행
./build/bin/echo_scp --port 11112 --ae-title MY_ECHO
```

### Echo SCU (검증 클라이언트)

dcmtk 호환 DICOM 연결 검증 도구입니다.

```bash
# 기본 연결 테스트
./build/bin/echo_scu localhost 11112

# 커스텀 AE Title 지정
./build/bin/echo_scu -aet MY_SCU -aec PACS_SCP localhost 11112

# 상세 출력 및 타임아웃 설정
./build/bin/echo_scu -v -to 60 localhost 11112

# 연결 모니터링을 위한 반복 테스트
./build/bin/echo_scu -r 10 --repeat-delay 1000 localhost 11112

# 조용한 모드 (종료 코드만 반환)
./build/bin/echo_scu -q localhost 11112

# 모든 옵션 확인
./build/bin/echo_scu --help
```

### Secure Echo SCU/SCP (TLS 보안 DICOM)

TLS 1.2/1.3 및 상호 TLS를 지원하는 보안 DICOM 연결 테스트입니다.

```bash
# 먼저 테스트 인증서 생성
cd examples/secure_dicom
./generate_certs.sh

# 보안 서버 시작 (TLS)
./build/bin/secure_echo_scp 2762 MY_PACS \
    --cert certs/server.crt \
    --key certs/server.key \
    --ca certs/ca.crt

# 보안 연결 테스트 (서버 인증서만 검증)
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --ca certs/ca.crt

# 상호 TLS 테스트 (클라이언트 인증서 사용)
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --cert certs/client.crt \
    --key certs/client.key \
    --ca certs/ca.crt

# TLS 1.3 사용
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --ca certs/ca.crt \
    --tls-version 1.3
```

### Storage SCU (이미지 전송)

```bash
# 단일 DICOM 파일 전송
./build/bin/store_scu localhost 11112 image.dcm

# 사용자 지정 AE 타이틀로 전송
./build/bin/store_scu -aet MYSCU -aec PACS localhost 11112 image.dcm

# 디렉토리의 모든 파일 전송 (재귀) + 진행률 표시
./build/bin/store_scu -r --progress localhost 11112 ./dicom_folder/

# Transfer Syntax 선호 설정
./build/bin/store_scu --prefer-lossless localhost 11112 *.dcm

# 상세 출력 + 타임아웃 설정
./build/bin/store_scu -v -to 60 localhost 11112 image.dcm

# 전송 리포트 생성
./build/bin/store_scu --report-file transfer.log localhost 11112 ./data/

# 조용한 모드 (최소 출력)
./build/bin/store_scu -q localhost 11112 image.dcm

# 도움말 표시
./build/bin/store_scu --help
```

### Query SCU (C-FIND 클라이언트)

```bash
# 환자 이름으로 Study 검색 (와일드카드 지원)
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"

# 날짜 범위로 검색
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --study-date "20240101-20241231"

# 특정 Study의 Series 검색
./build/bin/query_scu localhost 11112 PACS_SCP --level SERIES --study-uid "1.2.3.4.5"

# 연동을 위한 JSON 출력
./build/bin/query_scu localhost 11112 PACS_SCP --patient-id "12345" --format json

# CSV로 내보내기
./build/bin/query_scu localhost 11112 PACS_SCP --modality CT --format csv > results.csv
```

### Worklist SCU (Modality Worklist 조회 클라이언트)

```bash
# CT 모달리티 Worklist 조회
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT

# 오늘 예정된 검사 조회
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality MR --date today

# 스테이션 AE 타이틀로 조회
./build/bin/worklist_scu localhost 11112 RIS_SCP --station "CT_SCANNER_01" --date 20241215

# 환자 필터로 조회
./build/bin/worklist_scu localhost 11112 RIS_SCP --patient-name "DOE^*" --modality CT

# 연동을 위한 JSON 출력
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT --format json > worklist.json

# CSV로 내보내기
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT --format csv > worklist.csv
```

### MPPS SCU (검사 수행 상태 클라이언트)

```bash
# 새 MPPS 인스턴스 생성 (검사 시작)
./build/bin/mpps_scu localhost 11112 RIS_SCP create \
  --patient-id "12345" \
  --patient-name "Doe^John" \
  --modality CT

# 검사 완료
./build/bin/mpps_scu localhost 11112 RIS_SCP set \
  --mpps-uid "1.2.3.4.5.6.7.8" \
  --status COMPLETED \
  --series-uid "1.2.3.4.5.6.7.8.9"

# 검사 중단 (취소)
./build/bin/mpps_scu localhost 11112 RIS_SCP set \
  --mpps-uid "1.2.3.4.5.6.7.8" \
  --status DISCONTINUED \
  --reason "Patient refused"

# 디버깅을 위한 상세 출력
./build/bin/mpps_scu localhost 11112 RIS_SCP create \
  --patient-id "12345" \
  --modality MR \
  --verbose
```

### MPPS SCP (검사 수행 상태 서버)

모달리티 장비로부터 검사 수행 상태 업데이트를 수신하는 독립형 MPPS 서버입니다.

```bash
# 기본 사용법 - MPPS 메시지 수신
./build/bin/mpps_scp 11112 MY_MPPS

# MPPS 레코드를 개별 JSON 파일로 저장
./build/bin/mpps_scp 11112 MY_MPPS --output-dir ./mpps_records

# MPPS 레코드를 단일 JSON 파일에 추가
./build/bin/mpps_scp 11112 MY_MPPS --output-file ./mpps.json

# 사용자 정의 연결 제한
./build/bin/mpps_scp 11112 MY_MPPS --max-assoc 20 --timeout 600

# 도움말 표시
./build/bin/mpps_scp --help
```

주요 기능:
- **N-CREATE**: 검사 시작 알림 수신 (상태 = IN PROGRESS)
- **N-SET**: 검사 완료 (COMPLETED) 또는 취소 (DISCONTINUED) 수신
- **JSON 저장**: 통합을 위한 JSON 파일로 MPPS 레코드 저장
- **통계**: 종료 시 세션 통계 표시
- **우아한 종료**: 시그널 처리 (SIGINT, SIGTERM)

### Query/Retrieve SCP (C-FIND/C-MOVE/C-GET 서버)

저장 디렉토리에서 DICOM 파일을 제공하는 경량 Query/Retrieve SCP 서버입니다.

```bash
# 기본 사용법 - 디렉토리에서 파일 제공
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom

# 영구 인덱스 데이터베이스 사용 (재시작 시 더 빠름)
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --index-db ./pacs.db

# C-MOVE 대상 피어 설정
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --peer VIEWER:192.168.1.10:11113

# 다중 대상 C-MOVE를 위한 여러 피어 설정
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom \
  --peer WS1:10.0.0.1:104 --peer WS2:10.0.0.2:104

# 서버 시작 없이 스토리지 스캔 및 인덱싱만 수행
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --scan-only

# 연결 제한 커스터마이징
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --max-assoc 20 --timeout 600
```

기능:
- **C-FIND**: Patient, Study, Series, Image 레벨에서 조회
- **C-MOVE**: 설정된 대상 AE Title로 이미지 전송
- **C-GET**: 동일 연결을 통한 직접 이미지 검색
- **자동 인덱싱**: SQLite 기반 빠른 쿼리 응답
- **정상 종료**: 시그널 처리 (SIGINT, SIGTERM)

### Modality Worklist SCP

모달리티에 예약된 검사 정보를 제공하는 독립 실행형 Modality Worklist (MWL) 서버입니다.

```bash
# JSON 워크리스트 파일로 기본 사용
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json

# 디렉토리에서 워크리스트 로드
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-dir ./worklist_data

# 결과 제한 설정
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json --max-results 100
```

**샘플 워크리스트 JSON** (`worklist.json`):
```json
[
  {
    "patientId": "12345",
    "patientName": "DOE^JOHN",
    "patientBirthDate": "19800101",
    "patientSex": "M",
    "studyInstanceUid": "1.2.3.4.5.6.7.8.9",
    "accessionNumber": "ACC001",
    "scheduledStationAeTitle": "CT_01",
    "scheduledProcedureStepStartDate": "20241220",
    "scheduledProcedureStepStartTime": "100000",
    "modality": "CT",
    "scheduledProcedureStepId": "SPS001",
    "scheduledProcedureStepDescription": "CT Abdomen"
  }
]
```

기능:
- **MWL C-FIND**: Modality Worklist 쿼리 응답
- **JSON 데이터 소스**: JSON 파일에서 워크리스트 항목 로드
- **필터링**: Patient ID, 이름, 모달리티, 스테이션 AE, 예약 날짜
- **정상 종료**: 시그널 처리 (SIGINT, SIGTERM)

### 전체 PACS 서버

```bash
# 설정 파일로 실행
./build/examples/pacs_server/pacs_server --config pacs_server.yaml
```

**샘플 설정** (`pacs_server.yaml`):
```yaml
server:
  ae_title: MY_PACS
  port: 11112
  max_associations: 50

storage:
  directory: ./archive
  naming: hierarchical

database:
  path: ./pacs.db
  wal_mode: true
```

---

## 코드 통계

| 항목 | 값 |
|------|-----|
| **헤더 파일** | 222개 |
| **소스 파일** | 148개 |
| **헤더 LOC** | 61,462줄 |
| **소스 LOC** | 85,141줄 |
| **예제 LOC** | 34,752줄 |
| **테스트 LOC** | 62,241줄 |
| **총 LOC** | 251,242줄 |
| **테스트 파일** | 141개 이상 |
| **테스트 케이스** | 1,980개 이상 |
| **예제 프로그램** | 32개 |
| **문서** | 57개 마크다운 |
| **CI/CD 워크플로우** | 10개 |
| **버전** | 0.2.0 |
| **최종 업데이트** | 2026-02-20 |

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
