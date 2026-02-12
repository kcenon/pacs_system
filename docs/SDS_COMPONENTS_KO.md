# SDS - 컴포넌트 설계 명세

> **버전:** 0.1.2
> **상위 문서:** [SDS_KO.md](SDS_KO.md)
> **최종 수정일:** 2025-12-07

---

## 목차

- [1. 코어 모듈 컴포넌트](#1-코어-모듈-컴포넌트)
- [2. 인코딩 모듈 컴포넌트](#2-인코딩-모듈-컴포넌트)
- [3. 네트워크 모듈 컴포넌트](#3-네트워크-모듈-컴포넌트)
- [4. 서비스 모듈 컴포넌트](#4-서비스-모듈-컴포넌트)
- [5. 스토리지 모듈 컴포넌트](#5-스토리지-모듈-컴포넌트)
- [6. 통합 모듈 컴포넌트](#6-통합-모듈-컴포넌트)

---

## 1. 코어 모듈 컴포넌트

### DES-CORE-001: dicom_tag

**추적:** SRS-CORE-001, FR-1.1

**목적:** DICOM 태그(Group, Element 쌍)를 표현합니다.

**클래스 설계:**

```cpp
namespace pacs::core {

class dicom_tag {
public:
    // ─────────────────────────────────────────────────────
    // 상수
    // ─────────────────────────────────────────────────────
    static constexpr uint16_t PRIVATE_GROUP_START = 0x0009;
    static constexpr uint16_t PRIVATE_GROUP_END = 0xFFFF;

    // ─────────────────────────────────────────────────────
    // 생성자
    // ─────────────────────────────────────────────────────
    constexpr dicom_tag() noexcept;
    constexpr dicom_tag(uint16_t group, uint16_t element) noexcept;
    constexpr explicit dicom_tag(uint32_t combined) noexcept;

    // ─────────────────────────────────────────────────────
    // 접근자
    // ─────────────────────────────────────────────────────
    [[nodiscard]] constexpr uint16_t group() const noexcept;
    [[nodiscard]] constexpr uint16_t element() const noexcept;
    [[nodiscard]] constexpr uint32_t value() const noexcept;

    // ─────────────────────────────────────────────────────
    // 분류
    // ─────────────────────────────────────────────────────
    [[nodiscard]] constexpr bool is_private() const noexcept;
    [[nodiscard]] constexpr bool is_group_length() const noexcept;
    [[nodiscard]] constexpr bool is_meta_info() const noexcept;

    // ─────────────────────────────────────────────────────
    // 비교 (std::map 정렬용)
    // ─────────────────────────────────────────────────────
    constexpr auto operator<=>(const dicom_tag&) const noexcept = default;

    // ─────────────────────────────────────────────────────
    // 문자열 표현
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::string to_string() const;  // "(GGGG,EEEE)"

private:
    uint16_t group_{0};
    uint16_t element_{0};
};

} // namespace pacs::core
```

**설계 결정:**

| 결정 | 근거 |
|------|------|
| `constexpr` 생성 | 컴파일 타임 태그 상수 |
| `operator<=>` | C++20 삼방 비교로 정렬 지원 |
| group/element 분리 | DICOM 바이너리 형식과 직접 매핑 |

**메모리 레이아웃:**

```
┌──────────────────────────────────────┐
│           dicom_tag (4 바이트)        │
├──────────────────┬───────────────────┤
│  group_ (2바이트)│ element_ (2바이트) │
│    0x0010        │      0x0010       │
│   (Patient)      │   (PatientName)   │
└──────────────────┴───────────────────┘
```

---

### DES-CORE-002: dicom_element

**추적:** SRS-CORE-002, FR-1.1

**목적:** 태그, VR, 값을 포함하는 DICOM 데이터 요소를 표현합니다.

**클래스 설계:**

```cpp
namespace pacs::core {

class dicom_element {
public:
    // ─────────────────────────────────────────────────────
    // 팩토리 메서드
    // ─────────────────────────────────────────────────────
    static dicom_element create(dicom_tag tag, vr_type vr);
    static dicom_element create(dicom_tag tag, vr_type vr,
                                 std::string_view value);
    static dicom_element create(dicom_tag tag, vr_type vr,
                                 std::span<const uint8_t> bytes);

    template<std::integral T>
    static dicom_element create_numeric(dicom_tag tag, vr_type vr, T value);

    template<std::floating_point T>
    static dicom_element create_numeric(dicom_tag tag, vr_type vr, T value);

    static dicom_element create_sequence(dicom_tag tag);

    // ─────────────────────────────────────────────────────
    // 접근자 (const)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] dicom_tag tag() const noexcept;
    [[nodiscard]] vr_type vr() const noexcept;
    [[nodiscard]] uint32_t length() const noexcept;
    [[nodiscard]] bool is_empty() const noexcept;

    // ─────────────────────────────────────────────────────
    // 값 접근
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::string as_string() const;
    [[nodiscard]] std::vector<std::string> as_strings() const;

    template<typename T>
    [[nodiscard]] T as_numeric() const;

    [[nodiscard]] std::span<const uint8_t> as_bytes() const;

    // ─────────────────────────────────────────────────────
    // 시퀀스 접근
    // ─────────────────────────────────────────────────────
    [[nodiscard]] bool is_sequence() const noexcept;
    [[nodiscard]] std::vector<dicom_dataset>& items();
    [[nodiscard]] const std::vector<dicom_dataset>& items() const;

    // ─────────────────────────────────────────────────────
    // 값 수정
    // ─────────────────────────────────────────────────────
    void set_string(std::string_view value);
    void set_bytes(std::span<const uint8_t> bytes);

    template<typename T>
    void set_numeric(T value);

    void add_item(dicom_dataset item);

    // ─────────────────────────────────────────────────────
    // 직렬화
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::vector<uint8_t> serialize(
        const transfer_syntax& ts) const;

    static common::Result<dicom_element> deserialize(
        std::span<const uint8_t> data,
        const transfer_syntax& ts,
        const dicom_dictionary& dict);

private:
    dicom_tag tag_;
    vr_type vr_;
    std::variant<
        std::string,                    // 문자열 VR (AE, CS, LO 등)
        std::vector<uint8_t>,           // 바이너리 VR (OB, OW, UN 등)
        std::vector<dicom_dataset>      // 시퀀스 VR (SQ)
    > value_;
};

} // namespace pacs::core
```

**값 저장 전략:**

```
┌─────────────────────────────────────────────────────────────────┐
│                  dicom_element 값 저장                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  std::variant<                                                   │
│      std::string,          ◄── 문자열 VR: AE, AS, CS, DA, DS,   │
│                                DT, IS, LO, LT, PN, SH, ST, TM,  │
│                                UI, UT                            │
│                                                                  │
│      std::vector<uint8_t>, ◄── 바이너리 VR: AT, FL, FD, OB, OD, │
│                                OF, OL, OW, SL, SS, UL, US, UN   │
│                                                                  │
│      std::vector<dicom_dataset>  ◄── 시퀀스 VR: SQ              │
│  >                                                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### DES-CORE-003: dicom_dataset

**추적:** SRS-CORE-003, FR-1.1

**목적:** DICOM 데이터 요소들의 정렬된 컬렉션입니다.

**클래스 설계:**

```cpp
namespace pacs::core {

class dicom_dataset {
public:
    // ─────────────────────────────────────────────────────
    // 타입 별칭
    // ─────────────────────────────────────────────────────
    using container_type = std::map<dicom_tag, dicom_element>;
    using iterator = container_type::iterator;
    using const_iterator = container_type::const_iterator;

    // ─────────────────────────────────────────────────────
    // 생성 / 복사 / 이동
    // ─────────────────────────────────────────────────────
    dicom_dataset() = default;
    dicom_dataset(const dicom_dataset&);
    dicom_dataset(dicom_dataset&&) noexcept;
    dicom_dataset& operator=(const dicom_dataset&);
    dicom_dataset& operator=(dicom_dataset&&) noexcept;

    // ─────────────────────────────────────────────────────
    // 요소 접근
    // ─────────────────────────────────────────────────────
    [[nodiscard]] bool contains(dicom_tag tag) const noexcept;
    [[nodiscard]] const dicom_element* get(dicom_tag tag) const;
    [[nodiscard]] dicom_element* get(dicom_tag tag);

    // ─────────────────────────────────────────────────────
    // 타입별 게터 (기본값 포함)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::string get_string(
        dicom_tag tag,
        std::string_view default_value = "") const;

    [[nodiscard]] std::vector<std::string> get_strings(dicom_tag tag) const;

    template<typename T>
    [[nodiscard]] T get_numeric(dicom_tag tag, T default_value = T{}) const;

    [[nodiscard]] std::vector<uint8_t> get_bytes(dicom_tag tag) const;

    // ─────────────────────────────────────────────────────
    // 타입별 세터 (자동 VR 조회)
    // ─────────────────────────────────────────────────────
    void set_string(dicom_tag tag, std::string_view value);
    void set_strings(dicom_tag tag, const std::vector<std::string>& values);

    template<typename T>
    void set_numeric(dicom_tag tag, T value);

    void set_bytes(dicom_tag tag, std::span<const uint8_t> bytes);

    // ─────────────────────────────────────────────────────
    // 요소 관리
    // ─────────────────────────────────────────────────────
    void add(dicom_element element);
    void remove(dicom_tag tag);
    void clear();

    // ─────────────────────────────────────────────────────
    // 반복 (태그 순서)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] iterator end() noexcept;
    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator end() const noexcept;

    // ─────────────────────────────────────────────────────
    // 속성
    // ─────────────────────────────────────────────────────
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    // ─────────────────────────────────────────────────────
    // 직렬화
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::vector<uint8_t> serialize(
        const transfer_syntax& ts) const;

    [[nodiscard]] static common::Result<dicom_dataset> deserialize(
        std::span<const uint8_t> data,
        const transfer_syntax& ts);

private:
    container_type elements_;
    const dicom_dictionary* dictionary_ = &dicom_dictionary::instance();
};

} // namespace pacs::core
```

**내부 구조:**

```
┌─────────────────────────────────────────────────────────────────┐
│                        dicom_dataset                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  elements_: std::map<dicom_tag, dicom_element>                  │
│                                                                  │
│  ┌─────────────┬────────────────────────────────────────────┐   │
│  │  (0008,0016)│ SOPClassUID = "1.2.840.10008.5.1.4.1.1.2" │   │
│  ├─────────────┼────────────────────────────────────────────┤   │
│  │  (0008,0018)│ SOPInstanceUID = "1.2.3.4.5..."           │   │
│  ├─────────────┼────────────────────────────────────────────┤   │
│  │  (0010,0010)│ PatientName = "홍^길동"                    │   │
│  ├─────────────┼────────────────────────────────────────────┤   │
│  │  (0010,0020)│ PatientID = "12345"                        │   │
│  ├─────────────┼────────────────────────────────────────────┤   │
│  │     ...     │        ...                                 │   │
│  └─────────────┴────────────────────────────────────────────┘   │
│                                                                  │
│  참고: 요소들은 태그 오름차순으로 저장됨 (std::map)              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### DES-CORE-004: dicom_file

**추적:** SRS-CORE-004, FR-1.3

**목적:** DICOM Part 10 파일 읽기/쓰기 작업을 처리합니다.

**파일 형식 구조:**

```
┌──────────────────────────────────────────────────────────────────┐
│                    DICOM Part 10 파일 형식                        │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                    프리앰블 (128 바이트)                     │  │
│  │                     (일반적으로 모두 0)                      │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                  DICM 접두사 (4 바이트)                      │  │
│  │                     "DICM" 리터럴                            │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │              파일 메타 정보 (Group 0002)                     │  │
│  │  ─────────────────────────────────────────────────────────  │  │
│  │  항상 Explicit VR Little Endian                             │  │
│  │                                                              │  │
│  │  (0002,0000) FileMetaInformationGroupLength                 │  │
│  │  (0002,0001) FileMetaInformationVersion                     │  │
│  │  (0002,0002) MediaStorageSOPClassUID                        │  │
│  │  (0002,0003) MediaStorageSOPInstanceUID                     │  │
│  │  (0002,0010) TransferSyntaxUID                              │  │
│  │  (0002,0012) ImplementationClassUID                         │  │
│  │  (0002,0013) ImplementationVersionName                      │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                      데이터 셋                               │  │
│  │  ─────────────────────────────────────────────────────────  │  │
│  │  (0002,0010)의 Transfer Syntax에 따라 인코딩                │  │
│  │                                                              │  │
│  │  (0008,0016) SOPClassUID                                    │  │
│  │  (0008,0018) SOPInstanceUID                                 │  │
│  │  (0010,0010) PatientName                                    │  │
│  │  (0010,0020) PatientID                                      │  │
│  │  ... (모든 다른 데이터 요소)                                 │  │
│  │  (7FE0,0010) PixelData                                      │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

### DES-CORE-005: dicom_dictionary

**추적:** SRS-CORE-005, FR-1.1

**목적:** 태그 메타데이터 조회 및 등록을 처리합니다.

**클래스 설계:**

```cpp
namespace pacs::core {

// 태그 메타데이터 엔트리
struct tag_info {
    vr_type vr;
    std::string_view keyword;
    std::string_view name;
    std::string_view vm;  // 값 다중도 (Value Multiplicity)
    bool retired;
};

class dicom_dictionary {
public:
    // ─────────────────────────────────────────────────────
    // 싱글톤 접근
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static const dicom_dictionary& instance();

    // ─────────────────────────────────────────────────────
    // 태그 조회
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::optional<tag_info> lookup(dicom_tag tag) const;
    [[nodiscard]] std::optional<vr_type> lookup_vr(dicom_tag tag) const;
    [[nodiscard]] std::optional<std::string_view> lookup_name(
        dicom_tag tag) const;

    // ─────────────────────────────────────────────────────
    // 키워드 조회
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::optional<dicom_tag> lookup_tag(
        std::string_view keyword) const;

    // ─────────────────────────────────────────────────────
    // 프라이빗 태그 등록
    // ─────────────────────────────────────────────────────
    void register_private_tag(
        dicom_tag tag,
        std::string_view creator,
        tag_info info);

    // ─────────────────────────────────────────────────────
    // SOP 클래스 레지스트리
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::optional<std::string_view> sop_class_name(
        std::string_view uid) const;

    [[nodiscard]] bool is_storage_sop_class(std::string_view uid) const;

private:
    dicom_dictionary();  // PS3.6 데이터로 초기화

    std::unordered_map<uint32_t, tag_info> standard_tags_;
    std::unordered_map<std::string_view, dicom_tag> keyword_map_;
    std::map<std::pair<dicom_tag, std::string>, tag_info> private_tags_;
    std::unordered_map<std::string_view, std::string_view> sop_classes_;
};

} // namespace pacs::core
```

---

## 2. 인코딩 모듈 컴포넌트

### DES-ENC-001: vr_type

**추적:** SRS-CORE-006, FR-1.2

**목적:** 27개 DICOM 값 표현(Value Representation)의 열거형입니다.

**설계:**

```cpp
namespace pacs::encoding {

// VR 타입 열거형 (2바이트 문자 코드)
enum class vr_type : uint16_t {
    // 문자열 VR
    AE = 0x4145,  // Application Entity (최대 16자)
    AS = 0x4153,  // Age String (4자: nnnD/W/M/Y)
    CS = 0x4353,  // Code String (최대 16자)
    DA = 0x4441,  // Date (8자: YYYYMMDD)
    DS = 0x4453,  // Decimal String (최대 16자)
    DT = 0x4454,  // Date Time (최대 26자)
    IS = 0x4953,  // Integer String (최대 12자)
    LO = 0x4C4F,  // Long String (최대 64자)
    LT = 0x4C54,  // Long Text (최대 10240자)
    PN = 0x504E,  // Person Name (컴포넌트당 최대 64자)
    SH = 0x5348,  // Short String (최대 16자)
    ST = 0x5354,  // Short Text (최대 1024자)
    TM = 0x544D,  // Time (최대 14자)
    UI = 0x5549,  // Unique Identifier (최대 64자)
    UT = 0x5554,  // Unlimited Text (최대 2^32-2)

    // 바이너리 숫자 VR
    AT = 0x4154,  // Attribute Tag (4바이트)
    FL = 0x464C,  // Floating Point Single (4바이트)
    FD = 0x4644,  // Floating Point Double (8바이트)
    SL = 0x534C,  // Signed Long (4바이트)
    SS = 0x5353,  // Signed Short (2바이트)
    UL = 0x554C,  // Unsigned Long (4바이트)
    US = 0x5553,  // Unsigned Short (2바이트)

    // 바이너리 데이터 VR
    OB = 0x4F42,  // Other Byte
    OD = 0x4F44,  // Other Double
    OF = 0x4F46,  // Other Float
    OL = 0x4F4C,  // Other Long
    OW = 0x4F57,  // Other Word

    // 특수 VR
    SQ = 0x5351,  // Sequence
    UN = 0x554E,  // Unknown
};

} // namespace pacs::encoding
```

**VR 분류 표:**

| VR | 분류 | 최대 길이 | 패딩 | 확장 |
|----|------|----------|------|------|
| AE | 문자열 | 16 | 공백 | 아니오 |
| AS | 문자열 | 4 | 공백 | 아니오 |
| CS | 문자열 | 16 | 공백 | 아니오 |
| DA | 문자열 | 8 | 공백 | 아니오 |
| DS | 문자열 | 16 | 공백 | 아니오 |
| FL | 숫자 | 4 | N/A | 아니오 |
| FD | 숫자 | 8 | N/A | 아니오 |
| OB | 바이너리 | 2^32-2 | 0x00 | 예 |
| OW | 바이너리 | 2^32-2 | N/A | 예 |
| SQ | 시퀀스 | 미정의 | N/A | 예 |

---

### DES-ENC-003: transfer_syntax

**추적:** SRS-CORE-007, FR-1.4

**목적:** Transfer Syntax 관리 및 코덱 선택을 처리합니다.

**Transfer Syntax UID:**

| UID | 이름 | VR | 엔디안 | 캡슐화 |
|-----|------|-----|--------|--------|
| 1.2.840.10008.1.2 | Implicit VR Little Endian | Implicit | Little | 아니오 |
| 1.2.840.10008.1.2.1 | Explicit VR Little Endian | Explicit | Little | 아니오 |
| 1.2.840.10008.1.2.2 | Explicit VR Big Endian | Explicit | Big | 아니오 |
| 1.2.840.10008.1.2.4.50 | JPEG Baseline | Explicit | Little | 예 |
| 1.2.840.10008.1.2.4.90 | JPEG 2000 Lossless | Explicit | Little | 예 |

---

## 3. 네트워크 모듈 컴포넌트

### DES-NET-001: pdu_encoder

**추적:** SRS-NET-001, FR-2.1

**목적:** PDU를 바이트 스트림으로 직렬화합니다.

**PDU 헤더 형식:**

```
┌─────────────────────────────────────────────────────────────────┐
│                       PDU 헤더 (6 바이트)                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────┬────────────┬────────────────────────────────┐   │
│  │  PDU 타입  │   예약     │          PDU 길이              │   │
│  │  (1바이트) │  (1바이트) │        (4바이트, BE)           │   │
│  └────────────┴────────────┴────────────────────────────────┘   │
│                                                                  │
│  PDU 타입:                                                       │
│    0x01 = A-ASSOCIATE-RQ                                        │
│    0x02 = A-ASSOCIATE-AC                                        │
│    0x03 = A-ASSOCIATE-RJ                                        │
│    0x04 = P-DATA-TF                                             │
│    0x05 = A-RELEASE-RQ                                          │
│    0x06 = A-RELEASE-RP                                          │
│    0x07 = A-ABORT                                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### DES-NET-004: association

**추적:** SRS-NET-003, FR-2.2

**목적:** DICOM Association 상태 머신 및 관리를 처리합니다.

**상태 머신 다이어그램:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Association 상태 머신 (PS3.8)                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                            SCU 시작                                          │
│                                                                              │
│  ┌───────┐  A-ASSOCIATE   ┌───────┐  전송계층      ┌───────┐                │
│  │ Sta1  │ ──────────────►│ Sta4  │ ─────────────►│ Sta5  │                │
│  │ 유휴  │   요청         │ 전송  │   연결됨       │ A-AC  │                │
│  └───┬───┘                │ 대기  │               │ 대기  │                │
│      │                    └───────┘               └───┬───┘                │
│      │                                                │                     │
│      │                    ┌───────────────────────────┘                     │
│      │                    │                                                 │
│      │                    │ A-ASSOCIATE-AC 수신                             │
│      │                    ▼                                                 │
│      │               ┌───────┐                                              │
│      │               │ Sta6  │◄────────────────────────────┐               │
│      │               │ Assoc │                              │               │
│      │               │ 확립  │                              │               │
│      │               └───┬───┘                              │               │
│      │                   │                                  │               │
│      │                   │ A-RELEASE-RQ                     │               │
│      │                   ▼                                  │               │
│      │               ┌───────┐                              │               │
│      │               │ Sta7  │                              │               │
│      │               │ A-RP  │                              │               │
│      │               │ 대기  │                              │               │
│      │               └───┬───┘                              │               │
│      │                   │                                  │               │
│      │                   │ A-RELEASE-RP 수신                │               │
│      └───────────────────┴──────────────────────────────────┘               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. 서비스 모듈 컴포넌트

### DES-SVC-002: storage_scp

**추적:** SRS-SVC-002, FR-3.1

**목적:** DICOM Storage SCP (C-STORE 수신기)입니다.

**클래스 설계:**

```cpp
namespace pacs::services {

// 저장 결과 콜백
using storage_handler = std::function<storage_status(
    const core::dicom_dataset& dataset,
    const std::string& calling_ae,
    const std::string& sop_class_uid,
    const std::string& sop_instance_uid
)>;

// Storage SCP 설정
struct storage_scp_config {
    std::string ae_title;
    uint16_t port = 11112;

    // 수락할 SOP 클래스 (비어있으면 모든 저장 SOP 클래스 수락)
    std::vector<std::string> accepted_sop_classes;

    // Transfer Syntax (비어있으면 표준 비압축 수락)
    std::vector<transfer_syntax> accepted_transfer_syntaxes;

    // Association 제한
    size_t max_associations = 10;
    std::chrono::seconds association_timeout{30};
    std::chrono::seconds dimse_timeout{60};

    // PDU 설정
    uint32_t max_pdu_size = 16384;

    // 저장소 백엔드 (선택 - 자동 저장용)
    storage::storage_interface* storage_backend = nullptr;
};

class storage_scp {
public:
    // ─────────────────────────────────────────────────────
    // 생성
    // ─────────────────────────────────────────────────────
    explicit storage_scp(const storage_scp_config& config);
    ~storage_scp();

    // 복사 불가, 이동 불가 (스레드 소유)
    storage_scp(const storage_scp&) = delete;
    storage_scp& operator=(const storage_scp&) = delete;

    // ─────────────────────────────────────────────────────
    // 핸들러 등록
    // ─────────────────────────────────────────────────────
    void set_handler(storage_handler handler);
    void set_pre_store_handler(
        std::function<bool(const core::dicom_dataset&)> handler);

    // ─────────────────────────────────────────────────────
    // 생명주기
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> start();
    void stop();
    void wait_for_shutdown();

    // ─────────────────────────────────────────────────────
    // 상태
    // ─────────────────────────────────────────────────────
    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] size_t active_associations() const noexcept;
    [[nodiscard]] uint64_t total_stored() const noexcept;

    // ─────────────────────────────────────────────────────
    // 통계
    // ─────────────────────────────────────────────────────
    struct statistics {
        uint64_t images_received;
        uint64_t images_stored;
        uint64_t images_failed;
        uint64_t bytes_received;
        std::chrono::steady_clock::time_point start_time;
    };

    [[nodiscard]] statistics get_statistics() const;

private:
    void handle_association(association assoc);
    void handle_c_store(association& assoc, const dimse::c_store_rq& request);

    storage_scp_config config_;
    storage_handler handler_;
    std::unique_ptr<network::dicom_server> server_;
    std::atomic<bool> running_{false};
    std::atomic<size_t> active_associations_{0};
    mutable std::mutex stats_mutex_;
    statistics stats_;
};

} // namespace pacs::services
```

---

## 5. 스토리지 모듈 컴포넌트

### DES-STOR-002: file_storage

**추적:** SRS-STOR-002, FR-4.1

**목적:** 파일 시스템 기반 DICOM 저장소입니다.

**디렉토리 구조 (uid_hierarchical):**

```
{root}/
├── 1.2.840.113619.2.55.3.604688.../     # Study Instance UID
│   ├── 1.2.840.113619.2.55.3.60468.../  # Series Instance UID
│   │   ├── 1.2.840.113619.2.55.3.6...001.dcm  # SOP Instance UID
│   │   ├── 1.2.840.113619.2.55.3.6...002.dcm
│   │   └── 1.2.840.113619.2.55.3.6...003.dcm
│   │
│   └── 1.2.840.113619.2.55.3.60469.../  # 다른 시리즈
│       └── ...
│
└── 1.2.840.113619.2.55.3.604689.../     # 다른 스터디
    └── ...
```

---

### DES-STOR-003: index_database

**추적:** SRS-STOR-003, FR-4.2

**목적:** 빠른 DICOM 쿼리를 위한 SQLite 기반 인덱스입니다.

**클래스 설계:**

```cpp
namespace pacs::storage {

// 인덱스 데이터베이스 설정
struct index_database_config {
    std::filesystem::path db_path;

    bool create_if_not_exists = true;
    bool enable_wal_mode = true;
    size_t cache_size_mb = 64;

    // 연결 풀링
    size_t pool_size = 4;
};

class index_database {
public:
    explicit index_database(const index_database_config& config);
    ~index_database();

    // ─────────────────────────────────────────────────────
    // 인덱싱 작업
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> index(
        const core::dicom_dataset& dataset,
        const std::filesystem::path& file_path);

    [[nodiscard]] common::Result<void> remove(
        const std::string& sop_instance_uid);

    [[nodiscard]] bool exists(
        const std::string& sop_instance_uid) const;

    // ─────────────────────────────────────────────────────
    // 쿼리 작업
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::vector<core::dicom_dataset> find_patients(
        const core::dicom_dataset& query_keys) const;

    [[nodiscard]] std::vector<core::dicom_dataset> find_studies(
        const core::dicom_dataset& query_keys) const;

    [[nodiscard]] std::vector<core::dicom_dataset> find_series(
        const core::dicom_dataset& query_keys) const;

    [[nodiscard]] std::vector<core::dicom_dataset> find_instances(
        const core::dicom_dataset& query_keys) const;

    // ─────────────────────────────────────────────────────
    // 경로 조회
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::optional<std::filesystem::path> get_file_path(
        const std::string& sop_instance_uid) const;

    // ─────────────────────────────────────────────────────
    // 통계
    // ─────────────────────────────────────────────────────
    struct statistics {
        size_t patient_count;
        size_t study_count;
        size_t series_count;
        size_t instance_count;
        uint64_t total_size_bytes;
    };

    [[nodiscard]] statistics get_statistics() const;

private:
    void create_schema();
    std::string build_where_clause(
        const core::dicom_dataset& keys) const;

    index_database_config config_;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db_;
    mutable std::shared_mutex db_mutex_;
};

} // namespace pacs::storage
```

---

## 6. 통합 모듈 컴포넌트

### DES-INT-001: container_adapter

**추적:** IR-1 (container_system 통합)

**목적:** DICOM VR 타입을 container_system 값에 매핑합니다.

**VR에서 Container 타입 매핑:**

| VR 분류 | DICOM VR | container::value 타입 |
|---------|----------|----------------------|
| 문자열 | AE, AS, CS, DA, DS, DT, IS, LO, LT, PN, SH, ST, TM, UI, UT | `std::string` |
| 정수 | SS, US, SL, UL | `int64_t` |
| 실수 | FL, FD, DS | `double` |
| 바이너리 | OB, OW, OF, OD, OL, UN | `std::vector<uint8_t>` |
| 시퀀스 | SQ | `std::vector<value_container>` |

---

### DES-INT-002: network_adapter

**추적:** IR-2 (network_system 통합)

**목적:** DICOM 프로토콜용 network_system을 어댑트합니다.

---

### DES-INT-003: itk_adapter

**추적:** Issue #463 (ITK/VTK 통합 어댑터)

**목적:** dicom_viewer 통합을 위해 pacs_system DICOM 데이터 구조를 ITK 이미지 타입으로 변환합니다.

**주요 기능:**

| 함수 | 설명 |
|------|------|
| `extract_metadata()` | DICOM 데이터셋에서 이미지 메타데이터 추출 |
| `dataset_to_image<>()` | 단일 데이터셋을 ITK 이미지로 변환 |
| `series_to_image<>()` | DICOM 시리즈를 3D ITK 볼륨으로 변환 |
| `load_ct_series()` | CT 시리즈를 HU 변환과 함께 로드 |
| `load_mr_series()` | MR 시리즈 로드 |
| `scan_dicom_directory()` | 디렉토리에서 DICOM 파일 검색 |
| `group_by_series()` | Series Instance UID로 파일 그룹화 |

**DICOM-ITK 매핑:**

| DICOM 태그 | ITK 속성 |
|-----------|---------|
| Image Position Patient (0020,0032) | 이미지 원점 |
| Pixel Spacing (0028,0030) | 이미지 간격 [0], [1] |
| Slice Thickness (0018,0050) | 이미지 간격 [2] |
| Image Orientation Patient (0020,0037) | 방향 코사인 |

**조건부 컴파일:**

- CMake 옵션: `PACS_BUILD_ITK_ADAPTER=ON`
- ITK 5.x 개발 헤더 필요
- 컴파일러 정의: `PACS_WITH_ITK`

---

*문서 버전: 0.1.2*
*작성일: 2025-11-30*
*작성자: kcenon@naver.com*
