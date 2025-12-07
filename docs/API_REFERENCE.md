# API Reference - PACS System

> **Version:** 1.4.0
> **Last Updated:** 2025-12-08
> **Language:** **English** | [한국어](API_REFERENCE_KO.md)

---

## Table of Contents

- [Core Module](#core-module)
- [Encoding Module](#encoding-module)
- [Network Module](#network-module)
- [Network V2 Module (Optional)](#network-v2-module-optional)
- [Services Module](#services-module)
- [Storage Module](#storage-module)
- [Monitoring Module](#monitoring-module)
- [Integration Module](#integration-module)
- [Common Module](#common-module)

---

## Core Module

### `pacs::core::dicom_tag`

Represents a DICOM Tag (Group, Element pair).

```cpp
#include <pacs/core/dicom_tags.h>

namespace pacs::core {

class dicom_tag {
public:
    // Constructors
    constexpr dicom_tag(uint16_t group, uint16_t element);
    constexpr dicom_tag(uint32_t tag);  // Combined tag value

    // Accessors
    constexpr uint16_t group() const noexcept;
    constexpr uint16_t element() const noexcept;
    constexpr uint32_t value() const noexcept;

    // Comparison
    constexpr bool operator==(const dicom_tag& other) const noexcept;
    constexpr bool operator<(const dicom_tag& other) const noexcept;

    // String representation
    std::string to_string() const;  // "(GGGG,EEEE)"
};

// Standard tag constants
namespace tags {
    // Patient Module
    constexpr dicom_tag PatientName{0x0010, 0x0010};
    constexpr dicom_tag PatientID{0x0010, 0x0020};
    constexpr dicom_tag PatientBirthDate{0x0010, 0x0030};
    constexpr dicom_tag PatientSex{0x0010, 0x0040};

    // Study Module
    constexpr dicom_tag StudyInstanceUID{0x0020, 0x000D};
    constexpr dicom_tag StudyDate{0x0008, 0x0020};
    constexpr dicom_tag StudyTime{0x0008, 0x0030};
    constexpr dicom_tag AccessionNumber{0x0008, 0x0050};

    // Series Module
    constexpr dicom_tag SeriesInstanceUID{0x0020, 0x000E};
    constexpr dicom_tag Modality{0x0008, 0x0060};
    constexpr dicom_tag SeriesNumber{0x0020, 0x0011};

    // Instance Module
    constexpr dicom_tag SOPInstanceUID{0x0008, 0x0018};
    constexpr dicom_tag SOPClassUID{0x0008, 0x0016};
    constexpr dicom_tag InstanceNumber{0x0020, 0x0013};

    // Image Module
    constexpr dicom_tag Rows{0x0028, 0x0010};
    constexpr dicom_tag Columns{0x0028, 0x0011};
    constexpr dicom_tag BitsAllocated{0x0028, 0x0100};
    constexpr dicom_tag PixelData{0x7FE0, 0x0010};

    // ... (complete list in dicom_tags.h)
}

} // namespace pacs::core
```

---

### `pacs::core::dicom_element`

Represents a DICOM Data Element.

```cpp
#include <pacs/core/dicom_element.h>

namespace pacs::core {

class dicom_element {
public:
    // Factory methods
    static dicom_element create(dicom_tag tag, vr_type vr);
    static dicom_element create(dicom_tag tag, vr_type vr, const std::string& value);
    static dicom_element create(dicom_tag tag, vr_type vr, const std::vector<uint8_t>& bytes);

    template<typename T>
    static dicom_element create_numeric(dicom_tag tag, vr_type vr, T value);

    // Accessors
    dicom_tag tag() const noexcept;
    vr_type vr() const noexcept;
    uint32_t length() const noexcept;
    bool is_empty() const noexcept;

    // Value access
    std::string as_string() const;
    std::vector<std::string> as_strings() const;  // Multi-valued

    template<typename T>
    T as_numeric() const;

    std::vector<uint8_t> as_bytes() const;

    // For SQ (Sequence)
    bool is_sequence() const noexcept;
    std::vector<dicom_dataset>& items();
    const std::vector<dicom_dataset>& items() const;

    // Value modification
    void set_string(const std::string& value);
    void set_bytes(const std::vector<uint8_t>& bytes);

    template<typename T>
    void set_numeric(T value);

    // Serialization
    std::vector<uint8_t> serialize(const transfer_syntax& ts) const;
    static common::Result<dicom_element> deserialize(
        const std::vector<uint8_t>& data,
        const transfer_syntax& ts
    );
};

} // namespace pacs::core
```

**Example:**
```cpp
using namespace pacs::core;

// Create string element
auto patient_name = dicom_element::create(
    tags::PatientName,
    vr_type::PN,
    "Doe^John"
);

// Create numeric element
auto rows = dicom_element::create_numeric(
    tags::Rows,
    vr_type::US,
    uint16_t{512}
);

// Access values
std::cout << patient_name.as_string() << std::endl;  // "Doe^John"
std::cout << rows.as_numeric<uint16_t>() << std::endl;  // 512
```

---

### `pacs::core::dicom_dataset`

Ordered collection of DICOM Data Elements.

```cpp
#include <pacs/core/dicom_dataset.h>

namespace pacs::core {

class dicom_dataset {
public:
    // Construction
    dicom_dataset() = default;
    dicom_dataset(const dicom_dataset& other);
    dicom_dataset(dicom_dataset&& other) noexcept;

    // Element access
    bool contains(dicom_tag tag) const noexcept;
    const dicom_element* get(dicom_tag tag) const;
    dicom_element* get(dicom_tag tag);

    // Typed getters (return default if not found)
    std::string get_string(dicom_tag tag, const std::string& default_value = "") const;
    std::vector<std::string> get_strings(dicom_tag tag) const;

    template<typename T>
    T get_numeric(dicom_tag tag, T default_value = T{}) const;

    std::vector<uint8_t> get_bytes(dicom_tag tag) const;

    // Typed setters
    void set_string(dicom_tag tag, const std::string& value);
    void set_strings(dicom_tag tag, const std::vector<std::string>& values);

    template<typename T>
    void set_numeric(dicom_tag tag, T value);

    void set_bytes(dicom_tag tag, const std::vector<uint8_t>& bytes);

    // Element management
    void add(dicom_element element);
    void remove(dicom_tag tag);
    void clear();

    // Iteration (tag order)
    using iterator = /* implementation-defined */;
    using const_iterator = /* implementation-defined */;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    // Properties
    size_t size() const noexcept;
    bool empty() const noexcept;

    // Serialization
    std::vector<uint8_t> serialize(const transfer_syntax& ts) const;
    static common::Result<dicom_dataset> deserialize(
        const std::vector<uint8_t>& data,
        const transfer_syntax& ts
    );

    // Copy subset
    dicom_dataset subset(const std::vector<dicom_tag>& tags) const;

    // Merge
    void merge(const dicom_dataset& other, bool overwrite = false);
};

} // namespace pacs::core
```

**Example:**
```cpp
using namespace pacs::core;

dicom_dataset dataset;

// Set values
dataset.set_string(tags::PatientName, "Doe^John");
dataset.set_string(tags::PatientID, "12345");
dataset.set_numeric(tags::Rows, uint16_t{512});
dataset.set_numeric(tags::Columns, uint16_t{512});

// Get values
auto name = dataset.get_string(tags::PatientName);
auto rows = dataset.get_numeric<uint16_t>(tags::Rows);

// Check existence
if (dataset.contains(tags::PixelData)) {
    auto pixels = dataset.get_bytes(tags::PixelData);
}

// Iterate
for (const auto& element : dataset) {
    std::cout << element.tag().to_string() << ": "
              << element.as_string() << std::endl;
}
```

---

### `pacs::core::dicom_file`

DICOM Part 10 file handling.

```cpp
#include <pacs/core/dicom_file.h>

namespace pacs::core {

class dicom_file {
public:
    // Factory methods
    static common::Result<dicom_file> read(const std::filesystem::path& path);
    static common::Result<dicom_file> read(std::istream& stream);

    // Construction
    dicom_file() = default;

    // File Meta Information
    const dicom_dataset& file_meta_info() const noexcept;
    dicom_dataset& file_meta_info() noexcept;

    // Data Set
    const dicom_dataset& dataset() const noexcept;
    dicom_dataset& dataset() noexcept;
    void set_dataset(dicom_dataset dataset);

    // Transfer Syntax
    transfer_syntax transfer_syntax() const;
    void set_transfer_syntax(transfer_syntax ts);

    // Media Storage SOP
    std::string media_storage_sop_class_uid() const;
    std::string media_storage_sop_instance_uid() const;

    // Write
    common::Result<void> write(const std::filesystem::path& path) const;
    common::Result<void> write(std::ostream& stream) const;

    // Validation
    common::Result<void> validate() const;
};

} // namespace pacs::core
```

**Example:**
```cpp
using namespace pacs::core;

// Read file
auto result = dicom_file::read("/path/to/image.dcm");
if (result.is_err()) {
    std::cerr << "Read failed: " << result.error().message << std::endl;
    return;
}

auto& file = result.value();

// Access data
std::cout << "Patient: " << file.dataset().get_string(tags::PatientName) << std::endl;
std::cout << "Transfer Syntax: " << file.transfer_syntax().uid() << std::endl;

// Modify and write
file.dataset().set_string(tags::InstitutionName, "My Hospital");
auto write_result = file.write("/path/to/modified.dcm");
```

---

## Encoding Module

### `pacs::encoding::vr_type`

Enumeration of DICOM Value Representations.

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

// VR utilities
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

Transfer Syntax management.

```cpp
#include <pacs/encoding/transfer_syntax.h>

namespace pacs::encoding {

class transfer_syntax {
public:
    // Predefined transfer syntaxes
    static transfer_syntax implicit_vr_little_endian();
    static transfer_syntax explicit_vr_little_endian();
    static transfer_syntax explicit_vr_big_endian();

    // Construction
    explicit transfer_syntax(const std::string& uid);

    // Properties
    const std::string& uid() const noexcept;
    bool is_implicit_vr() const noexcept;
    bool is_little_endian() const noexcept;
    bool is_encapsulated() const noexcept;

    // Comparison
    bool operator==(const transfer_syntax& other) const noexcept;

    // Name lookup
    std::string name() const;

    // Validation
    static bool is_valid_uid(const std::string& uid);
};

} // namespace pacs::encoding
```

---

### `pacs::encoding::compression::compression_codec`

Abstract base class for image compression codecs.

```cpp
#include <pacs/encoding/compression/compression_codec.hpp>

namespace pacs::encoding::compression {

// Compression options
struct compression_options {
    int quality = 75;           // 1-100 for JPEG
    bool lossless = false;       // Enable lossless mode if supported
    bool progressive = false;    // Progressive encoding (JPEG)
    int chroma_subsampling = 2;  // 0=4:4:4, 1=4:2:2, 2=4:2:0
};

// Codec operation result
struct codec_result {
    std::vector<uint8_t> data;
    bool success = false;
    std::string error_message;
    image_params output_params;

    static codec_result ok(std::vector<uint8_t> d, const image_params& params);
    static codec_result error(std::string msg);
};

// Abstract codec interface
class compression_codec {
public:
    virtual ~compression_codec() = default;

    // Codec information
    [[nodiscard]] virtual std::string_view transfer_syntax_uid() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual bool is_lossy() const noexcept = 0;
    [[nodiscard]] virtual bool can_encode(const image_params& params) const noexcept = 0;
    [[nodiscard]] virtual bool can_decode(const image_params& params) const noexcept = 0;

    // Compression operations
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

JPEG Baseline (Process 1) codec using libjpeg-turbo.

```cpp
#include <pacs/encoding/compression/jpeg_baseline_codec.hpp>

namespace pacs::encoding::compression {

class jpeg_baseline_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.4.50";

    jpeg_baseline_codec();
    ~jpeg_baseline_codec() override;

    // Move-only (PIMPL)
    jpeg_baseline_codec(jpeg_baseline_codec&&) noexcept;
    jpeg_baseline_codec& operator=(jpeg_baseline_codec&&) noexcept;

    // Codec interface implementation
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

// Usage example
void compress_image() {
    jpeg_baseline_codec codec;

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.samples_per_pixel = 1;  // Grayscale

    std::vector<uint8_t> pixel_data(512 * 512);  // Your image data

    compression_options options;
    options.quality = 90;

    auto result = codec.encode(pixel_data, params, options);
    if (result.success) {
        // result.data contains compressed JPEG
    }
}

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::codec_factory`

Factory for creating compression codecs by Transfer Syntax UID.

```cpp
#include <pacs/encoding/compression/codec_factory.hpp>

namespace pacs::encoding::compression {

class codec_factory {
public:
    // Create codec by Transfer Syntax UID
    [[nodiscard]] static std::unique_ptr<compression_codec> create(
        std::string_view transfer_syntax_uid);

    // Create codec by transfer_syntax object
    [[nodiscard]] static std::unique_ptr<compression_codec> create(
        const transfer_syntax& ts);

    // Query supported codecs
    [[nodiscard]] static std::vector<std::string_view> supported_transfer_syntaxes();
    [[nodiscard]] static bool is_supported(std::string_view transfer_syntax_uid);
    [[nodiscard]] static bool is_supported(const transfer_syntax& ts);
};

// Currently supported Transfer Syntaxes:
// - 1.2.840.10008.1.2.4.50 - JPEG Baseline (Process 1)

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::image_params`

Parameters describing image pixel data for compression.

```cpp
#include <pacs/encoding/compression/image_params.hpp>

namespace pacs::encoding::compression {

enum class photometric_interpretation {
    monochrome1,      // Min = white
    monochrome2,      // Min = black
    rgb,              // RGB color
    ycbcr_full,       // YCbCr full range
    ycbcr_full_422,   // YCbCr 4:2:2
    palette_color,    // Palette lookup
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

    // Utility methods
    [[nodiscard]] size_t frame_size_bytes() const noexcept;
    [[nodiscard]] bool is_grayscale() const noexcept;
    [[nodiscard]] bool is_color() const noexcept;
    [[nodiscard]] bool is_signed() const noexcept;
    [[nodiscard]] bool valid_for_jpeg_baseline() const noexcept;
};

// String conversion
[[nodiscard]] std::string to_string(photometric_interpretation pi);
[[nodiscard]] photometric_interpretation parse_photometric_interpretation(const std::string& str);

} // namespace pacs::encoding::compression
```

---

## Network Module

### `pacs::network::association`

DICOM Association management.

```cpp
#include <pacs/network/association.h>

namespace pacs::network {

// Association configuration
struct association_config {
    std::string calling_ae_title;    // SCU AE Title
    std::string called_ae_title;     // SCP AE Title
    uint32_t max_pdu_size = 16384;   // Maximum PDU size
    std::chrono::seconds timeout{30}; // Association timeout

    // Add presentation context
    void add_context(
        const std::string& abstract_syntax,
        const std::vector<transfer_syntax>& transfer_syntaxes
    );
};

// Association state
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
    // Factory (SCU)
    static common::Result<association> connect(
        const std::string& host,
        uint16_t port,
        const association_config& config
    );

    // Factory (SCP) - created by server
    // association is move-only
    association(association&&) noexcept;
    association& operator=(association&&) noexcept;
    ~association();

    // State
    association_state state() const noexcept;
    bool is_established() const noexcept;

    // Properties
    const std::string& calling_ae_title() const noexcept;
    const std::string& called_ae_title() const noexcept;
    uint32_t max_pdu_size() const noexcept;

    // Presentation context
    bool has_accepted_context(const std::string& abstract_syntax) const;
    std::optional<uint8_t> accepted_context_id(
        const std::string& abstract_syntax
    ) const;
    transfer_syntax accepted_transfer_syntax(uint8_t context_id) const;

    // DIMSE operations
    common::Result<void> send_dimse(
        uint8_t context_id,
        const dimse::dimse_message& message
    );

    common::Result<dimse::dimse_message> receive_dimse();

    // Lifecycle
    common::Result<void> release();
    void abort();
};

} // namespace pacs::network
```

---

### `pacs::network::dimse::dimse_message`

DIMSE message handling for both DIMSE-C and DIMSE-N services.

```cpp
#include <pacs/network/dimse/dimse_message.hpp>

namespace pacs::network::dimse {

class dimse_message {
public:
    // DIMSE-C Factory Methods
    static dimse_message make_c_echo_rq(uint16_t msg_id, std::string_view sop_class_uid);
    static dimse_message make_c_store_rq(uint16_t msg_id, ...);
    static dimse_message make_c_find_rq(uint16_t msg_id, ...);
    static dimse_message make_c_move_rq(uint16_t msg_id, ...);
    static dimse_message make_c_get_rq(uint16_t msg_id, ...);

    // DIMSE-N Factory Methods
    static dimse_message make_n_create_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid = ""
    );
    static dimse_message make_n_create_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        status_code status
    );

    static dimse_message make_n_set_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid
    );
    static dimse_message make_n_set_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        status_code status
    );

    static dimse_message make_n_get_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        const std::vector<core::dicom_tag>& attribute_list = {}
    );
    static dimse_message make_n_get_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        status_code status
    );

    static dimse_message make_n_event_report_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        uint16_t event_type_id
    );
    static dimse_message make_n_event_report_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        uint16_t event_type_id,
        status_code status
    );

    static dimse_message make_n_action_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        uint16_t action_type_id
    );
    static dimse_message make_n_action_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        uint16_t action_type_id,
        status_code status
    );

    static dimse_message make_n_delete_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid
    );
    static dimse_message make_n_delete_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        status_code status
    );

    // DIMSE-N Specific Accessors
    std::string requested_sop_class_uid() const;
    void set_requested_sop_class_uid(std::string_view uid);
    std::string requested_sop_instance_uid() const;
    void set_requested_sop_instance_uid(std::string_view uid);
    std::optional<uint16_t> event_type_id() const;
    void set_event_type_id(uint16_t type_id);
    std::optional<uint16_t> action_type_id() const;
    void set_action_type_id(uint16_t type_id);
    std::vector<core::dicom_tag> attribute_identifier_list() const;
    void set_attribute_identifier_list(const std::vector<core::dicom_tag>& tags);
};

} // namespace pacs::network::dimse
```

**DIMSE-N Example:**
```cpp
using namespace pacs::network::dimse;

