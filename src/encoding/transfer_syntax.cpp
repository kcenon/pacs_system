#include "pacs/encoding/transfer_syntax.hpp"

#include <algorithm>
#include <array>

namespace pacs::encoding {

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
    bool supported;  // Phase 1: only uncompressed syntaxes
};

/**
 * @brief Static registry of all known DICOM Transfer Syntaxes.
 *
 * This table contains the standard Transfer Syntaxes defined in DICOM PS3.5.
 * Compression support will be added in later phases.
 */
static constexpr std::array<ts_entry, 9> TS_REGISTRY = {{
    // Uncompressed Transfer Syntaxes (supported in Phase 1)
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

    // Deflated (not supported in Phase 1)
    {"1.2.840.10008.1.2.1.99",
     "Deflated Explicit VR Little Endian",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     false, true, false},

    // JPEG Transfer Syntaxes
    {"1.2.840.10008.1.2.4.50",
     "JPEG Baseline (Process 1)",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},  // Supported in Phase 3

    {"1.2.840.10008.1.2.4.70",
     "JPEG Lossless, Non-Hierarchical, First-Order Prediction",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},

    // JPEG 2000 Transfer Syntaxes (not supported in Phase 1)
    {"1.2.840.10008.1.2.4.90",
     "JPEG 2000 Image Compression (Lossless Only)",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},

    {"1.2.840.10008.1.2.4.91",
     "JPEG 2000 Image Compression",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, false},

    // RLE Lossless Transfer Syntax
    {"1.2.840.10008.1.2.5",
     "RLE Lossless",
     byte_order::little_endian,
     vr_encoding::explicit_vr,
     true, false, true},
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

}  // namespace

// Static Transfer Syntax instances
const transfer_syntax transfer_syntax::implicit_vr_little_endian{
    "1.2.840.10008.1.2",
    "Implicit VR Little Endian",
    byte_order::little_endian,
    vr_encoding::implicit,
    false, false, true};

const transfer_syntax transfer_syntax::explicit_vr_little_endian{
    "1.2.840.10008.1.2.1",
    "Explicit VR Little Endian",
    byte_order::little_endian,
    vr_encoding::explicit_vr,
    false, false, true};

const transfer_syntax transfer_syntax::explicit_vr_big_endian{
    "1.2.840.10008.1.2.2",
    "Explicit VR Big Endian",
    byte_order::big_endian,
    vr_encoding::explicit_vr,
    false, false, true};

const transfer_syntax transfer_syntax::deflated_explicit_vr_le{
    "1.2.840.10008.1.2.1.99",
    "Deflated Explicit VR Little Endian",
    byte_order::little_endian,
    vr_encoding::explicit_vr,
    false, true, false};

const transfer_syntax transfer_syntax::jpeg_baseline{
    "1.2.840.10008.1.2.4.50",
    "JPEG Baseline (Process 1)",
    byte_order::little_endian,
    vr_encoding::explicit_vr,
    true, false, true};  // Supported in Phase 3

const transfer_syntax transfer_syntax::jpeg_lossless{
    "1.2.840.10008.1.2.4.70",
    "JPEG Lossless, Non-Hierarchical, First-Order Prediction",
    byte_order::little_endian,
    vr_encoding::explicit_vr,
    true, false, false};

const transfer_syntax transfer_syntax::jpeg2000_lossless{
    "1.2.840.10008.1.2.4.90",
    "JPEG 2000 Image Compression (Lossless Only)",
    byte_order::little_endian,
    vr_encoding::explicit_vr,
    true, false, false};

const transfer_syntax transfer_syntax::jpeg2000_lossy{
    "1.2.840.10008.1.2.4.91",
    "JPEG 2000 Image Compression",
    byte_order::little_endian,
    vr_encoding::explicit_vr,
    true, false, false};

const transfer_syntax transfer_syntax::rle_lossless{
    "1.2.840.10008.1.2.5",
    "RLE Lossless",
    byte_order::little_endian,
    vr_encoding::explicit_vr,
    true, false, true};

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

}  // namespace pacs::encoding
