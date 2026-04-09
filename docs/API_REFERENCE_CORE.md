---
doc_id: "PAC-API-002-CORE"
doc_title: "API Reference - Core & Encoding Modules"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "API"
---

# API Reference - Core & Encoding Modules

> **Part of**: [API Reference Index](API_REFERENCE.md)
> **Covers**: Core classes, DICOM tags, dataset, file I/O, VR types, transfer syntaxes, compression codecs

> **Version:** 0.1.5.0
> **Last Updated:** 2025-12-13
> **Language:** **English**

---

## Table of Contents

- [Core Module](#core-module)
- [Encoding Module](#encoding-module)

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

    // Value access (returns Result<T> for exception-free error handling)
    pacs::Result<std::string> as_string() const;
    pacs::Result<std::vector<std::string>> as_string_list() const;  // Multi-valued

    template<typename T>
    pacs::Result<T> as_numeric() const;

    template<typename T>
    pacs::Result<std::vector<T>> as_numeric_list() const;  // Multi-valued

    std::vector<uint8_t> as_bytes() const;

    // For SQ (Sequence)
    bool is_sequence() const noexcept;
    std::size_t sequence_item_count() const noexcept;
    const dicom_dataset& sequence_item(std::size_t index) const;
    dicom_dataset& sequence_item(std::size_t index);
    std::vector<dicom_dataset>& sequence_items();
    const std::vector<dicom_dataset>& sequence_items() const;
    void add_sequence_item(dicom_dataset item);

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

// Access values (Result<T> for exception-free error handling)
std::cout << patient_name.as_string().unwrap_or("") << std::endl;  // "Doe^John"
std::cout << rows.as_numeric<uint16_t>().unwrap_or(0) << std::endl;  // 512

// Or check for errors
if (auto result = patient_name.as_string(); result.is_ok()) {
    std::cout << result.value() << std::endl;
} else {
    std::cerr << "Error: " << result.error().message() << std::endl;
}
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

    // Sequence access
    bool has_sequence(dicom_tag tag) const noexcept;
    const std::vector<dicom_dataset>* get_sequence(dicom_tag tag) const noexcept;
    std::vector<dicom_dataset>* get_sequence(dicom_tag tag) noexcept;
    std::vector<dicom_dataset>& get_or_create_sequence(dicom_tag tag);

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
              << element.as_string().unwrap_or("") << std::endl;
}

// Sequence access (e.g., MPPS Performed Series Sequence)
dicom_tag performed_series_seq{0x0040, 0x0340};

// Create and populate a sequence
auto& series_items = dataset.get_or_create_sequence(performed_series_seq);
dicom_dataset series_item;
series_item.set_string(tags::SeriesInstanceUID, "1.2.3.4.5");
series_item.set_string(tags::SeriesDescription, "CT Chest");
series_items.push_back(std::move(series_item));

// Access sequence items
if (const auto* items = dataset.get_sequence(performed_series_seq)) {
    for (const auto& item : *items) {
        std::cout << "Series UID: " << item.get_string(tags::SeriesInstanceUID) << std::endl;
    }
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

// Successful compression/decompression result
struct compression_result {
    std::vector<uint8_t> data;    // Processed pixel data
    image_params output_params;    // Output image parameters
};

// Result type alias using pacs::Result<T> pattern
using codec_result = pacs::Result<compression_result>;

// Usage example:
// auto result = codec.encode(pixels, params, options);
// if (result.is_err()) {
//     std::cerr << pacs::get_error(result).message << std::endl;
// } else {
//     auto& data = pacs::get_value(result).data;
// }

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
