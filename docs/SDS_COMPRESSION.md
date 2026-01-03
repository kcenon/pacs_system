# SDS - Compression Codecs Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-03
> **Status:** Initial

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-ENC-002 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#472](https://github.com/kcenon/pacs_system/issues/472), [#468](https://github.com/kcenon/pacs_system/issues/468) |

### Related Standards

| Standard | Description |
|----------|-------------|
| DICOM PS3.5 Section 8.2 | Native and Encapsulated Pixel Data |
| DICOM PS3.5 Annex A.4.1 | JPEG Image Compression |
| DICOM PS3.5 Annex A.4.2 | JPEG Lossless Image Compression |
| DICOM PS3.5 Annex A.4.3 | JPEG-LS Image Compression |
| DICOM PS3.5 Annex A.4.4 | JPEG 2000 Image Compression |
| DICOM PS3.5 Annex G | RLE Lossless Compression |
| ITU-T T.81 | JPEG Specification |
| ISO/IEC 14495-1 | JPEG-LS Standard |
| ISO/IEC 15444-1 | JPEG 2000 Part 1 |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Codec Interface](#2-codec-interface)
- [3. JPEG Codecs](#3-jpeg-codecs)
- [4. JPEG 2000 Codec](#4-jpeg-2000-codec)
- [5. JPEG-LS Codec](#5-jpeg-ls-codec)
- [6. RLE Lossless Codec](#6-rle-lossless-codec)
- [7. Codec Factory](#7-codec-factory)
- [8. Big Endian Codec](#8-big-endian-codec)
- [9. Class Diagrams](#9-class-diagrams)
- [10. Sequence Diagrams](#10-sequence-diagrams)
- [11. Traceability](#11-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the Compression Codecs module for the PACS System. The module provides:

- **Image Compression**: Encode uncompressed pixel data to various formats
- **Image Decompression**: Decode compressed data back to raw pixels
- **Transfer Syntax Support**: Handle DICOM Transfer Syntaxes for compressed data
- **Codec Factory**: Create appropriate codec instances based on Transfer Syntax UID

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| Big Endian Codec | 1 header, 1 source | DES-ENC-006 |
| Codec Factory | 1 header, 1 source | DES-ENC-007 |
| Codec Interface | 2 headers | DES-ENC-008 |
| JPEG Baseline Codec | 1 header, 1 source | DES-ENC-009 |
| JPEG Lossless Codec | 1 header, 1 source | DES-ENC-010 |
| JPEG-LS Codec | 1 header, 1 source | DES-ENC-011 |
| JPEG 2000 Codec | 1 header, 1 source | DES-ENC-012 |
| RLE Codec | 1 header, 1 source | DES-ENC-013 |

### 1.3 Design Identifier Convention

```
DES-ENC-<NUMBER>

Where:
- DES: Design Specification prefix
- ENC: Encoding module identifier
- NUMBER: 3-digit sequential number (006-013)
```

### 1.4 External Library Dependencies

| Codec | Library | Purpose |
|-------|---------|---------|
| JPEG Baseline/Lossless | libjpeg-turbo | SIMD-accelerated JPEG processing |
| JPEG 2000 | OpenJPEG 2.4+ | Wavelet-based compression |
| JPEG-LS | CharLS 2.0+ | Lossless/near-lossless compression |
| RLE | Pure C++ | No external dependency |

---

## 2. Codec Interface

### 2.1 DES-ENC-008: Compression Codec Base Class

**Traces to:** SRS-CODEC-001 (Image Compression), NFR-1 (Performance)

**File:** `include/pacs/encoding/compression/compression_codec.hpp`

Abstract base class for all image compression codecs using Strategy pattern.

```cpp
namespace pacs::encoding::compression {

struct compression_options {
    int quality{75};           // Quality setting (1-100 for JPEG)
    bool lossless{false};      // Enable lossless mode if supported
    bool progressive{false};   // Progressive encoding (JPEG only)
    int chroma_subsampling{2}; // 0=4:4:4, 1=4:2:2, 2=4:2:0
};

struct compression_result {
    std::vector<uint8_t> data;     // Processed pixel data
    image_params output_params;    // Output image parameters
};

using codec_result = pacs::Result<compression_result>;

class compression_codec {
public:
    virtual ~compression_codec() = default;

    // Codec Information
    [[nodiscard]] virtual std::string_view transfer_syntax_uid() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual bool is_lossy() const noexcept = 0;
    [[nodiscard]] virtual bool can_encode(const image_params& params) const noexcept = 0;
    [[nodiscard]] virtual bool can_decode(const image_params& params) const noexcept = 0;

    // Compression Operations
    [[nodiscard]] virtual codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const = 0;

    [[nodiscard]] virtual codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const = 0;

protected:
    compression_codec() = default;
    compression_codec(const compression_codec&) = default;
    compression_codec& operator=(const compression_codec&) = default;
};

} // namespace pacs::encoding::compression
```

**Design Decisions:**
- Strategy pattern allows runtime codec selection
- `codec_result` uses `pacs::Result<T>` for explicit error handling
- `std::span<const uint8_t>` for zero-copy data passing
- Thread safety: Codec instances are NOT thread-safe; use separate instances per thread

---

### 2.2 Image Parameters

**Traces to:** SRS-CODEC-001 (Image Compression)

**File:** `include/pacs/encoding/compression/image_params.hpp`

Image parameter structure mapping to DICOM Image Pixel Module (C.7.6.3).

```cpp
namespace pacs::encoding::compression {

enum class photometric_interpretation {
    monochrome1,     // Minimum pixel = white
    monochrome2,     // Minimum pixel = black
    rgb,             // RGB color model
    ycbcr_full,      // YCbCr full range
    ycbcr_full_422,  // YCbCr 4:2:2 subsampling
    palette_color,   // Palette color lookup
    unknown
};

struct image_params {
    uint16_t width{0};               // Columns (0028,0011)
    uint16_t height{0};              // Rows (0028,0010)
    uint16_t bits_allocated{0};      // (0028,0100) - 8 or 16
    uint16_t bits_stored{0};         // (0028,0101) - <= bits_allocated
    uint16_t high_bit{0};            // (0028,0102) - typically bits_stored - 1
    uint16_t samples_per_pixel{1};   // (0028,0002) - 1 for grayscale, 3 for color
    uint16_t planar_configuration{0};// (0028,0006) - 0=interleaved, 1=planar
    uint16_t pixel_representation{0};// (0028,0103) - 0=unsigned, 1=signed
    photometric_interpretation photometric{photometric_interpretation::monochrome2};
    uint32_t number_of_frames{1};

    // Helper methods
    [[nodiscard]] size_t frame_size_bytes() const noexcept;
    [[nodiscard]] bool is_grayscale() const noexcept;
    [[nodiscard]] bool is_color() const noexcept;
    [[nodiscard]] bool is_signed() const noexcept;

    // Validation per codec type
    [[nodiscard]] bool valid_for_jpeg_baseline() const noexcept;
    [[nodiscard]] bool valid_for_jpeg_lossless() const noexcept;
    [[nodiscard]] bool valid_for_jpeg2000() const noexcept;
    [[nodiscard]] bool valid_for_jpeg_ls() const noexcept;
    [[nodiscard]] bool valid_for_rle() const noexcept;
};

} // namespace pacs::encoding::compression
```

**Validation Rules per Codec:**

| Codec | Bits | Samples | Max Size | Notes |
|-------|------|---------|----------|-------|
| JPEG Baseline | 8 only | 1 or 3 | 65535x65535 | Lossy only |
| JPEG Lossless | 8, 12, 16 | 1 | 65535x65535 | Grayscale only |
| JPEG 2000 | 1-16 | 1 or 3 | 2^32-1 | Memory limited |
| JPEG-LS | 2-16 | 1 or 3 | 65535x65535 | - |
| RLE | 8, 16 | 1-3 | 65535x65535 | Max 15 segments |

---

## 3. JPEG Codecs

### 3.1 DES-ENC-009: JPEG Baseline Codec

**Traces to:** SRS-CODEC-001 (Image Compression), NFR-1 (Performance)

**File:** `include/pacs/encoding/compression/jpeg_baseline_codec.hpp`, `src/encoding/compression/jpeg_baseline_codec.cpp`

JPEG Baseline (Process 1) implementation using libjpeg-turbo.

```cpp
namespace pacs::encoding::compression {

class jpeg_baseline_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.4.50";

    jpeg_baseline_codec();
    ~jpeg_baseline_codec() override;

    // Move semantics (non-copyable due to internal state)
    jpeg_baseline_codec(jpeg_baseline_codec&&) noexcept;
    jpeg_baseline_codec& operator=(jpeg_baseline_codec&&) noexcept;

    // Codec Information
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] bool is_lossy() const noexcept override; // Always true

    [[nodiscard]] bool can_encode(const image_params& params) const noexcept override;
    [[nodiscard]] bool can_decode(const image_params& params) const noexcept override;

    // Compression Operations
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

private:
    class impl;
    std::unique_ptr<impl> impl_;  // PIMPL pattern for libjpeg-turbo
};

} // namespace pacs::encoding::compression
```

**Transfer Syntax:** `1.2.840.10008.1.2.4.50` (JPEG Baseline Process 1)

**Features:**
- 8-bit grayscale and color images
- Quality settings 1-100 (75 default)
- Chroma subsampling (4:4:4, 4:2:2, 4:2:0)
- SIMD acceleration via libjpeg-turbo

**Limitations:**
- Lossy only (no lossless mode)
- 8-bit samples only
- Max size: 65535 x 65535

---

### 3.2 DES-ENC-010: JPEG Lossless Codec

**Traces to:** SRS-CODEC-001 (Image Compression), SRS-CODEC-002 (Lossless Compression)

**File:** `include/pacs/encoding/compression/jpeg_lossless_codec.hpp`, `src/encoding/compression/jpeg_lossless_codec.cpp`

JPEG Lossless (Process 14, Selection Value 1) implementation.

```cpp
namespace pacs::encoding::compression {

class jpeg_lossless_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.4.70";
    static constexpr int kDefaultPredictor = 1;       // Ra (left neighbor)
    static constexpr int kDefaultPointTransform = 0;  // No scaling

    explicit jpeg_lossless_codec(int predictor = kDefaultPredictor,
                                  int point_transform = kDefaultPointTransform);
    ~jpeg_lossless_codec() override;

    // Move semantics
    jpeg_lossless_codec(jpeg_lossless_codec&&) noexcept;
    jpeg_lossless_codec& operator=(jpeg_lossless_codec&&) noexcept;

    // Codec Information
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] bool is_lossy() const noexcept override; // Always false

    // Configuration
    [[nodiscard]] int predictor() const noexcept;
    [[nodiscard]] int point_transform() const noexcept;

    // Compression Operations
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace pacs::encoding::compression
```

**Transfer Syntax:** `1.2.840.10008.1.2.4.70` (JPEG Lossless, First-Order Prediction)

**Predictor Selection Values:**

| Value | Formula | Description |
|-------|---------|-------------|
| 1 | Ra | Left neighbor (default) |
| 2 | Rb | Above neighbor |
| 3 | Rc | Diagonal upper-left |
| 4 | Ra + Rb - Rc | Linear combination |
| 5 | Ra + (Rb - Rc) / 2 | Modified linear |
| 6 | Rb + (Ra - Rc) / 2 | Modified linear |
| 7 | (Ra + Rb) / 2 | Average |

**Features:**
- Lossless compression (exact reconstruction)
- 8-bit, 12-bit, 16-bit grayscale images
- Huffman coding
- Configurable predictor selection

**Limitations:**
- Grayscale only (no color support for lossless in DICOM)
- Requires libjpeg-turbo 3.0+ for native lossless

---

## 4. JPEG 2000 Codec

### 4.1 DES-ENC-012: JPEG 2000 Codec

**Traces to:** SRS-CODEC-001 (Image Compression), SRS-CODEC-002 (Lossless Compression)

**File:** `include/pacs/encoding/compression/jpeg2000_codec.hpp`, `src/encoding/compression/jpeg2000_codec.cpp`

JPEG 2000 implementation using OpenJPEG library.

```cpp
namespace pacs::encoding::compression {

class jpeg2000_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.90";
    static constexpr std::string_view kTransferSyntaxUIDLossy =
        "1.2.840.10008.1.2.4.91";
    static constexpr float kDefaultCompressionRatio = 20.0f;
    static constexpr int kDefaultResolutionLevels = 6;

    explicit jpeg2000_codec(bool lossless = true,
                            float compression_ratio = kDefaultCompressionRatio,
                            int resolution_levels = kDefaultResolutionLevels);
    ~jpeg2000_codec() override;

    // Move semantics
    jpeg2000_codec(jpeg2000_codec&&) noexcept;
    jpeg2000_codec& operator=(jpeg2000_codec&&) noexcept;

    // Codec Information
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] bool is_lossy() const noexcept override;

    // Configuration
    [[nodiscard]] bool is_lossless_mode() const noexcept;
    [[nodiscard]] float compression_ratio() const noexcept;
    [[nodiscard]] int resolution_levels() const noexcept;

    // Compression Operations
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace pacs::encoding::compression
```

**Transfer Syntaxes:**
- `1.2.840.10008.1.2.4.90` - JPEG 2000 Lossless Only
- `1.2.840.10008.1.2.4.91` - JPEG 2000 (Lossy or Lossless)

**Wavelet Transforms:**

| Mode | Transform | Precision | Use Case |
|------|-----------|-----------|----------|
| Lossless | 5/3 Integer | Exact | Diagnostic quality |
| Lossy | 9/7 Float | Approx | Archival/transmission |

**Features:**
- 8-bit, 12-bit, 16-bit grayscale and color
- Progressive decoding at multiple resolutions
- Configurable compression ratio (10-50 typical for medical)
- Tile-based encoding for large images

**Performance Characteristics:**
- Encode: ~10-20 MB/s (depending on image size and settings)
- Decode: ~50-100 MB/s
- Compression ratio: 10:1 to 50:1 (lossy), 2:1 to 3:1 (lossless)

---

## 5. JPEG-LS Codec

### 5.1 DES-ENC-011: JPEG-LS Codec

**Traces to:** SRS-CODEC-001 (Image Compression), SRS-CODEC-002 (Lossless Compression)

**File:** `include/pacs/encoding/compression/jpeg_ls_codec.hpp`, `src/encoding/compression/jpeg_ls_codec.cpp`

JPEG-LS implementation using CharLS library.

```cpp
namespace pacs::encoding::compression {

class jpeg_ls_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.80";
    static constexpr std::string_view kTransferSyntaxUIDNearLossless =
        "1.2.840.10008.1.2.4.81";
    static constexpr int kAutoNearValue = -1;
    static constexpr int kLosslessNearValue = 0;
    static constexpr int kDefaultNearLosslessValue = 2;
    static constexpr int kMaxNearValue = 255;

    explicit jpeg_ls_codec(bool lossless = true, int near_value = kAutoNearValue);
    ~jpeg_ls_codec() override;

    // Move semantics
    jpeg_ls_codec(jpeg_ls_codec&&) noexcept;
    jpeg_ls_codec& operator=(jpeg_ls_codec&&) noexcept;

    // Codec Information
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] bool is_lossy() const noexcept override;

    // Configuration
    [[nodiscard]] bool is_lossless_mode() const noexcept;
    [[nodiscard]] int near_value() const noexcept;

    // Compression Operations
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace pacs::encoding::compression
```

**Transfer Syntaxes:**
- `1.2.840.10008.1.2.4.80` - JPEG-LS Lossless
- `1.2.840.10008.1.2.4.81` - JPEG-LS Lossy (Near-Lossless)

**NEAR Parameter:**

| NEAR | Mode | Max Error | Use Case |
|------|------|-----------|----------|
| 0 | Lossless | 0 | Diagnostic quality |
| 1-3 | Visually lossless | 1-3 pixels | General medical |
| 4-10 | Near-lossless | 4-10 pixels | Archival |

**Features:**
- 2-16 bit precision per component
- Grayscale and color (interleaved)
- Bounded error in near-lossless mode
- Excellent compression for medical images

**Performance Characteristics:**
- Fastest lossless codec for medical images
- Typical lossless ratio: 2:1 to 4:1
- Near-lossless with NEAR=2: 4:1 to 8:1

---

## 6. RLE Lossless Codec

### 6.1 DES-ENC-013: RLE Codec

**Traces to:** SRS-CODEC-002 (Lossless Compression)

**File:** `include/pacs/encoding/compression/rle_codec.hpp`, `src/encoding/compression/rle_codec.cpp`

DICOM RLE Lossless implementation (pure C++, no external dependencies).

```cpp
namespace pacs::encoding::compression {

class rle_codec final : public compression_codec {
public:
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.5";
    static constexpr int kMaxSegments = 15;
    static constexpr size_t kRLEHeaderSize = 64;  // 16 x 4-byte offsets

    rle_codec();
    ~rle_codec() override;

    // Move semantics
    rle_codec(rle_codec&&) noexcept;
    rle_codec& operator=(rle_codec&&) noexcept;

    // Codec Information
    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] bool is_lossy() const noexcept override; // Always false

    // Compression Operations
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace pacs::encoding::compression
```

**Transfer Syntax:** `1.2.840.10008.1.2.5` (RLE Lossless)

**RLE Segment Structure:**

```
┌────────────────────────────────────────────────────────────┐
│                    RLE Header (64 bytes)                   │
├────────────────────────────────────────────────────────────┤
│ Segment Count (4 bytes) │ Offset 1 │ Offset 2 │ ... │ Offset 15 │
├────────────────────────────────────────────────────────────┤
│                    Segment 1 Data                          │
├────────────────────────────────────────────────────────────┤
│                    Segment 2 Data                          │
├────────────────────────────────────────────────────────────┤
│                    ...                                     │
└────────────────────────────────────────────────────────────┘
```

**Segment Allocation:**

| Image Type | Bytes/Sample | Segments |
|------------|--------------|----------|
| 8-bit Grayscale | 1 | 1 |
| 16-bit Grayscale | 2 | 2 (high, low bytes) |
| 8-bit RGB | 1 | 3 (R, G, B) |
| 16-bit RGB | 2 | 6 (Rh, Rl, Gh, Gl, Bh, Bl) |

**Features:**
- No external library dependency
- 8-bit and 16-bit support
- Grayscale and RGB color
- Maximum 15 segments per frame

**Limitations:**
- Poor compression for natural images
- Best for images with large uniform regions
- No support for signed pixel representation in encoding

---

## 7. Codec Factory

### 7.1 DES-ENC-007: Codec Factory

**Traces to:** SRS-CODEC-001 (Image Compression)

**File:** `include/pacs/encoding/compression/codec_factory.hpp`, `src/encoding/compression/codec_factory.cpp`

Factory class for creating codec instances based on Transfer Syntax UID.

```cpp
namespace pacs::encoding::compression {

class codec_factory {
public:
    // Create codec by Transfer Syntax UID
    [[nodiscard]] static std::unique_ptr<compression_codec> create(
        std::string_view transfer_syntax_uid);

    // Create codec by Transfer Syntax object
    [[nodiscard]] static std::unique_ptr<compression_codec> create(
        const transfer_syntax& ts);

    // Query supported Transfer Syntaxes
    [[nodiscard]] static std::vector<std::string_view> supported_transfer_syntaxes();
    [[nodiscard]] static bool is_supported(std::string_view transfer_syntax_uid);
    [[nodiscard]] static bool is_supported(const transfer_syntax& ts);

private:
    codec_factory() = delete;  // Static-only class
};

} // namespace pacs::encoding::compression
```

**Supported Transfer Syntaxes:**

| UID | Codec Class | Description |
|-----|-------------|-------------|
| 1.2.840.10008.1.2.4.50 | jpeg_baseline_codec | JPEG Baseline (Process 1) |
| 1.2.840.10008.1.2.4.70 | jpeg_lossless_codec | JPEG Lossless (Process 14) |
| 1.2.840.10008.1.2.4.80 | jpeg_ls_codec (lossless) | JPEG-LS Lossless |
| 1.2.840.10008.1.2.4.81 | jpeg_ls_codec (near-lossless) | JPEG-LS Near-Lossless |
| 1.2.840.10008.1.2.4.90 | jpeg2000_codec (lossless) | JPEG 2000 Lossless Only |
| 1.2.840.10008.1.2.4.91 | jpeg2000_codec (lossy) | JPEG 2000 |
| 1.2.840.10008.1.2.5 | rle_codec | RLE Lossless |

**Usage Example:**

```cpp
auto codec = codec_factory::create("1.2.840.10008.1.2.4.50");
if (codec) {
    auto result = codec->encode(pixel_data, params);
    if (result.is_ok()) {
        auto compressed = result.value();
        // Use compressed.data
    }
}
```

**Thread Safety:** All factory methods are thread-safe.

---

## 8. Big Endian Codec

### 8.1 DES-ENC-006: Explicit VR Big Endian Codec

**Traces to:** SRS-CODEC-003 (Transfer Syntax Support)

**File:** `include/pacs/encoding/explicit_vr_big_endian_codec.hpp`, `src/encoding/explicit_vr_big_endian_codec.cpp`

Encoder/decoder for Explicit VR Big Endian transfer syntax (retired but required for legacy interoperability).

```cpp
namespace pacs::encoding {

class explicit_vr_big_endian_codec {
public:
    template <typename T>
    using result = pacs::Result<T>;

    // Dataset Encoding/Decoding
    [[nodiscard]] static std::vector<uint8_t> encode(
        const core::dicom_dataset& dataset);
    [[nodiscard]] static result<core::dicom_dataset> decode(
        std::span<const uint8_t> data);

    // Element Encoding/Decoding
    [[nodiscard]] static std::vector<uint8_t> encode_element(
        const core::dicom_element& element);
    [[nodiscard]] static result<core::dicom_element> decode_element(
        std::span<const uint8_t>& data);

    // Byte Order Conversion
    [[nodiscard]] static std::vector<uint8_t> to_big_endian(
        vr_type vr, std::span<const uint8_t> data);
    [[nodiscard]] static std::vector<uint8_t> from_big_endian(
        vr_type vr, std::span<const uint8_t> data);

private:
    static void encode_sequence(std::vector<uint8_t>& buffer,
                                const core::dicom_element& element);
    static result<core::dicom_element> decode_undefined_length(
        core::dicom_tag tag, vr_type vr,
        std::span<const uint8_t>& data);
};

} // namespace pacs::encoding
```

**Transfer Syntax:** `1.2.840.10008.1.2.2` (Explicit VR Big Endian)

**Byte Swapping per VR:**

| VR Type | Swap Size | Example |
|---------|-----------|---------|
| US, SS | 16-bit | 0x0102 -> 0x0201 |
| UL, SL, FL, AT | 32-bit | 0x01020304 -> 0x04030201 |
| FD | 64-bit | 8-byte swap |
| OW | 16-bit words | Each word swapped |
| OL, OF | 32-bit values | Each value swapped |
| OD | 64-bit values | Each value swapped |
| String VRs | None | No swap needed |

**Note:** This transfer syntax is retired in DICOM 2024 but implementation is required for legacy system interoperability.

---

## 9. Class Diagrams

### 9.1 Compression Codec Hierarchy

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                           Compression Codec Hierarchy                          │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │                    compression_codec (DES-ENC-008)                       │  │
│  │                          <<abstract>>                                    │  │
│  ├─────────────────────────────────────────────────────────────────────────┤  │
│  │ +transfer_syntax_uid() -> string_view                                   │  │
│  │ +name() -> string_view                                                  │  │
│  │ +is_lossy() -> bool                                                     │  │
│  │ +can_encode(params) -> bool                                             │  │
│  │ +can_decode(params) -> bool                                             │  │
│  │ +encode(data, params, options) -> codec_result                          │  │
│  │ +decode(data, params) -> codec_result                                   │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                    △                                           │
│                                    │                                           │
│       ┌────────────────────────────┼────────────────────────────┐             │
│       │                            │                            │             │
│       │         ┌──────────────────┼──────────────────┐        │             │
│       │         │                  │                  │        │             │
│       ▼         ▼                  ▼                  ▼        ▼             │
│  ┌─────────┐ ┌───────────┐ ┌────────────┐ ┌──────────┐ ┌─────────┐          │
│  │ jpeg_   │ │ jpeg_     │ │ jpeg2000_  │ │ jpeg_ls_ │ │ rle_    │          │
│  │baseline │ │lossless_  │ │ codec      │ │ codec    │ │ codec   │          │
│  │ _codec  │ │ codec     │ │            │ │          │ │         │          │
│  │(009)    │ │(010)      │ │(012)       │ │(011)     │ │(013)    │          │
│  ├─────────┤ ├───────────┤ ├────────────┤ ├──────────┤ ├─────────┤          │
│  │Transfer │ │Transfer   │ │Transfer    │ │Transfer  │ │Transfer │          │
│  │Syntax:  │ │Syntax:    │ │Syntax:     │ │Syntax:   │ │Syntax:  │          │
│  │.4.50    │ │.4.70      │ │.4.90/.4.91 │ │.4.80/.81 │ │.5       │          │
│  │         │ │           │ │            │ │          │ │         │          │
│  │Library: │ │Library:   │ │Library:    │ │Library:  │ │Library: │          │
│  │libjpeg- │ │libjpeg-   │ │OpenJPEG    │ │CharLS    │ │Pure C++ │          │
│  │turbo    │ │turbo 3.0+ │ │            │ │          │ │         │          │
│  │         │ │           │ │            │ │          │ │         │          │
│  │Lossy:   │ │Lossy:     │ │Lossy:      │ │Lossy:    │ │Lossy:   │          │
│  │Always   │ │Never      │ │Configurable│ │Config.   │ │Never    │          │
│  └─────────┘ └───────────┘ └────────────┘ └──────────┘ └─────────┘          │
│                                                                               │
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                      codec_factory (DES-ENC-007)                         │ │
│  │                          <<static class>>                                │ │
│  ├─────────────────────────────────────────────────────────────────────────┤ │
│  │ +create(transfer_syntax_uid) -> unique_ptr<compression_codec>           │ │
│  │ +create(transfer_syntax) -> unique_ptr<compression_codec>               │ │
│  │ +supported_transfer_syntaxes() -> vector<string_view>                   │ │
│  │ +is_supported(transfer_syntax_uid) -> bool                              │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                               │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 9.2 Image Parameters and Options

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                         Supporting Structures                                   │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ┌─────────────────────────────────┐    ┌─────────────────────────────────┐   │
│  │       image_params              │    │     compression_options         │   │
│  │                                 │    │       (DES-ENC-008)             │   │
│  ├─────────────────────────────────┤    ├─────────────────────────────────┤   │
│  │ - width: uint16                 │    │ - quality: int (1-100)          │   │
│  │ - height: uint16                │    │ - lossless: bool                │   │
│  │ - bits_allocated: uint16        │    │ - progressive: bool             │   │
│  │ - bits_stored: uint16           │    │ - chroma_subsampling: int       │   │
│  │ - high_bit: uint16              │    │   (0=4:4:4, 1=4:2:2, 2=4:2:0)   │   │
│  │ - samples_per_pixel: uint16     │    └─────────────────────────────────┘   │
│  │ - planar_configuration: uint16  │                                         │
│  │ - pixel_representation: uint16  │    ┌─────────────────────────────────┐   │
│  │ - photometric: enum             │    │     compression_result          │   │
│  │ - number_of_frames: uint32      │    │       (DES-ENC-008)             │   │
│  ├─────────────────────────────────┤    ├─────────────────────────────────┤   │
│  │ +frame_size_bytes() -> size_t   │    │ - data: vector<uint8_t>         │   │
│  │ +is_grayscale() -> bool         │    │ - output_params: image_params   │   │
│  │ +is_color() -> bool             │    └─────────────────────────────────┘   │
│  │ +is_signed() -> bool            │                                         │
│  │ +valid_for_jpeg_baseline()      │    ┌─────────────────────────────────┐   │
│  │ +valid_for_jpeg_lossless()      │    │ photometric_interpretation      │   │
│  │ +valid_for_jpeg2000()           │    │       <<enumeration>>           │   │
│  │ +valid_for_jpeg_ls()            │    ├─────────────────────────────────┤   │
│  │ +valid_for_rle()                │    │ monochrome1                     │   │
│  └─────────────────────────────────┘    │ monochrome2                     │   │
│                                         │ rgb                             │   │
│                                         │ ycbcr_full                      │   │
│                                         │ ycbcr_full_422                  │   │
│                                         │ palette_color                   │   │
│                                         │ unknown                         │   │
│                                         └─────────────────────────────────┘   │
│                                                                                │
└───────────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Sequence Diagrams

### 10.1 SEQ-ENC-001: Image Compression Flow

```
┌─────────┐     ┌───────────────┐     ┌──────────────────┐     ┌─────────────┐
│  Client │     │ codec_factory │     │compression_codec │     │  External   │
│         │     │               │     │  (concrete)      │     │  Library    │
└────┬────┘     └──────┬────────┘     └────────┬─────────┘     └──────┬──────┘
     │                 │                       │                      │
     │ 1. create(transfer_syntax_uid)          │                      │
     │────────────────►│                       │                      │
     │                 │                       │                      │
     │                 │ 2. Match UID to codec │                      │
     │                 │──────────────────────►│                      │
     │                 │                       │                      │
     │ 3. unique_ptr<codec>                    │                      │
     │◄────────────────│                       │                      │
     │                 │                       │                      │
     │ 4. can_encode(params)                   │                      │
     │────────────────────────────────────────►│                      │
     │                 │                       │                      │
     │ 5. true/false   │                       │                      │
     │◄────────────────────────────────────────│                      │
     │                 │                       │                      │
     │ 6. encode(pixel_data, params, options)  │                      │
     │────────────────────────────────────────►│                      │
     │                 │                       │                      │
     │                 │                       │ 7. Library encode    │
     │                 │                       │─────────────────────►│
     │                 │                       │                      │
     │                 │                       │ 8. Compressed bytes  │
     │                 │                       │◄─────────────────────│
     │                 │                       │                      │
     │ 9. codec_result (compressed data)       │                      │
     │◄────────────────────────────────────────│                      │
     │                 │                       │                      │
```

### 10.2 SEQ-ENC-002: RLE Encoding Flow

```
┌─────────┐     ┌───────────┐     ┌──────────────────────────────────────────┐
│  Client │     │ rle_codec │     │              Internal Processing         │
└────┬────┘     └─────┬─────┘     └─────────────────────┬────────────────────┘
     │                │                                 │
     │ 1. encode(pixel_data, params)                    │
     │───────────────►│                                 │
     │                │                                 │
     │                │ 2. Calculate segment count      │
     │                │    samples_per_pixel * bytes_per_sample
     │                │────────────────────────────────►│
     │                │                                 │
     │                │ 3. Allocate RLE header (64 bytes)
     │                │────────────────────────────────►│
     │                │                                 │
     │                │ 4. For each segment:            │
     │                │    a. Extract byte plane        │
     │                │    b. Apply RLE encoding:       │
     │                │       - Replicate runs (n, byte)│
     │                │       - Literal runs (256-n, bytes...)
     │                │────────────────────────────────►│
     │                │                                 │
     │                │ 5. Write segment offsets to header
     │                │────────────────────────────────►│
     │                │                                 │
     │ 6. codec_result                                  │
     │    - RLE header (64 bytes)                       │
     │    - Segment 1 data                              │
     │    - Segment 2 data (if 16-bit or color)         │
     │    - ...                                         │
     │◄───────────────│                                 │
     │                │                                 │
```

### 10.3 SEQ-ENC-003: JPEG 2000 Multi-Resolution Encode

```
┌─────────┐     ┌──────────────┐     ┌───────────┐     ┌──────────────────────┐
│  Client │     │ jpeg2000_    │     │ OpenJPEG  │     │ Internal Processing  │
│         │     │ codec        │     │ Library   │     │                      │
└────┬────┘     └──────┬───────┘     └─────┬─────┘     └──────────┬───────────┘
     │                 │                   │                      │
     │ 1. encode(pixel_data, params)       │                      │
     │────────────────►│                   │                      │
     │                 │                   │                      │
     │                 │ 2. Create image structure               │
     │                 │──────────────────────────────────────────►
     │                 │                   │                      │
     │                 │ 3. Set encoding parameters              │
     │                 │    - lossless/lossy mode                │
     │                 │    - resolution_levels (default: 6)     │
     │                 │    - compression_ratio (for lossy)      │
     │                 │──────────────────►│                      │
     │                 │                   │                      │
     │                 │                   │ 4. Apply DWT:       │
     │                 │                   │    - 5/3 (lossless) │
     │                 │                   │    - 9/7 (lossy)    │
     │                 │                   │                      │
     │                 │                   │ 5. Tier-1 coding    │
     │                 │                   │    (bit-plane coding)│
     │                 │                   │                      │
     │                 │                   │ 6. Tier-2 coding    │
     │                 │                   │    (packet formation)│
     │                 │                   │                      │
     │                 │ 7. J2K codestream │                      │
     │                 │◄──────────────────│                      │
     │                 │                   │                      │
     │ 8. codec_result (J2K data)          │                      │
     │◄────────────────│                   │                      │
     │                 │                   │                      │
```

---

## 11. Traceability

### 11.1 SRS to SDS Traceability

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-CODEC-001** | Image Compression | DES-ENC-007, DES-ENC-008, DES-ENC-009, DES-ENC-010, DES-ENC-011, DES-ENC-012, DES-ENC-013 | compression_codec, jpeg codecs, codec_factory, image_params |
| **SRS-CODEC-002** | Lossless Compression | DES-ENC-010, DES-ENC-011, DES-ENC-012, DES-ENC-013 | jpeg_lossless_codec, jpeg_ls_codec, jpeg2000_codec (lossless), rle_codec |
| **SRS-CODEC-003** | Transfer Syntax Support | DES-ENC-006, DES-ENC-007 | explicit_vr_big_endian_codec, codec_factory |

### 11.2 SDS to Implementation Traceability

| SDS ID | Design Element | Header File | Source File |
|--------|----------------|-------------|-------------|
| DES-ENC-006 | explicit_vr_big_endian_codec | `include/pacs/encoding/explicit_vr_big_endian_codec.hpp` | `src/encoding/explicit_vr_big_endian_codec.cpp` |
| DES-ENC-007 | codec_factory | `include/pacs/encoding/compression/codec_factory.hpp` | `src/encoding/compression/codec_factory.cpp` |
| DES-ENC-008 | compression_codec, compression_options, compression_result | `include/pacs/encoding/compression/compression_codec.hpp` | - |
| DES-ENC-009 | jpeg_baseline_codec | `include/pacs/encoding/compression/jpeg_baseline_codec.hpp` | `src/encoding/compression/jpeg_baseline_codec.cpp` |
| DES-ENC-010 | jpeg_lossless_codec | `include/pacs/encoding/compression/jpeg_lossless_codec.hpp` | `src/encoding/compression/jpeg_lossless_codec.cpp` |
| DES-ENC-011 | jpeg_ls_codec | `include/pacs/encoding/compression/jpeg_ls_codec.hpp` | `src/encoding/compression/jpeg_ls_codec.cpp` |
| DES-ENC-012 | jpeg2000_codec | `include/pacs/encoding/compression/jpeg2000_codec.hpp` | `src/encoding/compression/jpeg2000_codec.cpp` |
| DES-ENC-013 | rle_codec | `include/pacs/encoding/compression/rle_codec.hpp` | `src/encoding/compression/rle_codec.cpp` |
| - | image_params, photometric_interpretation | `include/pacs/encoding/compression/image_params.hpp` | - |

### 11.3 SDS to Test Traceability

| SDS ID | Design Element | Test File |
|--------|----------------|-----------|
| DES-ENC-006 | explicit_vr_big_endian_codec | `tests/encoding/explicit_vr_big_endian_codec_test.cpp` |
| DES-ENC-009 | jpeg_baseline_codec | `tests/encoding/compression/jpeg_baseline_codec_test.cpp` |
| DES-ENC-011 | jpeg_ls_codec | `tests/encoding/compression/jpeg_ls_codec_test.cpp` |
| DES-ENC-012 | jpeg2000_codec | `tests/encoding/compression/jpeg2000_codec_test.cpp` |
| DES-ENC-013 | rle_codec | `tests/encoding/compression/rle_codec_test.cpp` |

---

## Appendix A: Transfer Syntax UIDs

### Compression Transfer Syntaxes

| UID | Name | Codec |
|-----|------|-------|
| 1.2.840.10008.1.2.4.50 | JPEG Baseline (Process 1) | jpeg_baseline_codec |
| 1.2.840.10008.1.2.4.51 | JPEG Extended (Process 2 & 4) | (Not implemented) |
| 1.2.840.10008.1.2.4.57 | JPEG Lossless, Non-Hierarchical (Process 14) | (Subset of .4.70) |
| 1.2.840.10008.1.2.4.70 | JPEG Lossless, First-Order Prediction | jpeg_lossless_codec |
| 1.2.840.10008.1.2.4.80 | JPEG-LS Lossless | jpeg_ls_codec |
| 1.2.840.10008.1.2.4.81 | JPEG-LS Lossy (Near-Lossless) | jpeg_ls_codec |
| 1.2.840.10008.1.2.4.90 | JPEG 2000 Lossless Only | jpeg2000_codec |
| 1.2.840.10008.1.2.4.91 | JPEG 2000 | jpeg2000_codec |
| 1.2.840.10008.1.2.5 | RLE Lossless | rle_codec |

### Non-Compression Transfer Syntaxes

| UID | Name | Notes |
|-----|------|-------|
| 1.2.840.10008.1.2 | Implicit VR Little Endian | Default |
| 1.2.840.10008.1.2.1 | Explicit VR Little Endian | Common |
| 1.2.840.10008.1.2.2 | Explicit VR Big Endian | Retired (2024) |

---

## Appendix B: Compression Ratio Guidelines

### Recommended Settings for Medical Imaging

| Modality | Recommended Codec | Quality/Ratio | Notes |
|----------|-------------------|---------------|-------|
| CT | JPEG 2000 Lossless | - | Diagnostic quality required |
| MR | JPEG-LS or JPEG 2000 | Lossless | High dynamic range |
| CR/DR | JPEG 2000 | 15:1 to 20:1 | Visually lossless |
| US | JPEG Baseline | 75-85 quality | Real-time considerations |
| MG | JPEG 2000 Lossless | - | Microcalcification detection |
| NM/PET | RLE | - | Low complexity images |

### Performance Comparison

| Codec | Encode Speed | Decode Speed | Compression Ratio (Lossless) |
|-------|--------------|--------------|------------------------------|
| JPEG Baseline | 50-100 MB/s | 100-200 MB/s | N/A (lossy only) |
| JPEG Lossless | 20-40 MB/s | 40-80 MB/s | 2:1 to 3:1 |
| JPEG 2000 | 10-20 MB/s | 50-100 MB/s | 2:1 to 3:1 |
| JPEG-LS | 30-60 MB/s | 60-120 MB/s | 2:1 to 4:1 |
| RLE | 80-150 MB/s | 150-300 MB/s | 1.5:1 to 2:1 |

*Performance measured on Intel Core i7, single-threaded*

---

*Document generated for Issue #472 - Compression Codecs SDS*
