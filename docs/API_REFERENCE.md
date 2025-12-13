# API Reference - PACS System

> **Version:** 0.1.5.0
> **Last Updated:** 2025-12-13
> **Language:** **English** | [한국어](API_REFERENCE_KO.md)

---

## Table of Contents

- [Core Module](#core-module)
- [Encoding Module](#encoding-module)
- [Network Module](#network-module)
- [Network V2 Module (Optional)](#network-v2-module-optional)
- [Services Module](#services-module)
- [Storage Module](#storage-module)
- [AI Module](#ai-module)
- [Monitoring Module](#monitoring-module)
- [Integration Module](#integration-module)
- [Web Module](#web-module)
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

### `pacs::encoding::compression::jpeg_lossless_codec`

JPEG Lossless (Process 14, Selection Value 1) codec for diagnostic quality images.

```cpp
#include <pacs/encoding/compression/jpeg_lossless_codec.hpp>

namespace pacs::encoding::compression {

class jpeg_lossless_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.4.70";
    static constexpr int kDefaultPredictor = 1;       // Ra (left neighbor)
    static constexpr int kDefaultPointTransform = 0;  // No scaling

    // Construct with optional predictor (1-7) and point transform (0-15)
    explicit jpeg_lossless_codec(int predictor = kDefaultPredictor,
                                  int point_transform = kDefaultPointTransform);
    ~jpeg_lossless_codec() override;

    // Move-only (PIMPL)
    jpeg_lossless_codec(jpeg_lossless_codec&&) noexcept;
    jpeg_lossless_codec& operator=(jpeg_lossless_codec&&) noexcept;

    // Configuration
    [[nodiscard]] int predictor() const noexcept;
    [[nodiscard]] int point_transform() const noexcept;

    // Codec interface implementation
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;  // "JPEG Lossless (Process 14, SV1)"
    [[nodiscard]] bool is_lossy() const noexcept override;  // false (lossless!)

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

// Supported Predictors:
// 1: Ra (left neighbor)
// 2: Rb (above neighbor)
// 3: Rc (diagonal upper-left neighbor)
// 4: Ra + Rb - Rc
// 5: Ra + (Rb - Rc) / 2
// 6: Rb + (Ra - Rc) / 2
// 7: (Ra + Rb) / 2

// Supported bit depths: 8, 12, 16-bit grayscale

// Usage example
void compress_ct_image_lossless() {
    jpeg_lossless_codec codec;  // Default predictor 1

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 16;
    params.bits_stored = 12;    // 12-bit CT image
    params.samples_per_pixel = 1;  // Grayscale only

    std::vector<uint8_t> pixel_data(512 * 512 * 2);  // 16-bit storage

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data contains losslessly compressed JPEG
        // Original data can be perfectly reconstructed
    }
}

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::jpeg2000_codec`

JPEG 2000 codec supporting both lossless and lossy compression modes.

```cpp
#include <pacs/encoding/compression/jpeg2000_codec.hpp>

namespace pacs::encoding::compression {

class jpeg2000_codec final : public compression_codec {
public:
    // Transfer Syntax UIDs
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.90";  // JPEG 2000 Lossless Only
    static constexpr std::string_view kTransferSyntaxUIDLossy =
        "1.2.840.10008.1.2.4.91";  // JPEG 2000 (Lossy or Lossless)

    static constexpr float kDefaultCompressionRatio = 20.0f;
    static constexpr int kDefaultResolutionLevels = 6;

    // Construct with mode selection
    // lossless: true for 4.90 (lossless only), false for 4.91 (default lossy)
    // compression_ratio: Target ratio for lossy mode (ignored in lossless)
    // resolution_levels: Number of DWT resolution levels (1-32)
    explicit jpeg2000_codec(bool lossless = true,
                            float compression_ratio = kDefaultCompressionRatio,
                            int resolution_levels = kDefaultResolutionLevels);
    ~jpeg2000_codec() override;

    // Move-only (PIMPL)
    jpeg2000_codec(jpeg2000_codec&&) noexcept;
    jpeg2000_codec& operator=(jpeg2000_codec&&) noexcept;

    // Configuration
    [[nodiscard]] bool is_lossless_mode() const noexcept;
    [[nodiscard]] float compression_ratio() const noexcept;
    [[nodiscard]] int resolution_levels() const noexcept;

    // Codec interface implementation
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;  // "JPEG 2000 Lossless" or "JPEG 2000"
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

// Supported features:
// - Bit depths: 1-16 bit (8, 12, 16-bit typical for medical)
// - Grayscale (samples_per_pixel = 1) and Color (samples_per_pixel = 3)
// - Lossless: Reversible 5/3 integer wavelet transform
// - Lossy: Irreversible 9/7 floating-point wavelet transform
// - Automatic J2K/JP2 format detection on decode

// Usage example - Lossless compression for diagnostic images
void compress_mri_lossless() {
    jpeg2000_codec codec(true);  // Lossless mode

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 16;
    params.bits_stored = 16;    // 16-bit MRI
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(512 * 512 * 2);

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data contains losslessly compressed J2K codestream
    }
}

// Usage example - Lossy compression for archival
void compress_xray_lossy() {
    jpeg2000_codec codec(false, 20.0f);  // Lossy, 20:1 ratio

    image_params params;
    params.width = 2048;
    params.height = 2048;
    params.bits_allocated = 16;
    params.bits_stored = 12;
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(2048 * 2048 * 2);

    compression_options options;
    options.quality = 80;  // Maps to compression ratio

    auto result = codec.encode(pixel_data, params, options);
    if (result.success) {
        // result.data contains lossy compressed J2K codestream
    }
}

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::jpeg_ls_codec`

JPEG-LS codec supporting both lossless and near-lossless compression modes.

```cpp
#include <pacs/encoding/compression/jpeg_ls_codec.hpp>

namespace pacs::encoding::compression {

class jpeg_ls_codec final : public compression_codec {
public:
    // Transfer Syntax UIDs
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.80";  // JPEG-LS Lossless
    static constexpr std::string_view kTransferSyntaxUIDNearLossless =
        "1.2.840.10008.1.2.4.81";  // JPEG-LS Near-Lossless (Lossy)

    static constexpr int kAutoNearValue = -1;  // Auto-determine NEAR
    static constexpr int kLosslessNearValue = 0;
    static constexpr int kDefaultNearLosslessValue = 2;
    static constexpr int kMaxNearValue = 255;

    // Construct with mode selection
    // lossless: true for 4.80 (lossless), false for 4.81 (near-lossless)
    // near_value: -1 = auto, 0 = lossless, 1-255 = bounded error
    explicit jpeg_ls_codec(bool lossless = true, int near_value = kAutoNearValue);
    ~jpeg_ls_codec() override;

    // Move-only (PIMPL)
    jpeg_ls_codec(jpeg_ls_codec&&) noexcept;
    jpeg_ls_codec& operator=(jpeg_ls_codec&&) noexcept;

    // Configuration
    [[nodiscard]] bool is_lossless_mode() const noexcept;
    [[nodiscard]] int near_value() const noexcept;

    // Codec interface implementation
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;  // "JPEG-LS Lossless" or "JPEG-LS Near-Lossless"
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

// Supported features:
// - Bit depths: 2-16 bit (8, 12, 16-bit typical for medical imaging)
// - Grayscale (samples_per_pixel = 1) and Color (samples_per_pixel = 3)
// - Lossless: NEAR=0, exact reconstruction
// - Near-Lossless: NEAR>0, bounded error (max |original - decoded| <= NEAR)
// - Uses CharLS library (BSD-3 license) for high-performance encoding/decoding

// Usage example - Lossless compression for CT images
void compress_ct_lossless() {
    jpeg_ls_codec codec(true);  // Lossless mode

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 16;
    params.bits_stored = 12;    // 12-bit CT
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(512 * 512 * 2);

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data contains losslessly compressed JPEG-LS data
        // Typical compression: 2:1 to 4:1 for medical images
    }
}

// Usage example - Near-lossless compression for archival
void compress_xray_near_lossless() {
    jpeg_ls_codec codec(false, 2);  // Near-lossless, NEAR=2

    image_params params;
    params.width = 2048;
    params.height = 2048;
    params.bits_allocated = 16;
    params.bits_stored = 12;
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(2048 * 2048 * 2);

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data contains near-losslessly compressed data
        // Max pixel error: 2 (visually lossless quality)
    }
}

} // namespace pacs::encoding::compression
```

---

### `pacs::encoding::compression::rle_codec`

DICOM RLE Lossless codec implementation (pure C++, no external dependencies).

```cpp
#include <pacs/encoding/compression/rle_codec.hpp>

namespace pacs::encoding::compression {

class rle_codec final : public compression_codec {
public:
    // Transfer Syntax UID
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.5";
    static constexpr int kMaxSegments = 15;
    static constexpr size_t kRLEHeaderSize = 64;

    rle_codec();
    ~rle_codec() override;

    // Move-only (PIMPL)
    rle_codec(rle_codec&&) noexcept;
    rle_codec& operator=(rle_codec&&) noexcept;

    // Codec interface implementation
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;  // "RLE Lossless"
    [[nodiscard]] bool is_lossy() const noexcept override;  // Always false

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

// Supported features:
// - Bit depths: 8-bit and 16-bit
// - Grayscale (samples_per_pixel = 1) and RGB Color (samples_per_pixel = 3)
// - Always lossless (exact reconstruction)
// - Pure C++ implementation using PackBits algorithm
// - No external library dependencies
// - Good compression for images with large uniform areas

// Usage example - Compress 8-bit grayscale image
void compress_grayscale() {
    rle_codec codec;

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.samples_per_pixel = 1;
    params.photometric = photometric_interpretation::monochrome2;

    std::vector<uint8_t> pixel_data(512 * 512);

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // result.data contains RLE compressed data with 64-byte header
    }
}

// Usage example - Compress 16-bit CT image
void compress_ct_16bit() {
    rle_codec codec;

    image_params params;
    params.width = 512;
    params.height = 512;
    params.bits_allocated = 16;
    params.bits_stored = 12;  // 12-bit CT
    params.samples_per_pixel = 1;

    std::vector<uint8_t> pixel_data(512 * 512 * 2);

    auto result = codec.encode(pixel_data, params);
    if (result.success) {
        // 16-bit images are stored as 2 segments (high byte, low byte)
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
// - 1.2.840.10008.1.2.5   - RLE Lossless (pure C++ implementation)
// - 1.2.840.10008.1.2.4.50 - JPEG Baseline (Process 1) - Lossy
// - 1.2.840.10008.1.2.4.70 - JPEG Lossless (Process 14, SV1) - Lossless
// - 1.2.840.10008.1.2.4.80 - JPEG-LS Lossless Image Compression
// - 1.2.840.10008.1.2.4.81 - JPEG-LS Lossy (Near-Lossless) Image Compression
// - 1.2.840.10008.1.2.4.90 - JPEG 2000 Lossless Only
// - 1.2.840.10008.1.2.4.91 - JPEG 2000 (Lossy or Lossless)

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
    [[nodiscard]] bool valid_for_jpeg_baseline() const noexcept;  // 8-bit only
    [[nodiscard]] bool valid_for_jpeg_lossless() const noexcept;  // 2-16 bit grayscale
    [[nodiscard]] bool valid_for_jpeg_ls() const noexcept;        // 2-16 bit grayscale/color
    [[nodiscard]] bool valid_for_jpeg2000() const noexcept;       // 1-16 bit grayscale/color
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
    template<typename Predicate>
    size_type invalidate_if(Predicate pred);  // Conditional invalidation
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
    size_type invalidate_by_prefix(const std::string& prefix);
    size_type invalidate_by_query_level(const std::string& level);
    template<typename Predicate>
    size_type invalidate_if(Predicate pred);
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

// Cache invalidation on C-STORE (when new data is stored)
// Use post_store_handler to invalidate affected cache entries
storage_scp scp{config};
scp.set_post_store_handler([&cache](const auto& dataset,
                                     const auto& patient_id,
                                     const auto& study_uid,
                                     const auto& series_uid,
                                     const auto& sop_uid) {
    // Invalidate all cached queries that might be affected
    cache.invalidate_by_query_level("IMAGE");
    cache.invalidate_by_query_level("SERIES");
    cache.invalidate_by_query_level("STUDY");
    cache.invalidate_by_query_level("PATIENT");
});

// Targeted invalidation using prefix
cache.invalidate_by_prefix("WORKSTATION1/");  // Invalidate specific AE

// Custom predicate invalidation
cache.invalidate_if([](const auto& key, const cached_query_result& r) {
    return r.match_count > 1000;  // Remove large result sets
});
```

---

### `pacs::services::cache::streaming_query_handler`

Streaming query handler for memory-efficient C-FIND responses with pagination.

```cpp
#include <pacs/services/cache/streaming_query_handler.hpp>

namespace pacs::services {

class streaming_query_handler {
public:
    using StreamResult = Result<std::unique_ptr<query_result_stream>>;

    // Construction
    explicit streaming_query_handler(storage::index_database* db);

    // Configuration
    void set_page_size(size_t size) noexcept;        // Default: 100
    [[nodiscard]] auto page_size() const noexcept -> size_t;

    void set_max_results(size_t max) noexcept;       // Default: 0 (unlimited)
    [[nodiscard]] auto max_results() const noexcept -> size_t;

    // Stream Operations
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

    // Compatibility with query_scp
    [[nodiscard]] auto as_query_handler() -> query_handler;
};

} // namespace pacs::services
```

**Example - Streaming interface:**
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
                // Process each DICOM dataset
            }
        }
    }
}
```

**Example - Query SCP integration:**
```cpp
using namespace pacs::services;

query_scp scp;
streaming_query_handler handler(db);
handler.set_max_results(500);

// Use adapter for backward compatibility
scp.set_handler(handler.as_query_handler());
scp.start();
```

---

### `pacs::services::cache::query_result_stream`

Converts database cursor results to DICOM datasets with batch support.

```cpp
#include <pacs/services/cache/query_result_stream.hpp>

namespace pacs::services {

// Stream configuration
struct stream_config {
    size_t page_size{100};  // Batch size for fetching
};

class query_result_stream {
public:
    // Factory methods
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

    // Stream state
    [[nodiscard]] auto has_more() const noexcept -> bool;
    [[nodiscard]] auto position() const noexcept -> size_t;
    [[nodiscard]] auto level() const noexcept -> query_level;
    [[nodiscard]] auto total_count() const -> std::optional<size_t>;

    // Data retrieval
    [[nodiscard]] auto next_batch()
        -> std::optional<std::vector<core::dicom_dataset>>;

    // Cursor for resumption
    [[nodiscard]] auto cursor() const -> std::string;
};

} // namespace pacs::services
```

---

### `pacs::services::cache::database_cursor`

Low-level SQLite cursor for streaming query results with lazy evaluation.

```cpp
#include <pacs/services/cache/database_cursor.hpp>

namespace pacs::services {

// Query record types (variant of all DICOM levels)
using query_record = std::variant<
    storage::patient_record,
    storage::study_record,
    storage::series_record,
    storage::instance_record
>;

class database_cursor {
public:
    enum class record_type { patient, study, series, instance };

    // Factory methods for each query level
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

    // Cursor state
    [[nodiscard]] auto has_more() const noexcept -> bool;
    [[nodiscard]] auto position() const noexcept -> size_t;
    [[nodiscard]] auto type() const noexcept -> record_type;

    // Data retrieval
    [[nodiscard]] auto fetch_next() -> std::optional<query_record>;
    [[nodiscard]] auto fetch_batch(size_t batch_size)
        -> std::vector<query_record>;

    // Cursor control
    [[nodiscard]] auto reset() -> VoidResult;
    [[nodiscard]] auto serialize() const -> std::string;
};

} // namespace pacs::services
```

**Example - Resumable pagination:**
```cpp
using namespace pacs::services;

// First page request
auto cursor_result = database_cursor::create_study_cursor(
    db->native_handle(), study_query{});

if (cursor_result.is_ok()) {
    auto& cursor = cursor_result.value();

    // Fetch first 100 records
    auto batch = cursor->fetch_batch(100);

    // Save cursor state for resumption
    std::string saved_state = cursor->serialize();

    // ... Later, resume from saved state
    // query_result_stream::from_cursor(..., saved_state, ...)
}
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

## MG Modality Module

### `pacs::services::sop_classes::mg_storage`

Digital Mammography X-Ray SOP Class definitions and utilities for breast imaging.

```cpp
#include <pacs/services/sop_classes/mg_storage.hpp>

namespace pacs::services::sop_classes {

// SOP Class UIDs
inline constexpr std::string_view mg_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.2";
inline constexpr std::string_view mg_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.2.1";
inline constexpr std::string_view breast_tomosynthesis_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.3";

// Breast laterality enumeration
enum class breast_laterality { left, right, bilateral, unknown };

// Mammography view positions
enum class mg_view_position {
    cc,      ///< Craniocaudal
    mlo,     ///< Mediolateral Oblique
    ml,      ///< Mediolateral
    lm,      ///< Lateromedial
    xccl,    ///< Exaggerated CC Laterally
    xccm,    ///< Exaggerated CC Medially
    fb,      ///< From Below
    spot,    ///< Spot compression
    mag,     ///< Magnification
    spot_mag,///< Spot with magnification
    implant, ///< Implant displaced (Eklund)
    // ... additional views
    other
};

// Mammography SOP class information
struct mg_sop_class_info {
    std::string_view uid;
    std::string_view name;
    std::string_view description;
    mg_image_type image_type;
    bool is_tomosynthesis;
    bool supports_multiframe;
};

// Utility functions
[[nodiscard]] std::string_view to_string(breast_laterality laterality) noexcept;
[[nodiscard]] breast_laterality parse_breast_laterality(std::string_view value) noexcept;
[[nodiscard]] bool is_valid_breast_laterality(std::string_view value) noexcept;

[[nodiscard]] std::string_view to_string(mg_view_position position) noexcept;
[[nodiscard]] mg_view_position parse_mg_view_position(std::string_view value) noexcept;
[[nodiscard]] bool is_screening_view(mg_view_position position) noexcept;
[[nodiscard]] bool is_magnification_view(mg_view_position position) noexcept;

[[nodiscard]] bool is_valid_compression_force(double force_n) noexcept;
[[nodiscard]] std::pair<double, double> get_typical_compression_force_range() noexcept;

[[nodiscard]] std::vector<std::string> get_mg_storage_sop_classes(bool include_tomosynthesis = true);
[[nodiscard]] const mg_sop_class_info* get_mg_sop_class_info(std::string_view uid) noexcept;
[[nodiscard]] bool is_mg_storage_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_breast_tomosynthesis_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_mg_for_processing_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_mg_for_presentation_sop_class(std::string_view uid) noexcept;

} // namespace pacs::services::sop_classes
```

**Example:**
```cpp
using namespace pacs::services::sop_classes;

// Check if a SOP Class UID is Mammography
if (is_mg_storage_sop_class(sop_class_uid)) {
    const auto* info = get_mg_sop_class_info(sop_class_uid);

    if (info->is_tomosynthesis) {
        // Handle 3D breast tomosynthesis
    }
}

// Parse mammography-specific attributes
auto laterality = parse_breast_laterality("L");  // Left breast
auto view = parse_mg_view_position("MLO");       // Mediolateral oblique

// Check if this is a standard screening view
if (is_screening_view(view)) {
    // CC or MLO - standard screening exam
}

// Validate compression force (typical: 50-200 N)
if (is_valid_compression_force(compression_force)) {
    // Within acceptable range
}
```

---

### `pacs::services::validation::mg_iod_validator`

IOD Validator for Digital Mammography images per DICOM PS3.3 Section A.26.2.

```cpp
#include <pacs/services/validation/mg_iod_validator.hpp>

namespace pacs::services::validation {

// Validation options specific to mammography
struct mg_validation_options {
    bool check_type1 = true;
    bool check_type2 = true;
    bool check_conditional = true;
    bool validate_pixel_data = true;
    bool validate_mg_specific = true;
    bool validate_laterality = true;        // Breast laterality (0020,0060)
    bool validate_view_position = true;     // View position (0018,5101)
    bool validate_compression = true;       // Compression force (0018,11A2)
    bool validate_implant_attributes = true;
    bool validate_dose_parameters = true;
    bool strict_mode = false;
};

class mg_iod_validator {
public:
    mg_iod_validator() = default;
    explicit mg_iod_validator(const mg_validation_options& options);

    // Full IOD validation
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    // Specialized validations
    [[nodiscard]] validation_result validate_for_presentation(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_for_processing(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_laterality(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_view_position(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_compression_force(const core::dicom_dataset& dataset) const;

    // Quick validation
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    // Configuration
    [[nodiscard]] const mg_validation_options& options() const noexcept;
    void set_options(const mg_validation_options& options);
};

// Convenience functions
[[nodiscard]] validation_result validate_mg_iod(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_valid_mg_dataset(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_presentation_mg(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_processing_mg(const core::dicom_dataset& dataset);
[[nodiscard]] bool has_breast_implant(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_screening_mammogram(const core::dicom_dataset& dataset);

} // namespace pacs::services::validation
```

**Error Codes (MG-specific):**
| Code | Severity | Description |
|------|----------|-------------|
| MG-ERR-001 | Error | SOPClassUID is not a mammography storage class |
| MG-ERR-002 | Error | Modality must be 'MG' |
| MG-ERR-003 | Error | Laterality not specified (required for mammography) |
| MG-ERR-004 | Error | Invalid laterality value |
| MG-ERR-005 | Error | Invalid image laterality value |
| MG-ERR-006 | Error | BitsStored exceeds BitsAllocated |
| MG-ERR-007 | Error | Mammography must be grayscale |
| MG-ERR-008 | Error | Invalid photometric interpretation |
| MG-WARN-001 | Warning | Laterality mismatch between series and image level |
| MG-WARN-003 | Warning | Body Part Examined should be 'BREAST' |
| MG-WARN-005 | Warning | View Position not present |
| MG-WARN-010 | Warning | Unrecognized view position |
| MG-WARN-013 | Warning | Compression force outside typical range |
| MG-INFO-007 | Info | Breast implant present but not using ID view |
| MG-INFO-008 | Info | Compression force not documented |

**Example:**
```cpp
using namespace pacs::services::validation;

// Validate a mammography dataset
auto result = validate_mg_iod(dataset);
if (!result.is_valid) {
    for (const auto& finding : result.findings) {
        if (finding.code.starts_with("MG-ERR-")) {
            std::cerr << "Error: " << finding.message << "\n";
        }
    }
}

// Validate specific aspects
mg_iod_validator validator;

auto laterality_result = validator.validate_laterality(dataset);
auto view_result = validator.validate_view_position(dataset);
auto compression_result = validator.validate_compression_force(dataset);

// Check for screening exam
if (is_screening_mammogram(dataset)) {
    // Standard 4-view screening (RCC, LCC, RMLO, LMLO)
}

// Check for implant
if (has_breast_implant(dataset)) {
    // May need implant-displaced views
}
```

---

## AI Module

### `pacs::ai::ai_result_handler`

Handler for AI-generated DICOM objects including Structured Reports (SR), Segmentation objects (SEG), and Presentation States (PR).

```cpp
#include <pacs/ai/ai_result_handler.hpp>

namespace pacs::ai {

// Result types
template <typename T>
using Result = kcenon::common::Result<T>;
using VoidResult = kcenon::common::VoidResult;

// AI result type enumeration
enum class ai_result_type {
    structured_report,    // DICOM SR (Structured Report)
    segmentation,         // DICOM SEG (Segmentation)
    presentation_state    // DICOM PR (Presentation State)
};

// Validation status enumeration
enum class validation_status {
    valid,                    // All validations passed
    missing_required_tags,    // Required DICOM tags are missing
    invalid_reference,        // Referenced source images not found
    invalid_template,         // SR template conformance failed
    invalid_segment_data,     // Segmentation data is malformed
    unknown_error             // Unexpected validation error
};

// AI result information structure
struct ai_result_info {
    std::string sop_instance_uid;           // SOP Instance UID of the AI result
    ai_result_type type;                    // Type of AI result
    std::string sop_class_uid;              // SOP Class UID
    std::string series_instance_uid;        // Series Instance UID
    std::string source_study_uid;           // Study Instance UID of source study
    std::string algorithm_name;             // AI model/algorithm identifier
    std::string algorithm_version;          // Algorithm version
    std::chrono::system_clock::time_point received_at;  // Reception timestamp
    std::optional<std::string> description; // Optional description
};

// Source reference structure
struct source_reference {
    std::string study_instance_uid;
    std::optional<std::string> series_instance_uid;
    std::vector<std::string> sop_instance_uids;
};

// CAD finding from Structured Report
struct cad_finding {
    std::string finding_type;               // Finding type/category
    std::string location;                   // Location/site description
    std::optional<double> confidence;       // Confidence score (0.0 to 1.0)
    std::optional<std::string> measurement; // Additional measurement data
    std::optional<std::string> referenced_sop_instance_uid;
};

// Segment information from Segmentation
struct segment_info {
    uint16_t segment_number;                // Segment number (1-based)
    std::string segment_label;              // Segment label
    std::optional<std::string> description; // Segment description
    std::string algorithm_type;             // Algorithm type
    std::optional<std::tuple<uint8_t, uint8_t, uint8_t>> recommended_display_color;
};

// Validation result
struct validation_result {
    validation_status status;
    std::optional<std::string> error_message;
    std::vector<std::string> missing_tags;
    std::vector<std::string> invalid_references;
};

// Handler configuration
struct ai_handler_config {
    bool validate_source_references = true;
    bool validate_sr_templates = true;
    bool auto_link_to_source = true;
    std::vector<std::string> accepted_sr_templates;
    uint16_t max_segments = 256;
};

// Callback types
using ai_result_received_callback = std::function<void(const ai_result_info& info)>;
using pre_store_validator = std::function<bool(
    const core::dicom_dataset& dataset,
    ai_result_type type)>;

// Main handler class
class ai_result_handler {
public:
    // Factory method
    [[nodiscard]] static auto create(
        std::shared_ptr<storage::storage_interface> storage,
        std::shared_ptr<storage::index_database> database)
        -> std::unique_ptr<ai_result_handler>;

    virtual ~ai_result_handler();

    // Configuration
    void configure(const ai_handler_config& config);
    [[nodiscard]] auto get_config() const -> ai_handler_config;
    void set_received_callback(ai_result_received_callback callback);
    void set_pre_store_validator(pre_store_validator validator);

    // Structured Report operations
    [[nodiscard]] auto receive_structured_report(const core::dicom_dataset& sr)
        -> VoidResult;
    [[nodiscard]] auto validate_sr_template(const core::dicom_dataset& sr)
        -> validation_result;
    [[nodiscard]] auto get_cad_findings(std::string_view sr_sop_instance_uid)
        -> Result<std::vector<cad_finding>>;

    // Segmentation operations
    [[nodiscard]] auto receive_segmentation(const core::dicom_dataset& seg)
        -> VoidResult;
    [[nodiscard]] auto validate_segmentation(const core::dicom_dataset& seg)
        -> validation_result;
    [[nodiscard]] auto get_segment_info(std::string_view seg_sop_instance_uid)
        -> Result<std::vector<segment_info>>;

    // Presentation State operations
    [[nodiscard]] auto receive_presentation_state(const core::dicom_dataset& pr)
        -> VoidResult;
    [[nodiscard]] auto validate_presentation_state(const core::dicom_dataset& pr)
        -> validation_result;

    // Source linking operations
    [[nodiscard]] auto link_to_source(
        std::string_view result_uid,
        std::string_view source_study_uid) -> VoidResult;
    [[nodiscard]] auto link_to_source(
        std::string_view result_uid,
        const source_reference& references) -> VoidResult;
    [[nodiscard]] auto get_source_reference(std::string_view result_uid)
        -> Result<source_reference>;

    // Query operations
    [[nodiscard]] auto find_ai_results_for_study(std::string_view study_instance_uid)
        -> Result<std::vector<ai_result_info>>;
    [[nodiscard]] auto find_ai_results_by_type(
        std::string_view study_instance_uid,
        ai_result_type type) -> Result<std::vector<ai_result_info>>;
    [[nodiscard]] auto get_ai_result_info(std::string_view sop_instance_uid)
        -> std::optional<ai_result_info>;
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const -> bool;

    // Removal operations
    [[nodiscard]] auto remove(std::string_view sop_instance_uid) -> VoidResult;
    [[nodiscard]] auto remove_ai_results_for_study(std::string_view study_instance_uid)
        -> Result<std::size_t>;

protected:
    ai_result_handler(
        std::shared_ptr<storage::storage_interface> storage,
        std::shared_ptr<storage::index_database> database);

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace pacs::ai
```

#### Usage Example

```cpp
#include <pacs/ai/ai_result_handler.hpp>
#include <pacs/storage/file_storage.hpp>
#include <pacs/storage/index_database.hpp>

// Create handler with storage and database
auto storage = std::make_shared<pacs::storage::file_storage>("/data/pacs");
auto database = std::make_shared<pacs::storage::index_database>("/data/pacs/index.db");
auto handler = pacs::ai::ai_result_handler::create(storage, database);

// Configure validation options
pacs::ai::ai_handler_config config;
config.validate_source_references = true;
config.auto_link_to_source = true;
handler->configure(config);

// Set notification callback
handler->set_received_callback([](const pacs::ai::ai_result_info& info) {
    std::cout << "Received AI result: " << info.sop_instance_uid
              << " (" << info.algorithm_name << ")\n";
});

// Receive AI-generated structured report
pacs::core::dicom_dataset sr_dataset;
// ... populate SR dataset with CAD findings ...
auto result = handler->receive_structured_report(sr_dataset);
if (result.is_ok()) {
    // Successfully stored
    auto findings = handler->get_cad_findings(sr_dataset.get_string(
        pacs::core::tags::sop_instance_uid));
    if (findings.is_ok()) {
        for (const auto& f : findings.value()) {
            std::cout << "Finding: " << f.finding_type
                      << " at " << f.location << "\n";
        }
    }
}

// Query AI results for a study
auto ai_results = handler->find_ai_results_for_study("1.2.3.4.5.6.7.8.9");
if (ai_results.is_ok()) {
    for (const auto& info : ai_results.value()) {
        std::cout << "AI Result: " << info.sop_instance_uid
                  << " Type: " << static_cast<int>(info.type) << "\n";
    }
}
```

---

### `pacs::ai::ai_service_connector`

Connector for external AI inference services with support for async job submission, status tracking, and result retrieval.

```cpp
#include <pacs/ai/ai_service_connector.hpp>

namespace pacs::ai {

// Result type alias
template <typename T>
using Result = kcenon::common::Result<T>;

// Authentication types for AI service connection
enum class authentication_type {
    none,           // No authentication
    api_key,        // API key in header
    bearer_token,   // Bearer token (JWT)
    basic           // HTTP Basic authentication
};

// Inference job status codes
enum class inference_status_code {
    pending,    // Job is queued
    running,    // Job is being processed
    completed,  // Job completed successfully
    failed,     // Job failed
    cancelled,  // Job was cancelled
    timeout     // Job timed out
};

// AI service configuration
struct ai_service_config {
    std::string base_url;                           // AI service endpoint
    std::string service_name{"ai_service"};         // Service identifier for logging
    authentication_type auth_type{authentication_type::none};
    std::string api_key;                            // For api_key auth
    std::string bearer_token;                       // For bearer_token auth
    std::string username;                           // For basic auth
    std::string password;                           // For basic auth
    std::chrono::seconds connection_timeout{30};    // Connection timeout
    std::chrono::seconds request_timeout{300};      // Request timeout
    std::size_t max_retries{3};                     // Max retry attempts
    std::chrono::seconds retry_delay{5};            // Delay between retries
    bool enable_metrics{true};                      // Enable performance metrics
    bool enable_tracing{true};                      // Enable distributed tracing
};

// Inference request structure
struct inference_request {
    std::string study_instance_uid;                 // Required: Study UID
    std::optional<std::string> series_instance_uid; // Optional: Series UID
    std::string model_id;                           // Required: AI model identifier
    std::map<std::string, std::string> parameters;  // Model-specific parameters
    int priority{0};                                // Job priority (higher = more urgent)
    std::optional<std::string> callback_url;        // Webhook for completion
    std::map<std::string, std::string> metadata;    // Custom metadata
};

// Inference status structure
struct inference_status {
    std::string job_id;                             // Unique job identifier
    inference_status_code status;                   // Current status
    int progress{0};                                // Progress 0-100
    std::optional<std::string> message;             // Status message
    std::optional<std::string> result_study_uid;    // Result study UID if completed
    std::optional<std::string> error_code;          // Error code if failed
    std::chrono::system_clock::time_point submitted_at;
    std::optional<std::chrono::system_clock::time_point> started_at;
    std::optional<std::chrono::system_clock::time_point> completed_at;
};

// AI model information
struct model_info {
    std::string model_id;                           // Model identifier
    std::string name;                               // Display name
    std::string version;                            // Model version
    std::string description;                        // Model description
    std::vector<std::string> supported_modalities;  // e.g., ["CT", "MR"]
    std::vector<std::string> output_types;          // e.g., ["SR", "SEG"]
    bool available{true};                           // Model availability
};

// Main connector class (Singleton pattern with static methods)
class ai_service_connector {
public:
    // Initialization and lifecycle
    [[nodiscard]] static auto initialize(const ai_service_config& config)
        -> Result<std::monostate>;
    static void shutdown();
    [[nodiscard]] static auto is_initialized() noexcept -> bool;

    // Inference operations
    [[nodiscard]] static auto request_inference(const inference_request& request)
        -> Result<std::string>;  // Returns job_id
    [[nodiscard]] static auto check_status(const std::string& job_id)
        -> Result<inference_status>;
    [[nodiscard]] static auto cancel(const std::string& job_id)
        -> Result<std::monostate>;
    [[nodiscard]] static auto list_active_jobs()
        -> Result<std::vector<inference_status>>;

    // Model discovery
    [[nodiscard]] static auto list_models()
        -> Result<std::vector<model_info>>;
    [[nodiscard]] static auto get_model_info(const std::string& model_id)
        -> Result<model_info>;

    // Health and diagnostics
    [[nodiscard]] static auto check_health() -> bool;
    [[nodiscard]] static auto get_latency()
        -> std::optional<std::chrono::milliseconds>;

    // Configuration
    [[nodiscard]] static auto update_credentials(
        authentication_type auth_type,
        const std::string& credential) -> Result<std::monostate>;
    [[nodiscard]] static auto get_config() -> const ai_service_config&;

private:
    class impl;
    static std::unique_ptr<impl> pimpl_;
};

// Helper functions
[[nodiscard]] auto to_string(inference_status_code status) -> std::string;
[[nodiscard]] auto to_string(authentication_type auth) -> std::string;

} // namespace pacs::ai
```

**Design Decisions:**

| Decision | Rationale |
|----------|-----------|
| Static singleton pattern | Global access, consistent with other adapters |
| PIMPL pattern | ABI stability, hidden implementation details |
| Result<T> return type | Consistent error handling with common_system |
| Async job model | Long-running inference tasks with status tracking |
| Metrics integration | Performance monitoring via monitoring_adapter |

**Thread Safety:**

All public methods are thread-safe. Internal synchronization uses:
- `std::mutex` for job tracking state
- `std::shared_mutex` for read-heavy operations
- `std::atomic<bool>` for initialization flag

**Usage Example:**

```cpp
#include <pacs/ai/ai_service_connector.hpp>
#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/monitoring_adapter.hpp>

using namespace pacs::ai;

// Initialize dependencies first
pacs::integration::logger_adapter::initialize({});
pacs::integration::monitoring_adapter::initialize({});

// Configure AI service
ai_service_config config;
config.base_url = "https://ai-service.example.com/api/v1";
config.service_name = "radiology_ai";
config.auth_type = authentication_type::api_key;
config.api_key = "your-api-key-here";
config.request_timeout = std::chrono::minutes(5);

// Initialize connector
auto init_result = ai_service_connector::initialize(config);
if (init_result.is_err()) {
    std::cerr << "Failed to initialize: " << init_result.error().message << "\n";
    return 1;
}

// Submit inference request
inference_request request;
request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
request.model_id = "chest-xray-nodule-detector-v2";
request.parameters["threshold"] = "0.7";
request.parameters["output_format"] = "SR";
request.priority = 5;

auto submit_result = ai_service_connector::request_inference(request);
if (submit_result.is_err()) {
    std::cerr << "Submission failed: " << submit_result.error().message << "\n";
    return 1;
}

std::string job_id = submit_result.value();
std::cout << "Job submitted: " << job_id << "\n";

// Poll for completion
while (true) {
    auto status_result = ai_service_connector::check_status(job_id);
    if (status_result.is_err()) {
        std::cerr << "Status check failed\n";
        break;
    }

    auto& status = status_result.value();
    std::cout << "Progress: " << status.progress << "%\n";

    if (status.status == inference_status_code::completed) {
        std::cout << "Result available: " << *status.result_study_uid << "\n";
        break;
    } else if (status.status == inference_status_code::failed) {
        std::cerr << "Job failed: " << *status.error_code << "\n";
        break;
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
}

// Cleanup
ai_service_connector::shutdown();
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

## Web Module

### `pacs::web::rest_server`

REST API server implementation using Crow framework.

```cpp
#include <pacs/web/rest_server.hpp>

namespace pacs::web {

class rest_server {
public:
    // Construction
    explicit rest_server(const rest_server_config& config);
    ~rest_server();

    // Lifecycle
    void start();        // Blocking
    void start_async();  // Non-blocking
    void stop();         // Graceful shutdown

    // Configuration
    void set_health_checker(std::shared_ptr<monitoring::health_checker> checker);

    // Status
    bool is_running() const;
    uint16_t port() const;
};

} // namespace pacs::web
```

### `pacs::web::rest_server_config`

Configuration for the REST server.

```cpp
#include <pacs/web/rest_server_config.hpp>

namespace pacs::web {

struct rest_server_config {
    uint16_t port = 8080;
    size_t concurrency = 2;
    bool enable_cors = true;
    std::string bind_address = "0.0.0.0";
    
    // TLS options (future)
    bool use_tls = false;
    std::string cert_file;
    std::string key_file;
};

} // namespace pacs::web
```

### DICOMweb (WADO-RS) API

The DICOMweb module implements the WADO-RS (Web Access to DICOM Objects - RESTful) specification
as defined in DICOM PS3.18 for retrieving DICOM objects over HTTP.

#### `pacs::web::dicomweb::multipart_builder`

Builds multipart/related MIME responses for returning multiple DICOM objects.

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

class multipart_builder {
public:
    // Construction (default content type: application/dicom)
    explicit multipart_builder(std::string_view content_type = media_type::dicom);

    // Add parts
    void add_part(std::vector<uint8_t> data,
                  std::optional<std::string_view> content_type = std::nullopt);
    void add_part_with_location(std::vector<uint8_t> data,
                                std::string_view location,
                                std::optional<std::string_view> content_type = std::nullopt);

    // Build response
    [[nodiscard]] auto build() const -> std::string;
    [[nodiscard]] auto content_type_header() const -> std::string;
    [[nodiscard]] auto boundary() const -> std::string_view;

    // Status
    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto size() const noexcept -> size_t;
};

} // namespace pacs::web::dicomweb
```

#### Utility Functions

```cpp
namespace pacs::web::dicomweb {

// Parse Accept header into structured format, sorted by quality
[[nodiscard]] auto parse_accept_header(std::string_view accept_header)
    -> std::vector<accept_info>;

// Check if a media type is acceptable based on parsed Accept header
[[nodiscard]] auto is_acceptable(const std::vector<accept_info>& accept_infos,
                                  std::string_view media_type) -> bool;

// Convert DICOM dataset to DicomJSON format
[[nodiscard]] auto dataset_to_dicom_json(
    const core::dicom_dataset& dataset,
    bool include_bulk_data = false,
    std::string_view bulk_data_uri_prefix = "") -> std::string;

// Check if a DICOM tag contains bulk data (Pixel Data, etc.)
[[nodiscard]] auto is_bulk_data_tag(uint32_t tag) -> bool;

} // namespace pacs::web::dicomweb
```

#### Media Type Constants

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

#### REST API Endpoints (WADO-RS)

**Base Path**: `/dicomweb`

##### Retrieve Study
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>`
*   **Description**: Retrieve all DICOM instances of a study.
*   **Accept Headers**:
    *   `application/dicom` (default)
    *   `multipart/related; type="application/dicom"`
    *   `*/*`
*   **Responses**:
    *   `200 OK`: Multipart response with DICOM instances.
    *   `404 Not Found`: Study not found.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Study Metadata
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/metadata`
*   **Description**: Retrieve metadata for all instances in a study.
*   **Accept Headers**:
    *   `application/dicom+json` (default)
*   **Response Body**:
    ```json
    [
      {
        "00080018": { "vr": "UI", "Value": ["1.2.840..."] },
        "00100010": { "vr": "PN", "Value": [{ "Alphabetic": "DOE^JOHN" }] }
      }
    ]
    ```
*   **Responses**:
    *   `200 OK`: DicomJSON array of instance metadata.
    *   `404 Not Found`: Study not found.

##### Retrieve Series
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>`
*   **Description**: Retrieve all DICOM instances of a series.
*   **Accept Headers**: Same as Retrieve Study.
*   **Responses**:
    *   `200 OK`: Multipart response with DICOM instances.
    *   `404 Not Found`: Series not found.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Series Metadata
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/metadata`
*   **Description**: Retrieve metadata for all instances in a series.
*   **Accept Headers**: `application/dicom+json` (default)
*   **Responses**:
    *   `200 OK`: DicomJSON array of instance metadata.
    *   `404 Not Found`: Series not found.

##### Retrieve Instance
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>`
*   **Description**: Retrieve a single DICOM instance.
*   **Accept Headers**:
    *   `application/dicom` (default)
    *   `multipart/related; type="application/dicom"`
    *   `*/*`
*   **Responses**:
    *   `200 OK`: Multipart response with single DICOM instance.
    *   `404 Not Found`: Instance not found.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Instance Metadata
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/metadata`
*   **Description**: Retrieve metadata for a single instance.
*   **Accept Headers**: `application/dicom+json` (default)
*   **Response Body**:
    ```json
    [
      {
        "00080016": { "vr": "UI", "Value": ["1.2.840.10008.5.1.4.1.1.2"] },
        "00080018": { "vr": "UI", "Value": ["1.2.840...instance"] },
        "00100010": { "vr": "PN", "Value": [{ "Alphabetic": "DOE^JOHN" }] },
        "7FE00010": { "vr": "OW", "BulkDataURI": "/dicomweb/studies/.../bulkdata/..." }
      }
    ]
    ```
*   **Responses**:
    *   `200 OK`: DicomJSON array with single instance metadata.
    *   `404 Not Found`: Instance not found.

##### Retrieve Frame
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/frames/<frameList>`
*   **Description**: Retrieve specific frame(s) from a multi-frame DICOM instance.
*   **Path Parameters**:
    *   `frameList`: Comma-separated frame numbers or ranges (1-based). Examples:
        *   `1` - Single frame
        *   `1,3,5` - Multiple frames
        *   `1-5` - Frame range (1, 2, 3, 4, 5)
        *   `1,3-5,7` - Mixed notation
*   **Accept Headers**:
    *   `application/octet-stream` (default) - Raw pixel data
    *   `multipart/related; type="application/octet-stream"` - Multiple frames
*   **Responses**:
    *   `200 OK`: Multipart response with frame pixel data.
    *   `400 Bad Request`: Invalid frame list syntax.
    *   `404 Not Found`: Instance not found or frame number out of range.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Rendered Image
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/rendered`
*   **Description**: Retrieve a rendered (consumer-ready) image from a DICOM instance.
*   **Query Parameters**:
    *   `window-center` (optional): VOI LUT window center value
    *   `window-width` (optional): VOI LUT window width value
    *   `quality` (optional): JPEG quality (1-100, default 75)
    *   `viewport` (optional): Output size as `width,height`
    *   `frame` (optional): Frame number for multi-frame images (1-based, default 1)
*   **Accept Headers**:
    *   `image/jpeg` (default) - JPEG output
    *   `image/png` - PNG output
    *   `*/*` - JPEG (default)
*   **Responses**:
    *   `200 OK`: Rendered image in requested format.
    *   `404 Not Found`: Instance not found.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Frame Rendered Image
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/frames/<frameNumber>/rendered`
*   **Description**: Retrieve a rendered image of a specific frame from a multi-frame DICOM instance.
*   **Query Parameters**: Same as Retrieve Rendered Image (except `frame`).
*   **Accept Headers**: Same as Retrieve Rendered Image.
*   **Responses**:
    *   `200 OK`: Rendered image in requested format.
    *   `400 Bad Request`: Invalid frame number.
    *   `404 Not Found`: Instance not found or frame out of range.
    *   `406 Not Acceptable`: Requested media type not supported.

#### Frame Retrieval Utilities

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

/**
 * @brief Parse frame numbers from URL path
 * @param frame_list Comma-separated frame numbers (e.g., "1,3,5" or "1-5")
 * @return Vector of frame numbers (1-based), empty on parse error
 */
[[nodiscard]] auto parse_frame_numbers(std::string_view frame_list)
    -> std::vector<uint32_t>;

/**
 * @brief Extract a single frame from pixel data
 * @param pixel_data Complete pixel data buffer
 * @param frame_number Frame number to extract (1-based)
 * @param frame_size Size of each frame in bytes
 * @return Frame data, or empty vector if frame doesn't exist
 */
[[nodiscard]] auto extract_frame(
    std::span<const uint8_t> pixel_data,
    uint32_t frame_number,
    size_t frame_size) -> std::vector<uint8_t>;

} // namespace pacs::web::dicomweb
```

#### Rendered Image Utilities

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

/**
 * @brief Rendered image output format
 */
enum class rendered_format {
    jpeg,   ///< JPEG format (default)
    png     ///< PNG format
};

/**
 * @brief Parameters for rendered image requests
 */
struct rendered_params {
    rendered_format format{rendered_format::jpeg};  ///< Output format
    int quality{75};                                 ///< JPEG quality (1-100)
    std::optional<double> window_center;             ///< VOI LUT window center
    std::optional<double> window_width;              ///< VOI LUT window width
    uint16_t viewport_width{0};                      ///< Output width (0 = original)
    uint16_t viewport_height{0};                     ///< Output height (0 = original)
    uint32_t frame{1};                               ///< Frame number (1-based)
    std::optional<std::string> presentation_state_uid;  ///< Optional GSPS UID
    bool burn_annotations{false};                    ///< Burn-in annotations
};

/**
 * @brief Result of a rendering operation
 */
struct rendered_result {
    std::vector<uint8_t> data;      ///< Rendered image data
    std::string content_type;        ///< MIME type of result
    bool success{false};             ///< Whether rendering succeeded
    std::string error_message;       ///< Error message if failed

    [[nodiscard]] static rendered_result ok(std::vector<uint8_t> data,
                                            std::string content_type);
    [[nodiscard]] static rendered_result error(std::string msg);
};

/**
 * @brief Parse rendered image parameters from HTTP request
 */
[[nodiscard]] auto parse_rendered_params(
    std::string_view query_string,
    std::string_view accept_header) -> rendered_params;

/**
 * @brief Apply window/level transformation to pixel data
 */
[[nodiscard]] auto apply_window_level(
    std::span<const uint8_t> pixel_data,
    uint16_t width, uint16_t height,
    uint16_t bits_stored, bool is_signed,
    double window_center, double window_width,
    double rescale_slope, double rescale_intercept) -> std::vector<uint8_t>;

/**
 * @brief Render a DICOM image to consumer format
 */
[[nodiscard]] auto render_dicom_image(
    std::string_view file_path,
    const rendered_params& params) -> rendered_result;

} // namespace pacs::web::dicomweb
```

### STOW-RS (Store Over the Web) API

The STOW-RS module implements the Store Over the Web - RESTful specification
as defined in DICOM PS3.18 for storing DICOM objects via HTTP.

#### `pacs::web::dicomweb::multipart_parser`

Parses multipart/related request bodies for STOW-RS uploads.

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

struct multipart_part {
    std::string content_type;       // Content-Type of this part
    std::string content_location;   // Content-Location header (optional)
    std::string content_id;         // Content-ID header (optional)
    std::vector<uint8_t> data;      // Binary data of this part
};

class multipart_parser {
public:
    struct parse_error {
        std::string code;       // Error code (e.g., "INVALID_BOUNDARY")
        std::string message;    // Human-readable error message
    };

    struct parse_result {
        std::vector<multipart_part> parts;  // Parsed parts (empty on error)
        std::optional<parse_error> error;   // Error if parsing failed

        [[nodiscard]] bool success() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };

    // Parse a multipart/related request body
    [[nodiscard]] static auto parse(std::string_view content_type,
                                    std::string_view body) -> parse_result;

    // Extract boundary from Content-Type header
    [[nodiscard]] static auto extract_boundary(std::string_view content_type)
        -> std::optional<std::string>;

    // Extract type parameter from Content-Type header
    [[nodiscard]] static auto extract_type(std::string_view content_type)
        -> std::optional<std::string>;
};

} // namespace pacs::web::dicomweb
```

#### Store Response Types

```cpp
namespace pacs::web::dicomweb {

struct store_instance_result {
    bool success = false;                   // Whether storage succeeded
    std::string sop_class_uid;              // SOP Class UID of the instance
    std::string sop_instance_uid;           // SOP Instance UID of the instance
    std::string retrieve_url;               // URL to retrieve the stored instance
    std::optional<std::string> error_code;  // Error code if failed
    std::optional<std::string> error_message; // Error message if failed
};

struct store_response {
    std::vector<store_instance_result> referenced_instances;  // Successfully stored
    std::vector<store_instance_result> failed_instances;      // Failed to store

    [[nodiscard]] bool all_success() const noexcept;
    [[nodiscard]] bool all_failed() const noexcept;
    [[nodiscard]] bool partial_success() const noexcept;
};

struct validation_result {
    bool valid = true;                      // Whether validation passed
    std::string error_code;                 // Error code if invalid
    std::string error_message;              // Error message if invalid

    [[nodiscard]] explicit operator bool() const noexcept;

    static validation_result ok();
    static validation_result error(std::string code, std::string message);
};

} // namespace pacs::web::dicomweb
```

#### STOW-RS Utility Functions

```cpp
namespace pacs::web::dicomweb {

// Validate a DICOM instance for STOW-RS storage
[[nodiscard]] auto validate_instance(
    const core::dicom_dataset& dataset,
    std::optional<std::string_view> target_study_uid = std::nullopt)
    -> validation_result;

// Build STOW-RS response in DicomJSON format
[[nodiscard]] auto build_store_response_json(
    const store_response& response,
    std::string_view base_url) -> std::string;

} // namespace pacs::web::dicomweb
```

#### REST API Endpoints (STOW-RS)

**Base Path**: `/dicomweb`

##### Store Instances (Study-Independent)
*   **Method**: `POST`
*   **Path**: `/studies`
*   **Description**: Store DICOM instances without specifying a target study.
*   **Content-Type**: `multipart/related; type="application/dicom"; boundary=...`
*   **Request Body**: Multipart/related with DICOM Part 10 files.
    ```
    --boundary
    Content-Type: application/dicom

    <DICOM Part 10 binary data>
    --boundary
    Content-Type: application/dicom

    <DICOM Part 10 binary data>
    --boundary--
    ```
*   **Responses**:
    *   `200 OK`: All instances stored successfully.
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
    *   `202 Accepted`: Some instances failed (partial success).
    *   `409 Conflict`: All instances failed.
    *   `415 Unsupported Media Type`: Invalid Content-Type.

##### Store Instances (Study-Targeted)
*   **Method**: `POST`
*   **Path**: `/studies/<studyUID>`
*   **Description**: Store DICOM instances to a specific study.
*   **Content-Type**: `multipart/related; type="application/dicom"; boundary=...`
*   **Request Body**: Same as Study-Independent store.
*   **Validation**: All instances must have Study Instance UID matching `<studyUID>`.
*   **Responses**:
    *   `200 OK`: All instances stored successfully.
    *   `202 Accepted`: Some instances failed (partial success).
    *   `409 Conflict`: All instances failed or Study UID mismatch.
    *   `415 Unsupported Media Type`: Invalid Content-Type.

### QIDO-RS (Query based on ID for DICOM Objects) API

The QIDO-RS module implements the Query based on ID for DICOM Objects - RESTful specification
as defined in DICOM PS3.18 for searching DICOM objects via HTTP.

#### QIDO-RS Utility Functions

```cpp
namespace pacs::web::dicomweb {

// Convert database records to DicomJSON format for QIDO-RS responses
[[nodiscard]] auto study_record_to_dicom_json(
    const storage::study_record& record,
    std::string_view patient_id,
    std::string_view patient_name) -> std::string;

[[nodiscard]] auto series_record_to_dicom_json(
    const storage::series_record& record,
    std::string_view study_uid) -> std::string;

[[nodiscard]] auto instance_record_to_dicom_json(
    const storage::instance_record& record,
    std::string_view series_uid,
    std::string_view study_uid) -> std::string;

// Parse QIDO-RS query parameters from HTTP request URL
[[nodiscard]] auto parse_study_query_params(
    const std::string& url_params) -> storage::study_query;

[[nodiscard]] auto parse_series_query_params(
    const std::string& url_params) -> storage::series_query;

[[nodiscard]] auto parse_instance_query_params(
    const std::string& url_params) -> storage::instance_query;

} // namespace pacs::web::dicomweb
```

#### REST API Endpoints (QIDO-RS)

**Base Path**: `/dicomweb`

##### Search Studies
*   **Method**: `GET`
*   **Path**: `/studies`
*   **Description**: Search for studies matching query parameters.
*   **Query Parameters**:
    *   `PatientID` or `00100020`: Patient ID (supports `*` wildcard)
    *   `PatientName` or `00100010`: Patient name (supports `*` wildcard)
    *   `StudyInstanceUID` or `0020000D`: Study Instance UID
    *   `StudyID` or `00200010`: Study ID
    *   `StudyDate` or `00080020`: Study date (YYYYMMDD or YYYYMMDD-YYYYMMDD for range)
    *   `AccessionNumber` or `00080050`: Accession number
    *   `ModalitiesInStudy` or `00080061`: Modalities in study
    *   `ReferringPhysicianName` or `00080090`: Referring physician
    *   `StudyDescription` or `00081030`: Study description
    *   `limit`: Maximum number of results (default: 100)
    *   `offset`: Pagination offset
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching studies.
    *   `503 Service Unavailable`: Database not configured.

##### Search Series
*   **Method**: `GET`
*   **Path**: `/series`
*   **Description**: Search for all series matching query parameters.
*   **Query Parameters**:
    *   `StudyInstanceUID` or `0020000D`: Study Instance UID
    *   `SeriesInstanceUID` or `0020000E`: Series Instance UID
    *   `Modality` or `00080060`: Modality (e.g., CT, MR, US)
    *   `SeriesNumber` or `00200011`: Series number
    *   `SeriesDescription` or `0008103E`: Series description
    *   `BodyPartExamined` or `00180015`: Body part examined
    *   `limit`: Maximum number of results (default: 100)
    *   `offset`: Pagination offset
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching series.
    *   `503 Service Unavailable`: Database not configured.

##### Search Instances
*   **Method**: `GET`
*   **Path**: `/instances`
*   **Description**: Search for all instances matching query parameters.
*   **Query Parameters**:
    *   `SeriesInstanceUID` or `0020000E`: Series Instance UID
    *   `SOPInstanceUID` or `00080018`: SOP Instance UID
    *   `SOPClassUID` or `00080016`: SOP Class UID
    *   `InstanceNumber` or `00200013`: Instance number
    *   `limit`: Maximum number of results (default: 100)
    *   `offset`: Pagination offset
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching instances.
    *   `503 Service Unavailable`: Database not configured.

##### Search Series in Study
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series`
*   **Description**: Search for series within a specific study.
*   **Path Parameters**:
    *   `studyUID`: Study Instance UID
*   **Query Parameters**: Same as Search Series (except StudyInstanceUID is from path)
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching series.
    *   `404 Not Found`: Study not found.
    *   `503 Service Unavailable`: Database not configured.

##### Search Instances in Study
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/instances`
*   **Description**: Search for instances within a specific study.
*   **Path Parameters**:
    *   `studyUID`: Study Instance UID
*   **Query Parameters**: Same as Search Instances
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching instances.
    *   `404 Not Found`: Study not found.
    *   `503 Service Unavailable`: Database not configured.

##### Search Instances in Series
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances`
*   **Description**: Search for instances within a specific series.
*   **Path Parameters**:
    *   `studyUID`: Study Instance UID
    *   `seriesUID`: Series Instance UID
*   **Query Parameters**: Same as Search Instances (except SeriesInstanceUID is from path)
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching instances.
    *   `404 Not Found`: Study or series not found.
    *   `503 Service Unavailable`: Database not configured.

---

## Document History

| Version | Date       | Changes                                      |
|---------|------------|----------------------------------------------|
| 1.0.0   | 2025-11-30 | Initial API Reference document               |
| 1.1.0   | 2025-12-05 | Added thread_system integration APIs         |
| 1.2.0   | 2025-12-07 | Added Network V2 Module (dicom_server_v2, dicom_association_handler) |
| 1.3.0   | 2025-12-07 | Added Monitoring Module (pacs_metrics for DIMSE operation tracking) |
| 1.4.0   | 2025-12-07 | Added DX Modality Module (dx_storage, dx_iod_validator) |
| 1.5.0   | 2025-12-08 | Added MG Modality Module (mg_storage, mg_iod_validator) |
| 1.6.0   | 2025-12-09 | Added Web Module (rest_server foundation)    |
| 1.7.0   | 2025-12-11 | Added Patient, Study, Series REST API endpoints |
| 1.8.0   | 2025-12-12 | Added DICOMweb (WADO-RS) API endpoints and utilities |
| 1.9.0   | 2025-12-13 | Added STOW-RS (Store Over the Web) API endpoints and multipart parser |
| 1.10.0  | 2025-12-13 | Added QIDO-RS (Query based on ID for DICOM Objects) API endpoints |
| 1.11.0  | 2025-12-13 | Added WADO-RS Frame retrieval and Rendered image endpoints |


---

## Security Module

### `pacs::security::access_control_manager`

Manages Users, Roles, and Permission enforcement.

```cpp
#include <pacs/security/access_control_manager.hpp>

namespace pacs::security {

class access_control_manager {
public:
    // Setup
    void set_storage(std::shared_ptr<security_storage_interface> storage);

    // User Management
    VoidResult create_user(const User& user);
    Result<User> get_user(std::string_view id);
    VoidResult assign_role(std::string_view user_id, Role role);

    // Permission Check
    bool check_permission(const User& user, ResourceType resource, uint32_t action_mask) const;
    bool has_role(const User& user, Role role) const;
};

} // namespace pacs::security
```

### REST API Endpoints

**Base Path**: `/api/v1/security`

#### Create User
*   **Method**: `POST`
*   **Path**: `/users`
*   **Body**:
    ```json
    {
      "id": "u123",
      "username": "jdoe"
    }
    ```
*   **Responses**:
    *   `201 Created`: User created.
    *   `400 Bad Request`: Invalid input or user exists.
    *   `401 Unauthorized`: Missing or invalid authentication.

#### Assign Role
*   **Method**: `POST`
*   **Path**: `/users/<id>/roles`
*   **Body**:
    ```json
    {
      "role": "Radiologist"
    }
    ```
*   **Role Values**: `Viewer`, `Technologist`, `Radiologist`, `Administrator`.
*   **Responses**:
    *   `200 OK`: Role assigned.
    *   `400 Bad Request`: Invalid role.
    *   `404 Not Found`: User not found.
    *   `401 Unauthorized`: Missing or invalid authentication.

### Patient REST API Endpoints

**Base Path**: `/api/v1/patients`

#### List Patients
*   **Method**: `GET`
*   **Path**: `/`
*   **Query Parameters**:
    *   `patient_id`: Filter by patient ID (supports `*` wildcard)
    *   `patient_name`: Filter by patient name (supports `*` wildcard)
    *   `birth_date`: Filter by birth date (YYYYMMDD)
    *   `birth_date_from`: Birth date range start
    *   `birth_date_to`: Birth date range end
    *   `sex`: Filter by sex (M, F, O)
    *   `limit`: Maximum results (default: 20, max: 100)
    *   `offset`: Pagination offset
*   **Responses**:
    *   `200 OK`: Returns paginated patient list with total count
    *   `503 Service Unavailable`: Database not configured

#### Get Patient Details
*   **Method**: `GET`
*   **Path**: `/<patient_id>`
*   **Responses**:
    *   `200 OK`: Returns patient details
    *   `404 Not Found`: Patient not found
    *   `503 Service Unavailable`: Database not configured

#### Get Patient's Studies
*   **Method**: `GET`
*   **Path**: `/<patient_id>/studies`
*   **Responses**:
    *   `200 OK`: Returns list of studies for the patient
    *   `404 Not Found`: Patient not found
    *   `503 Service Unavailable`: Database not configured

### Study REST API Endpoints

**Base Path**: `/api/v1/studies`

#### List Studies
*   **Method**: `GET`
*   **Path**: `/`
*   **Query Parameters**:
    *   `patient_id`: Filter by patient ID
    *   `patient_name`: Filter by patient name (supports `*` wildcard)
    *   `study_uid`: Filter by Study Instance UID
    *   `study_id`: Filter by Study ID
    *   `study_date`: Filter by study date (YYYYMMDD)
    *   `study_date_from`: Study date range start
    *   `study_date_to`: Study date range end
    *   `accession_number`: Filter by accession number
    *   `modality`: Filter by modality (CT, MR, etc.)
    *   `referring_physician`: Filter by referring physician
    *   `study_description`: Filter by study description
    *   `limit`: Maximum results (default: 20, max: 100)
    *   `offset`: Pagination offset
*   **Responses**:
    *   `200 OK`: Returns paginated study list with total count
    *   `503 Service Unavailable`: Database not configured

#### Get Study Details
*   **Method**: `GET`
*   **Path**: `/<study_uid>`
*   **Responses**:
    *   `200 OK`: Returns study details
    *   `404 Not Found`: Study not found
    *   `503 Service Unavailable`: Database not configured

#### Get Study's Series
*   **Method**: `GET`
*   **Path**: `/<study_uid>/series`
*   **Responses**:
    *   `200 OK`: Returns list of series for the study
    *   `404 Not Found`: Study not found
    *   `503 Service Unavailable`: Database not configured

#### Get Study's Instances
*   **Method**: `GET`
*   **Path**: `/<study_uid>/instances`
*   **Responses**:
    *   `200 OK`: Returns list of all instances in the study
    *   `404 Not Found`: Study not found
    *   `503 Service Unavailable`: Database not configured

#### Delete Study
*   **Method**: `DELETE`
*   **Path**: `/<study_uid>`
*   **Responses**:
    *   `200 OK`: Study deleted successfully
    *   `404 Not Found`: Study not found
    *   `500 Internal Server Error`: Delete operation failed
    *   `503 Service Unavailable`: Database not configured

### Series REST API Endpoints

**Base Path**: `/api/v1/series`

#### Get Series Details
*   **Method**: `GET`
*   **Path**: `/<series_uid>`
*   **Responses**:
    *   `200 OK`: Returns series details
    *   `404 Not Found`: Series not found
    *   `503 Service Unavailable`: Database not configured

#### Get Series Instances
*   **Method**: `GET`
*   **Path**: `/<series_uid>/instances`
*   **Responses**:
    *   `200 OK`: Returns list of instances in the series
    *   `404 Not Found`: Series not found
    *   `503 Service Unavailable`: Database not configured

### Worklist REST API Endpoints

**Base Path**: `/api/v1/worklist`

#### List Worklist Items
*   **Method**: `GET`
*   **Path**: `/`
*   **Query Parameters**:
    *   `limit` (optional): Maximum number of results (default: 20, max: 100)
    *   `offset` (optional): Offset for pagination
    *   `station_ae` (optional): Filter by station AE title
    *   `modality` (optional): Filter by modality
    *   `scheduled_date_from` (optional): Start of date range
    *   `scheduled_date_to` (optional): End of date range
    *   `patient_id` (optional): Filter by patient ID
    *   `patient_name` (optional): Filter by patient name (supports wildcards)
    *   `accession_no` (optional): Filter by accession number
    *   `step_id` (optional): Filter by step ID
    *   `include_all_status` (optional): Include all statuses if "true"
*   **Responses**:
    *   `200 OK`: Returns paginated list of worklist items
    *   `503 Service Unavailable`: Database not configured

#### Create Worklist Item
*   **Method**: `POST`
*   **Path**: `/`
*   **Request Body**: JSON object with worklist item fields
*   **Required Fields**: `step_id`, `patient_id`, `modality`, `scheduled_datetime`
*   **Responses**:
    *   `201 Created`: Returns created worklist item
    *   `400 Bad Request`: Missing required fields
    *   `500 Internal Server Error`: Create operation failed
    *   `503 Service Unavailable`: Database not configured

#### Get Worklist Item
*   **Method**: `GET`
*   **Path**: `/<id>`
*   **Responses**:
    *   `200 OK`: Returns worklist item details
    *   `404 Not Found`: Worklist item not found
    *   `503 Service Unavailable`: Database not configured

#### Update Worklist Item
*   **Method**: `PUT`
*   **Path**: `/<id>`
*   **Request Body**: JSON object with `step_status` field
*   **Valid Statuses**: `SCHEDULED`, `STARTED`, `COMPLETED`
*   **Responses**:
    *   `200 OK`: Returns updated worklist item
    *   `400 Bad Request`: Invalid status value
    *   `404 Not Found`: Worklist item not found
    *   `500 Internal Server Error`: Update operation failed
    *   `503 Service Unavailable`: Database not configured

#### Delete Worklist Item
*   **Method**: `DELETE`
*   **Path**: `/<id>`
*   **Responses**:
    *   `200 OK`: Worklist item deleted successfully
    *   `404 Not Found`: Worklist item not found
    *   `500 Internal Server Error`: Delete operation failed
    *   `503 Service Unavailable`: Database not configured

### Audit Log REST API Endpoints

**Base Path**: `/api/v1/audit`

#### List Audit Logs
*   **Method**: `GET`
*   **Path**: `/logs`
*   **Query Parameters**:
    *   `limit` (optional): Maximum number of results (default: 20, max: 100)
    *   `offset` (optional): Offset for pagination
    *   `event_type` (optional): Filter by event type (e.g., C_STORE, C_FIND)
    *   `outcome` (optional): Filter by outcome (SUCCESS, FAILURE, WARNING)
    *   `user_id` (optional): Filter by user/AE title
    *   `source_ae` (optional): Filter by source AE title
    *   `patient_id` (optional): Filter by patient ID
    *   `study_uid` (optional): Filter by study UID
    *   `date_from` (optional): Start of date range
    *   `date_to` (optional): End of date range
    *   `format` (optional): Export format ("csv" for CSV, default JSON)
*   **Responses**:
    *   `200 OK`: Returns paginated list of audit log entries
    *   `503 Service Unavailable`: Database not configured

#### Get Audit Log Entry
*   **Method**: `GET`
*   **Path**: `/logs/<id>`
*   **Responses**:
    *   `200 OK`: Returns audit log entry details
    *   `404 Not Found`: Audit log entry not found
    *   `503 Service Unavailable`: Database not configured

#### Export Audit Logs
*   **Method**: `GET`
*   **Path**: `/export`
*   **Query Parameters**: Same as List Audit Logs (without pagination)
    *   `format` (optional): Export format ("csv" or "json", default: "json")
*   **Responses**:
    *   `200 OK`: Returns audit logs as downloadable file
    *   `503 Service Unavailable`: Database not configured

### Association REST API Endpoints

**Base Path**: `/api/v1/associations`

#### List Active Associations
*   **Method**: `GET`
*   **Path**: `/active`
*   **Responses**:
    *   `200 OK`: Returns list of active DICOM associations
*   **Note**: Requires integration with DICOM server for real-time data

#### Get Association Details
*   **Method**: `GET`
*   **Path**: `/<id>`
*   **Responses**:
    *   `200 OK`: Returns association details
    *   `404 Not Found`: Association not found
*   **Note**: Requires integration with DICOM server for real-time data

#### Terminate Association
*   **Method**: `DELETE`
*   **Path**: `/<id>`
*   **Responses**:
    *   `200 OK`: Association terminated successfully
    *   `400 Bad Request`: Invalid association ID
    *   `501 Not Implemented`: Requires DICOM server integration
*   **Note**: Requires integration with DICOM server

---

*Document Version: 0.1.8.0*
*Created: 2025-11-30*
*Last Updated: 2025-12-11*
*Author: kcenon@naver.com*