// N-CREATE: Create MPPS instance
auto create_rq = make_n_create_rq(1, mpps_sop_class_uid, generated_instance_uid);
create_rq.set_data_set(mpps_dataset);

// N-SET: Update MPPS status
auto set_rq = make_n_set_rq(2, mpps_sop_class_uid, mpps_instance_uid);
set_rq.set_data_set(status_update_dataset);

// N-EVENT-REPORT: Storage commitment notification
auto event_rq = make_n_event_report_rq(3, storage_commitment_sop_class, instance_uid, 1);
event_rq.set_data_set(commitment_result);

// N-ACTION: Request storage commitment
auto action_rq = make_n_action_rq(4, storage_commitment_sop_class, instance_uid, 1);
action_rq.set_data_set(commitment_request);

// N-GET: Retrieve specific attributes
std::vector<core::dicom_tag> attrs = {tags::PatientName, tags::PatientID};
auto get_rq = make_n_get_rq(5, printer_sop_class, printer_instance_uid, attrs);

// N-DELETE: Delete print job
auto delete_rq = make_n_delete_rq(6, print_job_sop_class, job_instance_uid);
```

**Example:**
```cpp
using namespace pacs::network;

// Configure association
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.add_context(
    sop_class::verification,
    {transfer_syntax::implicit_vr_little_endian()}
);

