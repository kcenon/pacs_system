---
doc_id: "PAC-FEAT-001"
doc_title: "PACS 시스템 기능"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "FEAT"
---

# PACS 시스템 기능

> **SSOT**: This document is the single source of truth for **PACS 시스템 기능**.

> **버전:** 0.2.0.0
> **최종 수정일:** 2026-02-09
> **언어:** [English](FEATURES.md) | **한국어**

이 문서는 PACS 시스템에서 사용 가능한 모든 기능에 대한 포괄적인 세부 정보를 제공합니다.

> **단계 정의**: 공식 단계 정의는 [PRD Development Phases](PRD.md#development-phases)를 참조하세요. 아래 기능 섹션은 다음과 같이 단계에 대응됩니다:
> - **Phase 1**: DICOM 코어 기능
> - **Phase 2**: 네트워크 프로토콜 기능, 압축 코덱
> - **Phase 3**: DICOM 서비스, 저장 백엔드
> - **Phase 4**: 보안, 모니터링, 워크플로우, 에코시스템 통합 (고급), 에러 처리
> - **Phase 5** (계획됨): 엔터프라이즈 기능 — 향상된 메트릭 ([계획된 기능](#계획된-기능) 참조)
>
> **출시 상태**: Phase 1-4에 나열된 컴포넌트는 `v0.1.0` 태그에 포함되어 있습니다. 이 문서의 기능 설명은
> 그 태그 이전에 작성되었으며, 이후 기본 브랜치의 작업(예: `include/kcenon/pacs/ihe/xds/` 아래의 IHE XDS.b
> 액터, `include/kcenon/pacs/` 헤더 구조, `pacs_system::pacs_system` CMake 계약)은 다루지 않습니다. 이 작업은
> 미출시 상태입니다. 코드 예제는 기본 브랜치의 헤더와 `kcenon::pacs::` 네임스페이스를 사용하므로, 헤더가
> `include/pacs/` 아래에 있는 `v0.1.0` 태그에서는 빌드되지 않습니다.
>
> **참고**: 연결 풀링은 원래 Phase 5 기능으로 나열되었지만 클라이언트 모듈(`remote_node_manager`)에 이미 완전히 구현되어 있습니다. 자세한 내용은 [원격 노드 관리자](#원격-노드-관리자-연결-풀링)를 참조하세요.

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

**구현**: 34개의 모든 Value Representation(VR)을 지원하는 완전한 DICOM Data Element 파서 및 인코더.

**기능**:
- DICOM 데이터 요소 파싱 및 인코딩 (태그, VR, 길이, 값)
- 34개 표준 VR 타입 모두 지원 (PS3.5, DICOM 2019b 포함)
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
#include <kcenon/pacs/core/dicom_element.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>

using namespace kcenon::pacs::core;
using kcenon::pacs::encoding::vr_type;

// 환자 이름 요소 생성
auto patient_name = dicom_element::from_string(
    tags::patient_name,          // (0010,0010)
    vr_type::PN,                 // Person Name
    "Doe^John"                   // 값
);

// 요소 속성 접근
auto tag = patient_name.tag();           // (0010,0010)
auto vr = patient_name.vr();             // vr_type::PN
auto value = patient_name.as_string();   // "Doe^John"을 담은 Result<std::string>
```

### Value Representation 타입

container_system과의 적절한 매핑으로 34개의 모든 DICOM VR 타입이 지원됩니다:

| 카테고리 | VR 타입 | 설명 |
|----------|---------|------|
| **문자열** | AE, AS, CS, DA, DS, DT, IS, LO, LT, PN, SH, ST, TM, UC, UI, UR, UT | 텍스트 기반 값 |
| **숫자** | FL, FD, SL, SS, UL, US | 숫자 값 |
| **바이너리** | OB, OD, OF, OL, OV, OW, UN | 바이너리 데이터 |
| **특수** | AT, SQ, SV, UV | 태그, 시퀀스 및 64비트 정수 |

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
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>

using namespace kcenon::pacs::core;
using kcenon::pacs::encoding::vr_type;

// 데이터 세트 생성
dicom_dataset dataset;

// 요소 추가
dataset.set_string(tags::patient_name, vr_type::PN, "Doe^John");
dataset.set_string(tags::patient_id, vr_type::LO, "12345");
dataset.set_string(tags::study_date, vr_type::DA, "20250130");
dataset.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
dataset.set_numeric<uint16_t>(tags::columns, vr_type::US, 512);

// 값 가져오기
auto name = dataset.get_string(tags::patient_name);
auto rows = dataset.get_numeric<uint16_t>(tags::rows).value_or(0);

// 존재 여부 확인
if (dataset.contains(tags::pixel_data)) {
    auto pixels = dataset.get(tags::pixel_data)->raw_data();
}

// 모든 요소 순회 (태그 순서)
for (const auto& [tag, element] : dataset) {
    std::cout << tag.to_string() << ": " << element.as_string().unwrap_or("") << "\n";
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
#include <kcenon/pacs/core/dicom_file.h>

using namespace kcenon::pacs::core;
using kcenon::pacs::encoding::transfer_syntax;
using kcenon::pacs::encoding::vr_type;

// DICOM 파일 읽기
auto result = dicom_file::open("/path/to/image.dcm");
if (result.is_ok()) {
    auto& file = result.value();
    auto& dataset = file.dataset();
    auto ts = file.transfer_syntax();

    std::cout << "환자: " << dataset.get_string(tags::patient_name) << "\n";
    std::cout << "Transfer Syntax: " << ts.uid() << "\n";
}

// DICOM 파일 쓰기 (파일 메타 정보는 데이터 세트에서 생성됨)
dicom_dataset dataset;
dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
dataset.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
auto file = dicom_file::create(std::move(dataset), transfer_syntax::explicit_vr_little_endian);

auto write_result = file.save("/path/to/output.dcm");
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
#include <kcenon/pacs/core/dicom_file.h>

using namespace kcenon::pacs::core;

// Undefined length 시퀀스가 있는 DICOM 파일 읽기
auto result = dicom_file::open("compressed_image.dcm");
if (result.is_ok()) {
    auto& file = result.value();

    // Undefined length 시퀀스 접근
    if (file.dataset().has_sequence(tags::referenced_sop_sequence)) {
        const auto* seq = file.dataset().get_sequence(tags::referenced_sop_sequence);
        for (const auto& item : *seq) {
            std::cout << "참조 SOP Instance UID: "
                      << item.get_string(tags::referenced_sop_instance_uid) << "\n";
        }
    }

    // 캡슐화된 픽셀 데이터 접근
    if (const auto* pixel_elem = file.dataset().get(tags::pixel_data)) {
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
- 13개 상태 연결 상태 머신 (PS3.8 Sta1-Sta13)
- 프레젠테이션 컨텍스트 협상
- Abstract Syntax 및 Transfer Syntax 매칭
- 최대 PDU 크기 협상
- 다중 동시 연결
- 연결 타임아웃 처리
- 확장 협상 지원

**연결 상태**:
```
┌──────────────────────────────────────────────────────────┐
│            연결 상태 머신 (PS3.8 Sta1-Sta13)               │
├──────────────────────────────────────────────────────────┤
│  Sta1  (유휴)                                             │
│  Sta2  (전송 연결 열림 - 개시자)                           │
│  Sta3  (로컬 A-ASSOCIATE 응답 대기)                        │
│  Sta4  (전송 연결 열림 대기 - 수락자)                      │
│  Sta5  (A-ASSOCIATE-AC/RJ 대기)                           │
│  Sta6  (연결 수립됨)                                       │
│  Sta7  (A-RELEASE-RP 대기)                                │
│  Sta8  (A-RELEASE-RP 대기 - 해제 충돌)                     │
│  Sta9  (해제 충돌 요청자 측)                               │
│  Sta10 (해제 충돌 수락자 측)                               │
│  Sta11 (해제 충돌 요청자 - RP 대기)                        │
│  Sta12 (해제 충돌 수락자 - RP 대기)                        │
│  Sta13 (전송 연결 종료 대기 - 오류)                        │
│                                                          │
│  기본 흐름: Sta1 → Sta4 → Sta5 → Sta6 → Sta7 → Sta1      │
└──────────────────────────────────────────────────────────┘
```

**예제**:
```cpp
#include <kcenon/pacs/network/association.h>
#include <kcenon/pacs/services/sop_classes/ct_storage.h>

using namespace kcenon::pacs::network;
using kcenon::pacs::services::sop_classes::ct_image_storage_uid;

// 연결 구성
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.max_pdu_length = 16384;

// 프레젠테이션 컨텍스트 추가 (컨텍스트 ID는 홀수)
config.proposed_contexts.push_back({
    1,
    std::string(ct_image_storage_uid),
    {"1.2.840.10008.1.2.1"}  // Explicit VR Little Endian
});

// 연결 생성
auto result = association::connect("192.168.1.100", 11112, config);
if (result.is_ok()) {
    auto& assoc = result.value();

    // DIMSE 연산 수행...

    auto release_result = assoc.release();  // 정상 해제
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

SCP 측은 `dicom_server`에 등록하는 `verification_scp`입니다. 별도의 검증 SCU 클래스는 없으며,
SCU는 `tools/echo_scu`처럼 `association`을 통해 C-ECHO-RQ를 보냅니다.

**예제**:
```cpp
#include <kcenon/pacs/network/association.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/services/verification_scp.h>

using namespace kcenon::pacs::network;
using kcenon::pacs::services::verification_sop_class_uid;

// Verification SOP 클래스 제안
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.proposed_contexts.push_back({
    1, std::string(verification_sop_class_uid), {"1.2.840.10008.1.2.1"}});

// 연결 테스트
auto result = association::connect("192.168.1.100", 11112, config);
if (result.is_ok()) {
    auto& assoc = result.value();
    if (auto context_id = assoc.accepted_context_id(verification_sop_class_uid)) {
        auto sent = assoc.send_dimse(*context_id, dimse::make_c_echo_rq(1));
        auto response = assoc.receive_dimse();
        if (sent.is_ok() && response.is_ok() &&
            response.value().second.status() == dimse::status_success) {
            std::cout << "DICOM Echo 성공!\n";
        }
    }
    auto release_result = assoc.release();
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
- 저장 커밋 (N-ACTION/N-EVENT-REPORT)

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
| Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.4 | ✅ |
| Surface Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.5 | ✅ |
| Basic Text SR Storage | 1.2.840.10008.5.1.4.1.1.88.11 | ✅ |
| Enhanced SR Storage | 1.2.840.10008.5.1.4.1.1.88.22 | ✅ |
| Comprehensive SR Storage | 1.2.840.10008.5.1.4.1.1.88.33 | ✅ |
| Comprehensive 3D SR Storage | 1.2.840.10008.5.1.4.1.1.88.34 | ✅ |
| Extensible SR Storage | 1.2.840.10008.5.1.4.1.1.88.35 | ✅ |
| Key Object Selection Document Storage | 1.2.840.10008.5.1.4.1.1.88.59 | ✅ |
| Mammography CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.50 | ✅ |
| Chest CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.65 | ✅ |
| Colon CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.69 | ✅ |
| X-Ray Radiation Dose SR Storage | 1.2.840.10008.5.1.4.1.1.88.67 | ✅ |

**예제**:
```cpp
#include <kcenon/pacs/network/dicom_server.h>
#include <kcenon/pacs/services/storage_scp.h>
#include <kcenon/pacs/storage/file_storage.h>

using namespace kcenon::pacs::network;
using namespace kcenon::pacs::services;
namespace core = kcenon::pacs::core;
namespace storage = kcenon::pacs::storage;

// 서버 구성 (AE 타이틀, 포트, 연결 수 제한)
server_config config;
config.ae_title = "PACS_SCP";
config.port = 11112;
config.max_associations = 10;

// 파일은 file_storage를 통해 /data/dicom에 저장
storage::file_storage_config storage_config;
storage_config.root_path = "/data/dicom";
storage::file_storage file_store{storage_config};

// Storage SCP 구성
auto storage_service = std::make_shared<storage_scp>(storage_scp_config{});
storage_service->set_handler(
    [&file_store](const core::dicom_dataset& dataset, const std::string& calling_ae,
                  const std::string& sop_class_uid, const std::string& sop_instance_uid) {
        std::cout << "수신됨: " << sop_instance_uid << "\n";
        return file_store.store(dataset).is_ok() ? storage_status::success
                                                 : storage_status::out_of_resources;
    });

// 서버 시작
dicom_server server{config};
server.register_service(storage_service);

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
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/services/query_scu.h>

using namespace kcenon::pacs::network;
using namespace kcenon::pacs::services;
using kcenon::pacs::encoding::vr_type;
namespace tags = kcenon::pacs::core::tags;

// Study Root C-FIND를 제안하는 연결 열기
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.proposed_contexts.push_back({
    1, std::string(study_root_find_sop_class_uid), {"1.2.840.10008.1.2.1"}});

auto assoc_result = association::connect("192.168.1.100", 11112, config);
if (assoc_result.is_ok()) {
    auto& assoc = assoc_result.value();

    // 조회 구성
    kcenon::pacs::core::dicom_dataset query;
    query.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
    query.set_string(tags::patient_name, vr_type::PN, "Doe*");
    query.set_string(tags::study_date, vr_type::DA, "20250101-20250130");
    query.set_string(tags::study_instance_uid, vr_type::UI, "");

    // 조회 실행
    query_scu scu;
    auto result = scu.find(assoc, query);
    if (result.is_ok()) {
        for (const auto& study : result.value().matches) {
            std::cout << "스터디: " << study.get_string(tags::study_instance_uid) << "\n";
        }
    }

    auto release_result = assoc.release();
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

### Storage Commitment Push Model 서비스

**구현**: DICOM PS3.4 Annex J에 따른 Storage Commitment Push Model SCP/SCU.

**기능**:
- 커밋 요청에 대한 N-ACTION 요청 처리
- 저장 백엔드에 대한 인스턴스 존재 확인
- 인스턴스별 성공/실패 상태를 담은 N-EVENT-REPORT
- 커밋 수명주기를 위한 Transaction UID 추적
- `commitment_repository`를 통한 커밋 레코드 영속화 (V8 데이터베이스 마이그레이션)
- 통계 추적 (처리된 액션, 커밋/실패한 인스턴스)

**프로토콜 흐름**:
```
SCU (모달리티)                       SCP (PACS)
 |                                   |
 |-- N-ACTION-RQ ------------------>|
 |   Transaction UID                 |
 |   Referenced SOP Sequence         |
 |<-- N-ACTION-RSP (Success) -------|
 |                                   |
 |   ... SCP가 저장 여부 확인 ...     |
 |                                   |
 |<-- N-EVENT-REPORT-RQ ------------|
 |   Transaction UID                 |
 |   Success/Failed Sequences        |
 |-- N-EVENT-REPORT-RSP ----------->|
```

**SOP 클래스**: Storage Commitment Push Model (1.2.840.10008.1.20.1)

**클래스**:
- `storage_commitment_scp` - N-ACTION 요청을 처리하는 SCP 서비스
- `storage_commitment_scu` - 커밋 요청을 보내는 SCU 클라이언트
- `storage_commitment_types` - 데이터 타입 (SOP 참조, 실패 사유, 트랜잭션 레코드)
- `commitment_repository` - 커밋 추적을 위한 데이터베이스 영속화

### 문자 집합 지원

**구현**: PS3.5 Section 6.1에 따른 DICOM 문자 집합 인코딩/디코딩.

**기능**:
- 모든 표준 문자 집합을 담은 DICOM 문자 집합 레지스트리
- 멀티바이트 문자 집합을 위한 ISO 2022 이스케이프 시퀀스 파서
- CJK (중국어, 일본어, 한국어) 문자 집합 디코딩
- iconv를 통한 ISO-2022-JP 상태 기반 인코딩/디코딩
- UTF-8 인코딩 지원
- Specific Character Set (0008,0005)로부터 문자 집합 자동 감지
- 데이터 세트와의 통합으로 투명한 문자 집합 처리

**지원되는 문자 집합**:

| 문자 집합 | ISO 등록 | 설명 |
|----------|---------|------|
| ISO-IR 100 | ISO 8859-1 | Latin Alphabet No. 1 |
| ISO-IR 101 | ISO 8859-2 | Latin Alphabet No. 2 |
| ISO-IR 109 | ISO 8859-3 | Latin Alphabet No. 3 |
| ISO-IR 110 | ISO 8859-4 | Latin Alphabet No. 4 |
| ISO-IR 144 | ISO 8859-5 | 키릴 문자 |
| ISO-IR 127 | ISO 8859-6 | 아랍 문자 |
| ISO-IR 126 | ISO 8859-7 | 그리스 문자 |
| ISO-IR 138 | ISO 8859-8 | 히브리 문자 |
| ISO-IR 148 | ISO 8859-9 | Latin Alphabet No. 5 |
| ISO-IR 13 | JIS X 0201 | 일본어 가타카나 |
| ISO-IR 166 | TIS 620-2533 | 태국어 |
| ISO 2022 IR 87 | JIS X 0208 | 일본어 한자 |
| ISO 2022 IR 149 | KS X 1001 | 한국어 |
| ISO 2022 IR 58 | GB 2312 | 중국어 간체 |
| ISO_IR 192 | UTF-8 | 유니코드 |
| GB18030 | GB 18030 | 중국어 (전체) |
| GBK | GBK | 중국어 확장 |

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
    modalities_in_study TEXT,
    num_series INTEGER DEFAULT 0,
    num_instances INTEGER DEFAULT 0
);

-- 시리즈 테이블
CREATE TABLE series (
    series_instance_uid TEXT PRIMARY KEY,
    study_instance_uid TEXT REFERENCES study(study_instance_uid),
    series_number INTEGER,
    modality TEXT,
    series_description TEXT,
    num_instances INTEGER DEFAULT 0
);

-- 인스턴스 테이블
CREATE TABLE instance (
    sop_instance_uid TEXT PRIMARY KEY,
    series_instance_uid TEXT REFERENCES series(series_instance_uid),
    sop_class_uid TEXT,
    instance_number INTEGER,
    file_path TEXT,
    file_size INTEGER,
    transfer_syntax TEXT
);
```

### S3 클라우드 저장소

**구현**: 목 클라이언트(기본값)와 전체 AWS SDK 클라이언트(`PACS_WITH_AWS_SDK` CMake 플래그로 활성화)의 이중 구현 구조를 갖춘 S3 호환 클라우드 저장소 백엔드.

**기능**:
- AWS S3 및 S3 호환 저장소 (MinIO 등)
- 계층적 객체 키 구조 (Study/Series/SOP)
- 대용량 파일을 위한 멀티파트 업로드 지원
- 업로드/다운로드 모니터링을 위한 진행 콜백
- shared_mutex를 통한 스레드 안전 연산
- 설정 가능한 연결 및 타임아웃 설정

**구성**:
```cpp
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/storage/s3_storage.h>

using namespace kcenon::pacs::storage;
namespace core = kcenon::pacs::core;

cloud_storage_config config;
config.bucket_name = "my-dicom-bucket";
config.region = "us-east-1";
config.access_key_id = "<access-key-id>";          // 비밀 저장소에서 읽어오기
config.secret_access_key = "<secret-access-key>";  // 실제 키는 커밋하지 않음

// MinIO 로컬 테스트용
config.endpoint_url = "http://localhost:9000";

// 멀티파트 업로드 설정
config.multipart_threshold = 100 * 1024 * 1024;  // 100MB
config.part_size = 10 * 1024 * 1024;  // 10MB
```

**사용 예제**:
```cpp
s3_storage storage{config};

// DICOM 데이터 세트 저장
core::dicom_dataset ds;
// ... UID로 데이터 세트 채우기 ...

auto store_result = storage.store(ds);
if (store_result.is_ok()) {
    std::cout << "저장 성공\n";
}

// SOP Instance UID로 검색
auto retrieve_result = storage.retrieve("1.2.3.4.5.6.7.8.9");
if (retrieve_result.is_ok()) {
    auto& dataset = retrieve_result.value();
    std::cout << "환자: " << dataset.get_string(core::tags::patient_name) << "\n";
}

// 진행 추적과 함께 저장
auto progress = [](std::size_t transferred, std::size_t total) -> bool {
    std::cout << "진행: " << (100 * transferred / total) << "%\n";
    return true;  // 업로드 계속
};
auto progress_result = storage.store_with_progress(ds, progress);
```

**객체 키 구조**:
```
{bucket}/
  └── {StudyInstanceUID}/
      └── {SeriesInstanceUID}/
          └── {SOPInstanceUID}.dcm
```

**빌드 구성**:
- 기본값 (플래그 없음): `mock_s3_client` 사용 — 클라우드 자격 증명 없이 테스트하기 위한 인메모리 목
- `PACS_WITH_AWS_SDK` 정의: `aws_s3_client` 사용 — `PutObject`, `GetObject`, `DeleteObject`, `HeadObject`, `ListObjectsV2`, 멀티파트 업로드(`CreateMultipartUpload`/`UploadPart`/`CompleteMultipartUpload`), MinIO용 path-style 주소 지정을 지원하는 전체 AWS SDK

### Azure Blob 저장소

**구현**: 목 클라이언트(기본값)와 전체 Azure SDK 클라이언트(`PACS_WITH_AZURE_SDK` CMake 플래그로 활성화)의 이중 구현 구조를 갖춘 Azure Blob 저장소 백엔드.

**기능**:
- Azure Blob Storage 컨테이너 지원
- 대용량 파일을 위한 블록 블롭 업로드 (병렬 블록 스테이징)
- 액세스 티어 관리 (Hot, Cool, Archive)
- 업로드/다운로드 모니터링을 위한 진행 콜백
- shared_mutex를 통한 스레드 안전 연산
- 로컬 테스트를 위한 Azurite 에뮬레이터 지원
- 설정 가능한 연결 및 타임아웃 설정

**구성**:
```cpp
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/storage/azure_blob_storage.h>

using namespace kcenon::pacs::storage;
namespace core = kcenon::pacs::core;

azure_storage_config config;
config.container_name = "dicom-container";
config.connection_string = "DefaultEndpointsProtocol=https;AccountName=...";

// Azurite 로컬 테스트용
config.endpoint_url = "http://127.0.0.1:10000/devstoreaccount1";

// 블록 블롭 업로드 설정
config.block_upload_threshold = 100 * 1024 * 1024;  // 100MB
config.block_size = 4 * 1024 * 1024;  // 4MB

// 액세스 티어
config.access_tier = "Hot";  // Hot, Cool, Archive 중 하나
```

**사용 예제**:
```cpp
azure_blob_storage storage{config};

// DICOM 데이터 세트 저장
core::dicom_dataset ds;
// ... UID로 데이터 세트 채우기 ...

auto store_result = storage.store(ds);
if (store_result.is_ok()) {
    std::cout << "저장 성공\n";
}

// SOP Instance UID로 검색
auto retrieve_result = storage.retrieve("1.2.3.4.5.6.7.8.9");
if (retrieve_result.is_ok()) {
    auto& dataset = retrieve_result.value();
    std::cout << "환자: " << dataset.get_string(core::tags::patient_name) << "\n";
}

// 보관용으로 액세스 티어 변경
auto tier_result = storage.set_access_tier("1.2.3.4.5.6.7.8.9", "Archive");

// 진행 추적과 함께 저장
auto progress = [](std::size_t transferred, std::size_t total) -> bool {
    std::cout << "진행: " << (100 * transferred / total) << "%\n";
    return true;  // 업로드 계속
};
auto progress_result = storage.store_with_progress(ds, progress);
```

**블롭 이름 구조**:
```
{container}/
  └── {StudyInstanceUID}/
      └── {SeriesInstanceUID}/
          └── {SOPInstanceUID}.dcm
```

**빌드 구성**:
- 기본값 (플래그 없음): `mock_azure_client` 사용 — 클라우드 자격 증명 없이 테스트하기 위한 인메모리 목
- `PACS_WITH_AZURE_SDK` 정의: `azure_sdk_client` 사용 — `Upload`, `Download`, `Delete`, `GetProperties`, `ListBlobs`, 블록 블롭 업로드(`StageBlock`/`CommitBlockList`), `SetAccessTier`(Hot/Cool/Archive)를 지원하는 전체 Azure SDK

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
#include <kcenon/pacs/storage/azure_blob_storage.h>
#include <kcenon/pacs/storage/file_storage.h>
#include <kcenon/pacs/storage/hsm_storage.h>
#include <kcenon/pacs/storage/s3_storage.h>

using namespace kcenon::pacs::storage;
namespace core = kcenon::pacs::core;

// 자동 마이그레이션의 기간 기준 구성
hsm_storage_config config;
config.policy.hot_to_warm = std::chrono::days{30};    // 30일 후 hot에서 warm으로
config.policy.warm_to_cold = std::chrono::days{180};  // 180일 후 warm에서 cold로
config.policy.auto_migrate = true;

// 티어 백엔드 구성 (warm과 cold는 nullptr일 수도 있음)
file_storage_config hot_config;
hot_config.root_path = "/data/dicom/hot";

cloud_storage_config warm_config;
warm_config.bucket_name = "dicom-warm";

azure_storage_config cold_config;
cold_config.container_name = "dicom-archive";
cold_config.access_tier = "Archive";

// 백엔드로 HSM 저장소 생성
hsm_storage storage{
    std::make_unique<file_storage>(hot_config),
    std::make_unique<s3_storage>(warm_config),
    std::make_unique<azure_blob_storage>(cold_config),
    config
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
    std::cout << "환자: " << dataset.get_string(core::tags::patient_name) << "\n";
}

// 인스턴스가 어느 티어에 있는지 확인
if (auto tier = storage.get_tier("1.2.3.4.5.6.7.8.9")) {
    std::cout << "저장 위치: " << to_string(*tier) << "\n";
}

// 특정 티어로 수동 마이그레이션
auto migrate_result = storage.migrate("1.2.3.4.5.6.7.8.9", storage_tier::cold);

// 저장소 통계 가져오기
auto stats = storage.get_hsm_statistics();
std::cout << "Hot 티어: " << stats.hot.instance_count << " 인스턴스\n";
std::cout << "Warm 티어: " << stats.warm.instance_count << " 인스턴스\n";
std::cout << "Cold 티어: " << stats.cold.instance_count << " 인스턴스\n";
```

**백그라운드 마이그레이션 서비스**:
```cpp
#include <kcenon/pacs/storage/hsm_migration_service.h>

// 마이그레이션 서비스 구성
migration_service_config config;
config.migration_interval = std::chrono::hours{1};  // 매시간 실행
config.max_concurrent_migrations = 4;

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
if (auto time_left = service.time_until_next_cycle()) {
    std::cout << "다음 사이클까지: " << time_left->count() << " 초\n";
}

// 즉시 마이그레이션 트리거
service.trigger_cycle();

// 통계 가져오기
auto stats = service.get_cumulative_stats();
std::cout << "총 마이그레이션: " << stats.instances_migrated << "\n";

// 정상 종료
service.stop();
```

**마이그레이션 결과 구조** (`include/kcenon/pacs/storage/hsm_types.h`에서 발췌):
```cpp
struct migration_result {
    std::size_t instances_migrated{0};      // 마이그레이션 성공 수
    std::size_t bytes_migrated{0};          // 전송된 총 바이트
    std::chrono::milliseconds duration{0};  // 소요 시간
    std::vector<std::string> failed_uids;   // 마이그레이션 실패 목록
    std::size_t instances_skipped{0};       // 아직 대상이 아닌 수

    [[nodiscard]] auto is_success() const noexcept -> bool;         // 실패 없음
    [[nodiscard]] auto total_processed() const noexcept -> std::size_t;
};
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

| 연산 | SIMD 지원 |
|------|-----------|
| 바이트 스왑 (16비트) | AVX2, SSSE3, NEON |
| 바이트 스왑 (32비트) | AVX2, SSSE3, NEON |
| 바이트 스왑 (64비트) | AVX2, SSSE3, NEON |

저장소에는 SIMD 속도 향상 측정 기록이 없습니다. 이 표에 있던 8-16배 범위는 SIMD 바이트 스왑을 추가할 때
제시한 예상치였으며 측정 결과가 아닙니다. 바이트 스왑 벤치마크는 `benchmarks/simd_performance/`에 있습니다.
[벤치마크](BENCHMARKS.kr.md#simd-벤치마크)를 참조하세요.

**사용법**:
```cpp
#include <kcenon/pacs/encoding/byte_swap.h>
#include <kcenon/pacs/encoding/simd/simd_config.h>

using namespace kcenon::pacs::encoding;

std::vector<uint8_t> pixel_data(512 * 512 * 2);  // 16비트 OW 픽셀 데이터

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

**헤더** (`include/kcenon/pacs/encoding/simd/` 아래):
- `simd_config.h` - CPU 기능 감지
- `simd_types.h` - 이식 가능한 SIMD 타입 래퍼
- `simd_utils.h` - SIMD 유틸리티 함수

### container_system 통합

**목적**: DICOM 데이터 직렬화 및 타입 안전 값 처리.

**통합 지점**:
- VR 타입을 컨테이너 값 타입에 매핑
- DICOM 인코딩을 위한 바이너리 직렬화
- SQ (시퀀스) 요소를 위한 중첩 컨테이너
- 대용량 배열을 위한 SIMD 가속

**예제**:
```cpp
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/integration/container_adapter.h>

using kcenon::pacs::integration::container_adapter;
namespace core = kcenon::pacs::core;

core::dicom_dataset dataset;
dataset.set_string(core::tags::patient_id, kcenon::pacs::encoding::vr_type::LO, "12345");

// DICOM 데이터세트를 컨테이너로 변환
auto container = container_adapter::serialize_dataset(dataset);

// 바이너리로 직렬화
auto binary = container_adapter::to_binary(dataset);

// 다시 변환
auto restored_dataset = container_adapter::deserialize_dataset(*container);
auto restored_from_binary = container_adapter::from_binary(binary);
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

**상태:** ✅ **완전 마이그레이션 (`v0.1.0` 포함)** - [MIGRATION_COMPLETE.kr.md](MIGRATION_COMPLETE.kr.md) 참조

**통합 지점**:
- jthread를 지원하는 accept 루프용 `accept_worker` (`thread_base` 상속)
- 부하 분산을 갖춘 연결 워커 관리용 `thread_adapter` 풀
- 협력적 정상 종료를 위한 `cancellation_token`
- 통합 스레드 통계 및 모니터링

**마이그레이션 효과 (Epic #153):**

| 항목 | 이전 | 이후 | 개선 |
|------|------|------|------|
| 스레드 관리 | 수동 수명주기 | 자동 jthread | 누수 없음 |
| 모니터링 | 없음 | 통합 통계 | 전체 가시성 |
| 취소 | 미지원 | cancellation_token | 협력적 |

마이그레이션 이전의 종료 시간은 측정되지 않았으므로 이전/이후 비율은 없습니다. 마이그레이션 이후
[성능 결과](PERFORMANCE_RESULTS.md#4-shutdown-time)에는 활성 연결 5개에서 107 ms (통과 기준 `< 5000 ms`),
워커 3개 부하에서 110 ms (통과 기준 `< 10000 ms`)가 기록되어 있습니다.

**예제:**
```cpp
#include <kcenon/pacs/network/dicom_server.h>
#include <kcenon/pacs/services/verification_scp.h>

using namespace kcenon::pacs::network;
using kcenon::pacs::services::verification_scp;

server_config config;
config.ae_title = "PACS_SCP";
config.port = 11112;

// 서버는 내부적으로 thread_system 사용
dicom_server server{config};
server.register_service(std::make_shared<verification_scp>());
auto start_result = server.start();

// 타임아웃이 있는 정상 종료 (cancellation_token 사용)
server.stop(std::chrono::seconds{5});
```

### network_system V2 통합 (선택 사항)

**목적**: TCP 관리에 `messaging_server`를 사용하는 대체 DICOM 서버.

**상태:** ✅ **사용 가능 (선택 사항)**

**컴파일 플래그:** `PACS_WITH_NETWORK_SYSTEM`

**구성 요소:**
- `dicom_server_v2` - 연결 관리에 `messaging_server` 사용
- `dicom_association_handler` - 세션별 PDU 프레이밍 및 디스패치

**예제:**
```cpp
#include <kcenon/pacs/network/v2/dicom_server_v2.h>
#include <kcenon/pacs/services/verification_scp.h>

using namespace kcenon::pacs::network;
using kcenon::pacs::services::verification_scp;

server_config config;
config.ae_title = "PACS_SCP";
config.port = 11112;

// 같은 API, 다른 구현
v2::dicom_server_v2 server{config};
server.register_service(std::make_shared<verification_scp>());
auto start_result = server.start();
```

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
#include <kcenon/pacs/di/service_registration.h>
#include <kcenon/pacs/services/storage_scp.h>
#include <kcenon/pacs/services/verification_scp.h>

using namespace kcenon::pacs::services;
namespace di = kcenon::pacs::di;

// 기본: 무출력 (NullLogger)
verification_scp scp;

// LoggerService로 로깅
auto logger = std::make_shared<di::LoggerService>();
verification_scp scp_with_logging(logger);

// DI 컨테이너 통해
auto container = di::create_container();
auto di_logger = container->resolve<di::ILogger>().value();
storage_scp scp_via_di(di_logger);
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

### 접근 제어 (RBAC)

**문서**: [SECURITY.md](SECURITY.md) (영어)

**기능**:
- **역할 기반 접근 제어**: 사전 정의된 역할 (Viewer, Technologist, Radiologist, Administrator).
- **세분화된 권한**: 리소스 수준 제어 (Study, Metadata, System, Audit).
- **사용자 관리**: API를 통한 사용자 생성 및 역할 할당.
- **API 적용**: 권한 검사를 갖춘 보안 REST API 엔드포인트.
- **AE Title 보안**: 연결 화이트리스팅 (기존 기능).

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
```cpp
#include <kcenon/pacs/monitoring/pacs_monitor.h>
using namespace kcenon::pacs::monitoring;

// 전역 모니터 인스턴스를 획득하고 메트릭 레이블에 쓸 AE 타이틀 설정
auto& monitor = pacs_monitor::global_monitor();
pacs_monitor_config config;
config.ae_title = "PACS_SCP";
monitor.update_config(config);

// 모든 메트릭 수집
auto snapshot = monitor.get_metrics();
for (const auto& m : snapshot.metrics) {
    std::cout << m.name << ": " << m.value << "\n";
}

// Prometheus 형식으로 내보내기
std::string prometheus_output = monitor.to_prometheus();

// 커스텀 상태 확인 등록
monitor.register_health_check("database", []() {
    return true;  // 실제 데이터베이스 점검으로 교체
});

// 전체 상태 확인
bool healthy = monitor.check_health().is_healthy();
```

**IMonitor와의 통합**:
`pacs_monitor` 클래스는 `common_system`의 `IMonitor`와 동일한 인터페이스 패턴을 따르므로, 모니터링 인프라와 원활하게 통합됩니다:

```cpp
// 커스텀 메트릭 기록
monitor.record_metric("custom_gauge", 42.0);

// 등록된 모든 상태 확인을 타이밍과 함께 실행
auto result = monitor.check_health();
std::cout << "상태: " << (result.is_healthy() ? "OK" : "FAIL")
          << " (" << result.check_duration.count() << "ms)\n";
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
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/core/pool_manager.h>
#include <kcenon/pacs/network/pdu_buffer_pool.h>

using namespace kcenon::pacs::core;
using kcenon::pacs::encoding::vr_type;

// 풀링된 DICOM 요소 획득
auto elem = make_pooled_element(tags::patient_name, vr_type::PN, "DOE^JOHN");

// 풀링된 데이터셋 획득
auto dataset = make_pooled_dataset();
dataset->set_string(tags::patient_id, vr_type::LO, "12345");

// 풀링된 PDU 버퍼 획득
auto buffer = kcenon::pacs::network::make_pooled_pdu_buffer(16384);

// 객체 소멸 시 자동으로 풀에 반환
```

**성능 이점**:
- 메모리 단편화 감소
- 캐시 지역성 향상
- GC 압력 감소

저장소에는 풀 사용 여부에 따른 할당 지연 시간 측정 기록이 없습니다.

**모니터링**:
```cpp
const auto& stats = pool_manager::get().element_statistics();
double hit_ratio = stats.hit_ratio();  // 0.0 - 1.0
```

### 분산 추적

**스팬 타입**:
- 연결 라이프사이클
- DIMSE 연산
- 저장 연산
- 데이터베이스 조회

### REST API 서버

**구현**: PACS 관리 및 모니터링을 위한 Crow 웹 프레임워크 기반 REST API 서버.

**기능**:
- 시스템 상태 및 메트릭 엔드포인트
- 구성 관리
- 웹 브라우저 접근을 위한 CORS 지원
- `pacs_monitoring` 모듈과의 통합

**클래스**:
- `rest_server` - 비동기/동기 모드를 갖춘 메인 서버 클래스
- `rest_server_config` - 구성 (포트, CORS, TLS 옵션)
- `rest_server_context` - 엔드포인트 간 공유 컨텍스트

**엔드포인트**:

| 엔드포인트 | 메서드 | 설명 |
|-----------|--------|------|
| `/api/v1/system/status` | GET | 시스템 상태 (health_checker 사용) |
| `/api/v1/system/metrics` | GET | 성능 메트릭 |
| `/api/v1/system/config` | GET | 현재 서버 구성 |
| `/api/v1/system/config` | PUT | 구성 업데이트 |
| `/api/v1/system/version` | GET | API 버전 정보 |

**예제**:
```cpp
#include <kcenon/pacs/monitoring/health_checker.h>
#include <kcenon/pacs/web/rest_server.h>

using namespace kcenon::pacs::web;

// REST 서버 구성 및 시작
rest_server_config config;
config.port = 8080;
config.concurrency = 4;
config.enable_cors = true;

rest_server server(config);
server.set_health_checker(std::make_shared<kcenon::pacs::monitoring::health_checker>());
server.start_async();  // 논블로킹

// 이후: 정상 종료
server.stop();
```

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
#include <kcenon/pacs/storage/index_database.h>
#include <kcenon/pacs/workflow/auto_prefetch_service.h>

using namespace kcenon::pacs::workflow;

// `database`는 열려 있는 kcenon::pacs::storage::index_database
// (storage::index_database::open 참조)

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

// 예약된 워크리스트 항목에 대한 사전 검색 트리거
auto worklist = database.query_worklist({});
if (worklist.is_ok()) {
    service.trigger_for_worklist(worklist.value());
}

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
#include <kcenon/pacs/storage/index_database.h>
#include <kcenon/pacs/workflow/task_scheduler.h>

using namespace kcenon::pacs::workflow;

// `database`는 열려 있는 kcenon::pacs::storage::index_database

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

config.on_task_error = [](const task_id& id, const std::string& error) {
    std::cerr << "태스크 " << id << " 실패: " << error << "\n";
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
    std::cout << "실행 횟수: " << task->execution_count << "\n";
}

// 모든 태스크 나열
for (const auto& t : scheduler.list_tasks()) {
    std::cout << t.id << ": " << t.name << " [" << to_string(t.state) << "]\n";
}

// 통계 조회
auto stats = scheduler.get_stats();
std::cout << "스케줄된 태스크: " << stats.scheduled_tasks << "\n";
std::cout << "실행 중: " << stats.running_tasks << "\n";
std::cout << "총 실행: " << stats.total_executions << "\n";

// 정상 종료
scheduler.stop(true);  // 실행 중인 태스크 대기
```

**Cron 스케줄 헬퍼**:
```cpp
// 15분마다
auto every_15_min = cron_schedule::every_minutes(15);

// 4시간마다
auto every_4_hours = cron_schedule::every_hours(4);

// 매일 오전 3:30
auto daily = cron_schedule::daily_at(3, 30);

// 매주 일요일 오전 2:00
auto weekly = cron_schedule::weekly_on(0, 2, 0);

// cron 표현식 파싱
auto custom = cron_schedule::parse("0 2 * * 1-5");  // 평일 오전 2:00
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
#include <kcenon/pacs/core/result.h>
#include <kcenon/pacs/workflow/study_lock_manager.h>

using namespace kcenon::pacs::workflow;

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
    auto unlock_result = manager.unlock(token);
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
auto force_result = manager.force_unlock("1.2.3.4.5", "긴급 해제");

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

### 원격 노드 관리자 (연결 풀링)

**구현**: 여러 DIMSE 연산에서 수립된 연결을 재사용하기 위한 원격 노드 관리자(`remote_node_manager`)의 DICOM 연결 풀링.

**기능**:
- 노드 ID별 `std::deque<pooled_association>`로 구성된 노드별 연결 풀
- `pool_connection_ttl` 구성을 통한 TTL 기반 만료 (기본값: 300초)
- `max_pool_connections_per_node`를 통한 최대 풀 크기 제한 (기본값: 4)
- 획득/반환 패턴: `acquire_association()`은 먼저 풀을 확인하고 필요하면 새로 생성
- 스레드 안전: `std::mutex`가 모든 풀 연산을 보호
- 정상 정리: 노드 제거/종료 시 풀 연결 해제
- 자동 검증: 획득 시 만료되었거나 끊어진 연결은 폐기

**구성 옵션**:

| 옵션 | 타입 | 기본값 | 설명 |
|------|------|--------|------|
| `max_pool_connections_per_node` | `size_t` | 4 | 원격 노드당 최대 풀 연결 수 |
| `pool_connection_ttl` | `std::chrono::seconds` | 300 | 풀 연결의 유지 시간 |

**예제**:
```cpp
#include <kcenon/pacs/client/remote_node_manager.h>
#include <kcenon/pacs/network/association.h>
#include <kcenon/pacs/services/query_scp.h>

using namespace kcenon::pacs::client;

// `repo`는 등록된 노드를 영속화하는
// std::shared_ptr<kcenon::pacs::storage::node_repository>

// 연결 풀 설정으로 원격 노드 관리자 구성
node_manager_config config;
config.local_ae_title = "MY_SCU";
config.max_pool_connections_per_node = 8;
config.pool_connection_ttl = std::chrono::seconds{600};

remote_node_manager manager{repo, config};

// 등록된 노드 ID로 연결 획득 (가능하면 풀링된 연결 재사용)
std::vector<std::string> sop_classes{
    std::string(kcenon::pacs::services::study_root_find_sop_class_uid)};
auto result = manager.acquire_association("REMOTE_PACS", sop_classes);
if (result.is_ok()) {
    auto assoc = std::move(result.value());

    // DIMSE 연산 수행...

    // 닫는 대신 재사용을 위해 풀에 반환
    manager.release_association("REMOTE_PACS", std::move(assoc));
}
```

**관련 풀링 메커니즘**:

PACS 시스템에는 다른 두 가지 객체 풀링 메커니즘도 있습니다:

| 풀 | 위치 | 목적 |
|----|------|------|
| **오브젝트 풀** | `src/core/pool_manager.cpp` | 할당 오버헤드를 줄이기 위해 `dicom_element`와 `dicom_dataset` 객체를 풀링 |
| **PDU 버퍼 풀** | `src/network/pdu_buffer_pool.cpp` | 효율적인 네트워크 I/O를 위해 네트워크 버퍼, PDV 아이템, P-DATA-TF PDU를 풀링 |

객체 풀링에 대한 자세한 내용은 [오브젝트 풀 메모리 관리](#오브젝트-풀-메모리-관리)를 참조하세요.

---

## 2025년 12월에 추가된 기능

| 기능 | 설명 | 이슈 | `v0.1.0` 포함 |
|------|------|------|---------------|
| 스터디 락 관리자 | 동시 접근 제어를 위한 스레드 안전 락 관리자 | #208 | 예 |
| 태스크 스케줄러 서비스 | 정리, 아카이브, 검증을 위한 자동화된 태스크 스케줄링 | #207 | 예 |
| 자동 사전 검색 서비스 | 워크리스트 쿼리 기반 이전 검사 자동 사전 검색 | #206 | 예 |
| 계층적 저장소 관리 (HSM) | 자동 연령 기반 마이그레이션을 갖춘 3계층 HSM | #200 | 예 |
| Azure Blob 저장소 | Azure Blob 저장소 백엔드 (목 + `PACS_WITH_AZURE_SDK`를 통한 전체 Azure SDK) | #199 | 예 |
| 세그먼테이션(SEG) 및 구조화된 보고서(SR) | IOD 검증을 갖춘 AI/CAD 출력용 SEG/SR SOP 클래스 | #187 | 예 |
| 방사선 치료(RT) 모달리티 | IOD 검증을 갖춘 RT Plan, RT Dose, RT Structure Set 지원 | #186 | 예 |
| RLE Lossless 코덱 | RLE 압축 코덱 (순수 C++) | #182 | 예 |
| REST API 서버 | Crow 프레임워크 기반 웹 관리 API | #194 | 예 |
| S3 클라우드 저장소 | S3 호환 저장소 백엔드 (목 + `PACS_WITH_AWS_SDK`를 통한 전체 AWS SDK) | #198 | 예 |
| 스레드 시스템 마이그레이션 | std::thread → thread_system | #153 | 예 |
| 네트워크 시스템 V2 | 선택적 messaging_server 통합 | #163 | 예 |
| DIMSE-N 서비스 | N-GET, N-ACTION, N-EVENT-REPORT, N-DELETE | #127 | 예 |
| 초음파 저장 | US, US-MF SOP 클래스 | #128 | 예 |
| XA 이미지 저장 | XA, Enhanced XA, XRF SOP 클래스 | #129 | 예 |
| Explicit VR Big Endian | Transfer Syntax 지원 | #126 | 예 |

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
| -800 ~ -819 | C-STORE 서비스 | Storage SCP/SCU 작업 |
| -820 ~ -839 | C-FIND 서비스 | Query SCP 작업 |
| -840 ~ -859 | C-MOVE/C-GET 서비스 | Retrieve SCP 작업 |
| -860 ~ -869 | 검증 서비스 | C-ECHO 작업 |
| -870 ~ -879 | MPPS 서비스 | MPPS N-CREATE/N-SET 작업 |
| -880 ~ -889 | 워크리스트 서비스 | Modality Worklist 작업 |
| -890 ~ -899 | 일반 서비스 | 공통 서비스 에러 |

**클래스 및 타입** (`include/kcenon/pacs/core/result.h`):
- `kcenon::pacs::Result<T>` - Result 타입 별칭
- `kcenon::pacs::VoidResult` - void 작업용 Result
- `kcenon::pacs::error_info` - 에러 정보 구조체
- `kcenon::pacs::error_codes` - 표준화된 에러 코드

**예제**:
```cpp
#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/result.h>

using namespace kcenon::pacs::core;

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
    .map([](const dicom_file& f) { return f.sop_instance_uid(); })
    .unwrap_or("unknown");

// PACS_ASSIGN_OR_RETURN 매크로 사용 (첫 번째 인자는 선언된 변수 이름이어야 함)
kcenon::pacs::Result<std::string> get_patient_name(const std::filesystem::path& path) {
    dicom_file file;
    PACS_ASSIGN_OR_RETURN(file, dicom_file::open(path));
    return kcenon::pacs::Result<std::string>::ok(
        file.dataset().get_string(tags::patient_name));
}
```

---

## 계획된 기능

### 이전에 계획됨, `v0.1.0` 포함

| 기능 | 설명 | 단계 | `v0.1.0` 포함 |
|------|------|------|---------------|
| ~~추가 SOP 클래스 (PET/NM)~~ | NM, PET 지원 | Phase 4 | 예 |
| ~~추가 SOP 클래스 (RT)~~ | RT 계획, RT 구조 세트 등 | Phase 4 | 예 |
| ~~C-GET~~ | 대체 검색 방법 | Phase 3 | 예 |
| ~~DICOMweb (WADO-RS, STOW-RS, QIDO-RS)~~ | DICOMweb 지원 (PS3.18) | Phase 4 | 예 |
| ~~AI 통합~~ | 외부 AI 서비스 커넥터 + 결과 핸들러 | Phase 4 | 예 |
| ~~전체 AWS S3 SDK~~ | 프로덕션 AWS SDK 통합 (`PACS_WITH_AWS_SDK`) | Phase 4 | 예 |
| ~~전체 Azure SDK~~ | 프로덕션 Azure SDK 통합 (`PACS_WITH_AZURE_SDK`) | Phase 4 | 예 |
| ~~연결 풀링~~ | `remote_node_manager`를 통한 DICOM 연결 재사용 | Phase 4 | 예 |

### Phase 5: 엔터프라이즈 기능 (향후)

| 기능 | 설명 | 목표 |
|------|------|------|
| 향상된 메트릭 | 연결별 타이밍 | Phase 5 |

---

## 문서 이력

| 버전 | 날짜 | 작성자 | 변경 사항 |
|------|------|--------|----------|
| 1.0.0 | 2025-11-30 | kcenon | 초기 작성 |
| 1.1.0 | 2025-12-04 | kcenon | DIMSE 상태 업데이트 |
| 1.2.0 | 2025-12-07 | kcenon | 추가: 스레드 마이그레이션, 네트워크 V2, DIMSE-N, 초음파, XA. 업데이트: 에코시스템 통합 |
| 1.3.0 | 2025-12-09 | raphaelshin | 추가: S3 클라우드 저장소 (목 구현), 이슈 #198 |
| 1.4.0 | 2025-12-09 | raphaelshin | 추가: Crow 프레임워크 기반 REST API 서버 기반, 이슈 #194 |
| 1.5.0 | 2025-12-09 | raphaelshin | 추가: RLE Lossless 코덱, 이슈 #182. 업데이트: 압축 코덱 상태 |
| 1.6.0 | 2025-12-10 | raphaelshin | 추가: IOD 검증을 갖춘 RT 모달리티 지원 (RT Plan, RT Dose, RT Structure Set), 이슈 #186 |
| 1.7.0 | 2025-12-10 | raphaelshin | 추가: AI/CAD 통합을 위한 SEG/SR 지원 (세그먼테이션, 구조화된 보고서), 이슈 #187 |
| 1.8.0 | 2025-12-12 | raphaelshin | 추가: 블록 블롭 업로드를 갖춘 Azure Blob 저장소 (목 구현), 이슈 #199 |
| 1.9.0 | 2025-12-12 | raphaelshin | 추가: 3계층 저장소와 자동 마이그레이션을 갖춘 계층적 저장소 관리 (HSM), 이슈 #200 |
| 2.0.0 | 2025-12-13 | raphaelshin | 추가: 워크리스트 트리거 이전 검사 사전 검색을 위한 자동 사전 검색 서비스, 이슈 #206 |
| 2.1.0 | 2025-12-13 | raphaelshin | 추가: 정리, 아카이브, 검증 자동화를 위한 태스크 스케줄러 서비스, 이슈 #207 |
| 2.2.0 | 2025-12-17 | raphaelshin | 추가: 통합 에러 처리를 위한 Result<T> 패턴, PACS 에러 코드 (-700 ~ -799), 이슈 #308 |
| 2.3.0 | 2025-12-17 | raphaelshin | 추가: exclusive/shared/migration 락을 갖춘 동시 접근 제어용 스터디 락 관리자, 이슈 #208 |
| 2.4.0 | 2025-12-17 | raphaelshin | 추가: 바이트 스왑 연산을 위한 SIMD 최적화 인프라 (SSE/AVX/NEON), 이슈 #313 |
| 2.5.0 | 2025-12-18 | raphaelshin | 추가: 서비스별 에러 코드 (-800 ~ -899), 서비스 모듈의 통합 Result<T> 패턴, 이슈 #325 |
| 2.6.0 | 2026-02-09 | raphaelshin | 수정: 버전 충돌 해결 (0.1.9.0 → 0.2.0.0), 완료 항목(DICOMweb, AI 통합)을 반영해 계획된 기능 업데이트, PRD와 단계 목표 정렬, 이슈 #673 |
| 2.7.0 | 2026-02-21 | raphaelshin | 이동: 연결 풀링을 Phase 5 계획에서 Phase 4 완료로. 추가: 구성 옵션과 예제를 갖춘 원격 노드 관리자 연결 풀링 문서, 이슈 #747 |

*참고: 위 문서 이력 표는 이 문서 자체의 개정(문서 개정 버전)을 추적합니다. 제품 버전은 헤더에 명시되어 있습니다.*

---

*문서 버전: 0.2.0.0*
*작성일: 2025-11-30*
*수정일: 2026-02-09*
*작성자: kcenon@naver.com*
