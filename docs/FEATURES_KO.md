# PACS 시스템 기능

> **버전:** 1.0.0
> **최종 수정일:** 2025-12-01
> **언어:** [English](FEATURES.md) | **한국어**

이 문서는 PACS 시스템에서 사용 가능한 모든 기능에 대한 포괄적인 세부 정보를 제공합니다.

---

## 목차

- [DICOM 코어 기능](#dicom-코어-기능)
- [네트워크 프로토콜 기능](#네트워크-프로토콜-기능)
- [DICOM 서비스](#dicom-서비스)
- [저장 백엔드](#저장-백엔드)
- [에코시스템 통합](#에코시스템-통합)
- [보안 기능](#보안-기능)
- [모니터링 및 관찰성](#모니터링-및-관찰성)
- [계획된 기능](#계획된-기능)

---

## DICOM 코어 기능

### 데이터 요소 처리

**구현**: 27개의 모든 Value Representation(VR)을 지원하는 완전한 DICOM Data Element 파서 및 인코더.

**기능**:
- DICOM 데이터 요소 파싱 및 인코딩 (태그, VR, 길이, 값)
- 27개 표준 VR 타입 모두 지원 (PS3.5)
- 데이터 딕셔너리에서 자동 VR 결정
- Little Endian 및 Big Endian 바이트 순서
- Explicit 및 Implicit VR 인코딩 지원

**클래스**:
- `dicom_element` - 핵심 데이터 요소 표현
- `dicom_tag` - 태그 식별 (그룹, 요소)
- `vr_type` - Value Representation 열거형
- `vr_codec` - VR별 인코딩/디코딩

**예제**:
```cpp
#include <pacs/core/dicom_element.h>

using namespace pacs::core;

// 환자 이름 요소 생성
auto patient_name = dicom_element::create(
    tags::PatientName,           // (0010,0010)
    vr_type::PN,                 // Person Name
    "Doe^John"                   // 값
);

// 요소 속성 접근
auto tag = patient_name.tag();           // 0x00100010
auto vr = patient_name.vr();             // PN
auto value = patient_name.as_string();   // "Doe^John"
```

### Value Representation 타입

container_system과의 적절한 매핑으로 27개의 모든 DICOM VR 타입이 지원됩니다:

| 카테고리 | VR 타입 | 설명 |
|----------|---------|------|
| **문자열** | AE, AS, CS, DA, DS, DT, IS, LO, LT, PN, SH, ST, TM, UI, UT | 텍스트 기반 값 |
| **숫자** | FL, FD, SL, SS, UL, US | 숫자 값 |
| **바이너리** | OB, OD, OF, OL, OW, UN | 바이너리 데이터 |
| **구조화** | AT, SQ | 태그 및 시퀀스 |

**VR 기능**:
- 고정 길이 VR에 대한 자동 패딩
- 문자 세트 처리 (ISO-IR 100, UTF-8)
- 값 구분자 `\`를 사용한 다중성 지원
- VR 제약 조건 검증 (길이, 형식)

### 데이터 세트 연산

**구현**: 효율적인 조회 및 조작이 가능한 정렬된 데이터 요소 컬렉션.

**기능**:
- 데이터 요소 생성, 읽기, 업데이트, 삭제
- 태그 순서로 요소 순회
- 태그, 키워드 또는 경로 표현식으로 검색
- 깊은 복사 및 이동 의미론
- 중첩 시퀀스 지원
- 충돌 해결을 통한 데이터 세트 병합

**클래스**:
- `dicom_dataset` - 메인 데이터 세트 컨테이너
- `dataset_iterator` - 순방향 반복자
- `dataset_path` - 중첩 접근을 위한 경로 표현식

**예제**:
```cpp
#include <pacs/core/dicom_dataset.h>

using namespace pacs::core;

// 데이터 세트 생성
dicom_dataset dataset;

// 요소 추가
dataset.set_string(tags::PatientName, "Doe^John");
dataset.set_string(tags::PatientID, "12345");
dataset.set_string(tags::StudyDate, "20250130");
dataset.set_uint16(tags::Rows, 512);
dataset.set_uint16(tags::Columns, 512);

// 값 가져오기
auto name = dataset.get_string(tags::PatientName);
auto rows = dataset.get_uint16(tags::Rows);

// 존재 여부 확인
if (dataset.contains(tags::PixelData)) {
    auto pixels = dataset.get_bytes(tags::PixelData);
}

// 모든 요소 순회
for (const auto& element : dataset) {
    std::cout << element.tag() << ": " << element.as_string() << "\n";
}
```

### DICOM 파일 처리 (Part 10)

**구현**: PS3.10에 따른 완전한 DICOM 파일 형식 지원.

**기능**:
- 128바이트 프리앰블이 있는 DICOM 파일 읽기
- DICM 접두사 검증
- 파일 메타 정보 파싱 (그룹 0002)
- 다중 Transfer Syntax 지원
- DICOM Part 10 규격 파일 쓰기
- 파일 메타 정보 헤더 처리

**클래스**:
- `dicom_file` - 파일 리더/라이터
- `file_meta_info` - 그룹 0002 요소
- `transfer_syntax` - Transfer Syntax 처리

**예제**:
```cpp
#include <pacs/core/dicom_file.h>

using namespace pacs::core;

// DICOM 파일 읽기
auto result = dicom_file::read("/path/to/image.dcm");
if (result.is_ok()) {
    auto& file = result.value();
    auto& dataset = file.dataset();
    auto transfer_syntax = file.transfer_syntax();

    std::cout << "환자: " << dataset.get_string(tags::PatientName) << "\n";
    std::cout << "Transfer Syntax: " << transfer_syntax.uid() << "\n";
}

// DICOM 파일 쓰기
dicom_file file;
file.set_dataset(dataset);
file.set_transfer_syntax(transfer_syntax::explicit_vr_little_endian());

auto write_result = file.write("/path/to/output.dcm");
if (write_result.is_err()) {
    std::cerr << "쓰기 실패: " << write_result.error().message << "\n";
}
```

### Transfer Syntax 지원

**지원되는 Transfer Syntax**:

| Transfer Syntax | UID | 상태 |
|-----------------|-----|------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | ✅ 구현됨 |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | ✅ 구현됨 |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | 🔜 계획됨 |
| JPEG Baseline | 1.2.840.10008.1.2.4.50 | 🔮 향후 |
| JPEG Lossless | 1.2.840.10008.1.2.4.70 | 🔮 향후 |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | 🔮 향후 |
| JPEG 2000 | 1.2.840.10008.1.2.4.91 | 🔮 향후 |
| RLE Lossless | 1.2.840.10008.1.2.5 | 🔮 향후 |

---

## 네트워크 프로토콜 기능

### 상위 레이어 프로토콜 (PDU)

**구현**: PS3.8에 따른 완전한 DICOM 상위 레이어 프로토콜.

**지원되는 PDU 타입**:

| PDU 타입 | 코드 | 설명 |
|----------|------|------|
| A-ASSOCIATE-RQ | 0x01 | 연결 요청 |
| A-ASSOCIATE-AC | 0x02 | 연결 수락 |
| A-ASSOCIATE-RJ | 0x03 | 연결 거부 |
| P-DATA-TF | 0x04 | 데이터 전송 |
| A-RELEASE-RQ | 0x05 | 해제 요청 |
| A-RELEASE-RP | 0x06 | 해제 응답 |
| A-ABORT | 0x07 | 중단 |

**클래스**:
- `pdu_encoder` - PDU 직렬화
- `pdu_decoder` - PDU 역직렬화
- `associate_rq_pdu` - A-ASSOCIATE-RQ 구조
- `associate_ac_pdu` - A-ASSOCIATE-AC 구조
- `p_data_pdu` - P-DATA-TF 구조

### 연결 관리

**구현**: 프레젠테이션 컨텍스트 협상을 갖춘 완전한 연결 상태 머신.

**기능**:
- 8개 상태 연결 상태 머신 (PS3.8 그림 9-1)
- 프레젠테이션 컨텍스트 협상
- Abstract Syntax 및 Transfer Syntax 매칭
- 최대 PDU 크기 협상
- 다중 동시 연결
- 연결 타임아웃 처리
- 확장 협상 지원

**연결 상태**:
```
┌──────────────────────────────────────────────────────────┐
│                  연결 상태 머신                            │
├──────────────────────────────────────────────────────────┤
│  Sta1 (유휴) ───────────────────────────────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta2 (전송 연결 열림) ─────────────────────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta3 (로컬 A-ASSOCIATE 응답 대기) ────────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta6 (연결 수립됨) ◄─────────────────────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta7 (A-RELEASE 응답 대기) ────────────────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta1 (유휴) ◄──────────────────────────────────────────── │
└──────────────────────────────────────────────────────────┘
```

**예제**:
```cpp
#include <pacs/network/association.h>

using namespace pacs::network;

// 연결 구성
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.max_pdu_size = 16384;

// 프레젠테이션 컨텍스트 추가
config.add_context(
    sop_class::ct_image_storage,
    {transfer_syntax::explicit_vr_little_endian()}
);

// 연결 생성
auto result = association::connect("192.168.1.100", 11112, config);
if (result.is_ok()) {
    auto& assoc = result.value();

    // DIMSE 연산 수행...

    assoc.release();  // 정상 해제
}
```

### DIMSE 메시지 교환

**구현**: 완전한 DIMSE-C 및 DIMSE-N 메시지 지원.

**DIMSE-C 서비스**:

| 서비스 | 상태 | 설명 |
|--------|------|------|
| C-ECHO | ✅ 구현됨 | 검증 |
| C-STORE | ✅ 구현됨 | 저장 |
| C-FIND | ✅ 구현됨 | 조회 |
| C-MOVE | ✅ 구현됨 | 검색 (이동) |
| C-GET | ✅ 구현됨 | 검색 (가져오기) |

**DIMSE-N 서비스**:

| 서비스 | 상태 | 설명 |
|--------|------|------|
| N-CREATE | ✅ 구현됨 | 객체 생성 (MPPS) |
| N-SET | ✅ 구현됨 | 객체 수정 (MPPS) |
| N-GET | 🔮 향후 | 속성 가져오기 |
| N-EVENT-REPORT | 🔮 향후 | 이벤트 알림 |
| N-ACTION | 🔮 향후 | 액션 요청 |
| N-DELETE | 🔮 향후 | 객체 삭제 |

---

## DICOM 서비스

### 검증 서비스 (C-ECHO)

**구현**: 연결 테스트를 위한 DICOM Verification SOP 클래스.

**기능**:
- Verification SCP (응답자)
- Verification SCU (개시자)
- 연결 상태 확인
- 연결 유효성 검증

**SOP 클래스**: Verification SOP Class (1.2.840.10008.1.1)

**예제**:
```cpp
#include <pacs/services/verification_scu.h>

using namespace pacs::services;

// 연결 테스트
verification_scu scu("MY_SCU");
auto result = scu.echo("PACS_SCP", "192.168.1.100", 11112);

if (result.is_ok()) {
    std::cout << "DICOM Echo 성공!\n";
} else {
    std::cerr << "Echo 실패: " << result.error().message << "\n";
}
```

### 저장 서비스 (C-STORE)

**구현**: DICOM 이미지를 수신하고 전송하기 위한 Storage SCP/SCU.

**기능**:
- 모달리티에서 이미지 수신 (SCP)
- PACS/뷰어로 이미지 전송 (SCU)
- 다중 저장 SOP 클래스
- 동시 이미지 수신
- 중복 감지
- 저장 커밋 (계획됨)

**지원되는 SOP 클래스**:

| SOP 클래스 | UID | 상태 |
|-----------|-----|------|
| CT Image Storage | 1.2.840.10008.5.1.4.1.1.2 | ✅ |
| MR Image Storage | 1.2.840.10008.5.1.4.1.1.4 | ✅ |
| CR Image Storage | 1.2.840.10008.5.1.4.1.1.1 | ✅ |
| DX Image Storage | 1.2.840.10008.5.1.4.1.1.1.1 | ✅ |
| Secondary Capture | 1.2.840.10008.5.1.4.1.1.7 | ✅ |
| Ultrasound Storage | 1.2.840.10008.5.1.4.1.1.6.1 | 🔜 |
| XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1 | 🔜 |

**예제**:
```cpp
#include <pacs/services/storage_scp.h>

using namespace pacs::services;

// Storage SCP 구성
storage_scp_config config;
config.ae_title = "PACS_SCP";
config.port = 11112;
config.storage_path = "/data/dicom";
config.max_associations = 10;

// 서버 시작
storage_scp server(config);
server.set_handler([](const dicom_dataset& dataset) {
    std::cout << "수신됨: "
              << dataset.get_string(tags::SOPInstanceUID) << "\n";
    return storage_status::success;
});

auto result = server.start();
if (result.is_ok()) {
    server.wait_for_shutdown();
}
```

### 조회/검색 서비스 (C-FIND, C-MOVE)

**구현**: 환자/스터디/시리즈/이미지 계층을 기반으로 이미지 조회 및 검색.

**조회 레벨**:

| 레벨 | 키 속성 | 선택적 속성 |
|------|---------|-------------|
| Patient | PatientID | PatientName, PatientBirthDate |
| Study | StudyInstanceUID | StudyDate, Modality, AccessionNumber |
| Series | SeriesInstanceUID | SeriesNumber, SeriesDescription |
| Image | SOPInstanceUID | InstanceNumber |

**정보 모델**:
- Patient Root Query/Retrieve Information Model
- Study Root Query/Retrieve Information Model

**예제**:
```cpp
#include <pacs/services/query_scu.h>

using namespace pacs::services;

// 스터디 조회
query_scu scu("MY_SCU");
auto assoc = scu.connect("PACS_SCP", "192.168.1.100", 11112);

// 조회 구성
dicom_dataset query;
query.set_string(tags::PatientName, "Doe*");
query.set_string(tags::StudyDate, "20250101-20250130");
query.set_string(tags::QueryRetrieveLevel, "STUDY");

// 조회 실행
auto results = scu.find(assoc, query);
for (const auto& study : results) {
    std::cout << "스터디: " << study.get_string(tags::StudyInstanceUID) << "\n";
}
```

### Modality Worklist 서비스

**구현**: RIS/HIS 시스템에서 예약된 절차 조회.

**기능**:
- 예약된 절차 단계 정보
- 환자 인구통계
- 예약된 스테이션 AE Title 필터링
- 날짜 범위 조회

**SOP 클래스**: Modality Worklist Information Model (1.2.840.10008.5.1.4.31)

### MPPS 서비스

**구현**: 절차 실행 상태 추적.

**기능**:
- MPPS 인스턴스 생성 (N-CREATE)
- 절차 상태 업데이트 (N-SET)
- 진행 중, 완료, 중단 상태 추적
- 수행된 시리즈를 예약된 절차에 연결

**SOP 클래스**: Modality Performed Procedure Step (1.2.840.10008.3.1.2.3.3)

---

## 저장 백엔드

### 파일 시스템 저장소

**구현**: 설정 가능한 이름 지정이 있는 계층적 파일 저장소.

**기능**:
- 설정 가능한 디렉토리 구조
- 환자/스터디/시리즈/이미지 계층
- 원자적 파일 연산
- 중복 감지
- 동시 접근을 위한 파일 잠금

**디렉토리 구조 옵션**:
```
# 옵션 1: UID 기반 (기본값)
/storage/
  └── {StudyInstanceUID}/
      └── {SeriesInstanceUID}/
          └── {SOPInstanceUID}.dcm

# 옵션 2: 날짜 기반
/storage/
  └── {YYYY}/
      └── {MM}/
          └── {DD}/
              └── {SOPInstanceUID}.dcm
```

### 인덱스 데이터베이스

**구현**: 빠른 조회를 위한 SQLite 기반 인덱스.

**스키마**:
```sql
-- 환자 테이블
CREATE TABLE patient (
    patient_id TEXT PRIMARY KEY,
    patient_name TEXT,
    patient_birth_date TEXT,
    patient_sex TEXT
);

-- 스터디 테이블
CREATE TABLE study (
    study_instance_uid TEXT PRIMARY KEY,
    patient_id TEXT REFERENCES patient(patient_id),
    study_date TEXT,
    study_time TEXT,
    accession_number TEXT,
    study_description TEXT,
    modalities_in_study TEXT
);

-- 시리즈 테이블
CREATE TABLE series (
    series_instance_uid TEXT PRIMARY KEY,
    study_instance_uid TEXT REFERENCES study(study_instance_uid),
    series_number INTEGER,
    modality TEXT,
    series_description TEXT
);

-- 인스턴스 테이블
CREATE TABLE instance (
    sop_instance_uid TEXT PRIMARY KEY,
    series_instance_uid TEXT REFERENCES series(series_instance_uid),
    sop_class_uid TEXT,
    instance_number INTEGER,
    file_path TEXT
);
```

---

## 에코시스템 통합

### container_system 통합

**목적**: DICOM 데이터 직렬화 및 타입 안전 값 처리.

**통합 지점**:
- VR 타입을 컨테이너 값 타입에 매핑
- DICOM 인코딩을 위한 바이너리 직렬화
- SQ (시퀀스) 요소를 위한 중첩 컨테이너
- 대용량 배열을 위한 SIMD 가속

**예제**:
```cpp
#include <pacs/integration/container_adapter.h>

// DICOM 데이터세트를 컨테이너로 변환
auto container = container_adapter::to_container(dataset);

// 직렬화
auto binary = container->serialize();

// 다시 변환
auto restored_dataset = container_adapter::from_container(container);
```

### network_system 통합

**목적**: DICOM 통신을 위한 TCP/TLS 전송.

**통합 지점**:
- `messaging_server` → DICOM SCP
- `messaging_client` → DICOM SCU
- `messaging_session` → DICOM Association
- 보안 DICOM을 위한 TLS 지원

### thread_system 통합

**목적**: DICOM 연산의 동시 처리.

**통합 지점**:
- DIMSE 메시지 처리를 위한 스레드 풀
- 이미지 저장 파이프라인을 위한 락프리 큐
- 장시간 연산을 위한 취소 토큰

### logger_system 통합

**목적**: 포괄적인 로깅 및 감사 추적.

**로그 카테고리**:
- 연결 이벤트
- DIMSE 연산
- 저장 연산
- 오류 조건
- 보안 이벤트 (감사)

### monitoring_system 통합

**목적**: 성능 메트릭 및 상태 모니터링.

**메트릭**:
- 활성 연결 수
- 분당 저장된 이미지
- 조회 응답 지연
- 저장 처리량 (MB/s)
- 오류율

---

## 보안 기능

### TLS 지원

**구현**: PS3.15에 따른 보안 DICOM 통신.

**기능**:
- TLS 1.2 및 TLS 1.3
- 최신 암호 스위트
- 인증서 검증
- 상호 인증 (선택 사항)

### 접근 제어

**기능**:
- AE Title 화이트리스팅
- IP 기반 제한
- 사용자 인증 (계획됨)

### 감사 로깅

**기능**:
- 모든 DICOM 연산 기록
- PHI 접근 추적
- HIPAA 준수 감사 추적
- 변조 방지 로깅

---

## 모니터링 및 관찰성

### 상태 확인

**엔드포인트**:
- `/health` - 기본 상태 확인
- `/ready` - 준비 상태 프로브
- `/live` - 활성 상태 프로브

### 메트릭

**Prometheus 호환 메트릭**:
```
# 연결 메트릭
pacs_associations_active{ae_title="PACS_SCP"}
pacs_associations_total{ae_title="PACS_SCP"}

# 저장 메트릭
pacs_images_stored_total{sop_class="CT"}
pacs_storage_bytes_total{sop_class="CT"}
pacs_storage_latency_seconds{quantile="0.95"}

# 조회 메트릭
pacs_queries_total{level="STUDY"}
pacs_query_latency_seconds{quantile="0.95"}
```

### 분산 추적

**스팬 타입**:
- 연결 라이프사이클
- DIMSE 연산
- 저장 연산
- 데이터베이스 조회

---

## 계획된 기능

### 단기 (다음 릴리스)

| 기능 | 설명 | 목표 |
|------|------|------|
| C-GET | 대체 검색 방법 | 3단계 |
| 확장 SOP 클래스 | US, XA 등 | 3단계 |
| 연결 풀링 | 연결 재사용 | 3단계 |

### 중기

| 기능 | 설명 | 목표 |
|------|------|------|
| JPEG 압축 | 손실/무손실 | 향후 |
| WADO-RS | DICOMweb 지원 | 향후 |
| 클러스터링 | 다중 노드 PACS | 향후 |

### 장기

| 기능 | 설명 | 목표 |
|------|------|------|
| AI 통합 | 추론 파이프라인 | 향후 |
| 클라우드 저장소 | S3/Azure Blob | 향후 |
| FHIR 통합 | 의료 상호운용성 | 향후 |

---

*문서 버전: 1.0.0*
*작성일: 2025-11-30*
*수정일: 2025-12-01*
*작성자: kcenon@naver.com*