// Connect
auto result = association::connect("192.168.1.100", 11112, config);
if (result.is_err()) {
    std::cerr << "Connect failed: " << result.error().message << std::endl;
    return;
}

auto assoc = std::move(result.value());

// Perform C-ECHO
// ...

// Release
assoc.release();
```

---

### `pacs::network::v2::dicom_association_handler`

Bridge between network_system's messaging_session and DICOM protocol handling.
This class is part of the Phase 4 network_system integration effort.

```cpp
#include <pacs/network/v2/dicom_association_handler.hpp>

namespace pacs::network::v2 {

// Handler state machine
enum class handler_state {
    idle,              // Initial state, waiting for A-ASSOCIATE-RQ
    awaiting_response, // Sent response, awaiting next PDU
    established,       // Association established, processing DIMSE
    releasing,         // Graceful release in progress
    closed             // Association closed (released or aborted)
};

// Callback types
using association_established_callback = std::function<void(
    const std::string& session_id,
    const std::string& calling_ae,
    const std::string& called_ae)>;

using association_closed_callback = std::function<void(
    const std::string& session_id,
    bool graceful)>;

using handler_error_callback = std::function<void(
    const std::string& session_id,
    const std::string& error)>;

class dicom_association_handler
    : public std::enable_shared_from_this<dicom_association_handler> {
public:
    using session_ptr = std::shared_ptr<network_system::session::messaging_session>;
    using service_map = std::map<std::string, services::scp_service*>;

    // Constants
    static constexpr size_t pdu_header_size = 6;
    static constexpr size_t max_pdu_size = 64 * 1024 * 1024;

    // Construction
    dicom_association_handler(
        session_ptr session,
        const server_config& config,
        const service_map& services);
    ~dicom_association_handler();

    // Non-copyable, non-movable
    dicom_association_handler(const dicom_association_handler&) = delete;
    dicom_association_handler& operator=(const dicom_association_handler&) = delete;

    // Lifecycle
    void start();                      // Begin processing session
    void stop(bool graceful = false);  // Stop handler

    // State queries
    handler_state state() const noexcept;
    bool is_established() const noexcept;
    bool is_closed() const noexcept;
    std::string session_id() const;
    std::string calling_ae() const;
    std::string called_ae() const;
    association& get_association();

    // Callbacks
    void set_established_callback(association_established_callback callback);
    void set_closed_callback(association_closed_callback callback);
    void set_error_callback(handler_error_callback callback);

    // Statistics
    uint64_t pdus_received() const noexcept;
    uint64_t pdus_sent() const noexcept;
    uint64_t messages_processed() const noexcept;
};

} // namespace pacs::network::v2
```

**Example Usage:**

```cpp
#include <pacs/network/v2/dicom_association_handler.hpp>

