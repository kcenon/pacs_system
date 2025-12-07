# API 참조 - PACS 시스템

> **버전:** 1.3.0
> **최종 수정일:** 2025-12-07
> **언어:** [English](API_REFERENCE.md) | **한국어**

---

## 목차

- [코어 모듈](#코어-모듈)
- [인코딩 모듈](#인코딩-모듈)
- [네트워크 모듈](#네트워크-모듈)
- [서비스 모듈](#서비스-모듈)
- [저장 모듈](#저장-모듈)
- [모니터링 모듈](#모니터링-모듈)
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

## 문서 이력

| 버전 | 날짜       | 변경 사항                                     |
|------|------------|-----------------------------------------------|
| 1.0.0 | 2025-11-30 | 초기 API 참조 문서                             |
| 1.1.0 | 2025-12-05 | thread_system 통합 API 추가                    |
| 1.2.0 | 2025-12-07 | Network V2 모듈 추가 (dicom_server_v2, dicom_association_handler) |
| 1.3.0 | 2025-12-07 | 모니터링 모듈 추가 (DIMSE 작업 추적용 pacs_metrics) |

---

*문서 버전: 1.3.0*
*작성일: 2025-11-30*
*최종 수정일: 2025-12-07*
*작성자: kcenon@naver.com*
