// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/pacs/encoding/transfer_syntax.h"

#include <algorithm>
#include <array>

namespace kcenon::pacs::encoding {

namespace {

/**
 * @brief Registry entry for a Transfer Syntax.
 *
 * Uses constexpr for compile-time initialization of the registry.
 */
struct ts_entry {
    std::string_view uid;
    std::string_view name;
    byte_order endian;
    vr_encoding vr;
    bool encapsulated;
    bool deflated;
    bool supported;  // see transfer_syntax::is_supported()
};

/**
 * @brief Static registry of all known DICOM Transfer Syntaxes.
 *
 * This table contains the standard Transfer Syntaxes defined in DICOM PS3.5.
 * The supported flag follows the definition of transfer_syntax::is_supported();
 * tests/encoding/transfer_syntax_support_test.cpp checks it against
 * compression::codec_factory.
 */
static constexpr std::array<ts_entry, 20> TS_REGISTRY = {{
    // Uncompressed Transfer Syntaxes
    {"1.2.840.10008.1.2",
     "Implicit VR Little Endian",
     byte_order::little_endian,
     vr_encoding::implicit,
     false, false, true},

    {"1.2.840.10008.1.2.1",
     "Explicit VR Little Endian",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     false, false, true},

    {"1.2.840.10008.1.2.2",
     "Explicit VR Big Endian",
     byte_order::big_endian,
     vr_encoding::explicit_vr,
     false, false, true},

    // Deflated (no inflate or deflate code)
    {"1.2.840.10008.1.2.1.99",
     "Deflated Explicit VR Little Endian",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     false, true, false},

    // JPEG Transfer Syntaxes (jpeg_baseline_codec needs libjpeg-turbo;
    // jpeg_lossless_codec is built in)
    {"1.2.840.10008.1.2.4.50",
     "JPEG Baseline (Process 1)",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    {"1.2.840.10008.1.2.4.70",
     "JPEG Lossless, Non-Hierarchical, First-Order Prediction",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    // JPEG-LS Transfer Syntaxes (jpeg_ls_codec needs CharLS)
    {"1.2.840.10008.1.2.4.80",
     "JPEG-LS Lossless Image Compression",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    {"1.2.840.10008.1.2.4.81",
     "JPEG-LS Lossy (Near-Lossless) Image Compression",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    // JPEG 2000 Transfer Syntaxes (jpeg2000_codec needs OpenJPEG)
    {"1.2.840.10008.1.2.4.90",
     "JPEG 2000 Image Compression (Lossless Only)",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    {"1.2.840.10008.1.2.4.91",
     "JPEG 2000 Image Compression",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    // RLE Lossless Transfer Syntax
    {"1.2.840.10008.1.2.5",
     "RLE Lossless",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    // High-Throughput JPEG 2000 Transfer Syntaxes (DICOM Supplement 235;
    // htj2k_codec needs OpenJPH)
    {"1.2.840.10008.1.2.4.201",
     "High-Throughput JPEG 2000 Image Compression (Lossless Only)",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    {"1.2.840.10008.1.2.4.202",
     "High-Throughput JPEG 2000 with RPCL Options Image Compression (Lossless Only)",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    {"1.2.840.10008.1.2.4.203",
     "High-Throughput JPEG 2000 Image Compression",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},

    // HEVC/H.265 Transfer Syntaxes (hevc_codec is a stub)
    {"1.2.840.10008.1.2.4.107",
     "HEVC/H.265 Main Profile / Level 5.1",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},

    {"1.2.840.10008.1.2.4.108",
     "HEVC/H.265 Main 10 Profile / Level 5.1",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},

    // JPEG XL Transfer Syntaxes (DICOM Supplement 232; jpegxl_codec is a stub)
    {"1.2.840.10008.1.2.4.110",
     "JPEG XL Lossless",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},

    {"1.2.840.10008.1.2.4.111",
     "JPEG XL JPEG Recompression",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},

    {"1.2.840.10008.1.2.4.112",
     "JPEG XL",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},

    // Frame Deflate Transfer Syntax (DICOM Supplement 244; frame_deflate_codec
    // works only with PACS_WITH_ZLIB, which no build defines)
    {"1.2.840.10008.1.2.11",
     "Frame Deflate",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},
}};

/**
 * @brief Finds an entry in the Transfer Syntax registry by UID.
 * @param uid The UID to search for
 * @return Pointer to the entry if found, nullptr otherwise
 */
const ts_entry* find_entry(std::string_view uid) {
    auto it = std::find_if(TS_REGISTRY.begin(), TS_REGISTRY.end(),
                           [&uid](const ts_entry& entry) {
                               return entry.uid == uid;
                           });
    return (it != TS_REGISTRY.end()) ? &(*it) : nullptr;
}

/**
 * @brief Copies a registry entry into a transfer_syntax.
 * @param uid A UID that is in TS_REGISTRY
 * @return The transfer_syntax for the entry
 */
transfer_syntax registry_instance(std::string_view uid) {
    return find_transfer_syntax(uid).value();
}

}  // namespace

// Static Transfer Syntax instances, copied from their registry entries so
// that the two cannot disagree
const transfer_syntax transfer_syntax::implicit_vr_little_endian =
    registry_instance("1.2.840.10008.1.2");

const transfer_syntax transfer_syntax::explicit_vr_little_endian =
    registry_instance("1.2.840.10008.1.2.1");

const transfer_syntax transfer_syntax::explicit_vr_big_endian =
    registry_instance("1.2.840.10008.1.2.2");

const transfer_syntax transfer_syntax::deflated_explicit_vr_le =
    registry_instance("1.2.840.10008.1.2.1.99");

const transfer_syntax transfer_syntax::jpeg_baseline =
    registry_instance("1.2.840.10008.1.2.4.50");

const transfer_syntax transfer_syntax::jpeg_lossless =
    registry_instance("1.2.840.10008.1.2.4.70");

const transfer_syntax transfer_syntax::jpeg_ls_lossless =
    registry_instance("1.2.840.10008.1.2.4.80");

const transfer_syntax transfer_syntax::jpeg_ls_near_lossless =
    registry_instance("1.2.840.10008.1.2.4.81");

const transfer_syntax transfer_syntax::jpeg2000_lossless =
    registry_instance("1.2.840.10008.1.2.4.90");

const transfer_syntax transfer_syntax::jpeg2000_lossy =
    registry_instance("1.2.840.10008.1.2.4.91");

const transfer_syntax transfer_syntax::rle_lossless =
    registry_instance("1.2.840.10008.1.2.5");

const transfer_syntax transfer_syntax::htj2k_lossless =
    registry_instance("1.2.840.10008.1.2.4.201");

const transfer_syntax transfer_syntax::htj2k_rpcl =
    registry_instance("1.2.840.10008.1.2.4.202");

const transfer_syntax transfer_syntax::htj2k_lossy =
    registry_instance("1.2.840.10008.1.2.4.203");

const transfer_syntax transfer_syntax::hevc_main =
    registry_instance("1.2.840.10008.1.2.4.107");

const transfer_syntax transfer_syntax::hevc_main10 =
    registry_instance("1.2.840.10008.1.2.4.108");

const transfer_syntax transfer_syntax::jpegxl_lossless =
    registry_instance("1.2.840.10008.1.2.4.110");

const transfer_syntax transfer_syntax::jpegxl_jpeg_recompression =
    registry_instance("1.2.840.10008.1.2.4.111");

const transfer_syntax transfer_syntax::jpegxl_lossy =
    registry_instance("1.2.840.10008.1.2.4.112");

const transfer_syntax transfer_syntax::frame_deflate =
    registry_instance("1.2.840.10008.1.2.11");

// Public constructor - looks up UID in registry
transfer_syntax::transfer_syntax(std::string_view uid)
    : uid_(uid),
      name_("Unknown"),
      endianness_(byte_order::little_endian),
      vr_type_(vr_encoding::implicit),
      encapsulated_(false),
      deflated_(false),
      valid_(false),
      supported_(false) {
    if (const auto* entry = find_entry(uid)) {
        name_ = entry->name;
        endianness_ = entry->endian;
        vr_type_ = entry->vr;
        encapsulated_ = entry->encapsulated;
        deflated_ = entry->deflated;
        valid_ = true;
        supported_ = entry->supported;
    }
}

// Private constructor for static instances
transfer_syntax::transfer_syntax(std::string_view uid, std::string_view name,
                                 byte_order endian, vr_encoding vr,
                                 bool encapsulated, bool deflated,
                                 bool supported)
    : uid_(uid),
      name_(name),
      endianness_(endian),
      vr_type_(vr),
      encapsulated_(encapsulated),
      deflated_(deflated),
      valid_(true),
      supported_(supported) {}

std::string_view transfer_syntax::uid() const noexcept {
    return uid_;
}

std::string_view transfer_syntax::name() const noexcept {
    return name_;
}

byte_order transfer_syntax::endianness() const noexcept {
    return endianness_;
}

vr_encoding transfer_syntax::vr_type() const noexcept {
    return vr_type_;
}

bool transfer_syntax::is_encapsulated() const noexcept {
    return encapsulated_;
}

bool transfer_syntax::is_deflated() const noexcept {
    return deflated_;
}

bool transfer_syntax::is_valid() const noexcept {
    return valid_;
}

bool transfer_syntax::is_supported() const noexcept {
    return supported_;
}

bool transfer_syntax::operator==(const transfer_syntax& other) const noexcept {
    return uid_ == other.uid_;
}

bool transfer_syntax::operator!=(const transfer_syntax& other) const noexcept {
    return !(*this == other);
}

std::optional<transfer_syntax> find_transfer_syntax(std::string_view uid) {
    if (const auto* entry = find_entry(uid)) {
        return transfer_syntax{entry->uid, entry->name, entry->endian,
                               entry->vr, entry->encapsulated, entry->deflated,
                               entry->supported};
    }
    return std::nullopt;
}

std::vector<transfer_syntax> supported_transfer_syntaxes() {
    std::vector<transfer_syntax> result;
    result.reserve(TS_REGISTRY.size());

    for (const auto& entry : TS_REGISTRY) {
        if (entry.supported) {
            result.push_back(transfer_syntax{entry.uid, entry.name, entry.endian,
                                             entry.vr, entry.encapsulated,
                                             entry.deflated, entry.supported});
        }
    }
    return result;
}

std::vector<transfer_syntax> all_transfer_syntaxes() {
    std::vector<transfer_syntax> result;
    result.reserve(TS_REGISTRY.size());

    for (const auto& entry : TS_REGISTRY) {
        result.push_back(transfer_syntax{entry.uid, entry.name, entry.endian,
                                         entry.vr, entry.encapsulated,
                                         entry.deflated, entry.supported});
    }
    return result;
}

}  // namespace kcenon::pacs::encoding