using namespace pacs::network;
using namespace pacs::network::v2;

// Create handler for an incoming session
auto handler = std::make_shared<dicom_association_handler>(
    session,           // messaging_session from network_system
    server_config,     // Server configuration
    service_map        // SOP Class -> Service mapping
);

// Set up callbacks
handler->set_established_callback(
    [](const auto& session_id, const auto& calling, const auto& called) {
        std::cout << "Association: " << calling << " -> " << called << '\n';
    });

handler->set_closed_callback(
    [](const auto& session_id, bool graceful) {
        std::cout << "Closed: " << (graceful ? "graceful" : "aborted") << '\n';
    });

handler->set_error_callback(
    [](const auto& session_id, const auto& error) {
        std::cerr << "Error: " << error << '\n';
    });

// Start processing
handler->start();

// Handler will automatically:
// 1. Receive and validate A-ASSOCIATE-RQ
// 2. Send A-ASSOCIATE-AC or A-ASSOCIATE-RJ
// 3. Dispatch DIMSE messages to registered services
// 4. Handle A-RELEASE-RQ with A-RELEASE-RP
// 5. Handle A-ABORT
```

---

### `pacs::network::v2::dicom_server_v2`

DICOM server using network_system's messaging_server for connection management.
This is the recommended server implementation for new applications.

```cpp
#include <pacs/network/v2/dicom_server_v2.hpp>

namespace pacs::network::v2 {

class dicom_server_v2 {
public:
    // Type aliases
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

    // Construction
    explicit dicom_server_v2(const server_config& config);
    ~dicom_server_v2();

    // Non-copyable, non-movable
    dicom_server_v2(const dicom_server_v2&) = delete;
    dicom_server_v2& operator=(const dicom_server_v2&) = delete;

    // Service registration (call before start)
    void register_service(services::scp_service_ptr service);
    std::vector<std::string> supported_sop_classes() const;

    // Lifecycle
    Result<std::monostate> start();
    void stop(duration timeout = std::chrono::seconds{30});
    void wait_for_shutdown();

    // Status
    bool is_running() const noexcept;
    size_t active_associations() const noexcept;
    server_statistics get_statistics() const;
    const server_config& config() const noexcept;

    // Callbacks
    void on_association_established(association_established_callback callback);
    void on_association_closed(association_closed_callback callback);
    void on_error(error_callback callback);
};

} // namespace pacs::network::v2
```

**Usage Example:**

```cpp
#include <pacs/network/v2/dicom_server_v2.hpp>
#include <pacs/services/verification_scp.hpp>
#include <pacs/services/storage_scp.hpp>

