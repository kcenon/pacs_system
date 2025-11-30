# API 참조 - PACS 시스템

> **버전:** 1.0.0
> **최종 수정일:** 2025-01-30
> **언어:** [English](API_REFERENCE.md) | **한국어**

---

## 목차

- [코어 모듈](#코어-모듈)
- [인코딩 모듈](#인코딩-모듈)
- [네트워크 모듈](#네트워크-모듈)
- [서비스 모듈](#서비스-모듈)
- [저장 모듈](#저장-모듈)
- [통합 모듈](#통합-모듈)
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

*문서 버전: 1.0.0*
*작성일: 2025-01-30*
*작성자: kcenon@naver.com*
