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

**현재 단계**: 🎯 Phase 2 거의 완료 - Network & Services (80%+)

| 마일스톤 | 상태 | 목표 |
|----------|------|------|
| 분석 및 문서화 | ✅ 완료 | 1주차 |
| 핵심 DICOM 구조 | ✅ 완료 | 2-5주차 |
| Encoding 모듈 | ✅ 완료 | 2-5주차 |
| Storage 백엔드 | ✅ 완료 | 6-9주차 |
| Integration 어댑터 | ✅ 완료 | 6-9주차 |
| 네트워크 프로토콜 (PDU) | ✅ 완료 | 6-9주차 |
| DIMSE 서비스 | ✅ 완료 | 10-13주차 |
| Query/Retrieve | ✅ 완료 | 14-17주차 |
| Worklist/MPPS | ✅ 완료 | 18-20주차 |
| 고급 압축 | 🔜 예정 | Phase 3 |

**테스트 커버리지**: 34개 테스트 파일, 113개 이상 테스트 통과

### Phase 1 성과 (완료)

**Core 모듈** (57개 테스트):
- `dicom_tag` - DICOM 태그 표현 (Group, Element 쌍)
- `dicom_element` - 태그, VR, 값을 가진 데이터 요소
- `dicom_dataset` - 데이터 요소의 정렬된 컬렉션
- `dicom_file` - DICOM Part 10 파일 읽기/쓰기
- `dicom_dictionary` - 표준 태그 메타데이터 조회 (5,000개 이상)

**Encoding 모듈** (41개 테스트):
- `vr_type` - 27개 DICOM Value Representation 타입 전체
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
- `query_scp` - C-FIND 서비스 (검색)
- `retrieve_scp` - C-MOVE/C-GET 서비스 (조회)
- `worklist_scp` - Modality Worklist 서비스 (MWL)
- `mpps_scp` - Modality Performed Procedure Step

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
│   │   └── mpps_scp.hpp         # MPPS SCP
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
│   └── integration/             # 에코시스템 어댑터 (✅ 완료)
│       ├── container_adapter.hpp # container_system 연동
│       ├── network_adapter.hpp  # network_system 연동
│       ├── thread_adapter.hpp   # thread_system 연동
│       ├── logger_adapter.hpp   # logger_system 연동
│       ├── monitoring_adapter.hpp # monitoring_system 연동
│       └── dicom_session.hpp    # 상위 레벨 세션
│
├── src/                         # 소스 파일 (~13,500줄)
│   ├── core/                    # Core 구현 (7개 파일)
│   ├── encoding/                # Encoding 구현 (4개 파일)
│   ├── network/                 # Network 구현 (8개 파일)
│   ├── services/                # Services 구현 (7개 파일)
│   ├── storage/                 # Storage 구현 (4개 파일)
│   └── integration/             # Adapter 구현 (6개 파일)
│
├── tests/                       # 테스트 스위트 (34개 파일, 113개 이상 테스트)
│   ├── core/                    # Core 모듈 테스트 (6개 파일)
│   ├── encoding/                # Encoding 모듈 테스트 (5개 파일)
│   ├── network/                 # Network 모듈 테스트 (5개 파일)
│   ├── services/                # Services 테스트 (7개 파일)
│   ├── storage/                 # Storage 테스트 (6개 파일)
│   └── integration/             # Adapter 테스트 (5개 파일)
│
├── examples/                    # 예제 애플리케이션 (5개, ~2,400줄)
│   ├── echo_scp/                # DICOM Echo SCP 서버
│   ├── echo_scu/                # DICOM Echo SCU 클라이언트
│   ├── store_scu/               # DICOM Storage SCU 클라이언트
│   ├── query_scu/               # DICOM Query SCU 클라이언트 (C-FIND)
│   └── pacs_server/             # 전체 PACS 서버 예제
│
├── docs/                        # 문서 (26개 이상 파일)
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

---

## DICOM 적합성

### 지원 SOP 클래스

| 서비스 | SOP Class | 상태 |
|--------|-----------|------|
| **Verification** | 1.2.840.10008.1.1 | ✅ 완료 |
| **CT Storage** | 1.2.840.10008.5.1.4.1.1.2 | ✅ 완료 |
| **MR Storage** | 1.2.840.10008.5.1.4.1.1.4 | ✅ 완료 |
| **X-Ray Storage** | 1.2.840.10008.5.1.4.1.1.1.1 | ✅ 완료 |
| **Patient Root Q/R** | 1.2.840.10008.5.1.4.1.2.1.x | ✅ 완료 |
| **Study Root Q/R** | 1.2.840.10008.5.1.4.1.2.2.x | ✅ 완료 |
| **Modality Worklist** | 1.2.840.10008.5.1.4.31 | ✅ 완료 |
| **MPPS** | 1.2.840.10008.3.1.2.3.3 | ✅ 완료 |

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

**테스트 결과**: 113개 이상 테스트 통과 (Core: 57, Encoding: 41, Network: 15, Storage/Integration: 15+)

### 빌드 옵션

```cmake
PACS_BUILD_TESTS (ON)              # 유닛 테스트 활성화
PACS_BUILD_EXAMPLES (OFF)          # 예제 빌드 활성화
PACS_BUILD_BENCHMARKS (OFF)        # 벤치마크 활성화
PACS_BUILD_STORAGE (ON)            # Storage 모듈 빌드
```

---

## 예제

### Echo SCP (검증 서버)

```bash
# 예제 빌드
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build

# Echo SCP 실행
./build/examples/echo_scp/echo_scp --port 11112 --ae-title MY_ECHO
```

### Echo SCU (검증 클라이언트)

```bash
# 연결 테스트
./build/examples/echo_scu/echo_scu --host localhost --port 11112 --ae-title TEST_SCU
```

### Storage SCU (이미지 전송)

```bash
# 단일 DICOM 파일 전송
./build/examples/store_scu/store_scu localhost 11112 PACS_SCP image.dcm

# 디렉토리의 모든 파일 전송 (재귀)
./build/examples/store_scu/store_scu localhost 11112 PACS_SCP ./dicom_folder/ --recursive

# Transfer Syntax 지정
./build/examples/store_scu/store_scu localhost 11112 PACS_SCP image.dcm --transfer-syntax explicit-le
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
| **헤더 파일** | 48개 |
| **소스 파일** | 33개 |
| **헤더 LOC** | ~13,500줄 |
| **소스 LOC** | ~13,800줄 |
| **예제 LOC** | ~1,200줄 |
| **총 LOC** | ~27,300줄 |
| **테스트 파일** | 34개 |
| **테스트 케이스** | 113개 이상 |
| **문서** | 26개 이상 마크다운 |
| **버전** | 0.2.0 |

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
