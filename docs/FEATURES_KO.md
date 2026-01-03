# PACS 시스템 기능

> **버전:** 0.1.9.0
> **최종 수정일:** 2025-12-17
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
- [워크플로우 서비스](#워크플로우-서비스)
- [에러 처리](#에러-처리)
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
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | ✅ 구현됨 |
| JPEG Baseline | 1.2.840.10008.1.2.4.50 | ✅ 구현됨 |
| JPEG Lossless | 1.2.840.10008.1.2.4.70 | ✅ 구현됨 |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | ✅ 구현됨 |
| JPEG 2000 | 1.2.840.10008.1.2.4.91 | ✅ 구현됨 |
| RLE Lossless | 1.2.840.10008.1.2.5 | ✅ 구현됨 |
| JPEG-LS Lossless | 1.2.840.10008.1.2.4.80 | ✅ 구현됨 |
| JPEG-LS Near-Lossless | 1.2.840.10008.1.2.4.81 | ✅ 구현됨 |

### Undefined Length 지원

**구현**: DICOM PS3.5에 따른 undefined length 데이터 요소의 완전한 지원.

| 기능 | 상태 | 설명 |
|------|------|------|
| Undefined Length 시퀀스 (SQ) | ✅ 구현됨 | Sequence Delimitation Item으로 종료되는 길이 0xFFFFFFFF 시퀀스 |
| Undefined Length 시퀀스 아이템 | ✅ 구현됨 | Item Delimitation Item으로 종료되는 길이 0xFFFFFFFF 아이템 |
| 캡슐화된 픽셀 데이터 | ✅ 구현됨 | 압축된 프래그먼트를 포함하는 undefined length 픽셀 데이터 |
| Basic Offset Table | ✅ 구현됨 | 랜덤 액세스를 위한 캡슐화된 픽셀 데이터의 첫 번째 아이템 |
| Multi-fragment 프레임 | ✅ 구현됨 | 여러 프래그먼트 아이템에 걸쳐 있는 프레임 |
| 중첩 시퀀스 | ✅ 구현됨 | 모든 중첩 레벨에서 시퀀스 내부의 시퀀스 |

**구분자 태그**:
- Item 태그: `(FFFE,E000)`
- Item Delimitation Item: `(FFFE,E00D)`
- Sequence Delimitation Item: `(FFFE,E0DD)`

**예제**:
```cpp
#include <pacs/core/dicom_file.hpp>

using namespace pacs::core;

// Undefined length 시퀀스가 있는 DICOM 파일 읽기
auto result = dicom_file::open("compressed_image.dcm");
if (result.is_ok()) {
    auto& file = result.value();

    // Undefined length 시퀀스 접근
    if (file.dataset().has_sequence(tags::referenced_series_sequence)) {
        auto* seq = file.dataset().get_sequence(tags::referenced_series_sequence);
        for (const auto& item : *seq) {
            std::cout << "시리즈 UID: " << item.get_string(tags::series_instance_uid) << "\n";
        }
    }

    // 캡슐화된 픽셀 데이터 접근
    if (auto* pixel_elem = file.dataset().get(tags::pixel_data)) {
        auto raw_data = pixel_elem->raw_data();
        // raw_data에 캡슐화된 프래그먼트 포함
    }
}
```

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
| N-CREATE | ✅ 구현됨 | 관리 SOP 인스턴스 생성 (MPPS, 인쇄) |
| N-SET | ✅ 구현됨 | 객체 속성 수정 (MPPS) |
| N-GET | ✅ 구현됨 | 선택적 검색으로 속성 값 가져오기 |
| N-EVENT-REPORT | ✅ 구현됨 | 이벤트 알림 (저장 커밋) |
| N-ACTION | ✅ 구현됨 | 액션 요청 (저장 커밋) |
| N-DELETE | ✅ 구현됨 | 관리 SOP 인스턴스 삭제 (인쇄) |

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
| US Image Storage | 1.2.840.10008.5.1.4.1.1.6.1 | ✅ |
| US Multi-frame Image Storage | 1.2.840.10008.5.1.4.1.1.6.2 | ✅ |
| XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1 | ✅ |
| Enhanced XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1.1 | ✅ |
| XRF Image Storage | 1.2.840.10008.5.1.4.1.1.12.2 | ✅ |
| X-Ray 3D Angiographic Image Storage | 1.2.840.10008.5.1.4.1.1.13.1.1 | ✅ |
| X-Ray 3D Craniofacial Image Storage | 1.2.840.10008.5.1.4.1.1.13.1.2 | ✅ |
| NM Image Storage | 1.2.840.10008.5.1.4.1.1.20 | ✅ |
| NM Image Storage (Retired) | 1.2.840.10008.5.1.4.1.1.5 | ✅ |
| PET Image Storage | 1.2.840.10008.5.1.4.1.1.128 | ✅ |
| Enhanced PET Image Storage | 1.2.840.10008.5.1.4.1.1.130 | ✅ |
| Legacy Converted Enhanced PET Image Storage | 1.2.840.10008.5.1.4.1.1.128.1 | ✅ |
| RT Plan Storage | 1.2.840.10008.5.1.4.1.1.481.5 | ✅ |
| RT Dose Storage | 1.2.840.10008.5.1.4.1.1.481.2 | ✅ |
| RT Structure Set Storage | 1.2.840.10008.5.1.4.1.1.481.3 | ✅ |
| RT Image Storage | 1.2.840.10008.5.1.4.1.1.481.1 | ✅ |
| RT Beams Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.4 | ✅ |
| RT Brachy Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.6 | ✅ |
| RT Treatment Summary Record Storage | 1.2.840.10008.5.1.4.1.1.481.7 | ✅ |
| RT Ion Plan Storage | 1.2.840.10008.5.1.4.1.1.481.8 | ✅ |
| RT Ion Beams Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.9 | ✅ |

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

### S3 클라우드 저장소 (목 구현)

**구현**: 테스트용 목 클라이언트를 갖춘 S3 호환 클라우드 저장소 백엔드.

**기능**:
- AWS S3 및 S3 호환 저장소 (MinIO 등)
- 계층적 객체 키 구조 (Study/Series/SOP)
- 대용량 파일을 위한 멀티파트 업로드 지원 (플레이스홀더)
- 업로드/다운로드 모니터링을 위한 진행 콜백
- shared_mutex를 통한 스레드 안전 연산
- 설정 가능한 연결 및 타임아웃 설정

**참고**: API 검증 및 테스트를 위한 목 구현입니다. 전체 AWS SDK C++ 통합은 향후 릴리스에서 추가될 예정입니다.

### Azure Blob 저장소 (목 구현)

**구현**: 테스트용 목 클라이언트를 갖춘 Azure Blob 저장소 백엔드.

**기능**:
- Azure Blob Storage 컨테이너 지원
- 대용량 파일을 위한 블록 블롭 업로드 (병렬 블록 스테이징)
- 액세스 티어 관리 (Hot, Cool, Archive)
- 업로드/다운로드 모니터링을 위한 진행 콜백
- shared_mutex를 통한 스레드 안전 연산
- 로컬 테스트를 위한 Azurite 에뮬레이터 지원

**참고**: API 검증 및 테스트를 위한 목 구현입니다. 전체 Azure SDK C++ 통합은 향후 릴리스에서 추가될 예정입니다.

### 계층적 저장소 관리 (HSM)

**구현**: 티어 간 자동 연령 기반 데이터 마이그레이션이 있는 3계층 저장소 시스템.

**기능**:
- 세 가지 저장소 티어: Hot (빠른 접근), Warm (중간), Cold (보관)
- 설정 가능한 연령 정책에 기반한 자동 백그라운드 마이그레이션
- 모든 티어에서 투명한 데이터 검색
- 통계 추적 및 모니터링
- shared_mutex를 통한 스레드 안전 연산
- 마이그레이션 연산을 위한 진행 및 오류 콜백
- 병렬 마이그레이션을 위한 thread_system 통합

**티어 특성**:

| 티어 | 사용 사례 | 일반적인 백엔드 | 접근 패턴 |
|------|----------|-----------------|-----------|
| Hot | 활성 스터디 | 로컬 SSD, 파일 시스템 | 빈번한 읽기/쓰기 |
| Warm | 최근 아카이브 | 네트워크 저장소, S3 | 가끔 접근 |
| Cold | 장기 아카이브 | Azure Archive, Glacier | 드문 접근 |

**구성**:
```cpp
#include <pacs/storage/hsm_storage.hpp>
#include <pacs/storage/hsm_types.hpp>

using namespace pacs::storage;

// 티어 정책 구성
tier_policy hot_policy;
hot_policy.tier = storage_tier::hot;
hot_policy.migration_age = std::chrono::days{30};   // 30일 후 마이그레이션

tier_policy warm_policy;
warm_policy.tier = storage_tier::warm;
warm_policy.migration_age = std::chrono::days{180}; // 180일 후 마이그레이션

tier_policy cold_policy;
cold_policy.tier = storage_tier::cold;
// cold 티어에서는 마이그레이션 없음 (최종 목적지)

// 백엔드로 HSM 저장소 생성
auto hot_backend = std::make_shared<file_storage>(hot_path);
auto warm_backend = std::make_shared<s3_storage>(warm_config);
auto cold_backend = std::make_shared<azure_blob_storage>(cold_config);

hsm_storage storage{
    hot_backend, hot_policy,
    warm_backend, warm_policy,
    cold_backend, cold_policy
};
```

**사용 예제**:
```cpp
// 저장은 항상 hot 티어로
core::dicom_dataset ds;
// ... 데이터세트 채우기 ...
auto store_result = storage.store(ds);

// 검색은 투명하게 모든 티어 검색
auto retrieve_result = storage.retrieve("1.2.3.4.5.6.7.8.9");
if (retrieve_result.is_ok()) {
    auto& dataset = retrieve_result.value();
    std::cout << "환자: " << dataset.get_string(tags::patient_name) << "\n";
}

// 인스턴스가 어느 티어에 있는지 확인
auto tier_result = storage.get_tier("1.2.3.4.5.6.7.8.9");
if (tier_result.is_ok()) {
    std::cout << "저장 위치: " << to_string(tier_result.value()) << "\n";
}

// 특정 티어로 수동 마이그레이션
storage.migrate("1.2.3.4.5.6.7.8.9", storage_tier::cold);

// 저장소 통계 가져오기
auto stats = storage.get_statistics();
std::cout << "Hot 티어: " << stats.hot_count << " 인스턴스\n";
std::cout << "Warm 티어: " << stats.warm_count << " 인스턴스\n";
std::cout << "Cold 티어: " << stats.cold_count << " 인스턴스\n";
```

**백그라운드 마이그레이션 서비스**:
```cpp
#include <pacs/storage/hsm_migration_service.hpp>

// 마이그레이션 서비스 구성
migration_service_config config;
config.migration_interval = std::chrono::hours{1};  // 매시간 실행
config.max_concurrent_migrations = 4;
config.auto_start = true;

config.on_cycle_complete = [](const migration_result& r) {
    std::cout << r.instances_migrated << " 인스턴스 마이그레이션됨\n";
    std::cout << "전송된 바이트: " << r.bytes_migrated << "\n";
};

config.on_migration_error = [](const std::string& uid, const std::string& err) {
    std::cerr << uid << " 마이그레이션 실패: " << err << "\n";
};

// 마이그레이션 서비스 생성 및 시작
hsm_migration_service service{storage, config};
service.start();

// 진행 상황 모니터링
auto time_left = service.time_until_next_cycle();
std::cout << "다음 사이클까지: " << time_left->count() << " 초\n";

// 즉시 마이그레이션 트리거
service.trigger_cycle();

// 통계 가져오기
auto stats = service.get_cumulative_stats();
std::cout << "총 마이그레이션: " << stats.instances_migrated << "\n";

// 정상 종료
service.stop();
```

---

## 에코시스템 통합

### SIMD 최적화

**구현**: 성능 중요 작업을 위한 플랫폼별 SIMD 가속.

**기능**:
- 자동 CPU 기능 감지 (x86의 SSE2/SSSE3/AVX2/AVX-512, ARM의 NEON)
- 최적 SIMD 경로로 런타임 디스패치
- SIMD 미지원 시 스칼라 구현으로 폴백
- 엔디언 변환을 위한 제로카피 바이트 스왑

**최적화된 연산**:

| 연산 | SIMD 지원 | 속도 향상 |
|------|-----------|----------|
| 바이트 스왑 (16비트) | AVX2, SSSE3, NEON | 8-16배 |
| 바이트 스왑 (32비트) | AVX2, SSSE3, NEON | 8-16배 |
| 바이트 스왑 (64비트) | AVX2, SSSE3, NEON | 8-16배 |

**사용법**:
```cpp
#include <pacs/encoding/byte_swap.hpp>
#include <pacs/encoding/simd/simd_config.hpp>

using namespace pacs::encoding;

// 대용량 바이트 스왑을 위한 자동 SIMD 디스패치
auto swapped = swap_ow_bytes(pixel_data);  // 최적 SIMD 사용

// 사용 가능한 SIMD 기능 확인
auto features = simd::get_features();
if (simd::has_avx2()) {
    std::cout << "AVX2 사용 가능: 32바이트 벡터 연산\n";
}

// 현재 CPU의 최적 벡터 너비 조회
size_t width = simd::optimal_vector_width();  // 16, 32, 또는 64 바이트
```

**클래스**:
- `simd_config.hpp` - CPU 기능 감지
- `simd_types.hpp` - 이식 가능한 SIMD 타입 래퍼
- `simd_utils.hpp` - SIMD 유틸리티 함수

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

**ILogger 인터페이스 (Issue #309)**:

모든 DICOM 서비스는 `ILogger` 인터페이스를 통한 주입 가능한 로깅을 지원합니다:

| 컴포넌트 | 로거 지원 |
|---------|----------|
| 모든 SCP 서비스 | 생성자/설정자 주입 |
| storage_scu | 생성자 주입 |
| scp_service 기본 클래스 | Protected 로거 멤버 |

**장점**:
- 모의 로거로 테스트 가능한 서비스
- 기본 무출력 동작 (NullLogger)
- 런타임 로거 전환
- DI 컨테이너 통합

**예시**:
```cpp
// 기본: 무출력 (NullLogger)
verification_scp scp;

// LoggerService로 로깅
auto logger = std::make_shared<pacs::di::LoggerService>();
verification_scp scp_with_logging(logger);

// DI 컨테이너 통해
auto container = pacs::di::create_container();
auto logger = container->resolve<pacs::di::ILogger>().value();
storage_scp scp_via_di(logger);
```

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

# 오브젝트 풀 메트릭
pacs_pool_element_hits_total{}
pacs_pool_element_misses_total{}
pacs_pool_dataset_hits_total{}
pacs_pool_dataset_misses_total{}
pacs_pool_pdu_buffer_hits_total{}
pacs_pool_pdu_buffer_misses_total{}
```

### DICOM 메트릭 수집기

**구현**: 외부 모니터링 시스템과의 통합을 위한 플러그인 호환 인터페이스 패턴을 따르는 모듈형 메트릭 수집기.

**수집기**:
| 수집기 | 설명 | 메트릭 |
|--------|------|--------|
| `dicom_association_collector` | 연결 수명주기 메트릭 | 활성, 최대, 수립, 거부, 중단, 성공률 |
| `dicom_service_collector` | DIMSE 작업 메트릭 | 요청, 성공/실패, 작업별 지속 시간 통계 |
| `dicom_storage_collector` | 저장 및 전송 메트릭 | 송수신 바이트, 저장/검색된 이미지, 처리량 |

**기능**:
- 원자적 카운터를 사용한 스레드 안전 수집
- Prometheus 텍스트 노출 형식 지원
- REST API 통합을 위한 JSON 내보내기
- 작업별 메트릭 수집 설정 가능
- 오브젝트 풀 모니터링 (요소, 데이터셋, PDU 버퍼 풀)

**사용법**:
{% raw %}
```cpp
#include <pacs/monitoring/pacs_monitor.hpp>
using namespace pacs::monitoring;

// 전역 모니터 인스턴스 획득
auto& monitor = pacs_monitor::global_monitor();
monitor.initialize({{"ae_title", "PACS_SCP"}});

// 모든 메트릭 수집
auto snapshot = monitor.get_metrics();
for (const auto& m : snapshot.association_metrics) {
    std::cout << m.name << ": " << m.value << "\n";
}

// Prometheus 형식으로 내보내기
std::string prometheus_output = monitor.to_prometheus();

// 커스텀 상태 확인 등록
monitor.register_health_check("database", []() {
    return database_is_healthy();
});

// 전체 상태 확인
bool healthy = monitor.is_healthy();
```
{% endraw %}

**IMonitor와의 통합**:
`pacs_monitor` 클래스는 `common_system`의 `IMonitor`와 동일한 인터페이스 패턴을 따르므로, 모니터링 인프라와 원활하게 통합됩니다:

```cpp
// 커스텀 메트릭 기록
monitor.record_metric("custom_gauge", 42.0);

// 타이밍 포함 상태 확인
auto result = monitor.check_health("database");
std::cout << "Database: " << (result.healthy ? "OK" : "FAIL")
          << " (" << result.latency.count() << "ms)\n";
```

### 오브젝트 풀 메모리 관리

**목적**: 자주 사용되는 DICOM 객체의 할당 오버헤드와 메모리 단편화 감소.

**구현**: `common_system`의 `ObjectPool`을 사용하며, RAII 기반 자동 풀 반환.

**풀링 대상**:
| 객체 타입 | 풀 크기 | 사용처 |
|----------|---------|--------|
| `dicom_element` | 1024 | 태그 파싱, 데이터 조작 |
| `dicom_dataset` | 128 | 데이터셋 구성, 조회 결과 |
| `pooled_buffer` | 256 | 네트워크 PDU 인코딩/디코딩 |
| `presentation_data_value` | 128 | P-DATA-TF 메시지 처리 |

**사용법**:
```cpp
#include <pacs/core/pool_manager.hpp>
#include <pacs/network/pdu_buffer_pool.hpp>

// 풀링된 DICOM 요소 획득
auto elem = make_pooled_element(tags::patient_name, vr_type::PN, "DOE^JOHN");

// 풀링된 데이터셋 획득
auto dataset = make_pooled_dataset();
dataset->set_string(tags::patient_id, vr_type::LO, "12345");

// 풀링된 PDU 버퍼 획득
auto buffer = make_pooled_pdu_buffer(16384);

// 객체 소멸 시 자동으로 풀에 반환
```

**성능 이점**:
- 할당 지연 시간 ~90% 감소
- 메모리 단편화 감소
- 캐시 지역성 향상
- GC 압력 감소

**모니터링**:
```cpp
auto& stats = pool_manager::get().element_statistics();
double hit_ratio = stats.hit_ratio();  // 0.0 - 1.0
```

### 분산 추적

**스팬 타입**:
- 연결 라이프사이클
- DIMSE 연산
- 저장 연산
- 데이터베이스 조회

---

## 워크플로우 서비스

### 자동 사전 검색 서비스

**구현**: 환자가 모달리티 워크리스트에 나타날 때 원격 PACS에서 이전 환자 검사를 자동으로 사전 검색하는 백그라운드 서비스.

**기능**:
- 워크리스트 트리거 사전 검색: MWL에 환자가 나타나면 자동으로 사전 검색 요청 큐에 추가
- 구성 가능한 선택 기준: 모달리티, 신체 부위, 검색 기간으로 이전 검사 필터링
- 다중 소스 지원: 여러 원격 PACS 서버에서 사전 검색 가능
- 병렬 처리: thread_pool을 사용한 동시 사전 검색 작업
- 속도 제한: 원격 PACS 과부하 방지
- 재시도 로직: 구성 가능한 지연으로 실패한 사전 검색 자동 재시도
- 중복 제거: 동일 환자에 대한 중복 사전 검색 요청 방지

**클래스**:
- `auto_prefetch_service` - 백그라운드 사전 검색을 위한 메인 서비스 클래스
- `prefetch_service_config` - 서비스 구성 옵션
- `prefetch_criteria` - 이전 검사 선택 기준
- `prefetch_result` - 사전 검색 작업 통계
- `prior_study_info` - 이전 검사 후보 정보
- `prefetch_request` - 환자 이전 검사 사전 검색 요청

**구성 옵션**:

| 옵션 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| `enabled` | bool | true | 사전 검색 서비스 활성화/비활성화 |
| `prefetch_interval` | seconds | 300 | 사전 검색 주기 간격 |
| `max_concurrent_prefetches` | size_t | 4 | 최대 병렬 사전 검색 작업 수 |
| `lookback_period` | days | 365 | 이전 검사 검색 기간 |
| `max_studies_per_patient` | size_t | 10 | 환자당 최대 이전 검사 수 |
| `prefer_same_modality` | bool | true | 동일 모달리티 이전 검사 우선 |
| `rate_limit_per_minute` | size_t | 0 | 분당 최대 사전 검색 수 (0=무제한) |

**예제**:
```cpp
#include <pacs/workflow/auto_prefetch_service.hpp>
#include <pacs/workflow/prefetch_config.hpp>

using namespace pacs::workflow;

// 사전 검색 서비스 구성
prefetch_service_config config;
config.prefetch_interval = std::chrono::minutes{5};
config.max_concurrent_prefetches = 4;
config.criteria.lookback_period = std::chrono::days{365};
config.criteria.max_studies_per_patient = 10;
config.criteria.prefer_same_modality = true;

// 원격 PACS 소스 추가
remote_pacs_config remote;
remote.ae_title = "ARCHIVE_PACS";
remote.host = "192.168.1.100";
remote.port = 11112;
config.remote_pacs.push_back(remote);

// 콜백 설정
config.on_cycle_complete = [](const prefetch_result& result) {
    std::cout << result.patients_processed << "명 환자에 대해 "
              << result.studies_prefetched << "개 검사 사전 검색 완료\n";
};

// 서비스 생성 및 시작
auto_prefetch_service service{database, config};
service.start();

// 워크리스트 항목에 대한 사전 검색 트리거
service.trigger_for_worklist(worklist_items);

// 특정 환자에 대한 수동 사전 검색
auto result = service.prefetch_priors("PATIENT123", std::chrono::days{180});

// 통계 모니터링
auto stats = service.get_cumulative_stats();
std::cout << "총 사전 검색: " << stats.studies_prefetched << "\n";

// 서비스 중지
service.stop();
```

**통합 포인트**:
- **MWL SCP**: 워크리스트 쿼리 완료 시 자동 알림
- **index_database**: 사전 검색 시작 전 로컬 저장소 확인
- **thread_pool**: 병렬 C-MOVE 작업
- **logger_adapter**: 사전 검색 작업 감사 로깅
- **monitoring_adapter**: 사전 검색 성공/실패율 메트릭

### 태스크 스케줄러 서비스

**구현**: 정리, 아카이브, 데이터 검증을 포함한 자동화된 유지보수 작업을 스케줄링하고 실행하는 백그라운드 서비스.

**기능**:
- 유연한 스케줄링: 인터벌 기반, cron 표현식, 일회성 실행
- 내장 태스크 타입: 정리, 아카이브, 검증, 사용자 정의
- 태스크 수명주기 관리: 일시정지, 재개, 취소, 즉시 실행
- 구성 가능한 보존 기간을 갖춘 실행 이력 추적
- 재시작 복구를 위한 태스크 영속성
- 통계 및 모니터링 기능
- 구성 가능한 제한을 갖춘 동시 실행
- **재시도 메커니즘**: 실패 시 구성 가능한 시도 횟수와 지연으로 자동 재시도
- **타임아웃 처리**: 비동기 실행 지원을 갖춘 태스크별 실행 타임아웃

**클래스**:
- `task_scheduler` - 스케줄링 및 실행을 위한 메인 서비스 클래스
- `task_scheduler_config` - 서비스 구성 옵션
- `cleanup_config` - 저장소 정리 태스크 구성
- `archive_config` - 검사 아카이브 태스크 구성
- `verification_config` - 데이터 무결성 검증 구성
- `scheduled_task` - 스케줄과 콜백을 갖춘 태스크 정의
- `cron_schedule` - Cron 스타일 스케줄 표현식

**스케줄 타입**:

| 타입 | 설명 | 예시 |
|------|------|------|
| `interval_schedule` | 실행 간 고정 간격 | 매 1시간 |
| `cron_schedule` | 분/시/일을 갖춘 cron 스타일 스케줄 | 매일 오전 2:00 |
| `one_time_schedule` | 특정 시간에 단일 실행 | 2025-12-15 03:00 |

**태스크 타입**:

| 타입 | 목적 | 구성 |
|------|------|------|
| Cleanup | 보존 정책에 따라 오래된 검사 삭제 | `cleanup_config` |
| Archive | 검사를 아카이브 저장소로 이동 | `archive_config` |
| Verification | 데이터 무결성 검사 (체크섬, DB 일관성) | `verification_config` |
| Custom | 사용자 정의 유지보수 태스크 | 사용자 정의 콜백 |

**구성 옵션**:

| 옵션 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| `enabled` | bool | true | 스케줄러 활성화/비활성화 |
| `auto_start` | bool | false | 생성 시 자동 시작 |
| `max_concurrent_tasks` | size_t | 4 | 최대 병렬 태스크 실행 수 |
| `check_interval` | seconds | 60 | 예정된 태스크 확인 간격 |
| `persistence_path` | string | "" | 태스크 영속성 경로 (빈 값=비활성화) |

**태스크 옵션 (scheduled_task별)**:

| 옵션 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| `timeout` | seconds | 0 | 최대 실행 시간 (0=제한 없음) |
| `max_retries` | size_t | 0 | 실패 시 재시도 횟수 |
| `retry_delay` | seconds | 60 | 재시도 간 지연 시간 |
| `priority` | int | 0 | 태스크 우선순위 (높을수록 중요) |
| `enabled` | bool | true | 개별 태스크 활성화/비활성화 |

**예제**:
```cpp
#include <pacs/workflow/task_scheduler.hpp>
#include <pacs/workflow/task_scheduler_config.hpp>

using namespace pacs::workflow;

// 스케줄러 구성
task_scheduler_config config;
config.max_concurrent_tasks = 4;
config.check_interval = std::chrono::seconds{60};

// 정리 태스크 구성
cleanup_config cleanup;
cleanup.default_retention = std::chrono::days{365};
cleanup.modality_retention["CR"] = std::chrono::days{180};
cleanup.modality_retention["CT"] = std::chrono::days{730};
cleanup.cleanup_schedule = cron_schedule::daily_at(2, 0);  // 오전 2:00
config.cleanup = cleanup;

// 아카이브 태스크 구성
archive_config archive;
archive.archive_after = std::chrono::days{90};
archive.destination_type = archive_destination_type::cloud_s3;
archive.destination = "s3://bucket/archive";
archive.verify_after_archive = true;
archive.archive_schedule = cron_schedule::daily_at(3, 0);  // 오전 3:00
config.archive = archive;

// 검증 태스크 구성
verification_config verification;
verification.check_checksums = true;
verification.check_db_consistency = true;
verification.verification_schedule = cron_schedule::weekly_on(0, 4, 0);  // 일요일 오전 4:00
config.verification = verification;

// 콜백 설정
config.on_task_complete = [](const task_id& id, const task_execution_record& record) {
    std::cout << "태스크 " << id << " 완료: "
              << record.duration()->count() << "ms\n";
};

// 스케줄러 생성 및 시작
task_scheduler scheduler{database, config};
scheduler.start();

// 사용자 정의 태스크 추가
auto custom_id = scheduler.schedule(
    "daily-backup",
    "데이터베이스 백업",
    cron_schedule::daily_at(1, 0),  // 오전 1:00
    []() -> std::optional<std::string> {
        // 백업 수행
        return std::nullopt;  // 성공
    }
);

// 태스크 관리
scheduler.pause_task(custom_id);
scheduler.resume_task(custom_id);
scheduler.trigger_task(custom_id);  // 즉시 실행

// 태스크 정보 조회
auto task = scheduler.get_task(custom_id);
if (task) {
    std::cout << "태스크: " << task->name << "\n";
    std::cout << "상태: " << to_string(task->state) << "\n";
}

// 통계 조회
auto stats = scheduler.get_stats();
std::cout << "스케줄된 태스크: " << stats.scheduled_tasks << "\n";
std::cout << "실행 중: " << stats.running_tasks << "\n";

// 정상 종료
scheduler.stop(true);  // 실행 중인 태스크 대기
```

**통합 포인트**:
- **index_database**: 정리/아카이브 대상 검사 쿼리
- **file_storage**: DICOM 파일 삭제 또는 아카이브
- **thread_pool**: 병렬 태스크 실행
- **logger_adapter**: 모든 태스크 작업 감사 로깅
- **monitoring_adapter**: 태스크 성공/실패율 메트릭

### 스터디 락 관리자

**구현**: 수정, 마이그레이션 및 배타적 접근이 필요한 작업 중 DICOM 스터디에 대한 동시 접근을 제어하는 스레드 안전 락 관리자.

**기능**:
- **배타적 락**: 수정 중 스터디에 대한 모든 다른 접근 차단
- **공유 락**: 배타적 락을 차단하면서 동시 읽기 접근 허용
- **마이그레이션 락**: 마이그레이션 작업을 위한 최고 우선순위 락
- **자동 만료**: 구성된 타임아웃 후 락 자동 만료
- **토큰 기반 해제**: 고유 토큰을 사용한 안전한 락 해제
- **강제 해제**: 관리자가 강제로 락 해제 가능
- **통계 추적**: 락 획득, 경합, 지속 시간 메트릭
- **이벤트 콜백**: 락 획득, 해제, 만료 알림

**클래스**:
- `study_lock_manager` - 스터디 락 관리 메인 클래스
- `study_lock_manager_config` - 설정 옵션
- `lock_token` - 보유한 락을 나타내는 고유 토큰
- `lock_info` - 락에 대한 상세 정보
- `lock_type` - 락 유형 열거형 (exclusive, shared, migration)
- `lock_manager_stats` - 락 작업 통계

**락 유형**:
| 유형 | 설명 | 사용 사례 |
|------|------|----------|
| `exclusive` | 다른 접근 불허 | 스터디 수정, 삭제 |
| `shared` | 읽기 전용 접근 허용 | 동시 읽기 작업 |
| `migration` | 최고 우선순위 락 | HSM 계층 마이그레이션 |

**예제**:
```cpp
#include <pacs/workflow/study_lock_manager.hpp>

using namespace pacs::workflow;

// 락 관리자 구성
study_lock_manager_config config;
config.default_timeout = std::chrono::minutes{30};
config.max_shared_locks = 100;
config.allow_force_unlock = true;

study_lock_manager manager{config};

// 수정을 위한 배타적 락 획득
auto result = manager.lock("1.2.3.4.5", "스터디 업데이트", "user123");
if (result.is_ok()) {
    auto token = result.value();

    // 수정 수행...

    // 락 해제
    manager.unlock(token);
}

// 읽기를 위한 공유 락 획득
auto shared_result = manager.lock(
    "1.2.3.4.5",
    lock_type::shared,
    "읽기 접근",
    "viewer"
);

// 스터디가 락되어 있는지 확인
if (manager.is_locked("1.2.3.4.5")) {
    auto info = manager.get_lock_info("1.2.3.4.5");
    if (info) {
        std::cout << "락 보유자: " << info->holder << "\n";
        std::cout << "지속 시간: " << info->duration().count() << "ms\n";
    }
}

// 강제 해제 (관리자 작업)
manager.force_unlock("1.2.3.4.5", "긴급 해제");

// 락 통계 조회
auto stats = manager.get_stats();
std::cout << "활성 락: " << stats.active_locks << "\n";
std::cout << "경합: " << stats.contention_count << "\n";
```

**설정 옵션**:
| 옵션 | 유형 | 기본값 | 설명 |
|------|------|--------|------|
| `default_timeout` | seconds | 0 | 기본 락 타임아웃 (0=무제한) |
| `acquire_wait_timeout` | milliseconds | 5000 | 락 대기 최대 시간 |
| `cleanup_interval` | seconds | 60 | 만료된 락 정리 간격 |
| `auto_cleanup` | bool | true | 자동 만료 락 정리 활성화 |
| `max_shared_locks` | size_t | 100 | 최대 동시 공유 락 수 |
| `allow_force_unlock` | bool | true | 관리자 강제 해제 허용 |

**통합 포인트**:
- **thread_system**: shared_mutex를 통한 스레드 안전 작업
- **common_system**: 에러 처리를 위한 Result<T> 패턴
- **logger_adapter**: 락 작업 감사 로깅
- **monitoring_adapter**: 락 경합 및 지속 시간 메트릭

---

## 에러 처리

### Result<T> 패턴

**구현**: 예외 기반 에러 처리를 대체하는 common_system의 `Result<T>` 패턴을 사용한 통합 에러 처리.

**기능**:
- 예외 없는 타입 안전 에러 전파
- 코드, 메시지, 모듈, 세부 정보를 포함하는 풍부한 에러 정보
- 표준화된 PACS 에러 코드 (-700 ~ -799 범위)
- 에러 변환을 위한 모나딕 연산 (`map`, `and_then`, `or_else`)
- 일반적인 패턴을 위한 편의 매크로

**에러 코드 범주**:

| 범위 | 범주 | 설명 |
|------|------|------|
| -700 ~ -719 | DICOM 파일 | 파일 작업, DICM 접두사, 메타 정보 |
| -720 ~ -739 | DICOM 요소 | 요소 접근, 값 변환 |
| -740 ~ -759 | 인코딩 | 인코딩, 디코딩, 압축 |
| -760 ~ -779 | 네트워크 | Association, DIMSE, PDU |
| -780 ~ -799 | 스토리지 | 저장, 검색, 조회 작업 |

**클래스 및 타입**:
- `pacs::Result<T>` - Result 타입 별칭
- `pacs::VoidResult` - void 작업용 Result
- `pacs::error_info` - 에러 정보 구조체
- `pacs::error_codes` - 표준화된 에러 코드

**예제**:
```cpp
#include <pacs/core/result.hpp>
#include <pacs/core/dicom_file.hpp>

using namespace pacs::core;

// Result<T>를 사용한 DICOM 파일 읽기
auto result = dicom_file::open("image.dcm");
if (result.is_ok()) {
    auto& file = result.value();
    std::cout << "SOP Class: " << file.sop_class_uid() << "\n";
} else {
    const auto& err = result.error();
    std::cerr << "에러 " << err.code << ": " << err.message << "\n";
}

// 모나딕 연산 사용
auto sop_uid = dicom_file::open("image.dcm")
    .map([](dicom_file& f) { return f.sop_instance_uid(); })
    .unwrap_or("unknown");

// PACS_ASSIGN_OR_RETURN 매크로 사용
pacs::Result<std::string> get_patient_name(const std::filesystem::path& path) {
    PACS_ASSIGN_OR_RETURN(auto file, dicom_file::open(path));
    return pacs::Result<std::string>::ok(
        file.dataset().get_string(tags::patient_name));
}
```

---

## 계획된 기능

### 단기 (다음 릴리스)

| 기능 | 설명 | 목표 |
|------|------|------|
| C-GET | 대체 검색 방법 | 3단계 |
| ~~추가 SOP 클래스 (PET/NM)~~ | ~~NM, PET 지원~~ | ✅ 완료 |
| ~~추가 SOP 클래스 (RT)~~ | ~~RT 계획, RT 구조 세트 등~~ | ✅ 완료 |
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
| 클라우드 저장소 (전체 SDK) | 프로덕션 S3/Azure SDK 통합 | 향후 |
| FHIR 통합 | 의료 상호운용성 | 향후 |

---

## 최근 완료된 기능 (v1.2.0 - 2025-12-13)

| 기능 | 설명 | 이슈 | 상태 |
|------|------|------|------|
| 태스크 스케줄러 서비스 | 정리, 아카이브, 검증을 위한 자동화된 태스크 스케줄링 | #207 | ✅ 완료 |
| 자동 사전 검색 서비스 | 워크리스트 쿼리 기반 이전 검사 자동 사전 검색 | #206 | ✅ 완료 |
| 계층적 저장소 관리 (HSM) | 자동 연령 기반 마이그레이션을 갖춘 3계층 HSM | #200 | ✅ 완료 |
| Azure Blob 저장소 | Azure Blob 저장소 백엔드 (목 구현) with 블록 블롭 업로드 | #199 | ✅ 완료 |
| S3 클라우드 저장소 | S3 호환 저장소 백엔드 (목 구현) | #198 | ✅ 완료 |
| SEG/SR 지원 | AI/CAD를 위한 세그먼테이션 및 구조화된 보고서 | #187 | ✅ 완료 |
| RT 모달리티 지원 | RT Plan, RT Dose, RT Structure Set | #186 | ✅ 완료 |
| REST API 서버 | Crow 프레임워크 기반 웹 관리 API | #194 | ✅ 완료 |

---

*문서 버전: 0.2.0.0*
*작성일: 2025-11-30*
*수정일: 2025-12-13*
*작성자: kcenon@naver.com*