using namespace pacs::network;
using namespace pacs::network::v2;
using namespace pacs::services;

// Configure server
server_config config;
config.ae_title = "MY_PACS";
config.port = 11112;
config.max_associations = 20;
config.idle_timeout = std::chrono::minutes{5};

// Create server
dicom_server_v2 server{config};

// Register services
server.register_service(std::make_unique<verification_scp>());
server.register_service(std::make_unique<storage_scp>("/path/to/storage"));

// Set callbacks
server.on_association_established(
    [](const std::string& session_id,
       const std::string& calling_ae,
       const std::string& called_ae) {
        std::cout << "Association: " << calling_ae << " -> " << called_ae << '\n';
    });

server.on_association_closed(
    [](const std::string& session_id, bool graceful) {
        std::cout << "Session " << session_id << " closed "
                  << (graceful ? "gracefully" : "abruptly") << '\n';
    });

server.on_error([](const std::string& error) {
    std::cerr << "Server error: " << error << '\n';
});

// Start server
auto result = server.start();
if (result.is_err()) {
    std::cerr << "Failed to start: " << result.error().message << '\n';
    return 1;
}

std::cout << "Server running on port " << config.port << '\n';

// Wait for shutdown signal
server.wait_for_shutdown();
```

**Key Features:**

- **API-compatible** with existing `dicom_server` for easy migration
- **Automatic session management** via network_system callbacks
- **Thread-safe** handler map with proper synchronization
- **Configurable limits** for max associations with automatic rejection
- **Idle timeout support** for cleaning up inactive connections

**Migration from dicom_server:**

1. Replace `dicom_server` with `dicom_server_v2`
2. No changes needed to service registration or callbacks
3. Requires `PACS_WITH_NETWORK_SYSTEM` compile definition

---

## Services Module

### `pacs::services::verification_scu`

Verification SCU (C-ECHO).

```cpp
#include <pacs/services/verification_scu.h>

namespace pacs::services {

class verification_scu {
public:
    explicit verification_scu(const std::string& ae_title);

    // Simple echo
    common::Result<void> echo(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port
    );

    // Echo with existing association
    common::Result<void> echo(network::association& assoc);
};

} // namespace pacs::services
```

---

### `pacs::services::storage_scp`

Storage SCP (C-STORE receiver).

```cpp
#include <pacs/services/storage_scp.h>

namespace pacs::services {

// Storage status codes
enum class storage_status : uint16_t {
    success = 0x0000,
    warning_coercion = 0xB000,
    warning_elements_discarded = 0xB007,
    error_cannot_understand = 0xC000,
    error_out_of_resources = 0xA700
};

// Storage handler callback
using storage_handler = std::function<storage_status(
    const core::dicom_dataset& dataset,
    const std::string& calling_ae
)>;

// Configuration
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

    // Handler
    void set_handler(storage_handler handler);

    // Lifecycle
    common::Result<void> start();
    void stop();
    void wait_for_shutdown();

    // Status
    bool is_running() const noexcept;
    size_t active_associations() const noexcept;
};

} // namespace pacs::services
```

**Example:**
```cpp
using namespace pacs::services;

storage_scp_config config;
config.ae_title = "PACS_SCP";
config.port = 11112;
config.storage_path = "/data/dicom";

storage_scp server(config);

