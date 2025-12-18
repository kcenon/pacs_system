# API 참조 - PACS 시스템

> **버전:** 0.1.4.0
> **최종 수정일:** 2025-12-08
> **언어:** [English](API_REFERENCE.md) | **한국어**

---

## 목차

- [코어 모듈](#코어-모듈)
- [인코딩 모듈](#인코딩-모듈)
- [네트워크 모듈](#네트워크-모듈)
- [서비스 모듈](#서비스-모듈)
- [저장 모듈](#저장-모듈)
- [AI 모듈](#ai-모듈)
- [모니터링 모듈](#모니터링-모듈)
- [통합 모듈](#통합-모듈)
- [DI 모듈](#di-모듈)
- [공통 모듈](#공통-모듈)

---

## 코어 모듈

### `pacs::core::dicom_tag`

DICOM 태그 (그룹, 요소 쌍)를 나타냅니다.

```cpp
#include <pacs/core/dicom_tags.h>

namespace pacs::core {

class dicom_tag {
public:
    // 생성자
    constexpr dicom_tag(uint16_t group, uint16_t element);
    constexpr dicom_tag(uint32_t tag);  // 결합된 태그 값

    // 접근자
    constexpr uint16_t group() const noexcept;
    constexpr uint16_t element() const noexcept;
    constexpr uint32_t value() const noexcept;

    // 비교
    constexpr bool operator==(const dicom_tag& other) const noexcept;
    constexpr bool operator<(const dicom_tag& other) const noexcept;

    // 문자열 표현
    std::string to_string() const;  // "(GGGG,EEEE)"
};

// 표준 태그 상수
namespace tags {
    // 환자 모듈
    constexpr dicom_tag PatientName{0x0010, 0x0010};
    constexpr dicom_tag PatientID{0x0010, 0x0020};
    constexpr dicom_tag PatientBirthDate{0x0010, 0x0030};
    constexpr dicom_tag PatientSex{0x0010, 0x0040};

    // 스터디 모듈
    constexpr dicom_tag StudyInstanceUID{0x0020, 0x000D};
    constexpr dicom_tag StudyDate{0x0008, 0x0020};
    constexpr dicom_tag StudyTime{0x0008, 0x0030};
    constexpr dicom_tag AccessionNumber{0x0008, 0x0050};

    // 시리즈 모듈
    constexpr dicom_tag SeriesInstanceUID{0x0020, 0x000E};
    constexpr dicom_tag Modality{0x0008, 0x0060};
    constexpr dicom_tag SeriesNumber{0x0020, 0x0011};

    // 인스턴스 모듈
    constexpr dicom_tag SOPInstanceUID{0x0008, 0x0018};
    constexpr dicom_tag SOPClassUID{0x0008, 0x0016};
    constexpr dicom_tag InstanceNumber{0x0020, 0x0013};

    // 이미지 모듈
    constexpr dicom_tag Rows{0x0028, 0x0010};
    constexpr dicom_tag Columns{0x0028, 0x0011};
    constexpr dicom_tag BitsAllocated{0x0028, 0x0100};
    constexpr dicom_tag PixelData{0x7FE0, 0x0010};

    // ... (전체 목록은 dicom_tags.h 참조)
}

} // namespace pacs::core
```

---

### `pacs::core::dicom_element`

DICOM 데이터 요소를 나타냅니다.

```cpp
#include <pacs/core/dicom_element.h>

namespace pacs::core {

class dicom_element {
public:
    // 팩토리 메서드
    static dicom_element create(dicom_tag tag, vr_type vr);
    static dicom_element create(dicom_tag tag, vr_type vr, const std::string& value);
    static dicom_element create(dicom_tag tag, vr_type vr, const std::vector<uint8_t>& bytes);

    template<typename T>
    static dicom_element create_numeric(dicom_tag tag, vr_type vr, T value);

    // 접근자
    dicom_tag tag() const noexcept;
    vr_type vr() const noexcept;
    uint32_t length() const noexcept;
    bool is_empty() const noexcept;

    // 값 접근
    std::string as_string() const;
    std::vector<std::string> as_strings() const;  // 다중 값

    template<typename T>
    T as_numeric() const;

    std::vector<uint8_t> as_bytes() const;

    // SQ (시퀀스)용
    bool is_sequence() const noexcept;
    std::vector<dicom_dataset>& items();
    const std::vector<dicom_dataset>& items() const;

    // 값 수정
    void set_string(const std::string& value);
    void set_bytes(const std::vector<uint8_t>& bytes);

    template<typename T>
    void set_numeric(T value);

    // 직렬화
    std::vector<uint8_t> serialize(const transfer_syntax& ts) const;
    static common::Result<dicom_element> deserialize(
        const std::vector<uint8_t>& data,
        const transfer_syntax& ts
    );
};

} // namespace pacs::core
```

**예제:**
```cpp
using namespace pacs::core;

// 문자열 요소 생성
auto patient_name = dicom_element::create(
    tags::PatientName,
    vr_type::PN,
    "Doe^John"
);

// 숫자 요소 생성
auto rows = dicom_element::create_numeric(
    tags::Rows,
    vr_type::US,
    uint16_t{512}
);

// 값 접근
std::cout << patient_name.as_string() << std::endl;  // "Doe^John"
std::cout << rows.as_numeric<uint16_t>() << std::endl;  // 512
```

---

### `pacs::core::dicom_dataset`

DICOM 데이터 요소의 정렬된 컬렉션.

```cpp
#include <pacs/core/dicom_dataset.h>

namespace pacs::core {

class dicom_dataset {
public:
    // 생성
    dicom_dataset() = default;
    dicom_dataset(const dicom_dataset& other);
    dicom_dataset(dicom_dataset&& other) noexcept;

    // 요소 접근
    bool contains(dicom_tag tag) const noexcept;
    const dicom_element* get(dicom_tag tag) const;
    dicom_element* get(dicom_tag tag);

    // 타입별 게터 (없으면 기본값 반환)
    std::string get_string(dicom_tag tag, const std::string& default_value = "") const;
    std::vector<std::string> get_strings(dicom_tag tag) const;

    template<typename T>
    T get_numeric(dicom_tag tag, T default_value = T{}) const;

    std::vector<uint8_t> get_bytes(dicom_tag tag) const;

    // 타입별 세터
    void set_string(dicom_tag tag, const std::string& value);
    void set_strings(dicom_tag tag, const std::vector<std::string>& values);

    template<typename T>
    void set_numeric(dicom_tag tag, T value);

    void set_bytes(dicom_tag tag, const std::vector<uint8_t>& bytes);

    // 요소 관리
    void add(dicom_element element);
    void remove(dicom_tag tag);
    void clear();

    // 반복 (태그 순서)
    using iterator = /* 구현 정의 */;
    using const_iterator = /* 구현 정의 */;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    // 속성
    size_t size() const noexcept;
    bool empty() const noexcept;

    // 직렬화
    std::vector<uint8_t> serialize(const transfer_syntax& ts) const;
    static common::Result<dicom_dataset> deserialize(
        const std::vector<uint8_t>& data,
        const transfer_syntax& ts
    );

    // 부분집합 복사
    dicom_dataset subset(const std::vector<dicom_tag>& tags) const;

    // 병합
    void merge(const dicom_dataset& other, bool overwrite = false);
};

} // namespace pacs::core
```

**예제:**
```cpp
using namespace pacs::core;

dicom_dataset dataset;

// 값 설정
dataset.set_string(tags::PatientName, "Doe^John");
dataset.set_string(tags::PatientID, "12345");
dataset.set_numeric(tags::Rows, uint16_t{512});
dataset.set_numeric(tags::Columns, uint16_t{512});

// 값 가져오기
auto name = dataset.get_string(tags::PatientName);
auto rows = dataset.get_numeric<uint16_t>(tags::Rows);

// 존재 여부 확인
if (dataset.contains(tags::PixelData)) {
    auto pixels = dataset.get_bytes(tags::PixelData);
}

// 반복
for (const auto& element : dataset) {
    std::cout << element.tag().to_string() << ": "
              << element.as_string() << std::endl;
}
```

---

### `pacs::core::dicom_file`

DICOM Part 10 파일 처리.

```cpp
#include <pacs/core/dicom_file.h>

namespace pacs::core {

class dicom_file {
public:
    // 팩토리 메서드
    static common::Result<dicom_file> read(const std::filesystem::path& path);
    static common::Result<dicom_file> read(std::istream& stream);

    // 생성
    dicom_file() = default;

    // 파일 메타 정보
    const dicom_dataset& file_meta_info() const noexcept;
    dicom_dataset& file_meta_info() noexcept;

    // 데이터 세트
    const dicom_dataset& dataset() const noexcept;
    dicom_dataset& dataset() noexcept;
    void set_dataset(dicom_dataset dataset);

    // Transfer Syntax
    transfer_syntax transfer_syntax() const;
    void set_transfer_syntax(transfer_syntax ts);

    // 미디어 저장 SOP
    std::string media_storage_sop_class_uid() const;
    std::string media_storage_sop_instance_uid() const;

    // 쓰기
    common::Result<void> write(const std::filesystem::path& path) const;
    common::Result<void> write(std::ostream& stream) const;

    // 검증
    common::Result<void> validate() const;
};

} // namespace pacs::core
```

**예제:**
```cpp
using namespace pacs::core;

// 파일 읽기
auto result = dicom_file::read("/path/to/image.dcm");
if (result.is_err()) {
    std::cerr << "읽기 실패: " << result.error().message << std::endl;
    return;
}

auto& file = result.value();

// 데이터 접근
std::cout << "환자: " << file.dataset().get_string(tags::PatientName) << std::endl;
std::cout << "Transfer Syntax: " << file.transfer_syntax().uid() << std::endl;

// 수정 후 쓰기
file.dataset().set_string(tags::InstitutionName, "My Hospital");
auto write_result = file.write("/path/to/modified.dcm");
```

---

## 인코딩 모듈

### `pacs::encoding::vr_type`

DICOM Value Representation의 열거형.

```cpp
#include <pacs/encoding/vr_types.h>

namespace pacs::encoding {

enum class vr_type : uint16_t {
    AE = 0x4145,  // Application Entity
    AS = 0x4153,  // Age String
    AT = 0x4154,  // Attribute Tag
    CS = 0x4353,  // Code String
    DA = 0x4441,  // Date
    DS = 0x4453,  // Decimal String
    DT = 0x4454,  // Date Time
    FL = 0x464C,  // Floating Point Single
    FD = 0x4644,  // Floating Point Double
    IS = 0x4953,  // Integer String
    LO = 0x4C4F,  // Long String
    LT = 0x4C54,  // Long Text
    OB = 0x4F42,  // Other Byte
    OD = 0x4F44,  // Other Double
    OF = 0x4F46,  // Other Float
    OL = 0x4F4C,  // Other Long
    OW = 0x4F57,  // Other Word
    PN = 0x504E,  // Person Name
    SH = 0x5348,  // Short String
    SL = 0x534C,  // Signed Long
    SQ = 0x5351,  // Sequence
    SS = 0x5353,  // Signed Short
    ST = 0x5354,  // Short Text
    TM = 0x544D,  // Time
    UI = 0x5549,  // Unique Identifier
    UL = 0x554C,  // Unsigned Long
    UN = 0x554E,  // Unknown
    US = 0x5553,  // Unsigned Short
    UT = 0x5554   // Unlimited Text
};

// VR 유틸리티
class vr_info {
public:
    static bool is_string_vr(vr_type vr) noexcept;
    static bool is_numeric_vr(vr_type vr) noexcept;
    static bool is_binary_vr(vr_type vr) noexcept;
    static bool is_sequence_vr(vr_type vr) noexcept;

    static uint32_t max_length(vr_type vr) noexcept;
    static char padding_character(vr_type vr) noexcept;
    static bool uses_extended_length(vr_type vr) noexcept;

    static std::string to_string(vr_type vr);
    static vr_type from_string(const std::string& str);
};

} // namespace pacs::encoding
```

---

### `pacs::encoding::transfer_syntax`

Transfer Syntax 관리.

```cpp
#include <pacs/encoding/transfer_syntax.h>

namespace pacs::encoding {

class transfer_syntax {
public:
    // 사전 정의된 transfer syntax
    static transfer_syntax implicit_vr_little_endian();
    static transfer_syntax explicit_vr_little_endian();
    static transfer_syntax explicit_vr_big_endian();

    // 생성
    explicit transfer_syntax(const std::string& uid);

    // 속성
    const std::string& uid() const noexcept;
    bool is_implicit_vr() const noexcept;
    bool is_little_endian() const noexcept;
    bool is_encapsulated() const noexcept;

    // 비교
    bool operator==(const transfer_syntax& other) const noexcept;

    // 이름 조회
    std::string name() const;

    // 검증
    static bool is_valid_uid(const std::string& uid);
};

} // namespace pacs::encoding
```

---

### `pacs::encoding::compression::compression_codec`

이미지 압축 코덱의 추상 기본 클래스.

```cpp
#include <pacs/encoding/compression/compression_codec.hpp>

namespace pacs::encoding::compression {

// 압축 옵션
struct compression_options {
    int quality = 75;           // JPEG의 경우 1-100
    bool lossless = false;       // 무손실 모드 활성화 (지원 시)
    bool progressive = false;    // 프로그레시브 인코딩 (JPEG)
    int chroma_subsampling = 2;  // 0=4:4:4, 1=4:2:2, 2=4:2:0
};

// 압축/압축해제 성공 결과
struct compression_result {
    std::vector<uint8_t> data;    // 처리된 픽셀 데이터
    image_params output_params;    // 출력 이미지 파라미터
};

// pacs::Result<T> 패턴을 사용한 결과 타입 별칭
using codec_result = pacs::Result<compression_result>;

// 사용 예시:
// auto result = codec.encode(pixels, params, options);
// if (result.is_err()) {
//     std::cerr << pacs::get_error(result).message << std::endl;
// } else {
//     auto& data = pacs::get_value(result).data;
// }

// 추상 코덱 인터페이스
class compression_codec {
public:
    virtual ~compression_codec() = default;

    // 코덱 정보
    [[nodiscard]] virtual std::string_view transfer_syntax_uid() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual bool is_lossy() const noexcept = 0;
    [[nodiscard]] virtual bool can_encode(const image_params& params) const noexcept = 0;
    [[nodiscard]] virtual bool can_decode(const image_params& params) const noexcept = 0;

    // 압축 연산
    [[nodiscard]] virtual codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const = 0;

    [[nodiscard]] virtual codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const = 0;
};

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::jpeg_baseline_codec`

libjpeg-turbo를 사용한 JPEG Baseline (Process 1) 코덱.

```cpp
#include <pacs/encoding/compression/jpeg_baseline_codec.hpp>

namespace pacs::encoding::compression {

class jpeg_baseline_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.4.50";

    jpeg_baseline_codec();
    ~jpeg_baseline_codec() override;

    // 이동 전용 (PIMPL)
    jpeg_baseline_codec(jpeg_baseline_codec&&) noexcept;
    jpeg_baseline_codec& operator=(jpeg_baseline_codec&&) noexcept;

    // 코덱 인터페이스 구현
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;  // "JPEG Baseline (Process 1)"
    [[nodiscard]] bool is_lossy() const noexcept override;  // true
    [[nodiscard]] bool can_encode(const image_params& params) const noexcept override;
    [[nodiscard]] bool can_decode(const image_params& params) const noexcept override;

    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;
};

// 사용 예시
void compress_image() {
    jpeg_baseline_codec codec;

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.samples_per_pixel = 1;  // 그레이스케일

    std::vector<uint8_t> pixel_data(512 * 512);  // 이미지 데이터

    compression_options options;
    options.quality = 90;

    auto result = codec.encode(pixel_data, params, options);
    if (result.success) {
        // result.data에 압축된 JPEG 포함
    }
}

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::jpeg_lossless_codec`

진단 품질 이미지를 위한 JPEG 무손실 (Process 14, Selection Value 1) 코덱.

```cpp
#include <pacs/encoding/compression/jpeg_lossless_codec.hpp>

namespace pacs::encoding::compression {

class jpeg_lossless_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.4.70";
    static constexpr int kDefaultPredictor = 1;       // Ra (왼쪽 이웃)
    static constexpr int kDefaultPointTransform = 0;  // 스케일링 없음

    // 선택적 predictor (1-7)와 point transform (0-15) 지정
    explicit jpeg_lossless_codec(int predictor = kDefaultPredictor,
                                  int point_transform = kDefaultPointTransform);
    ~jpeg_lossless_codec() override;

    // 이동 전용 (PIMPL)
    jpeg_lossless_codec(jpeg_lossless_codec&&) noexcept;
    jpeg_lossless_codec& operator=(jpeg_lossless_codec&&) noexcept;

    // 설정
    [[nodiscard]] int predictor() const noexcept;
    [[nodiscard]] int point_transform() const noexcept;

    // 코덱 인터페이스 구현
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;  // "JPEG Lossless (Process 14, SV1)"
    [[nodiscard]] bool is_lossy() const noexcept override;  // false (무손실!)

    [[nodiscard]] bool can_encode(const image_params& params) const noexcept override;
    [[nodiscard]] bool can_decode(const image_params& params) const noexcept override;

    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;
};

// 지원 Predictor:
// 1: Ra (왼쪽 이웃)
// 2: Rb (위쪽 이웃)
// 3: Rc (대각선 왼쪽 위 이웃)
// 4: Ra + Rb - Rc
// 5: Ra + (Rb - Rc) / 2
// 6: Rb + (Ra - Rc) / 2
// 7: (Ra + Rb) / 2

// 지원 비트 깊이: 8, 12, 16비트 흑백

// 사용 예시
void compress_ct_image_lossless() {
    jpeg_lossless_codec codec;  // 기본 predictor 1

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 16;
    params.bits_stored = 12;    // 12비트 CT 이미지
    params.samples_per_pixel = 1;  // 흑백 전용

    std::vector<uint8_t> pixel_data(512 * 512 * 2);  // 16비트 저장

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data에 무손실 압축된 JPEG 포함
        // 원본 데이터를 완벽하게 복원 가능
    }
}

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::jpeg2000_codec`

무손실 및 손실 압축 모드를 모두 지원하는 JPEG 2000 코덱.

```cpp
#include <pacs/encoding/compression/jpeg2000_codec.hpp>

namespace pacs::encoding::compression {

class jpeg2000_codec final : public compression_codec {
public:
    // Transfer Syntax UID
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.90";  // JPEG 2000 무손실 전용
    static constexpr std::string_view kTransferSyntaxUIDLossy =
        "1.2.840.10008.1.2.4.91";  // JPEG 2000 (손실 또는 무손실)

    static constexpr float kDefaultCompressionRatio = 20.0f;
    static constexpr int kDefaultResolutionLevels = 6;

    // 모드 선택으로 생성
    // lossless: 4.90(무손실 전용)은 true, 4.91(기본 손실)은 false
    // compression_ratio: 손실 모드의 목표 비율 (무손실에서는 무시)
    // resolution_levels: DWT 해상도 레벨 수 (1-32)
    explicit jpeg2000_codec(bool lossless = true,
                            float compression_ratio = kDefaultCompressionRatio,
                            int resolution_levels = kDefaultResolutionLevels);
    ~jpeg2000_codec() override;

    // Move-only (PIMPL)
    jpeg2000_codec(jpeg2000_codec&&) noexcept;
    jpeg2000_codec& operator=(jpeg2000_codec&&) noexcept;

    // 설정
    [[nodiscard]] bool is_lossless_mode() const noexcept;
    [[nodiscard]] float compression_ratio() const noexcept;
    [[nodiscard]] int resolution_levels() const noexcept;

    // 코덱 인터페이스 구현
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;  // "JPEG 2000 Lossless" 또는 "JPEG 2000"
    [[nodiscard]] bool is_lossy() const noexcept override;

    [[nodiscard]] bool can_encode(const image_params& params) const noexcept override;
    [[nodiscard]] bool can_decode(const image_params& params) const noexcept override;

    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;
};

// 지원 기능:
// - 비트 깊이: 1-16비트 (의료용은 주로 8, 12, 16비트)
// - 그레이스케일 (samples_per_pixel = 1) 및 컬러 (samples_per_pixel = 3)
// - 무손실: 가역 5/3 정수 웨이블릿 변환
// - 손실: 비가역 9/7 부동소수점 웨이블릿 변환
// - 디코딩 시 J2K/JP2 형식 자동 감지

// 사용 예시 - 진단용 이미지의 무손실 압축
void compress_mri_lossless() {
    jpeg2000_codec codec(true);  // 무손실 모드

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 16;
    params.bits_stored = 16;    // 16비트 MRI
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(512 * 512 * 2);

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data에 무손실 압축된 J2K 코드스트림 포함
    }
}

// 사용 예시 - 보관용 손실 압축
void compress_xray_lossy() {
    jpeg2000_codec codec(false, 20.0f);  // 손실, 20:1 비율

    image_params params;
    params.width = 2048;
    params.height = 2048;
    params.bits_allocated = 16;
    params.bits_stored = 12;
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(2048 * 2048 * 2);

    compression_options options;
    options.quality = 80;  // 압축 비율에 매핑

    auto result = codec.encode(pixel_data, params, options);
    if (result.success) {
        // result.data에 손실 압축된 J2K 코드스트림 포함
    }
}

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::jpeg_ls_codec`

무손실 및 근무손실 압축 모드를 지원하는 JPEG-LS 코덱.

```cpp
#include <pacs/encoding/compression/jpeg_ls_codec.hpp>

namespace pacs::encoding::compression {

class jpeg_ls_codec final : public compression_codec {
public:
    // Transfer Syntax UID
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.80";  // JPEG-LS 무손실
    static constexpr std::string_view kTransferSyntaxUIDNearLossless =
        "1.2.840.10008.1.2.4.81";  // JPEG-LS 근무손실 (손실)

    static constexpr int kAutoNearValue = -1;  // NEAR 자동 결정
    static constexpr int kLosslessNearValue = 0;
    static constexpr int kDefaultNearLosslessValue = 2;
    static constexpr int kMaxNearValue = 255;

    // 모드 선택으로 생성
    // lossless: true면 4.80 (무손실), false면 4.81 (근무손실)
    // near_value: -1 = 자동, 0 = 무손실, 1-255 = 제한된 오류
    explicit jpeg_ls_codec(bool lossless = true, int near_value = kAutoNearValue);
    ~jpeg_ls_codec() override;

    // 이동 전용 (PIMPL)
    jpeg_ls_codec(jpeg_ls_codec&&) noexcept;
    jpeg_ls_codec& operator=(jpeg_ls_codec&&) noexcept;

    // 구성
    [[nodiscard]] bool is_lossless_mode() const noexcept;
    [[nodiscard]] int near_value() const noexcept;

    // 코덱 인터페이스 구현
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;  // "JPEG-LS Lossless" 또는 "JPEG-LS Near-Lossless"
    [[nodiscard]] bool is_lossy() const noexcept override;

    [[nodiscard]] bool can_encode(const image_params& params) const noexcept override;
    [[nodiscard]] bool can_decode(const image_params& params) const noexcept override;

    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;
};

// 지원 기능:
// - 비트 깊이: 2-16비트 (의료 영상에서 일반적으로 8, 12, 16비트)
// - 그레이스케일 (samples_per_pixel = 1) 및 컬러 (samples_per_pixel = 3)
// - 무손실: NEAR=0, 정확한 재구성
// - 근무손실: NEAR>0, 제한된 오류 (최대 |원본 - 디코딩| <= NEAR)
// - 고성능 인코딩/디코딩을 위해 CharLS 라이브러리 사용 (BSD-3 라이선스)

// 사용 예시 - CT 영상 무손실 압축
void compress_ct_lossless() {
    jpeg_ls_codec codec(true);  // 무손실 모드

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 16;
    params.bits_stored = 12;    // 12비트 CT
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(512 * 512 * 2);

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data에 무손실 압축된 JPEG-LS 데이터 포함
        // 일반적인 압축률: 의료 영상의 경우 2:1 ~ 4:1
    }
}

// 사용 예시 - 아카이빙용 근무손실 압축
void compress_xray_near_lossless() {
    jpeg_ls_codec codec(false, 2);  // 근무손실, NEAR=2

    image_params params;
    params.width = 2048;
    params.height = 2048;
    params.bits_allocated = 16;
    params.bits_stored = 12;
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(2048 * 2048 * 2);

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data에 근무손실 압축된 데이터 포함
        // 최대 픽셀 오류: 2 (시각적으로 무손실 품질)
    }
}

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::codec_factory`

Transfer Syntax UID로 압축 코덱을 생성하는 팩토리.

```cpp
#include <pacs/encoding/compression/codec_factory.hpp>

namespace pacs::encoding::compression {

class codec_factory {
public:
    // Transfer Syntax UID로 코덱 생성
    [[nodiscard]] static std::unique_ptr<compression_codec> create(
        std::string_view transfer_syntax_uid);

    // transfer_syntax 객체로 코덱 생성
    [[nodiscard]] static std::unique_ptr<compression_codec> create(
        const transfer_syntax& ts);

    // 지원 코덱 조회
    [[nodiscard]] static std::vector<std::string_view> supported_transfer_syntaxes();
    [[nodiscard]] static bool is_supported(std::string_view transfer_syntax_uid);
    [[nodiscard]] static bool is_supported(const transfer_syntax& ts);
};

// 현재 지원되는 Transfer Syntax:
// - 1.2.840.10008.1.2.4.50 - JPEG Baseline (Process 1) - 손실
// - 1.2.840.10008.1.2.4.70 - JPEG Lossless (Process 14, SV1) - 무손실
// - 1.2.840.10008.1.2.4.80 - JPEG-LS 무손실 이미지 압축
// - 1.2.840.10008.1.2.4.81 - JPEG-LS 손실 (근무손실) 이미지 압축
// - 1.2.840.10008.1.2.4.90 - JPEG 2000 무손실 전용
// - 1.2.840.10008.1.2.4.91 - JPEG 2000 (손실 또는 무손실)

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::image_params`

압축을 위한 이미지 픽셀 데이터 파라미터.

```cpp
#include <pacs/encoding/compression/image_params.hpp>

namespace pacs::encoding::compression {

enum class photometric_interpretation {
    monochrome1,      // 최소값 = 흰색
    monochrome2,      // 최소값 = 검은색
    rgb,              // RGB 컬러
    ycbcr_full,       // YCbCr 전체 범위
    ycbcr_full_422,   // YCbCr 4:2:2
    palette_color,    // 팔레트 조회
    unknown
};

struct image_params {
    uint16_t width = 0;               // Columns (0028,0011)
    uint16_t height = 0;              // Rows (0028,0010)
    uint16_t bits_allocated = 0;      // (0028,0100)
    uint16_t bits_stored = 0;         // (0028,0101)
    uint16_t high_bit = 0;            // (0028,0102)
    uint16_t samples_per_pixel = 1;   // (0028,0002)
    uint16_t planar_configuration = 0; // (0028,0006)
    uint16_t pixel_representation = 0; // (0028,0103)
    photometric_interpretation photometric = photometric_interpretation::monochrome2;
    uint32_t number_of_frames = 1;

    // 유틸리티 메서드
    [[nodiscard]] size_t frame_size_bytes() const noexcept;
    [[nodiscard]] bool is_grayscale() const noexcept;
    [[nodiscard]] bool is_color() const noexcept;
    [[nodiscard]] bool is_signed() const noexcept;
    [[nodiscard]] bool valid_for_jpeg_baseline() const noexcept;  // 8비트 전용
    [[nodiscard]] bool valid_for_jpeg_lossless() const noexcept;  // 2-16비트 흑백
    [[nodiscard]] bool valid_for_jpeg_ls() const noexcept;        // 2-16비트 흑백/컬러
    [[nodiscard]] bool valid_for_jpeg2000() const noexcept;       // 1-16비트 흑백/컬러
};

// 문자열 변환
[[nodiscard]] std::string to_string(photometric_interpretation pi);
[[nodiscard]] photometric_interpretation parse_photometric_interpretation(const std::string& str);

} // namespace pacs::encoding::compression
```

---

## 네트워크 모듈

### `pacs::network::association`

DICOM 연결 관리.

```cpp
#include <pacs/network/association.h>

namespace pacs::network {

// 연결 구성
struct association_config {
    std::string calling_ae_title;    // SCU AE Title
    std::string called_ae_title;     // SCP AE Title
    uint32_t max_pdu_size = 16384;   // 최대 PDU 크기
    std::chrono::seconds timeout{30}; // 연결 타임아웃

    // 프레젠테이션 컨텍스트 추가
    void add_context(
        const std::string& abstract_syntax,
        const std::vector<transfer_syntax>& transfer_syntaxes
    );
};

// 연결 상태
enum class association_state {
    idle,
    awaiting_transport,
    awaiting_associate_response,
    awaiting_release_response,
    established,
    released,
    aborted
};

class association {
public:
    // 팩토리 (SCU)
    static common::Result<association> connect(
        const std::string& host,
        uint16_t port,
        const association_config& config
    );

    // 팩토리 (SCP) - 서버에서 생성
    // association은 이동 전용
    association(association&&) noexcept;
    association& operator=(association&&) noexcept;
    ~association();

    // 상태
    association_state state() const noexcept;
    bool is_established() const noexcept;

    // 속성
    const std::string& calling_ae_title() const noexcept;
    const std::string& called_ae_title() const noexcept;
    uint32_t max_pdu_size() const noexcept;

    // 프레젠테이션 컨텍스트
    bool has_accepted_context(const std::string& abstract_syntax) const;
    std::optional<uint8_t> accepted_context_id(
        const std::string& abstract_syntax
    ) const;
    transfer_syntax accepted_transfer_syntax(uint8_t context_id) const;

    // DIMSE 연산
    common::Result<void> send_dimse(
        uint8_t context_id,
        const dimse::dimse_message& message
    );

    common::Result<dimse::dimse_message> receive_dimse();

    // 라이프사이클
    common::Result<void> release();
    void abort();
};

} // namespace pacs::network
```

**예제:**
```cpp
using namespace pacs::network;

// 연결 구성
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.add_context(
    sop_class::verification,
    {transfer_syntax::implicit_vr_little_endian()}
);

// 연결
auto result = association::connect("192.168.1.100", 11112, config);
if (result.is_err()) {
    std::cerr << "연결 실패: " << result.error().message << std::endl;
    return;
}

auto assoc = std::move(result.value());

// C-ECHO 수행
// ...

// 해제
assoc.release();
```

---

### `pacs::network::v2::dicom_server_v2`

network_system의 messaging_server를 사용하는 DICOM 서버 구현.
새로운 애플리케이션에 권장되는 서버 구현입니다.

```cpp
#include <pacs/network/v2/dicom_server_v2.hpp>

namespace pacs::network::v2 {

class dicom_server_v2 {
public:
    // 타입 별칭
    using clock = std::chrono::steady_clock;
    using duration = std::chrono::milliseconds;
    using time_point = clock::time_point;
    using association_established_callback =
        std::function<void(const std::string& session_id,
                           const std::string& calling_ae,
                           const std::string& called_ae)>;
    using association_closed_callback =
        std::function<void(const std::string& session_id, bool graceful)>;
    using error_callback = std::function<void(const std::string& error)>;

    // 생성
    explicit dicom_server_v2(const server_config& config);
    ~dicom_server_v2();

    // 복사/이동 불가
    dicom_server_v2(const dicom_server_v2&) = delete;
    dicom_server_v2& operator=(const dicom_server_v2&) = delete;

    // 서비스 등록 (start 전에 호출)
    void register_service(services::scp_service_ptr service);
    std::vector<std::string> supported_sop_classes() const;

    // 생명주기
    Result<std::monostate> start();
    void stop(duration timeout = std::chrono::seconds{30});
    void wait_for_shutdown();

    // 상태
    bool is_running() const noexcept;
    size_t active_associations() const noexcept;
    server_statistics get_statistics() const;
    const server_config& config() const noexcept;

    // 콜백
    void on_association_established(association_established_callback callback);
    void on_association_closed(association_closed_callback callback);
    void on_error(error_callback callback);
};

} // namespace pacs::network::v2
```

**사용 예제:**

```cpp
#include <pacs/network/v2/dicom_server_v2.hpp>
#include <pacs/services/verification_scp.hpp>
#include <pacs/services/storage_scp.hpp>

using namespace pacs::network;
using namespace pacs::network::v2;
using namespace pacs::services;

// 서버 설정
server_config config;
config.ae_title = "MY_PACS";
config.port = 11112;
config.max_associations = 20;
config.idle_timeout = std::chrono::minutes{5};

// 서버 생성
dicom_server_v2 server{config};

// 서비스 등록
server.register_service(std::make_unique<verification_scp>());
server.register_service(std::make_unique<storage_scp>("/path/to/storage"));

// 콜백 설정
server.on_association_established(
    [](const std::string& session_id,
       const std::string& calling_ae,
       const std::string& called_ae) {
        std::cout << "연결: " << calling_ae << " -> " << called_ae << '\n';
    });

server.on_association_closed(
    [](const std::string& session_id, bool graceful) {
        std::cout << "세션 " << session_id << " 종료 "
                  << (graceful ? "(정상)" : "(비정상)") << '\n';
    });

server.on_error([](const std::string& error) {
    std::cerr << "서버 오류: " << error << '\n';
});

// 서버 시작
auto result = server.start();
if (result.is_err()) {
    std::cerr << "시작 실패: " << result.error().message << '\n';
    return 1;
}

std::cout << "서버가 포트 " << config.port << "에서 실행 중\n";

// 종료 신호 대기
server.wait_for_shutdown();
```

**주요 기능:**

- 기존 `dicom_server`와 **API 호환** - 쉬운 마이그레이션
- network_system 콜백을 통한 **자동 세션 관리**
- 적절한 동기화로 **스레드 안전** 핸들러 맵
- 자동 거부로 **최대 연결 제한** 설정 가능
- 비활성 연결 정리를 위한 **유휴 타임아웃 지원**

**dicom_server에서 마이그레이션:**

1. `dicom_server`를 `dicom_server_v2`로 교체
2. 서비스 등록이나 콜백 변경 불필요
3. `PACS_WITH_NETWORK_SYSTEM` 컴파일 정의 필요

---

## 서비스 모듈

### `pacs::services::verification_scu`

검증 SCU (C-ECHO).

```cpp
#include <pacs/services/verification_scu.h>

namespace pacs::services {

class verification_scu {
public:
    explicit verification_scu(const std::string& ae_title);

    // 간단한 에코
    common::Result<void> echo(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port
    );

    // 기존 연결로 에코
    common::Result<void> echo(network::association& assoc);
};

} // namespace pacs::services
```

---

### `pacs::services::storage_scp`

저장 SCP (C-STORE 수신자).

```cpp
#include <pacs/services/storage_scp.h>

namespace pacs::services {

// 저장 상태 코드
enum class storage_status : uint16_t {
    success = 0x0000,
    warning_coercion = 0xB000,
    warning_elements_discarded = 0xB007,
    error_cannot_understand = 0xC000,
    error_out_of_resources = 0xA700
};

// 저장 핸들러 콜백
using storage_handler = std::function<storage_status(
    const core::dicom_dataset& dataset,
    const std::string& calling_ae
)>;

// 구성
struct storage_scp_config {
    std::string ae_title;
    uint16_t port = 11112;
    std::filesystem::path storage_path;
    size_t max_associations = 10;
    std::vector<std::string> accepted_sop_classes;
};

class storage_scp {
public:
    explicit storage_scp(const storage_scp_config& config);
    ~storage_scp();

    // 핸들러
    void set_handler(storage_handler handler);

    // 라이프사이클
    common::Result<void> start();
    void stop();
    void wait_for_shutdown();

    // 상태
    bool is_running() const noexcept;
    size_t active_associations() const noexcept;
};

} // namespace pacs::services
```

**예제:**
```cpp
using namespace pacs::services;

storage_scp_config config;
config.ae_title = "PACS_SCP";
config.port = 11112;
config.storage_path = "/data/dicom";

storage_scp server(config);

server.set_handler([](const core::dicom_dataset& dataset,
                      const std::string& calling_ae) {
    std::cout << calling_ae << "에서 수신: "
              << dataset.get_string(tags::SOPInstanceUID) << std::endl;
    return storage_status::success;
});

auto result = server.start();
if (result.is_ok()) {
    server.wait_for_shutdown();
}
```

---

### `pacs::services::storage_scu`

저장 SCU (C-STORE 전송자).

```cpp
#include <pacs/services/storage_scu.h>

namespace pacs::services {

class storage_scu {
public:
    explicit storage_scu(const std::string& ae_title);

    // 단일 이미지 전송
    common::Result<void> store(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port,
        const core::dicom_file& file
    );

    // 다중 이미지 전송
    common::Result<void> store_batch(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port,
        const std::vector<std::filesystem::path>& files,
        std::function<void(size_t, size_t)> progress_callback = nullptr
    );

    // 기존 연결로 전송
    common::Result<void> store(
        network::association& assoc,
        const core::dicom_file& file
    );
};

} // namespace pacs::services
```

---

### `pacs::services::sop_classes` - XA/XRF 저장

X-Ray 혈관조영술 및 방사선투시 저장 SOP 클래스.

```cpp
#include <pacs/services/sop_classes/xa_storage.hpp>

namespace pacs::services::sop_classes {

// SOP 클래스 UID
inline constexpr std::string_view xa_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.1";
inline constexpr std::string_view enhanced_xa_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.1.1";
inline constexpr std::string_view xrf_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.2";
inline constexpr std::string_view xray_3d_angiographic_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.1";
inline constexpr std::string_view xray_3d_craniofacial_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.2";

// Photometric Interpretation (XA는 그레이스케일만)
enum class xa_photometric_interpretation {
    monochrome1,  // 최소값 = 흰색
    monochrome2   // 최소값 = 검은색
};

// XA SOP 클래스 정보
struct xa_sop_class_info {
    std::string_view uid;
    std::string_view name;
    bool is_enhanced;
    bool is_3d;
    bool supports_multiframe;
};

// 포지셔너 각도 (LAO/RAO, Cranial/Caudal)
struct xa_positioner_angles {
    double primary_angle;    // LAO(+) / RAO(-) 단위: 도
    double secondary_angle;  // Cranial(+) / Caudal(-) 단위: 도
};

// QCA 캘리브레이션 데이터
struct xa_calibration_data {
    double imager_pixel_spacing[2];       // mm/픽셀
    double distance_source_to_detector;   // SID (mm)
    double distance_source_to_patient;    // SOD (mm)
};

// 유틸리티 함수
[[nodiscard]] bool is_xa_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_enhanced_xa_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_xrf_sop_class(std::string_view uid) noexcept;
[[nodiscard]] const xa_sop_class_info* get_xa_sop_class_info(std::string_view uid);

[[nodiscard]] std::vector<std::string_view> get_xa_transfer_syntaxes();
[[nodiscard]] bool is_valid_xa_photometric(std::string_view photometric) noexcept;
[[nodiscard]] xa_photometric_interpretation
    parse_xa_photometric(std::string_view value) noexcept;

// 캘리브레이션 유틸리티
[[nodiscard]] double calculate_magnification_factor(
    double distance_source_to_detector,
    double distance_source_to_patient
) noexcept;

[[nodiscard]] double calculate_real_world_size(
    double pixel_size,
    double imager_pixel_spacing,
    double magnification_factor
) noexcept;

} // namespace pacs::services::sop_classes
```

**예제:**
```cpp
using namespace pacs::services::sop_classes;

// 데이터셋이 XA인지 확인
if (is_xa_sop_class(sop_class_uid)) {
    auto info = get_xa_sop_class_info(sop_class_uid);
    std::cout << "SOP 클래스: " << info->name << std::endl;

    if (info->is_enhanced) {
        // 향상된 XA의 기능 그룹 처리
    }
}

// QCA에서 실제 크기 계산
xa_calibration_data cal{
    {0.2, 0.2},  // 0.2mm 픽셀 간격
    1000.0,      // SID = 1000mm
    750.0        // SOD = 750mm
};
double mag_factor = calculate_magnification_factor(cal.distance_source_to_detector,
                                                    cal.distance_source_to_patient);
double real_size = calculate_real_world_size(100.0, cal.imager_pixel_spacing[0], mag_factor);
```

---

### `pacs::services::validation::xa_iod_validator`

X-Ray 혈관조영술 이미지용 IOD 검증기.

```cpp
#include <pacs/services/validation/xa_iod_validator.hpp>

namespace pacs::services::validation {

// XA 전용 DICOM 태그
namespace xa_tags {
    inline constexpr core::dicom_tag positioner_primary_angle{0x0018, 0x1510};
    inline constexpr core::dicom_tag positioner_secondary_angle{0x0018, 0x1511};
    inline constexpr core::dicom_tag imager_pixel_spacing{0x0018, 0x1164};
    inline constexpr core::dicom_tag distance_source_to_detector{0x0018, 0x1110};
    inline constexpr core::dicom_tag distance_source_to_patient{0x0018, 0x1111};
    inline constexpr core::dicom_tag frame_time{0x0018, 0x1063};
    inline constexpr core::dicom_tag cine_rate{0x0018, 0x0040};
    inline constexpr core::dicom_tag radiation_setting{0x0018, 0x1155};
}

// 검증 심각도 수준
enum class validation_severity {
    error,    // 반드시 수정해야 함
    warning,  // 수정 권장
    info      // 정보성
};

// 단일 발견 사항에 대한 검증 결과
struct validation_finding {
    validation_severity severity;
    std::string module;
    std::string message;
    std::optional<core::dicom_tag> tag;
};

// 전체 검증 결과
struct validation_result {
    bool is_valid;
    std::vector<validation_finding> findings;

    [[nodiscard]] bool has_errors() const noexcept;
    [[nodiscard]] bool has_warnings() const noexcept;
    [[nodiscard]] std::vector<validation_finding> errors() const;
    [[nodiscard]] std::vector<validation_finding> warnings() const;
};

// 검증 옵션
struct xa_validation_options {
    bool validate_patient_module = true;
    bool validate_study_module = true;
    bool validate_series_module = true;
    bool validate_equipment_module = true;
    bool validate_acquisition_module = true;
    bool validate_image_module = true;
    bool validate_calibration = true;       // XA 전용
    bool validate_multiframe = true;        // XA 전용
    bool strict_mode = false;               // 경고를 오류로 처리
};

class xa_iod_validator {
public:
    // 전체 XA IOD 검증
    [[nodiscard]] static validation_result validate(
        const core::dicom_dataset& dataset,
        const xa_validation_options& options = {}
    );

    // 특정 모듈 검증
    [[nodiscard]] static validation_result validate_patient_module(
        const core::dicom_dataset& dataset
    );
    [[nodiscard]] static validation_result validate_xa_acquisition_module(
        const core::dicom_dataset& dataset
    );
    [[nodiscard]] static validation_result validate_xa_image_module(
        const core::dicom_dataset& dataset
    );
    [[nodiscard]] static validation_result validate_calibration_data(
        const core::dicom_dataset& dataset
    );
    [[nodiscard]] static validation_result validate_multiframe_data(
        const core::dicom_dataset& dataset
    );

    // 빠른 확인
    [[nodiscard]] static bool has_required_xa_attributes(
        const core::dicom_dataset& dataset
    ) noexcept;
    [[nodiscard]] static bool has_valid_photometric_interpretation(
        const core::dicom_dataset& dataset
    ) noexcept;
    [[nodiscard]] static bool has_calibration_data(
        const core::dicom_dataset& dataset
    ) noexcept;
};

} // namespace pacs::services::validation
```

**예제:**
```cpp
using namespace pacs::services::validation;

// 전체 검증
xa_validation_options opts;
opts.strict_mode = true;  // 경고 시 실패

auto result = xa_iod_validator::validate(dataset, opts);

if (!result.is_valid) {
    for (const auto& finding : result.errors()) {
        std::cerr << "[" << finding.module << "] "
                  << finding.message << std::endl;
    }
}

// 빠른 캘리브레이션 확인
if (xa_iod_validator::has_calibration_data(dataset)) {
    auto cal_result = xa_iod_validator::validate_calibration_data(dataset);
    // 캘리브레이션 검증 처리...
}
```

---

### `pacs::services::cache::simple_lru_cache<Key, Value>`

TTL 지원이 포함된 스레드 안전 LRU (Least Recently Used) 캐시.

```cpp
#include <pacs/services/cache/simple_lru_cache.hpp>

namespace pacs::services::cache {

// 캐시 설정
struct cache_config {
    std::size_t max_size{1000};           // 최대 항목 수
    std::chrono::seconds ttl{300};        // 유효 시간 (기본 5분)
    bool enable_metrics{true};            // 히트/미스 추적
    std::string cache_name{"lru_cache"};  // 로깅/메트릭용 이름
};

// 캐시 통계
struct cache_stats {
    std::atomic<uint64_t> hits{0};        // 캐시 히트 수
    std::atomic<uint64_t> misses{0};      // 캐시 미스 수
    std::atomic<uint64_t> evictions{0};   // 제거된 항목 수
    std::atomic<uint64_t> expirations{0}; // 만료된 항목 수

    double hit_rate() const noexcept;     // 0.0-100.0 반환
    void reset() noexcept;
};

template <typename Key, typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class simple_lru_cache {
public:
    // 생성
    explicit simple_lru_cache(const cache_config& config = {});
    simple_lru_cache(size_type max_size, std::chrono::seconds ttl);

    // 캐시 연산
    [[nodiscard]] std::optional<Value> get(const Key& key);
    void put(const Key& key, const Value& value);
    void put(const Key& key, Value&& value);
    bool invalidate(const Key& key);
    template<typename Predicate>
    size_type invalidate_if(Predicate pred);  // 조건부 무효화
    void clear();
    size_type purge_expired();

    // 정보
    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_type max_size() const noexcept;
    [[nodiscard]] std::chrono::seconds ttl() const noexcept;

    // 통계
    [[nodiscard]] const cache_stats& stats() const noexcept;
    [[nodiscard]] double hit_rate() const noexcept;
    void reset_stats() noexcept;
};

// 문자열 키 캐시를 위한 타입 별칭
template <typename Value>
using string_lru_cache = simple_lru_cache<std::string, Value>;

} // namespace pacs::services::cache
```

**예제:**
```cpp
using namespace pacs::services::cache;

// 1000개 항목 제한과 5분 TTL로 캐시 생성
cache_config config;
config.max_size = 1000;
config.ttl = std::chrono::seconds{300};
config.cache_name = "query_cache";

simple_lru_cache<std::string, QueryResult> cache(config);

// 저장 및 조회
cache.put("patient_123", result);

auto cached = cache.get("patient_123");
if (cached) {
    // 캐시된 결과 사용
    process(*cached);
}

// 성능 확인
auto rate = cache.hit_rate();
logger_adapter::info("캐시 히트율: {:.1f}%", rate);
```

---

### `pacs::services::cache::query_cache`

DICOM C-FIND 쿼리 결과를 위한 특화된 캐시.

```cpp
#include <pacs/services/cache/query_cache.hpp>

namespace pacs::services::cache {

// 설정
struct query_cache_config {
    std::size_t max_entries{1000};
    std::chrono::seconds ttl{300};
    bool enable_logging{false};
    bool enable_metrics{true};
    std::string cache_name{"cfind_query_cache"};
};

// 캐시된 쿼리 결과
struct cached_query_result {
    std::vector<uint8_t> data;                        // 직렬화된 결과
    uint32_t match_count{0};                          // 일치 항목 수
    std::chrono::steady_clock::time_point cached_at;  // 캐시 타임스탬프
    std::string query_level;                          // PATIENT/STUDY/SERIES/IMAGE
};

class query_cache {
public:
    explicit query_cache(const query_cache_config& config = {});

    // 캐시 연산
    [[nodiscard]] std::optional<cached_query_result> get(const std::string& key);
    void put(const std::string& key, const cached_query_result& result);
    void put(const std::string& key, cached_query_result&& result);
    bool invalidate(const std::string& key);
    size_type invalidate_by_prefix(const std::string& prefix);
    size_type invalidate_by_query_level(const std::string& level);
    template<typename Predicate>
    size_type invalidate_if(Predicate pred);
    void clear();
    size_type purge_expired();

    // 정보
    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] const cache_stats& stats() const noexcept;
    [[nodiscard]] double hit_rate() const noexcept;

    // 키 생성 헬퍼
    [[nodiscard]] static std::string build_key(
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);

    [[nodiscard]] static std::string build_key_with_ae(
        const std::string& calling_ae,
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);
};

// 전역 캐시 접근
[[nodiscard]] query_cache& global_query_cache();
bool configure_global_cache(const query_cache_config& config);

} // namespace pacs::services::cache
```

**예제:**
```cpp
using namespace pacs::services::cache;

// 쿼리 파라미터로 캐시 키 생성
std::string key = query_cache::build_key("STUDY", {
    {"PatientID", "12345"},
    {"StudyDate", "20240101-20241231"}
});

// 먼저 캐시 확인
auto& cache = global_query_cache();
auto result = cache.get(key);

if (result) {
    // 캐시된 결과 반환
    return result->data;
}

// 쿼리 실행 후 결과 캐싱
auto query_result = execute_cfind(...);

cached_query_result cached;
cached.data = serialize(query_result);
cached.match_count = query_result.size();
cached.query_level = "STUDY";
cached.cached_at = std::chrono::steady_clock::now();

cache.put(key, std::move(cached));

// C-STORE 시 캐시 무효화 (새 데이터 저장 시)
// post_store_handler를 사용하여 영향받는 캐시 항목 무효화
storage_scp scp{config};
scp.set_post_store_handler([&cache](const auto& dataset,
                                     const auto& patient_id,
                                     const auto& study_uid,
                                     const auto& series_uid,
                                     const auto& sop_uid) {
    // 영향받을 수 있는 모든 캐시된 쿼리 무효화
    cache.invalidate_by_query_level("IMAGE");
    cache.invalidate_by_query_level("SERIES");
    cache.invalidate_by_query_level("STUDY");
    cache.invalidate_by_query_level("PATIENT");
});

// 접두사를 사용한 타겟 무효화
cache.invalidate_by_prefix("WORKSTATION1/");  // 특정 AE 무효화

// 커스텀 조건부 무효화
cache.invalidate_if([](const auto& key, const cached_query_result& r) {
    return r.match_count > 1000;  // 큰 결과 집합 제거
});
```

---

### `pacs::services::cache::streaming_query_handler`

페이지네이션을 지원하는 메모리 효율적인 C-FIND 응답용 스트리밍 쿼리 핸들러.

```cpp
#include <pacs/services/cache/streaming_query_handler.hpp>

namespace pacs::services {

class streaming_query_handler {
public:
    using StreamResult = Result<std::unique_ptr<query_result_stream>>;

    // 생성
    explicit streaming_query_handler(storage::index_database* db);

    // 설정
    void set_page_size(size_t size) noexcept;        // 기본값: 100
    [[nodiscard]] auto page_size() const noexcept -> size_t;

    void set_max_results(size_t max) noexcept;       // 기본값: 0 (무제한)
    [[nodiscard]] auto max_results() const noexcept -> size_t;

    // 스트림 작업
    [[nodiscard]] auto create_stream(
        query_level level,
        const core::dicom_dataset& query_keys,
        const std::string& calling_ae
    ) -> StreamResult;

    [[nodiscard]] auto resume_stream(
        const std::string& cursor_state,
        query_level level,
        const core::dicom_dataset& query_keys
    ) -> StreamResult;

    // query_scp 호환성
    [[nodiscard]] auto as_query_handler() -> query_handler;
};

} // namespace pacs::services
```

**예제 - 스트리밍 인터페이스:**
```cpp
using namespace pacs::services;

streaming_query_handler handler(db);
handler.set_page_size(100);

dicom_dataset query_keys;
query_keys.set_string(tags::modality, vr_type::CS, "CT");

auto stream_result = handler.create_stream(
    query_level::study, query_keys, "CALLING_AE");

if (stream_result.is_ok()) {
    auto& stream = stream_result.value();

    while (stream->has_more()) {
        auto batch = stream->next_batch();
        if (batch.has_value()) {
            for (const auto& dataset : batch.value()) {
                // 각 DICOM 데이터셋 처리
            }
        }
    }
}
```

**예제 - Query SCP 통합:**
```cpp
using namespace pacs::services;

query_scp scp;
streaming_query_handler handler(db);
handler.set_max_results(500);

// 하위 호환성을 위한 어댑터 사용
scp.set_handler(handler.as_query_handler());
scp.start();
```

---

### `pacs::services::cache::query_result_stream`

데이터베이스 커서 결과를 DICOM 데이터셋으로 변환하며 배치 지원.

```cpp
#include <pacs/services/cache/query_result_stream.hpp>

namespace pacs::services {

// 스트림 설정
struct stream_config {
    size_t page_size{100};  // 페치 배치 크기
};

class query_result_stream {
public:
    // 팩토리 메서드
    static auto create(
        storage::index_database* db,
        query_level level,
        const core::dicom_dataset& query_keys,
        const stream_config& config = {}
    ) -> Result<std::unique_ptr<query_result_stream>>;

    static auto from_cursor(
        storage::index_database* db,
        const std::string& cursor_state,
        query_level level,
        const core::dicom_dataset& query_keys,
        const stream_config& config = {}
    ) -> Result<std::unique_ptr<query_result_stream>>;

    // 스트림 상태
    [[nodiscard]] auto has_more() const noexcept -> bool;
    [[nodiscard]] auto position() const noexcept -> size_t;
    [[nodiscard]] auto level() const noexcept -> query_level;
    [[nodiscard]] auto total_count() const -> std::optional<size_t>;

    // 데이터 조회
    [[nodiscard]] auto next_batch()
        -> std::optional<std::vector<core::dicom_dataset>>;

    // 재개를 위한 커서
    [[nodiscard]] auto cursor() const -> std::string;
};

} // namespace pacs::services
```

---

### `pacs::services::cache::database_cursor`

지연 평가를 지원하는 스트리밍 쿼리 결과용 저수준 SQLite 커서.

```cpp
#include <pacs/services/cache/database_cursor.hpp>

namespace pacs::services {

// 쿼리 레코드 타입 (모든 DICOM 레벨의 variant)
using query_record = std::variant<
    storage::patient_record,
    storage::study_record,
    storage::series_record,
    storage::instance_record
>;

class database_cursor {
public:
    enum class record_type { patient, study, series, instance };

    // 각 쿼리 레벨별 팩토리 메서드
    static auto create_patient_cursor(
        sqlite3* db,
        const storage::patient_query& query
    ) -> Result<std::unique_ptr<database_cursor>>;

    static auto create_study_cursor(
        sqlite3* db,
        const storage::study_query& query
    ) -> Result<std::unique_ptr<database_cursor>>;

    static auto create_series_cursor(
        sqlite3* db,
        const storage::series_query& query
    ) -> Result<std::unique_ptr<database_cursor>>;

    static auto create_instance_cursor(
        sqlite3* db,
        const storage::instance_query& query
    ) -> Result<std::unique_ptr<database_cursor>>;

    // 커서 상태
    [[nodiscard]] auto has_more() const noexcept -> bool;
    [[nodiscard]] auto position() const noexcept -> size_t;
    [[nodiscard]] auto type() const noexcept -> record_type;

    // 데이터 조회
    [[nodiscard]] auto fetch_next() -> std::optional<query_record>;
    [[nodiscard]] auto fetch_batch(size_t batch_size)
        -> std::vector<query_record>;

    // 커서 제어
    [[nodiscard]] auto reset() -> VoidResult;
    [[nodiscard]] auto serialize() const -> std::string;
};

} // namespace pacs::services
```

**예제 - 재개 가능한 페이지네이션:**
```cpp
using namespace pacs::services;

// 첫 번째 페이지 요청
auto cursor_result = database_cursor::create_study_cursor(
    db->native_handle(), study_query{});

if (cursor_result.is_ok()) {
    auto& cursor = cursor_result.value();

    // 첫 100개 레코드 페치
    auto batch = cursor->fetch_batch(100);

    // 재개를 위한 커서 상태 저장
    std::string saved_state = cursor->serialize();

    // ... 나중에 저장된 상태에서 재개
    // query_result_stream::from_cursor(..., saved_state, ...)
}
```

---

## 저장 모듈

### `pacs::storage::storage_interface`

추상 저장 백엔드 인터페이스.

```cpp
#include <pacs/storage/storage_interface.h>

namespace pacs::storage {

class storage_interface {
public:
    virtual ~storage_interface() = default;

    // 저장
    virtual common::Result<void> store(const core::dicom_dataset& dataset) = 0;

    // 검색
    virtual common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid
    ) = 0;

    // 삭제
    virtual common::Result<void> remove(const std::string& sop_instance_uid) = 0;

    // 조회
    virtual common::Result<std::vector<core::dicom_dataset>> find(
        const core::dicom_dataset& query
    ) = 0;

    // 존재 여부 확인
    virtual bool exists(const std::string& sop_instance_uid) = 0;
};

} // namespace pacs::storage
```

---

### `pacs::storage::file_storage`

파일시스템 기반 저장 구현.

```cpp
#include <pacs/storage/file_storage.h>

namespace pacs::storage {

struct file_storage_config {
    std::filesystem::path root_path;

    enum class naming_scheme {
        uid_based,       // {StudyUID}/{SeriesUID}/{SOPUID}.dcm
        date_based,      // {YYYY}/{MM}/{DD}/{SOPUID}.dcm
        flat             // {SOPUID}.dcm
    } scheme = naming_scheme::uid_based;

    bool create_directories = true;
    bool overwrite_duplicates = false;
};

class file_storage : public storage_interface {
public:
    explicit file_storage(const file_storage_config& config);

    // storage_interface 구현
    common::Result<void> store(const core::dicom_dataset& dataset) override;
    common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid
    ) override;
    common::Result<void> remove(const std::string& sop_instance_uid) override;
    common::Result<std::vector<core::dicom_dataset>> find(
        const core::dicom_dataset& query
    ) override;
    bool exists(const std::string& sop_instance_uid) override;

    // 파일 경로
    std::filesystem::path file_path(const std::string& sop_instance_uid) const;
};

} // namespace pacs::storage
```

---

### `pacs::storage::index_database`

PACS 메타데이터를 위한 SQLite 기반 인덱스 데이터베이스.

모든 쿼리 메서드는 적절한 에러 처리를 위해 `Result<T>`를 반환합니다.

```cpp
#include <pacs/storage/index_database.hpp>

namespace pacs::storage {

class index_database {
public:
    // 팩토리 메서드
    static Result<std::unique_ptr<index_database>> open(const std::string& path);

    // 환자 작업
    Result<int64_t> upsert_patient(const std::string& patient_id,
                                    const std::string& patient_name,
                                    const std::string& birth_date,
                                    const std::string& sex);
    std::optional<patient_record> find_patient(const std::string& patient_id);
    Result<std::vector<patient_record>> search_patients(const patient_query& query);

    // 검사 작업
    Result<int64_t> upsert_study(int64_t patient_pk, const std::string& study_uid, ...);
    std::optional<study_record> find_study(const std::string& study_uid);
    Result<std::vector<study_record>> search_studies(const study_query& query);
    Result<std::vector<study_record>> list_studies(const std::string& patient_id);
    Result<void> delete_study(const std::string& study_uid);

    // 시리즈 작업
    Result<int64_t> upsert_series(int64_t study_pk, const std::string& series_uid, ...);
    std::optional<series_record> find_series(const std::string& series_uid);
    Result<std::vector<series_record>> search_series(const series_query& query);
    Result<std::vector<series_record>> list_series(const std::string& study_uid);

    // 인스턴스 작업
    Result<int64_t> upsert_instance(int64_t series_pk, const std::string& sop_uid, ...);
    std::optional<instance_record> find_instance(const std::string& sop_uid);
    Result<std::vector<instance_record>> search_instances(const instance_query& query);
    Result<std::vector<instance_record>> list_instances(const std::string& series_uid);
    Result<std::optional<std::string>> get_file_path(const std::string& sop_uid);

    // 파일 경로 작업
    Result<std::vector<std::string>> get_study_files(const std::string& study_uid);
    Result<std::vector<std::string>> get_series_files(const std::string& series_uid);

    // 카운트 작업
    Result<size_t> patient_count();
    Result<size_t> study_count();
    Result<size_t> study_count(const std::string& patient_id);
    Result<size_t> series_count();
    Result<size_t> series_count(const std::string& study_uid);
    Result<size_t> instance_count();
    Result<size_t> instance_count(const std::string& series_uid);

    // 감사 작업
    Result<int64_t> add_audit_log(const audit_record& record);
    Result<std::vector<audit_record>> query_audit_log(const audit_query& query);
    Result<size_t> audit_count();

    // 워크리스트 작업
    Result<int64_t> add_worklist_item(const worklist_item& item);
    Result<std::vector<worklist_item>> query_worklist(const worklist_query& query);
    Result<size_t> worklist_count(const std::string& status = "");

    // 데이터베이스 무결성
    Result<void> verify_integrity();
};

} // namespace pacs::storage
```

**예시 - Result<T> 패턴 사용:**
```cpp
auto db_result = pacs::storage::index_database::open("/data/pacs/index.db");
if (!db_result.is_ok()) {
    std::cerr << "데이터베이스 열기 실패: " << db_result.error().message << "\n";
    return;
}
auto& db = db_result.value();

// 에러 처리와 함께 검사 쿼리
pacs::storage::study_query query;
query.patient_id = "P001";
auto studies_result = db->search_studies(query);
if (!studies_result.is_ok()) {
    std::cerr << "쿼리 실패: " << studies_result.error().message << "\n";
    return;
}

for (const auto& study : studies_result.value()) {
    std::cout << "검사: " << study.study_uid << "\n";
}
```

---

## DX 모달리티 모듈

### `pacs::services::sop_classes::dx_storage`

일반 방사선 촬영을 위한 Digital X-Ray (DX) SOP 클래스 정의 및 유틸리티.

```cpp
#include <pacs/services/sop_classes/dx_storage.hpp>

namespace pacs::services::sop_classes {

// SOP 클래스 UID
inline constexpr std::string_view dx_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.1";
inline constexpr std::string_view dx_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.1.1";
inline constexpr std::string_view mammography_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.2";
inline constexpr std::string_view mammography_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.2.1";
inline constexpr std::string_view intraoral_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.3";
inline constexpr std::string_view intraoral_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.3.1";

// 열거형
enum class dx_photometric_interpretation { monochrome1, monochrome2 };
enum class dx_image_type { for_presentation, for_processing };
enum class dx_view_position { ap, pa, lateral, oblique, other };
enum class dx_detector_type { direct, indirect, storage, film };
enum class dx_body_part { chest, abdomen, pelvis, spine, skull, hand, foot,
                          knee, elbow, shoulder, hip, wrist, ankle, extremity,
                          breast, other };

// SOP 클래스 정보 구조체
struct dx_sop_class_info {
    std::string_view uid;
    std::string_view name;
    std::string_view description;
    dx_image_type image_type;
    bool is_mammography;
    bool is_intraoral;
};

// 유틸리티 함수
[[nodiscard]] std::vector<std::string> get_dx_transfer_syntaxes();
[[nodiscard]] std::vector<std::string> get_dx_storage_sop_classes(
    bool include_mammography = true, bool include_intraoral = true);
[[nodiscard]] const dx_sop_class_info* get_dx_sop_class_info(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_storage_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_for_processing_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_for_presentation_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_mammography_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_valid_dx_photometric(std::string_view value) noexcept;

// 열거형 변환 유틸리티
[[nodiscard]] std::string_view to_string(dx_photometric_interpretation interp) noexcept;
[[nodiscard]] std::string_view to_string(dx_view_position position) noexcept;
[[nodiscard]] std::string_view to_string(dx_detector_type type) noexcept;
[[nodiscard]] std::string_view to_string(dx_body_part part) noexcept;
[[nodiscard]] dx_view_position parse_view_position(std::string_view value) noexcept;
[[nodiscard]] dx_body_part parse_body_part(std::string_view value) noexcept;

} // namespace pacs::services::sop_classes
```

**예제:**
```cpp
using namespace pacs::services::sop_classes;

// SOP 클래스 UID가 DX인지 확인
if (is_dx_storage_sop_class(sop_class_uid)) {
    const auto* info = get_dx_sop_class_info(sop_class_uid);

    if (info->image_type == dx_image_type::for_presentation) {
        // 표시 준비된 이미지, VOI LUT 적용
    } else {
        // 원시 데이터, 처리 필요
    }

    if (info->is_mammography) {
        // 유방촬영 전용 디스플레이 설정 적용
    }
}

// 일반 DX SOP 클래스만 가져오기 (유방촬영/구강내 제외)
auto dx_classes = get_dx_storage_sop_classes(false, false);

// DICOM 데이터셋에서 신체 부위 파싱
auto body_part = parse_body_part("CHEST");  // dx_body_part::chest 반환
auto view = parse_view_position("PA");       // dx_view_position::pa 반환
```

---

### `pacs::services::validation::dx_iod_validator`

DICOM PS3.3 Section A.26에 따른 Digital X-Ray 이미지용 IOD 검증기.

```cpp
#include <pacs/services/validation/dx_iod_validator.hpp>

namespace pacs::services::validation {

// 검증 옵션
struct dx_validation_options {
    bool check_type1 = true;               // 필수 속성
    bool check_type2 = true;               // 필수, 비어있을 수 있음
    bool check_conditional = true;         // 조건부 필수
    bool validate_pixel_data = true;       // 픽셀 일관성 검사
    bool validate_dx_specific = true;      // DX 모듈 검증
    bool validate_anatomy = true;          // 신체 부위/촬영 각도 검증
    bool validate_presentation_requirements = true;
    bool validate_processing_requirements = true;
    bool allow_both_photometric = true;    // MONOCHROME1과 2 허용
    bool strict_mode = false;              // 경고를 오류로 처리
};

class dx_iod_validator {
public:
    dx_iod_validator() = default;
    explicit dx_iod_validator(const dx_validation_options& options);

    // 전체 검증
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    // 특수 검증
    [[nodiscard]] validation_result validate_for_presentation(
        const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_for_processing(
        const core::dicom_dataset& dataset) const;

    // 빠른 Type 1 검사만
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    // 옵션 접근
    [[nodiscard]] const dx_validation_options& options() const noexcept;
    void set_options(const dx_validation_options& options);
};

// 편의 함수
[[nodiscard]] validation_result validate_dx_iod(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_valid_dx_dataset(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_presentation_dx(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_processing_dx(const core::dicom_dataset& dataset);

} // namespace pacs::services::validation
```

**예제:**
```cpp
using namespace pacs::services::validation;

// 기본 검증
auto result = validate_dx_iod(dataset);

if (!result.is_valid) {
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            std::cerr << "[오류] " << finding.code << ": " << finding.message << "\n";
        }
    }
    return;
}

// 커스텀 옵션으로 엄격한 검증
dx_validation_options options;
options.strict_mode = true;  // 경고를 오류로 처리

dx_iod_validator validator(options);
auto strict_result = validator.validate(dataset);

// VOI LUT 검사가 포함된 For Presentation 이미지 검증
if (is_for_presentation_dx(dataset)) {
    auto result = validator.validate_for_presentation(dataset);
    // Window Center/Width 또는 VOI LUT Sequence 검증 포함
}
```

---

## MG 모달리티 모듈

### `pacs::services::sop_classes::mg_storage`

유방 영상을 위한 디지털 맘모그래피 X-Ray SOP 클래스 정의 및 유틸리티.

```cpp
#include <pacs/services/sop_classes/mg_storage.hpp>

namespace pacs::services::sop_classes {

// SOP 클래스 UID
inline constexpr std::string_view mg_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.2";
inline constexpr std::string_view mg_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.2.1";
inline constexpr std::string_view breast_tomosynthesis_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.3";

// 유방 측면 열거형
enum class breast_laterality { left, right, bilateral, unknown };

// 맘모그래피 뷰 위치
enum class mg_view_position {
    cc,      ///< 두개미측 (Craniocaudal)
    mlo,     ///< 내외사위 (Mediolateral Oblique)
    ml,      ///< 내외측 (Mediolateral)
    lm,      ///< 외내측 (Lateromedial)
    xccl,    ///< 확대 CC 외측
    spot,    ///< 스팟 압박
    mag,     ///< 확대 촬영
    implant, ///< 임플란트 변위 (Eklund)
    other
};

// 유틸리티 함수
[[nodiscard]] std::string_view to_string(breast_laterality laterality) noexcept;
[[nodiscard]] breast_laterality parse_breast_laterality(std::string_view value) noexcept;
[[nodiscard]] bool is_valid_breast_laterality(std::string_view value) noexcept;

[[nodiscard]] bool is_valid_compression_force(double force_n) noexcept;
[[nodiscard]] std::pair<double, double> get_typical_compression_force_range() noexcept;

[[nodiscard]] std::vector<std::string> get_mg_storage_sop_classes(bool include_tomosynthesis = true);
[[nodiscard]] bool is_mg_storage_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_breast_tomosynthesis_sop_class(std::string_view uid) noexcept;

} // namespace pacs::services::sop_classes
```

**예제:**
```cpp
using namespace pacs::services::sop_classes;

// SOP 클래스 UID가 맘모그래피인지 확인
if (is_mg_storage_sop_class(sop_class_uid)) {
    const auto* info = get_mg_sop_class_info(sop_class_uid);

    if (info->is_tomosynthesis) {
        // 3D 유방 토모신테시스 처리
    }
}

// 맘모그래피 전용 속성 파싱
auto laterality = parse_breast_laterality("L");  // 좌측 유방
auto view = parse_mg_view_position("MLO");       // 내외사위

// 표준 검진 뷰인지 확인
if (is_screening_view(view)) {
    // CC 또는 MLO - 표준 검진 검사
}

// 압박력 검증 (일반: 50-200 N)
if (is_valid_compression_force(compression_force)) {
    // 허용 범위 내
}
```

---

### `pacs::services::validation::mg_iod_validator`

DICOM PS3.3 섹션 A.26.2에 따른 디지털 맘모그래피 이미지용 IOD 검증기.

```cpp
#include <pacs/services/validation/mg_iod_validator.hpp>

namespace pacs::services::validation {

// 맘모그래피 전용 검증 옵션
struct mg_validation_options {
    bool check_type1 = true;
    bool check_type2 = true;
    bool validate_laterality = true;        // 유방 측면 (0020,0060)
    bool validate_view_position = true;     // 뷰 위치 (0018,5101)
    bool validate_compression = true;       // 압박력 (0018,11A2)
    bool validate_implant_attributes = true;
    bool strict_mode = false;
};

class mg_iod_validator {
public:
    // 전체 IOD 검증
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    // 특화 검증
    [[nodiscard]] validation_result validate_laterality(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_view_position(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_compression_force(const core::dicom_dataset& dataset) const;

    // 빠른 검증
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;
};

// 편의 함수
[[nodiscard]] validation_result validate_mg_iod(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_valid_mg_dataset(const core::dicom_dataset& dataset);
[[nodiscard]] bool has_breast_implant(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_screening_mammogram(const core::dicom_dataset& dataset);

} // namespace pacs::services::validation
```

**오류 코드 (MG 전용):**
| 코드 | 심각도 | 설명 |
|------|--------|------|
| MG-ERR-001 | 오류 | SOPClassUID가 맘모그래피 저장 클래스가 아님 |
| MG-ERR-002 | 오류 | Modality는 'MG'여야 함 |
| MG-ERR-003 | 오류 | 측면 미지정 (맘모그래피에 필수) |
| MG-ERR-004 | 오류 | 잘못된 측면 값 |
| MG-WARN-001 | 경고 | 시리즈와 이미지 레벨 간 측면 불일치 |
| MG-WARN-005 | 경고 | 뷰 위치 미지정 |
| MG-WARN-013 | 경고 | 압박력이 일반 범위 초과 |
| MG-INFO-007 | 정보 | 유방 임플란트 있으나 ID 뷰 미사용 |
| MG-INFO-008 | 정보 | 압박력 미기록 |

**예제:**
```cpp
using namespace pacs::services::validation;

// 맘모그래피 데이터셋 검증
auto result = validate_mg_iod(dataset);
if (!result.is_valid) {
    for (const auto& finding : result.findings) {
        if (finding.code.starts_with("MG-ERR-")) {
            std::cerr << "[오류] " << finding.message << "\n";
        }
    }
}

// 검진 검사 확인
if (is_screening_mammogram(dataset)) {
    // 표준 4뷰 검진 (RCC, LCC, RMLO, LMLO)
}

// 임플란트 확인
if (has_breast_implant(dataset)) {
    // 임플란트 변위 뷰가 필요할 수 있음
}
```

---

## AI 모듈

### `pacs::ai::ai_service_connector`

비동기 작업 제출, 상태 추적 및 결과 검색을 지원하는 외부 AI 추론 서비스 커넥터.

```cpp
#include <pacs/ai/ai_service_connector.hpp>

namespace pacs::ai {

// Result 타입 별칭
template <typename T>
using Result = kcenon::common::Result<T>;

// AI 서비스 연결을 위한 인증 유형
enum class authentication_type {
    none,           // 인증 없음
    api_key,        // 헤더의 API 키
    bearer_token,   // Bearer 토큰 (JWT)
    basic           // HTTP Basic 인증
};

// 추론 작업 상태 코드
enum class inference_status_code {
    pending,    // 작업 대기 중
    running,    // 작업 처리 중
    completed,  // 작업 완료
    failed,     // 작업 실패
    cancelled,  // 작업 취소됨
    timeout     // 작업 시간 초과
};

// AI 서비스 구성
struct ai_service_config {
    std::string base_url;                           // AI 서비스 엔드포인트
    std::string service_name{"ai_service"};         // 로깅용 서비스 식별자
    authentication_type auth_type{authentication_type::none};
    std::string api_key;                            // api_key 인증용
    std::string bearer_token;                       // bearer_token 인증용
    std::string username;                           // basic 인증용
    std::string password;                           // basic 인증용
    std::chrono::seconds connection_timeout{30};    // 연결 타임아웃
    std::chrono::seconds request_timeout{300};      // 요청 타임아웃
    std::size_t max_retries{3};                     // 최대 재시도 횟수
    std::chrono::seconds retry_delay{5};            // 재시도 간격
    bool enable_metrics{true};                      // 성능 메트릭 활성화
    bool enable_tracing{true};                      // 분산 추적 활성화
};

// 추론 요청 구조체
struct inference_request {
    std::string study_instance_uid;                 // 필수: Study UID
    std::optional<std::string> series_instance_uid; // 선택: Series UID
    std::string model_id;                           // 필수: AI 모델 식별자
    std::map<std::string, std::string> parameters;  // 모델별 파라미터
    int priority{0};                                // 작업 우선순위 (높을수록 긴급)
    std::optional<std::string> callback_url;        // 완료 웹훅
    std::map<std::string, std::string> metadata;    // 사용자 정의 메타데이터
};

// 추론 상태 구조체
struct inference_status {
    std::string job_id;                             // 고유 작업 식별자
    inference_status_code status;                   // 현재 상태
    int progress{0};                                // 진행률 0-100
    std::optional<std::string> message;             // 상태 메시지
    std::optional<std::string> result_study_uid;    // 완료 시 결과 Study UID
    std::optional<std::string> error_code;          // 실패 시 오류 코드
    std::chrono::system_clock::time_point submitted_at;
    std::optional<std::chrono::system_clock::time_point> started_at;
    std::optional<std::chrono::system_clock::time_point> completed_at;
};

// AI 모델 정보
struct model_info {
    std::string model_id;                           // 모델 식별자
    std::string name;                               // 표시 이름
    std::string version;                            // 모델 버전
    std::string description;                        // 모델 설명
    std::vector<std::string> supported_modalities;  // 예: ["CT", "MR"]
    std::vector<std::string> output_types;          // 예: ["SR", "SEG"]
    bool available{true};                           // 모델 가용성
};

// 메인 커넥터 클래스 (정적 메서드를 가진 싱글톤 패턴)
class ai_service_connector {
public:
    // 초기화 및 라이프사이클
    [[nodiscard]] static auto initialize(const ai_service_config& config)
        -> Result<std::monostate>;
    static void shutdown();
    [[nodiscard]] static auto is_initialized() noexcept -> bool;

    // 추론 작업
    [[nodiscard]] static auto request_inference(const inference_request& request)
        -> Result<std::string>;  // job_id 반환
    [[nodiscard]] static auto check_status(const std::string& job_id)
        -> Result<inference_status>;
    [[nodiscard]] static auto cancel(const std::string& job_id)
        -> Result<std::monostate>;
    [[nodiscard]] static auto list_active_jobs()
        -> Result<std::vector<inference_status>>;

    // 모델 검색
    [[nodiscard]] static auto list_models()
        -> Result<std::vector<model_info>>;
    [[nodiscard]] static auto get_model_info(const std::string& model_id)
        -> Result<model_info>;

    // 상태 확인 및 진단
    [[nodiscard]] static auto check_health() -> bool;
    [[nodiscard]] static auto get_latency()
        -> std::optional<std::chrono::milliseconds>;

    // 구성
    [[nodiscard]] static auto update_credentials(
        authentication_type auth_type,
        const std::string& credential) -> Result<std::monostate>;
    [[nodiscard]] static auto get_config() -> const ai_service_config&;

private:
    class impl;
    static std::unique_ptr<impl> pimpl_;
};

// 헬퍼 함수
[[nodiscard]] auto to_string(inference_status_code status) -> std::string;
[[nodiscard]] auto to_string(authentication_type auth) -> std::string;

} // namespace pacs::ai
```

**설계 결정:**

| 결정 | 근거 |
|------|------|
| 정적 싱글톤 패턴 | 전역 접근, 다른 어댑터와 일관성 유지 |
| PIMPL 패턴 | ABI 안정성, 구현 세부사항 숨김 |
| Result<T> 반환 타입 | common_system과 일관된 오류 처리 |
| 비동기 작업 모델 | 상태 추적이 가능한 장시간 실행 추론 작업 |
| 메트릭 통합 | monitoring_adapter를 통한 성능 모니터링 |

**스레드 안전성:**

모든 공개 메서드는 스레드 안전합니다. 내부 동기화는 다음을 사용합니다:
- 작업 추적 상태용 `std::mutex`
- 읽기 집약적 작업용 `std::shared_mutex`
- 초기화 플래그용 `std::atomic<bool>`

**사용 예제:**

```cpp
#include <pacs/ai/ai_service_connector.hpp>
#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/monitoring_adapter.hpp>

using namespace pacs::ai;

// 먼저 의존성 초기화
pacs::integration::logger_adapter::initialize({});
pacs::integration::monitoring_adapter::initialize({});

// AI 서비스 구성
ai_service_config config;
config.base_url = "https://ai-service.example.com/api/v1";
config.service_name = "radiology_ai";
config.auth_type = authentication_type::api_key;
config.api_key = "your-api-key-here";
config.request_timeout = std::chrono::minutes(5);

// 커넥터 초기화
auto init_result = ai_service_connector::initialize(config);
if (init_result.is_err()) {
    std::cerr << "초기화 실패: " << init_result.error().message << "\n";
    return 1;
}

// 추론 요청 제출
inference_request request;
request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
request.model_id = "chest-xray-nodule-detector-v2";
request.parameters["threshold"] = "0.7";
request.parameters["output_format"] = "SR";
request.priority = 5;

auto submit_result = ai_service_connector::request_inference(request);
if (submit_result.is_err()) {
    std::cerr << "제출 실패: " << submit_result.error().message << "\n";
    return 1;
}

std::string job_id = submit_result.value();
std::cout << "작업 제출됨: " << job_id << "\n";

// 완료 대기
while (true) {
    auto status_result = ai_service_connector::check_status(job_id);
    if (status_result.is_err()) {
        std::cerr << "상태 확인 실패\n";
        break;
    }

    auto& status = status_result.value();
    std::cout << "진행률: " << status.progress << "%\n";

    if (status.status == inference_status_code::completed) {
        std::cout << "결과 사용 가능: " << *status.result_study_uid << "\n";
        break;
    } else if (status.status == inference_status_code::failed) {
        std::cerr << "작업 실패: " << *status.error_code << "\n";
        break;
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
}

// 정리
ai_service_connector::shutdown();
```

---

## 모니터링 모듈

### `pacs::monitoring::pacs_metrics`

DICOM 작업에 대한 원자적 카운터와 타이밍 데이터를 사용한 스레드 안전 메트릭 수집.

```cpp
#include <pacs/monitoring/pacs_metrics.hpp>

namespace pacs::monitoring {

// DIMSE 작업 유형
enum class dimse_operation {
    c_echo,    // C-ECHO (검증)
    c_store,   // C-STORE (저장)
    c_find,    // C-FIND (조회)
    c_move,    // C-MOVE (검색)
    c_get,     // C-GET (검색)
    n_create,  // N-CREATE (MPPS)
    n_set,     // N-SET (MPPS)
    n_get,     // N-GET
    n_action,  // N-ACTION
    n_event,   // N-EVENT-REPORT
    n_delete   // N-DELETE
};

// 작업 통계를 위한 원자적 카운터
struct operation_counter {
    std::atomic<uint64_t> success_count{0};
    std::atomic<uint64_t> failure_count{0};
    std::atomic<uint64_t> total_duration_us{0};
    std::atomic<uint64_t> min_duration_us{UINT64_MAX};
    std::atomic<uint64_t> max_duration_us{0};

    uint64_t total_count() const noexcept;
    uint64_t average_duration_us() const noexcept;
    void record_success(std::chrono::microseconds duration) noexcept;
    void record_failure(std::chrono::microseconds duration) noexcept;
    void reset() noexcept;
};

// 데이터 전송 추적
struct data_transfer_metrics {
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> images_stored{0};
    std::atomic<uint64_t> images_retrieved{0};

    void add_bytes_sent(uint64_t bytes) noexcept;
    void add_bytes_received(uint64_t bytes) noexcept;
    void increment_images_stored() noexcept;
    void increment_images_retrieved() noexcept;
    void reset() noexcept;
};

// 연결 수명 주기 추적
struct association_counters {
    std::atomic<uint64_t> total_established{0};
    std::atomic<uint64_t> total_rejected{0};
    std::atomic<uint64_t> total_aborted{0};
    std::atomic<uint32_t> current_active{0};
    std::atomic<uint32_t> peak_active{0};

    void record_established() noexcept;
    void record_released() noexcept;
    void record_rejected() noexcept;
    void record_aborted() noexcept;
    void reset() noexcept;
};

class pacs_metrics {
public:
    // 싱글톤 접근
    static pacs_metrics& global_metrics() noexcept;

    // DIMSE 작업 기록
    void record_store(bool success, std::chrono::microseconds duration,
                      uint64_t bytes_stored = 0) noexcept;
    void record_query(bool success, std::chrono::microseconds duration,
                      uint32_t matches = 0) noexcept;
    void record_echo(bool success, std::chrono::microseconds duration) noexcept;
    void record_move(bool success, std::chrono::microseconds duration,
                     uint32_t images_moved = 0) noexcept;
    void record_get(bool success, std::chrono::microseconds duration,
                    uint32_t images_retrieved = 0, uint64_t bytes = 0) noexcept;
    void record_operation(dimse_operation op, bool success,
                          std::chrono::microseconds duration) noexcept;

    // 데이터 전송 기록
    void record_bytes_sent(uint64_t bytes) noexcept;
    void record_bytes_received(uint64_t bytes) noexcept;

    // 연결 기록
    void record_association_established() noexcept;
    void record_association_released() noexcept;
    void record_association_rejected() noexcept;
    void record_association_aborted() noexcept;

    // 메트릭 접근
    const operation_counter& get_counter(dimse_operation op) const noexcept;
    const data_transfer_metrics& transfer() const noexcept;
    const association_counters& associations() const noexcept;

    // 내보내기
    std::string to_json() const;
    std::string to_prometheus(std::string_view prefix = "pacs") const;

    // 모든 메트릭 초기화
    void reset() noexcept;
};

} // namespace pacs::monitoring
```

**사용 예제:**

```cpp
#include <pacs/monitoring/pacs_metrics.hpp>
using namespace pacs::monitoring;

// 전역 메트릭 인스턴스 가져오기
auto& metrics = pacs_metrics::global_metrics();

// C-STORE 작업 기록
auto start = std::chrono::steady_clock::now();
// ... C-STORE 수행 ...
auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
metrics.record_store(true, duration, 1024 * 1024);  // 1MB 이미지

// 모니터링 시스템용 내보내기
std::string json = metrics.to_json();           // REST API용
std::string prom = metrics.to_prometheus();      // Prometheus용
```

---

## 공통 모듈

### 오류 코드

```cpp
#include <pacs/common/error_codes.h>

namespace pacs::error_codes {

// DICOM 파싱 오류 (-800 ~ -819)
constexpr int INVALID_DICOM_FILE = -800;
constexpr int INVALID_VR = -801;
constexpr int MISSING_REQUIRED_TAG = -802;
constexpr int INVALID_TRANSFER_SYNTAX = -803;
constexpr int INVALID_DATA_ELEMENT = -804;
constexpr int SEQUENCE_ENCODING_ERROR = -805;

// 연결 오류 (-820 ~ -839)
constexpr int ASSOCIATION_REJECTED = -820;
constexpr int ASSOCIATION_ABORTED = -821;
constexpr int NO_PRESENTATION_CONTEXT = -822;
constexpr int INVALID_PDU = -823;
constexpr int PDU_TOO_LARGE = -824;
constexpr int ASSOCIATION_TIMEOUT = -825;

// DIMSE 오류 (-840 ~ -859)
constexpr int DIMSE_FAILURE = -840;
constexpr int DIMSE_TIMEOUT = -841;
constexpr int DIMSE_INVALID_RESPONSE = -842;
constexpr int DIMSE_CANCELLED = -843;
constexpr int DIMSE_STATUS_ERROR = -844;

// 저장 오류 (-860 ~ -879)
constexpr int STORAGE_FAILED = -860;
constexpr int DUPLICATE_SOP_INSTANCE = -861;
constexpr int INVALID_SOP_CLASS = -862;
constexpr int STORAGE_FULL = -863;
constexpr int FILE_WRITE_ERROR = -864;

// 조회 오류 (-880 ~ -899)
constexpr int QUERY_FAILED = -880;
constexpr int NO_MATCHES_FOUND = -881;
constexpr int TOO_MANY_MATCHES = -882;
constexpr int INVALID_QUERY_LEVEL = -883;
constexpr int DATABASE_ERROR = -884;

} // namespace pacs::error_codes
```

---

### UID 생성기

```cpp
#include <pacs/common/uid_generator.h>

namespace pacs::common {

class uid_generator {
public:
    // 새 UID 생성
    static std::string generate();

    // 접두사(조직 루트)로 생성
    static std::string generate(const std::string& root);

    // UID 형식 검증
    static bool is_valid(const std::string& uid);
};

} // namespace pacs::common
```

---

## 웹 모듈

### `pacs::web::rest_server`

Crow 프레임워크를 사용한 REST API 서버 구현체입니다.

```cpp
#include <pacs/web/rest_server.hpp>

namespace pacs::web {

class rest_server {
public:
    // 생성
    explicit rest_server(const rest_server_config& config);
    ~rest_server();

    // 수명 주기
    void start();        // 블로킹
    void start_async();  // 논블로킹
    void stop();         // 정상 종료

    // 설정
    void set_health_checker(std::shared_ptr<monitoring::health_checker> checker);

    // 상태
    bool is_running() const;
    uint16_t port() const;
};

} // namespace pacs::web
```

### `pacs::web::rest_server_config`

REST 서버 설정입니다.

```cpp
#include <pacs/web/rest_server_config.hpp>

namespace pacs::web {

struct rest_server_config {
    uint16_t port = 8080;
    size_t concurrency = 2;
    bool enable_cors = true;
    std::string bind_address = "0.0.0.0";

    // TLS 옵션 (향후)
    bool use_tls = false;
    std::string cert_file;
    std::string key_file;
};

} // namespace pacs::web
```

### DICOMweb (WADO-RS) API

DICOMweb 모듈은 HTTP를 통한 DICOM 객체 검색을 위해 DICOM PS3.18에 정의된
WADO-RS (Web Access to DICOM Objects - RESTful) 사양을 구현합니다.

#### `pacs::web::dicomweb::multipart_builder`

여러 DICOM 객체를 반환하기 위한 multipart/related MIME 응답을 생성합니다.

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

class multipart_builder {
public:
    // 생성 (기본 콘텐츠 타입: application/dicom)
    explicit multipart_builder(std::string_view content_type = media_type::dicom);

    // 파트 추가
    void add_part(std::vector<uint8_t> data,
                  std::optional<std::string_view> content_type = std::nullopt);
    void add_part_with_location(std::vector<uint8_t> data,
                                std::string_view location,
                                std::optional<std::string_view> content_type = std::nullopt);

    // 응답 빌드
    [[nodiscard]] auto build() const -> std::string;
    [[nodiscard]] auto content_type_header() const -> std::string;
    [[nodiscard]] auto boundary() const -> std::string_view;

    // 상태
    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto size() const noexcept -> size_t;
};

} // namespace pacs::web::dicomweb
```

#### 유틸리티 함수

```cpp
namespace pacs::web::dicomweb {

// Accept 헤더를 품질값 기준으로 정렬된 구조화 형식으로 파싱
[[nodiscard]] auto parse_accept_header(std::string_view accept_header)
    -> std::vector<accept_info>;

// 파싱된 Accept 헤더를 기반으로 미디어 타입의 허용 여부 확인
[[nodiscard]] auto is_acceptable(const std::vector<accept_info>& accept_infos,
                                  std::string_view media_type) -> bool;

// DICOM 데이터셋을 DicomJSON 형식으로 변환
[[nodiscard]] auto dataset_to_dicom_json(
    const core::dicom_dataset& dataset,
    bool include_bulk_data = false,
    std::string_view bulk_data_uri_prefix = "") -> std::string;

// DICOM 태그가 벌크 데이터(Pixel Data 등)를 포함하는지 확인
[[nodiscard]] auto is_bulk_data_tag(uint32_t tag) -> bool;

} // namespace pacs::web::dicomweb
```

#### 미디어 타입 상수

```cpp
namespace pacs::web::dicomweb {

struct media_type {
    static constexpr std::string_view dicom = "application/dicom";
    static constexpr std::string_view dicom_json = "application/dicom+json";
    static constexpr std::string_view dicom_xml = "application/dicom+xml";
    static constexpr std::string_view octet_stream = "application/octet-stream";
    static constexpr std::string_view jpeg = "image/jpeg";
    static constexpr std::string_view png = "image/png";
    static constexpr std::string_view multipart_related = "multipart/related";
};

} // namespace pacs::web::dicomweb
```

#### REST API 엔드포인트 (WADO-RS)

**기본 경로**: `/dicomweb`

##### 스터디 조회
*   **메서드**: `GET`
*   **경로**: `/studies/<studyUID>`
*   **설명**: 스터디의 모든 DICOM 인스턴스를 조회합니다.
*   **Accept 헤더**:
    *   `application/dicom` (기본값)
    *   `multipart/related; type="application/dicom"`
    *   `*/*`
*   **응답**:
    *   `200 OK`: DICOM 인스턴스가 포함된 멀티파트 응답.
    *   `404 Not Found`: 스터디를 찾을 수 없음.
    *   `406 Not Acceptable`: 요청한 미디어 타입이 지원되지 않음.

##### 스터디 메타데이터 조회
*   **메서드**: `GET`
*   **경로**: `/studies/<studyUID>/metadata`
*   **설명**: 스터디의 모든 인스턴스에 대한 메타데이터를 조회합니다.
*   **Accept 헤더**:
    *   `application/dicom+json` (기본값)
*   **응답 본문**:
    ```json
    [
      {
        "00080018": { "vr": "UI", "Value": ["1.2.840..."] },
        "00100010": { "vr": "PN", "Value": [{ "Alphabetic": "홍^길동" }] }
      }
    ]
    ```
*   **응답**:
    *   `200 OK`: 인스턴스 메타데이터의 DicomJSON 배열.
    *   `404 Not Found`: 스터디를 찾을 수 없음.

##### 시리즈 조회
*   **메서드**: `GET`
*   **경로**: `/studies/<studyUID>/series/<seriesUID>`
*   **설명**: 시리즈의 모든 DICOM 인스턴스를 조회합니다.
*   **Accept 헤더**: 스터디 조회와 동일.
*   **응답**:
    *   `200 OK`: DICOM 인스턴스가 포함된 멀티파트 응답.
    *   `404 Not Found`: 시리즈를 찾을 수 없음.
    *   `406 Not Acceptable`: 요청한 미디어 타입이 지원되지 않음.

##### 시리즈 메타데이터 조회
*   **메서드**: `GET`
*   **경로**: `/studies/<studyUID>/series/<seriesUID>/metadata`
*   **설명**: 시리즈의 모든 인스턴스에 대한 메타데이터를 조회합니다.
*   **Accept 헤더**: `application/dicom+json` (기본값)
*   **응답**:
    *   `200 OK`: 인스턴스 메타데이터의 DicomJSON 배열.
    *   `404 Not Found`: 시리즈를 찾을 수 없음.

##### 인스턴스 조회
*   **메서드**: `GET`
*   **경로**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>`
*   **설명**: 단일 DICOM 인스턴스를 조회합니다.
*   **Accept 헤더**:
    *   `application/dicom` (기본값)
    *   `multipart/related; type="application/dicom"`
    *   `*/*`
*   **응답**:
    *   `200 OK`: 단일 DICOM 인스턴스가 포함된 멀티파트 응답.
    *   `404 Not Found`: 인스턴스를 찾을 수 없음.
    *   `406 Not Acceptable`: 요청한 미디어 타입이 지원되지 않음.

##### 인스턴스 메타데이터 조회
*   **메서드**: `GET`
*   **경로**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/metadata`
*   **설명**: 단일 인스턴스의 메타데이터를 조회합니다.
*   **Accept 헤더**: `application/dicom+json` (기본값)
*   **응답 본문**:
    ```json
    [
      {
        "00080016": { "vr": "UI", "Value": ["1.2.840.10008.5.1.4.1.1.2"] },
        "00080018": { "vr": "UI", "Value": ["1.2.840...instance"] },
        "00100010": { "vr": "PN", "Value": [{ "Alphabetic": "홍^길동" }] },
        "7FE00010": { "vr": "OW", "BulkDataURI": "/dicomweb/studies/.../bulkdata/..." }
      }
    ]
    ```
*   **응답**:
    *   `200 OK`: 단일 인스턴스 메타데이터의 DicomJSON 배열.
    *   `404 Not Found`: 인스턴스를 찾을 수 없음.

##### 프레임 조회
*   **메소드**: `GET`
*   **경로**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/frames/<frameList>`
*   **설명**: 다중 프레임 DICOM 인스턴스에서 특정 프레임을 조회합니다.
*   **경로 매개변수**:
    *   `frameList`: 쉼표로 구분된 프레임 번호 또는 범위 (1부터 시작). 예시:
        *   `1` - 단일 프레임
        *   `1,3,5` - 여러 프레임
        *   `1-5` - 프레임 범위 (1, 2, 3, 4, 5)
        *   `1,3-5,7` - 혼합 표기
*   **Accept 헤더**:
    *   `application/octet-stream` (기본값) - 원시 픽셀 데이터
    *   `multipart/related; type="application/octet-stream"` - 여러 프레임
*   **응답**:
    *   `200 OK`: 프레임 픽셀 데이터의 멀티파트 응답.
    *   `400 Bad Request`: 잘못된 프레임 목록 구문.
    *   `404 Not Found`: 인스턴스를 찾을 수 없거나 프레임 번호가 범위를 벗어남.
    *   `406 Not Acceptable`: 요청된 미디어 타입이 지원되지 않음.

##### 렌더링된 이미지 조회
*   **메소드**: `GET`
*   **경로**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/rendered`
*   **설명**: DICOM 인스턴스에서 렌더링된 (소비자용) 이미지를 조회합니다.
*   **쿼리 매개변수**:
    *   `window-center` (선택): VOI LUT 윈도우 센터 값
    *   `window-width` (선택): VOI LUT 윈도우 너비 값
    *   `quality` (선택): JPEG 품질 (1-100, 기본값 75)
    *   `viewport` (선택): `너비,높이` 형식의 출력 크기
    *   `frame` (선택): 다중 프레임 이미지의 프레임 번호 (1부터 시작, 기본값 1)
*   **Accept 헤더**:
    *   `image/jpeg` (기본값) - JPEG 출력
    *   `image/png` - PNG 출력
    *   `*/*` - JPEG (기본값)
*   **응답**:
    *   `200 OK`: 요청된 형식의 렌더링된 이미지.
    *   `404 Not Found`: 인스턴스를 찾을 수 없음.
    *   `406 Not Acceptable`: 요청된 미디어 타입이 지원되지 않음.

##### 프레임 렌더링 이미지 조회
*   **메소드**: `GET`
*   **경로**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/frames/<frameNumber>/rendered`
*   **설명**: 다중 프레임 DICOM 인스턴스에서 특정 프레임의 렌더링된 이미지를 조회합니다.
*   **쿼리 매개변수**: 렌더링된 이미지 조회와 동일 (`frame` 제외).
*   **Accept 헤더**: 렌더링된 이미지 조회와 동일.
*   **응답**:
    *   `200 OK`: 요청된 형식의 렌더링된 이미지.
    *   `400 Bad Request`: 잘못된 프레임 번호.
    *   `404 Not Found`: 인스턴스를 찾을 수 없거나 프레임이 범위를 벗어남.
    *   `406 Not Acceptable`: 요청된 미디어 타입이 지원되지 않음.

#### 프레임 조회 유틸리티

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

/**
 * @brief URL 경로에서 프레임 번호 파싱
 * @param frame_list 쉼표로 구분된 프레임 번호 (예: "1,3,5" 또는 "1-5")
 * @return 프레임 번호 벡터 (1부터 시작), 파싱 오류 시 빈 벡터
 */
[[nodiscard]] auto parse_frame_numbers(std::string_view frame_list)
    -> std::vector<uint32_t>;

/**
 * @brief 픽셀 데이터에서 단일 프레임 추출
 * @param pixel_data 전체 픽셀 데이터 버퍼
 * @param frame_number 추출할 프레임 번호 (1부터 시작)
 * @param frame_size 각 프레임의 바이트 크기
 * @return 프레임 데이터, 존재하지 않으면 빈 벡터
 */
[[nodiscard]] auto extract_frame(
    std::span<const uint8_t> pixel_data,
    uint32_t frame_number,
    size_t frame_size) -> std::vector<uint8_t>;

} // namespace pacs::web::dicomweb
```

#### 렌더링 이미지 유틸리티

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

/**
 * @brief 렌더링된 이미지 출력 형식
 */
enum class rendered_format {
    jpeg,   ///< JPEG 형식 (기본값)
    png     ///< PNG 형식
};

/**
 * @brief 렌더링된 이미지 요청 매개변수
 */
struct rendered_params {
    rendered_format format{rendered_format::jpeg};  ///< 출력 형식
    int quality{75};                                 ///< JPEG 품질 (1-100)
    std::optional<double> window_center;             ///< VOI LUT 윈도우 센터
    std::optional<double> window_width;              ///< VOI LUT 윈도우 너비
    uint16_t viewport_width{0};                      ///< 출력 너비 (0 = 원본)
    uint16_t viewport_height{0};                     ///< 출력 높이 (0 = 원본)
    uint32_t frame{1};                               ///< 프레임 번호 (1부터 시작)
    std::optional<std::string> presentation_state_uid;  ///< 선택적 GSPS UID
    bool burn_annotations{false};                    ///< 주석 번인
};

/**
 * @brief 렌더링 작업 결과
 */
struct rendered_result {
    std::vector<uint8_t> data;      ///< 렌더링된 이미지 데이터
    std::string content_type;        ///< 결과의 MIME 타입
    bool success{false};             ///< 렌더링 성공 여부
    std::string error_message;       ///< 실패 시 오류 메시지

    [[nodiscard]] static rendered_result ok(std::vector<uint8_t> data,
                                            std::string content_type);
    [[nodiscard]] static rendered_result error(std::string msg);
};

/**
 * @brief HTTP 요청에서 렌더링된 이미지 매개변수 파싱
 */
[[nodiscard]] auto parse_rendered_params(
    std::string_view query_string,
    std::string_view accept_header) -> rendered_params;

/**
 * @brief 픽셀 데이터에 윈도우/레벨 변환 적용
 */
[[nodiscard]] auto apply_window_level(
    std::span<const uint8_t> pixel_data,
    uint16_t width, uint16_t height,
    uint16_t bits_stored, bool is_signed,
    double window_center, double window_width,
    double rescale_slope, double rescale_intercept) -> std::vector<uint8_t>;

/**
 * @brief DICOM 이미지를 소비자 형식으로 렌더링
 */
[[nodiscard]] auto render_dicom_image(
    std::string_view file_path,
    const rendered_params& params) -> rendered_result;

} // namespace pacs::web::dicomweb
```

### STOW-RS (Store Over the Web) API

STOW-RS 모듈은 HTTP를 통한 DICOM 객체 저장을 위해 DICOM PS3.18에 정의된
Store Over the Web - RESTful 사양을 구현합니다.

#### `pacs::web::dicomweb::multipart_parser`

STOW-RS 업로드를 위한 multipart/related 요청 본문을 파싱합니다.

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

struct multipart_part {
    std::string content_type;       // 이 파트의 Content-Type
    std::string content_location;   // Content-Location 헤더 (선택사항)
    std::string content_id;         // Content-ID 헤더 (선택사항)
    std::vector<uint8_t> data;      // 이 파트의 바이너리 데이터
};

class multipart_parser {
public:
    struct parse_error {
        std::string code;       // 오류 코드 (예: "INVALID_BOUNDARY")
        std::string message;    // 사람이 읽을 수 있는 오류 메시지
    };

    struct parse_result {
        std::vector<multipart_part> parts;  // 파싱된 파트 (오류 시 빈 값)
        std::optional<parse_error> error;   // 파싱 실패 시 오류

        [[nodiscard]] bool success() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };

    // multipart/related 요청 본문 파싱
    [[nodiscard]] static auto parse(std::string_view content_type,
                                    std::string_view body) -> parse_result;

    // Content-Type 헤더에서 boundary 추출
    [[nodiscard]] static auto extract_boundary(std::string_view content_type)
        -> std::optional<std::string>;

    // Content-Type 헤더에서 type 매개변수 추출
    [[nodiscard]] static auto extract_type(std::string_view content_type)
        -> std::optional<std::string>;
};

} // namespace pacs::web::dicomweb
```

#### 저장 응답 타입

```cpp
namespace pacs::web::dicomweb {

struct store_instance_result {
    bool success = false;                   // 저장 성공 여부
    std::string sop_class_uid;              // 인스턴스의 SOP Class UID
    std::string sop_instance_uid;           // 인스턴스의 SOP Instance UID
    std::string retrieve_url;               // 저장된 인스턴스 조회 URL
    std::optional<std::string> error_code;  // 실패 시 오류 코드
    std::optional<std::string> error_message; // 실패 시 오류 메시지
};

struct store_response {
    std::vector<store_instance_result> referenced_instances;  // 성공적으로 저장됨
    std::vector<store_instance_result> failed_instances;      // 저장 실패

    [[nodiscard]] bool all_success() const noexcept;
    [[nodiscard]] bool all_failed() const noexcept;
    [[nodiscard]] bool partial_success() const noexcept;
};

struct validation_result {
    bool valid = true;                      // 유효성 검사 통과 여부
    std::string error_code;                 // 유효하지 않은 경우 오류 코드
    std::string error_message;              // 유효하지 않은 경우 오류 메시지

    [[nodiscard]] explicit operator bool() const noexcept;

    static validation_result ok();
    static validation_result error(std::string code, std::string message);
};

} // namespace pacs::web::dicomweb
```

#### STOW-RS 유틸리티 함수

```cpp
namespace pacs::web::dicomweb {

// STOW-RS 저장을 위한 DICOM 인스턴스 유효성 검사
[[nodiscard]] auto validate_instance(
    const core::dicom_dataset& dataset,
    std::optional<std::string_view> target_study_uid = std::nullopt)
    -> validation_result;

// DicomJSON 형식으로 STOW-RS 응답 빌드
[[nodiscard]] auto build_store_response_json(
    const store_response& response,
    std::string_view base_url) -> std::string;

} // namespace pacs::web::dicomweb
```

#### REST API 엔드포인트 (STOW-RS)

**기본 경로**: `/dicomweb`

##### 인스턴스 저장 (Study 미지정)
*   **메서드**: `POST`
*   **경로**: `/studies`
*   **설명**: 대상 Study를 지정하지 않고 DICOM 인스턴스를 저장합니다.
*   **Content-Type**: `multipart/related; type="application/dicom"; boundary=...`
*   **요청 본문**: DICOM Part 10 파일이 포함된 Multipart/related.
    ```
    --boundary
    Content-Type: application/dicom

    <DICOM Part 10 바이너리 데이터>
    --boundary
    Content-Type: application/dicom

    <DICOM Part 10 바이너리 데이터>
    --boundary--
    ```
*   **응답**:
    *   `200 OK`: 모든 인스턴스가 성공적으로 저장됨.
        ```json
        {
          "00081190": { "vr": "UR", "Value": ["/dicomweb/studies/1.2.3"] },
          "00081199": {
            "vr": "SQ",
            "Value": [{
              "00081150": { "vr": "UI", "Value": ["1.2.840.10008.5.1.4.1.1.2"] },
              "00081155": { "vr": "UI", "Value": ["1.2.3.4.5.6.7.8.9"] },
              "00081190": { "vr": "UR", "Value": ["/dicomweb/studies/1.2.3/series/.../instances/..."] }
            }]
          }
        }
        ```
    *   `202 Accepted`: 일부 인스턴스 저장 실패 (부분 성공).
    *   `409 Conflict`: 모든 인스턴스 저장 실패.
    *   `415 Unsupported Media Type`: 유효하지 않은 Content-Type.

##### 인스턴스 저장 (Study 지정)
*   **메서드**: `POST`
*   **경로**: `/studies/<studyUID>`
*   **설명**: 특정 Study에 DICOM 인스턴스를 저장합니다.
*   **Content-Type**: `multipart/related; type="application/dicom"; boundary=...`
*   **요청 본문**: Study 미지정 저장과 동일.
*   **유효성 검사**: 모든 인스턴스의 Study Instance UID가 `<studyUID>`와 일치해야 합니다.
*   **응답**:
    *   `200 OK`: 모든 인스턴스가 성공적으로 저장됨.
    *   `202 Accepted`: 일부 인스턴스 저장 실패 (부분 성공).
    *   `409 Conflict`: 모든 인스턴스 저장 실패 또는 Study UID 불일치.
    *   `415 Unsupported Media Type`: 유효하지 않은 Content-Type.

---

## 문서 이력

| 버전 | 날짜       | 변경 사항                                     |
|------|------------|-----------------------------------------------|
| 1.0.0 | 2025-11-30 | 초기 API 참조 문서                             |
| 1.1.0 | 2025-12-05 | thread_system 통합 API 추가                    |
| 1.2.0 | 2025-12-07 | Network V2 모듈 추가 (dicom_server_v2, dicom_association_handler) |
| 1.3.0 | 2025-12-07 | 모니터링 모듈 추가 (DIMSE 작업 추적용 pacs_metrics) |
| 1.4.0 | 2025-12-07 | DX 모달리티 모듈 추가 (dx_storage, dx_iod_validator) |
| 1.5.0 | 2025-12-08 | MG 모달리티 모듈 추가 (mg_storage, mg_iod_validator) |
| 1.6.0 | 2025-12-09 | 웹 모듈 추가 (rest_server 기반)               |
| 1.7.0 | 2025-12-11 | Patient, Study, Series REST API 엔드포인트 추가 |
| 1.8.0 | 2025-12-12 | DICOMweb (WADO-RS) API 엔드포인트 및 유틸리티 추가 |
| 1.9.0 | 2025-12-13 | STOW-RS (Store Over the Web) API 엔드포인트 및 멀티파트 파서 추가 |
| 1.10.0 | 2025-12-13 | QIDO-RS (Query based on ID for DICOM Objects) API 엔드포인트 추가 |
| 1.11.0 | 2025-12-13 | WADO-RS 프레임 조회 및 렌더링 이미지 엔드포인트 추가 |
| 1.12.0 | 2025-12-18 | DI 모듈 추가 (ServiceContainer 기반 의존성 주입) |

---

## DI 모듈

DI (Dependency Injection) 모듈은 PACS 서비스를 위한 ServiceContainer 기반 의존성 주입을 제공합니다.

### `pacs::di::IDicomStorage`

`pacs::storage::storage_interface`의 별칭입니다. 서비스 등록 및 해결에 사용됩니다.

### `pacs::di::IDicomNetwork`

DICOM 네트워크 작업을 위한 인터페이스입니다.

```cpp
#include <pacs/di/service_interfaces.hpp>

namespace pacs::di {

class IDicomNetwork {
public:
    virtual ~IDicomNetwork() = default;

    // DICOM 서버 생성
    [[nodiscard]] virtual std::unique_ptr<network::dicom_server> create_server(
        const network::server_config& config,
        const integration::tls_config& tls_cfg = {}) = 0;

    // 원격 DICOM 피어에 연결
    [[nodiscard]] virtual integration::Result<integration::network_adapter::session_ptr>
        connect(const integration::connection_config& config) = 0;
};

} // namespace pacs::di
```

### `pacs::di::register_services`

ServiceContainer에 모든 PACS 서비스를 등록합니다.

```cpp
#include <pacs/di/service_registration.hpp>

// 컨테이너 생성 및 서비스 등록
kcenon::common::di::service_container container;
auto result = pacs::di::register_services(container);

// 서비스 해결
auto storage = container.resolve<pacs::di::IDicomStorage>().value();
auto network = container.resolve<pacs::di::IDicomNetwork>().value();
```

### 설정

```cpp
pacs::di::registration_config config;
config.storage_path = "/path/to/storage";  // 사용자 지정 저장 경로
config.enable_network = true;              // 네트워크 서비스 활성화
config.use_singletons = true;              // 싱글톤 수명 사용

auto result = pacs::di::register_services(container, config);
```

### 테스트 지원

이 모듈은 단위 테스트를 위한 모의 구현을 제공합니다.

```cpp
#include <pacs/di/test_support.hpp>

// 모의 서비스가 포함된 테스트 컨테이너 생성
auto container = pacs::di::test::create_test_container();

// 또는 빌더를 사용하여 더 세밀한 제어
auto mock_storage = std::make_shared<pacs::di::test::MockStorage>();
auto container = pacs::di::test::TestContainerBuilder()
    .with_storage(mock_storage)
    .with_mock_network()
    .build();

// 테스트에서 사용
auto storage = container->resolve<pacs::di::IDicomStorage>().value();
storage->store(dataset);  // 모의 구현 사용

// 모의 상호작용 확인
EXPECT_EQ(mock_storage->store_count(), 1);
```

---

*문서 버전: 0.1.12.0*
*작성일: 2025-11-30*
*최종 수정일: 2025-12-18*
*작성자: kcenon@naver.com*
