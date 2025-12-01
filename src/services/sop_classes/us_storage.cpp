/**
 * @file us_storage.cpp
 * @brief Implementation of Ultrasound Image Storage SOP Classes
 */

#include "pacs/services/sop_classes/us_storage.hpp"

#include <algorithm>
#include <array>

namespace pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_us_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // JPEG Baseline (lossy, good for color US)
        "1.2.840.10008.1.2.4.50",
        // JPEG Lossless (for diagnostic quality)
        "1.2.840.10008.1.2.4.70",
        // JPEG 2000 Lossless (better compression)
        "1.2.840.10008.1.2.4.90",
        // JPEG 2000 Lossy (for archival)
        "1.2.840.10008.1.2.4.91",
        // RLE Lossless
        "1.2.840.10008.1.2.5"
    };
}

// =============================================================================
// Photometric Interpretation
// =============================================================================

namespace {

constexpr std::string_view photometric_strings[] = {
    "MONOCHROME1",
    "MONOCHROME2",
    "PALETTE COLOR",
    "RGB",
    "YBR_FULL",
    "YBR_FULL_422"
};

}  // namespace

std::string_view to_string(us_photometric_interpretation interp) noexcept {
    auto index = static_cast<size_t>(interp);
    if (index < std::size(photometric_strings)) {
        return photometric_strings[index];
    }
    return "MONOCHROME2";
}

us_photometric_interpretation
parse_photometric_interpretation(std::string_view value) noexcept {
    if (value == "MONOCHROME1") {
        return us_photometric_interpretation::monochrome1;
    }
    if (value == "MONOCHROME2") {
        return us_photometric_interpretation::monochrome2;
    }
    if (value == "PALETTE COLOR") {
        return us_photometric_interpretation::palette_color;
    }
    if (value == "RGB") {
        return us_photometric_interpretation::rgb;
    }
    if (value == "YBR_FULL") {
        return us_photometric_interpretation::ybr_full;
    }
    if (value == "YBR_FULL_422") {
        return us_photometric_interpretation::ybr_full_422;
    }
    // Default to MONOCHROME2 for unknown values
    return us_photometric_interpretation::monochrome2;
}

bool is_valid_us_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME1" ||
           value == "MONOCHROME2" ||
           value == "PALETTE COLOR" ||
           value == "RGB" ||
           value == "YBR_FULL" ||
           value == "YBR_FULL_422";
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

// Static array of US SOP class information
constexpr std::array<us_sop_class_info, 4> us_sop_classes = {{
    {
        us_image_storage_uid,
        "US Image Storage",
        "Single-frame ultrasound images",
        false,
        false
    },
    {
        us_multiframe_image_storage_uid,
        "US Multi-frame Image Storage",
        "Multi-frame ultrasound cine loops and 3D/4D volumes",
        false,
        true
    },
    {
        us_image_storage_retired_uid,
        "US Image Storage (Retired)",
        "Legacy single-frame ultrasound (retired)",
        true,
        false
    },
    {
        us_multiframe_image_storage_retired_uid,
        "US Multi-frame Image Storage (Retired)",
        "Legacy multi-frame ultrasound (retired)",
        true,
        true
    }
}};

}  // namespace

std::vector<std::string> get_us_storage_sop_classes(bool include_retired) {
    std::vector<std::string> result;
    result.reserve(include_retired ? 4 : 2);

    for (const auto& info : us_sop_classes) {
        if (!info.is_retired || include_retired) {
            result.emplace_back(info.uid);
        }
    }

    return result;
}

const us_sop_class_info*
get_us_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(
        us_sop_classes.begin(),
        us_sop_classes.end(),
        [uid](const auto& info) { return info.uid == uid; }
    );

    if (it != us_sop_classes.end()) {
        return &(*it);
    }
    return nullptr;
}

bool is_us_storage_sop_class(std::string_view uid) noexcept {
    return get_us_sop_class_info(uid) != nullptr;
}

bool is_us_multiframe_sop_class(std::string_view uid) noexcept {
    const auto* info = get_us_sop_class_info(uid);
    return info != nullptr && info->supports_multiframe;
}

}  // namespace pacs::services::sop_classes