server.set_handler([](const core::dicom_dataset& dataset,
                      const std::string& calling_ae) {
    std::cout << "Received from " << calling_ae << ": "
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

Storage SCU (C-STORE sender).

```cpp
#include <pacs/services/storage_scu.h>

namespace pacs::services {

class storage_scu {
public:
    explicit storage_scu(const std::string& ae_title);

    // Send single image
    common::Result<void> store(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port,
        const core::dicom_file& file
    );

    // Send multiple images
    common::Result<void> store_batch(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port,
        const std::vector<std::filesystem::path>& files,
        std::function<void(size_t, size_t)> progress_callback = nullptr
    );

    // Send with existing association
    common::Result<void> store(
        network::association& assoc,
        const core::dicom_file& file
    );
};

} // namespace pacs::services
```

---

### `pacs::services::sop_classes` - XA/XRF Storage

X-Ray Angiographic and Radiofluoroscopic Storage SOP Classes.

```cpp
#include <pacs/services/sop_classes/xa_storage.hpp>

namespace pacs::services::sop_classes {

// SOP Class UIDs
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

// Photometric Interpretation (grayscale only for XA)
enum class xa_photometric_interpretation {
    monochrome1,  // Minimum value = white
    monochrome2   // Minimum value = black
};

// XA SOP Class information
struct xa_sop_class_info {
    std::string_view uid;
    std::string_view name;
    bool is_enhanced;
    bool is_3d;
    bool supports_multiframe;
};

// Positioner angles (LAO/RAO, Cranial/Caudal)
struct xa_positioner_angles {
    double primary_angle;    // LAO(+) / RAO(-) in degrees
    double secondary_angle;  // Cranial(+) / Caudal(-) in degrees
};

// QCA calibration data
struct xa_calibration_data {
    double imager_pixel_spacing[2];       // mm/pixel
    double distance_source_to_detector;   // SID in mm
    double distance_source_to_patient;    // SOD in mm
};

// Utility functions
[[nodiscard]] bool is_xa_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_enhanced_xa_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_xrf_sop_class(std::string_view uid) noexcept;
[[nodiscard]] const xa_sop_class_info* get_xa_sop_class_info(std::string_view uid);

[[nodiscard]] std::vector<std::string_view> get_xa_transfer_syntaxes();
[[nodiscard]] bool is_valid_xa_photometric(std::string_view photometric) noexcept;
[[nodiscard]] xa_photometric_interpretation
    parse_xa_photometric(std::string_view value) noexcept;

// Calibration utilities
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

**Example:**
```cpp
using namespace pacs::services::sop_classes;

// Check if dataset is XA
if (is_xa_sop_class(sop_class_uid)) {
    auto info = get_xa_sop_class_info(sop_class_uid);
    std::cout << "SOP Class: " << info->name << std::endl;

    if (info->is_enhanced) {
        // Handle enhanced XA with functional groups
    }
}

// Calculate real-world measurement from QCA
xa_calibration_data cal{
    {0.2, 0.2},  // 0.2mm pixel spacing
    1000.0,      // SID = 1000mm
    750.0        // SOD = 750mm
};
double mag_factor = calculate_magnification_factor(cal.distance_source_to_detector,
                                                    cal.distance_source_to_patient);
double real_size = calculate_real_world_size(100.0, cal.imager_pixel_spacing[0], mag_factor);
```

---

### `pacs::services::validation::xa_iod_validator`

IOD Validator for X-Ray Angiographic images.

```cpp
#include <pacs/services/validation/xa_iod_validator.hpp>

namespace pacs::services::validation {

// XA-specific DICOM tags
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

// Validation severity levels
enum class validation_severity {
    error,    // Must be corrected
    warning,  // Should be corrected
    info      // Informational
};

// Validation result for a single finding
struct validation_finding {
    validation_severity severity;
    std::string module;
    std::string message;
    std::optional<core::dicom_tag> tag;
};

// Complete validation result
struct validation_result {
    bool is_valid;
    std::vector<validation_finding> findings;

    [[nodiscard]] bool has_errors() const noexcept;
    [[nodiscard]] bool has_warnings() const noexcept;
    [[nodiscard]] std::vector<validation_finding> errors() const;
    [[nodiscard]] std::vector<validation_finding> warnings() const;
};

// Validation options
struct xa_validation_options {
    bool validate_patient_module = true;
    bool validate_study_module = true;
    bool validate_series_module = true;
    bool validate_equipment_module = true;
    bool validate_acquisition_module = true;
    bool validate_image_module = true;
    bool validate_calibration = true;       // XA-specific
    bool validate_multiframe = true;        // XA-specific
    bool strict_mode = false;               // Treat warnings as errors
};

class xa_iod_validator {
public:
    // Validate complete XA IOD
    [[nodiscard]] static validation_result validate(
        const core::dicom_dataset& dataset,
        const xa_validation_options& options = {}
    );

    // Validate specific modules
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

    // Quick checks
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

**Example:**
```cpp
using namespace pacs::services::validation;

// Full validation
xa_validation_options opts;
opts.strict_mode = true;  // Fail on warnings

auto result = xa_iod_validator::validate(dataset, opts);

if (!result.is_valid) {
    for (const auto& finding : result.errors()) {
        std::cerr << "[" << finding.module << "] "
                  << finding.message << std::endl;
    }
}

// Quick calibration check
if (xa_iod_validator::has_calibration_data(dataset)) {
    auto cal_result = xa_iod_validator::validate_calibration_data(dataset);
    // Process calibration validation...
}
```

---

### `pacs::services::cache::simple_lru_cache<Key, Value>`

Thread-safe LRU (Least Recently Used) cache with TTL support.

```cpp
#include <pacs/services/cache/simple_lru_cache.hpp>

namespace pacs::services::cache {

// Cache configuration
struct cache_config {
    std::size_t max_size{1000};           // Maximum entries
    std::chrono::seconds ttl{300};        // Time-to-live (5 min default)
    bool enable_metrics{true};            // Hit/miss tracking
    std::string cache_name{"lru_cache"};  // For logging/metrics
};

// Cache statistics
struct cache_stats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> expirations{0};

    double hit_rate() const noexcept;     // Returns 0.0-100.0
    void reset() noexcept;
};

template <typename Key, typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class simple_lru_cache {
public:
    // Construction
    explicit simple_lru_cache(const cache_config& config = {});
    simple_lru_cache(size_type max_size, std::chrono::seconds ttl);

    // Cache operations
    [[nodiscard]] std::optional<Value> get(const Key& key);
    void put(const Key& key, const Value& value);
    void put(const Key& key, Value&& value);
    bool invalidate(const Key& key);
    void clear();
    size_type purge_expired();

    // Information
    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_type max_size() const noexcept;
    [[nodiscard]] std::chrono::seconds ttl() const noexcept;

    // Statistics
    [[nodiscard]] const cache_stats& stats() const noexcept;
    [[nodiscard]] double hit_rate() const noexcept;
    void reset_stats() noexcept;
};

// Type alias for string-keyed cache
template <typename Value>
using string_lru_cache = simple_lru_cache<std::string, Value>;

} // namespace pacs::services::cache
```

**Example:**
```cpp
using namespace pacs::services::cache;

// Create cache with 1000 entry limit and 5-minute TTL
cache_config config;
config.max_size = 1000;
config.ttl = std::chrono::seconds{300};
config.cache_name = "query_cache";

simple_lru_cache<std::string, QueryResult> cache(config);

// Store and retrieve
cache.put("patient_123", result);

auto cached = cache.get("patient_123");
if (cached) {
    // Use cached result
    process(*cached);
}

// Check performance
auto rate = cache.hit_rate();
logger_adapter::info("Cache hit rate: {:.1f}%", rate);
```

---

### `pacs::services::cache::query_cache`

Specialized cache for DICOM C-FIND query results.

```cpp
#include <pacs/services/cache/query_cache.hpp>

namespace pacs::services::cache {

// Configuration
struct query_cache_config {
    std::size_t max_entries{1000};
    std::chrono::seconds ttl{300};
    bool enable_logging{false};
    bool enable_metrics{true};
    std::string cache_name{"cfind_query_cache"};
};

// Cached query result
struct cached_query_result {
    std::vector<uint8_t> data;                        // Serialized result
    uint32_t match_count{0};                          // Number of matches
    std::chrono::steady_clock::time_point cached_at;  // Cache timestamp
    std::string query_level;                          // PATIENT/STUDY/SERIES/IMAGE
};

class query_cache {
public:
    explicit query_cache(const query_cache_config& config = {});

    // Cache operations
    [[nodiscard]] std::optional<cached_query_result> get(const std::string& key);
    void put(const std::string& key, const cached_query_result& result);
    void put(const std::string& key, cached_query_result&& result);
    bool invalidate(const std::string& key);
    void clear();
    size_type purge_expired();

    // Information
    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] const cache_stats& stats() const noexcept;
    [[nodiscard]] double hit_rate() const noexcept;

    // Key generation helpers
    [[nodiscard]] static std::string build_key(
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);

    [[nodiscard]] static std::string build_key_with_ae(
        const std::string& calling_ae,
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);
};

// Global cache access
[[nodiscard]] query_cache& global_query_cache();
bool configure_global_cache(const query_cache_config& config);

} // namespace pacs::services::cache
```

**Example:**
```cpp
using namespace pacs::services::cache;

// Build cache key from query parameters
std::string key = query_cache::build_key("STUDY", {
    {"PatientID", "12345"},
    {"StudyDate", "20240101-20241231"}
});

// Check cache first
auto& cache = global_query_cache();
auto result = cache.get(key);

if (result) {
    // Return cached result
    return result->data;
}

// Execute query and cache result
auto query_result = execute_cfind(...);

cached_query_result cached;
cached.data = serialize(query_result);
cached.match_count = query_result.size();
cached.query_level = "STUDY";
cached.cached_at = std::chrono::steady_clock::now();

cache.put(key, std::move(cached));
```

---

## Storage Module

### `pacs::storage::storage_interface`

Abstract storage backend interface.

```cpp
#include <pacs/storage/storage_interface.h>

namespace pacs::storage {

class storage_interface {
public:
    virtual ~storage_interface() = default;

    // Store
    virtual common::Result<void> store(const core::dicom_dataset& dataset) = 0;

    // Retrieve
    virtual common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid
    ) = 0;

    // Delete
    virtual common::Result<void> remove(const std::string& sop_instance_uid) = 0;

    // Query
    virtual common::Result<std::vector<core::dicom_dataset>> find(
        const core::dicom_dataset& query
    ) = 0;

    // Check existence
    virtual bool exists(const std::string& sop_instance_uid) = 0;
};

} // namespace pacs::storage
```

---

### `pacs::storage::file_storage`

Filesystem-based storage implementation.

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

    // storage_interface implementation
    common::Result<void> store(const core::dicom_dataset& dataset) override;
    common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid
    ) override;
    common::Result<void> remove(const std::string& sop_instance_uid) override;
    common::Result<std::vector<core::dicom_dataset>> find(
        const core::dicom_dataset& query
    ) override;
    bool exists(const std::string& sop_instance_uid) override;

    // File path
    std::filesystem::path file_path(const std::string& sop_instance_uid) const;
};

} // namespace pacs::storage
```

---

## DX Modality Module

### `pacs::services::sop_classes::dx_storage`

Digital X-Ray (DX) SOP Class definitions and utilities for general radiography imaging.

```cpp
#include <pacs/services/sop_classes/dx_storage.hpp>

namespace pacs::services::sop_classes {

// SOP Class UIDs
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

// Enumerations
enum class dx_photometric_interpretation { monochrome1, monochrome2 };
enum class dx_image_type { for_presentation, for_processing };
enum class dx_view_position { ap, pa, lateral, oblique, other };
enum class dx_detector_type { direct, indirect, storage, film };
enum class dx_body_part { chest, abdomen, pelvis, spine, skull, hand, foot,
                          knee, elbow, shoulder, hip, wrist, ankle, extremity,
                          breast, other };

// SOP Class information structure
struct dx_sop_class_info {
    std::string_view uid;
    std::string_view name;
    std::string_view description;
    dx_image_type image_type;
    bool is_mammography;
    bool is_intraoral;
};

// Utility functions
[[nodiscard]] std::vector<std::string> get_dx_transfer_syntaxes();
[[nodiscard]] std::vector<std::string> get_dx_storage_sop_classes(
    bool include_mammography = true, bool include_intraoral = true);
[[nodiscard]] const dx_sop_class_info* get_dx_sop_class_info(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_storage_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_for_processing_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_for_presentation_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_mammography_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_valid_dx_photometric(std::string_view value) noexcept;

// Enum conversion utilities
[[nodiscard]] std::string_view to_string(dx_photometric_interpretation interp) noexcept;
[[nodiscard]] std::string_view to_string(dx_view_position position) noexcept;
[[nodiscard]] std::string_view to_string(dx_detector_type type) noexcept;
[[nodiscard]] std::string_view to_string(dx_body_part part) noexcept;
[[nodiscard]] dx_view_position parse_view_position(std::string_view value) noexcept;
[[nodiscard]] dx_body_part parse_body_part(std::string_view value) noexcept;

} // namespace pacs::services::sop_classes
```

**Example:**
```cpp
using namespace pacs::services::sop_classes;

// Check if a SOP Class UID is DX
if (is_dx_storage_sop_class(sop_class_uid)) {
    const auto* info = get_dx_sop_class_info(sop_class_uid);

    if (info->image_type == dx_image_type::for_presentation) {
        // Display-ready image, apply VOI LUT
    } else {
        // Raw data, requires processing
    }

    if (info->is_mammography) {
        // Apply mammography-specific display settings
    }
}

// Get all general DX SOP Classes (exclude mammography/intraoral)
auto dx_classes = get_dx_storage_sop_classes(false, false);

// Parse body part from DICOM dataset
auto body_part = parse_body_part("CHEST");  // Returns dx_body_part::chest
auto view = parse_view_position("PA");       // Returns dx_view_position::pa
```

---

### `pacs::services::validation::dx_iod_validator`

IOD Validator for Digital X-Ray images per DICOM PS3.3 Section A.26.

```cpp
#include <pacs/services/validation/dx_iod_validator.hpp>

namespace pacs::services::validation {

// Validation options
struct dx_validation_options {
    bool check_type1 = true;               // Required attributes
    bool check_type2 = true;               // Required, can be empty
    bool check_conditional = true;         // Conditionally required
    bool validate_pixel_data = true;       // Pixel consistency checks
    bool validate_dx_specific = true;      // DX module validation
    bool validate_anatomy = true;          // Body part/view validation
    bool validate_presentation_requirements = true;
    bool validate_processing_requirements = true;
    bool allow_both_photometric = true;    // MONOCHROME1 and 2
    bool strict_mode = false;              // Warnings as errors
};

class dx_iod_validator {
public:
    dx_iod_validator() = default;
    explicit dx_iod_validator(const dx_validation_options& options);

    // Full validation
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    // Specialized validation
    [[nodiscard]] validation_result validate_for_presentation(
        const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_for_processing(
        const core::dicom_dataset& dataset) const;

    // Quick Type 1 check only
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    // Options access
    [[nodiscard]] const dx_validation_options& options() const noexcept;
    void set_options(const dx_validation_options& options);
};

// Convenience functions
[[nodiscard]] validation_result validate_dx_iod(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_valid_dx_dataset(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_presentation_dx(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_processing_dx(const core::dicom_dataset& dataset);

} // namespace pacs::services::validation
```

**Example:**
```cpp
using namespace pacs::services::validation;

// Basic validation
auto result = validate_dx_iod(dataset);

if (!result.is_valid) {
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            std::cerr << "[ERROR] " << finding.code << ": " << finding.message << "\n";
        }
    }
    return;
}

// Strict validation with custom options
dx_validation_options options;
options.strict_mode = true;  // Treat warnings as errors

dx_iod_validator validator(options);
auto strict_result = validator.validate(dataset);

// Validate For Presentation image with VOI LUT checks
if (is_for_presentation_dx(dataset)) {
    auto result = validator.validate_for_presentation(dataset);
    // Includes Window Center/Width or VOI LUT Sequence validation
}
```

---

## Monitoring Module

### `pacs::monitoring::pacs_metrics`

Thread-safe metrics collection for DICOM operations with atomic counters and timing data.

```cpp
#include <pacs/monitoring/pacs_metrics.hpp>

namespace pacs::monitoring {

// DIMSE operation types
enum class dimse_operation {
    c_echo,    // C-ECHO (Verification)
    c_store,   // C-STORE (Storage)
    c_find,    // C-FIND (Query)
    c_move,    // C-MOVE (Retrieve)
    c_get,     // C-GET (Retrieve)
    n_create,  // N-CREATE (MPPS)
    n_set,     // N-SET (MPPS)
    n_get,     // N-GET
    n_action,  // N-ACTION
    n_event,   // N-EVENT-REPORT
    n_delete   // N-DELETE
};

// Atomic counter for operation statistics
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

// Data transfer tracking
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

// Association lifecycle tracking
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
    // Singleton access
    static pacs_metrics& global_metrics() noexcept;

    // DIMSE operation recording
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

    // Data transfer recording
    void record_bytes_sent(uint64_t bytes) noexcept;
    void record_bytes_received(uint64_t bytes) noexcept;

    // Association recording
    void record_association_established() noexcept;
    void record_association_released() noexcept;
    void record_association_rejected() noexcept;
    void record_association_aborted() noexcept;

    // Metric access
    const operation_counter& get_counter(dimse_operation op) const noexcept;
    const data_transfer_metrics& transfer() const noexcept;
    const association_counters& associations() const noexcept;

    // Export
    std::string to_json() const;
    std::string to_prometheus(std::string_view prefix = "pacs") const;

    // Reset all metrics
    void reset() noexcept;
};

} // namespace pacs::monitoring
```

**Usage Example:**

```cpp
#include <pacs/monitoring/pacs_metrics.hpp>
using namespace pacs::monitoring;

// Get global metrics instance
auto& metrics = pacs_metrics::global_metrics();

// Record a C-STORE operation
auto start = std::chrono::steady_clock::now();
// ... perform C-STORE ...
auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
metrics.record_store(true, duration, 1024 * 1024);  // 1MB image

// Export for monitoring systems
std::string json = metrics.to_json();           // For REST API
std::string prom = metrics.to_prometheus();      // For Prometheus
```

---

## Common Module

### Error Codes

```cpp
#include <pacs/common/error_codes.h>

namespace pacs::error_codes {

// DICOM Parsing Errors (-800 to -819)
constexpr int INVALID_DICOM_FILE = -800;
constexpr int INVALID_VR = -801;
constexpr int MISSING_REQUIRED_TAG = -802;
constexpr int INVALID_TRANSFER_SYNTAX = -803;
constexpr int INVALID_DATA_ELEMENT = -804;
constexpr int SEQUENCE_ENCODING_ERROR = -805;

// Association Errors (-820 to -839)
constexpr int ASSOCIATION_REJECTED = -820;
constexpr int ASSOCIATION_ABORTED = -821;
constexpr int NO_PRESENTATION_CONTEXT = -822;
constexpr int INVALID_PDU = -823;
constexpr int PDU_TOO_LARGE = -824;
constexpr int ASSOCIATION_TIMEOUT = -825;

// DIMSE Errors (-840 to -859)
constexpr int DIMSE_FAILURE = -840;
constexpr int DIMSE_TIMEOUT = -841;
constexpr int DIMSE_INVALID_RESPONSE = -842;
constexpr int DIMSE_CANCELLED = -843;
constexpr int DIMSE_STATUS_ERROR = -844;

// Storage Errors (-860 to -879)
constexpr int STORAGE_FAILED = -860;
constexpr int DUPLICATE_SOP_INSTANCE = -861;
constexpr int INVALID_SOP_CLASS = -862;
constexpr int STORAGE_FULL = -863;
constexpr int FILE_WRITE_ERROR = -864;

// Query Errors (-880 to -899)
constexpr int QUERY_FAILED = -880;
constexpr int NO_MATCHES_FOUND = -881;
constexpr int TOO_MANY_MATCHES = -882;
constexpr int INVALID_QUERY_LEVEL = -883;
constexpr int DATABASE_ERROR = -884;

} // namespace pacs::error_codes
```

---

### UID Generator

```cpp
#include <pacs/common/uid_generator.h>

namespace pacs::common {

class uid_generator {
public:
    // Generate new UID
    static std::string generate();

    // Generate with prefix (organization root)
    static std::string generate(const std::string& root);

    // Validate UID format
    static bool is_valid(const std::string& uid);
};

} // namespace pacs::common
```

---

## Document History

| Version | Date       | Changes                                      |
|---------|------------|----------------------------------------------|
| 1.0.0   | 2025-11-30 | Initial API Reference document               |
| 1.1.0   | 2025-12-05 | Added thread_system integration APIs         |
| 1.2.0   | 2025-12-07 | Added Network V2 Module (dicom_server_v2, dicom_association_handler) |
| 1.3.0   | 2025-12-07 | Added Monitoring Module (pacs_metrics for DIMSE operation tracking) |
| 1.4.0   | 2025-12-07 | Added DX Modality Module (dx_storage, dx_iod_validator) |

---

*Document Version: 1.4.0*
*Created: 2025-11-30*
*Last Updated: 2025-12-07*
*Author: kcenon@naver.com*
