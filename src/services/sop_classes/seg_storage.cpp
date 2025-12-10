/**
 * @file seg_storage.cpp
 * @brief Implementation of Segmentation Storage SOP Classes
 */

#include "pacs/services/sop_classes/seg_storage.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>

namespace pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_seg_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // JPEG 2000 Lossless (good for binary data)
        "1.2.840.10008.1.2.4.90",
        // RLE Lossless (excellent for binary segmentations)
        "1.2.840.10008.1.2.5",
        // JPEG Lossless
        "1.2.840.10008.1.2.4.70"
    };
}

// =============================================================================
// Segmentation Type
// =============================================================================

std::string_view to_string(segmentation_type type) noexcept {
    switch (type) {
        case segmentation_type::binary:
            return "BINARY";
        case segmentation_type::fractional:
            return "FRACTIONAL";
    }
    return "BINARY";
}

segmentation_type parse_segmentation_type(std::string_view value) noexcept {
    if (value == "FRACTIONAL") {
        return segmentation_type::fractional;
    }
    return segmentation_type::binary;
}

bool is_valid_segmentation_type(std::string_view value) noexcept {
    return value == "BINARY" || value == "FRACTIONAL";
}

// =============================================================================
// Segmentation Fractional Type
// =============================================================================

std::string_view to_string(segmentation_fractional_type type) noexcept {
    switch (type) {
        case segmentation_fractional_type::probability:
            return "PROBABILITY";
        case segmentation_fractional_type::occupancy:
            return "OCCUPANCY";
    }
    return "PROBABILITY";
}

segmentation_fractional_type
parse_segmentation_fractional_type(std::string_view value) noexcept {
    if (value == "OCCUPANCY") {
        return segmentation_fractional_type::occupancy;
    }
    return segmentation_fractional_type::probability;
}

// =============================================================================
// Segment Algorithm Type
// =============================================================================

std::string_view to_string(segment_algorithm_type type) noexcept {
    switch (type) {
        case segment_algorithm_type::automatic:
            return "AUTOMATIC";
        case segment_algorithm_type::semiautomatic:
            return "SEMIAUTOMATIC";
        case segment_algorithm_type::manual:
            return "MANUAL";
    }
    return "MANUAL";
}

segment_algorithm_type
parse_segment_algorithm_type(std::string_view value) noexcept {
    if (value == "AUTOMATIC") {
        return segment_algorithm_type::automatic;
    }
    if (value == "SEMIAUTOMATIC") {
        return segment_algorithm_type::semiautomatic;
    }
    return segment_algorithm_type::manual;
}

bool is_valid_segment_algorithm_type(std::string_view value) noexcept {
    return value == "AUTOMATIC" ||
           value == "SEMIAUTOMATIC" ||
           value == "MANUAL";
}

// =============================================================================
// Segment Colors
// =============================================================================

namespace {

// Common anatomical structure colors (CIELab values scaled to 16-bit)
// L*: 0-100 -> 0-65535
// a*, b*: -128 to 127 -> 0-65535 (32768 = 0)

const std::unordered_map<std::string_view, segment_color> standard_colors = {
    // Organs
    {"Liver", {43690, 39321, 42597}},       // Brown
    {"Kidney", {36044, 43690, 29491}},      // Dark red
    {"Spleen", {38229, 42597, 30583}},      // Purple-red
    {"Pancreas", {47185, 37945, 39321}},    // Tan
    {"Heart", {32768, 49151, 32768}},       // Red
    {"Lung", {54612, 32112, 34078}},        // Pink
    {"Brain", {54612, 34406, 32768}},       // Pink-gray
    {"Bone", {61166, 32768, 35389}},        // Light yellow
    {"Muscle", {40894, 39977, 32768}},      // Brown-red

    // Tumors/Lesions
    {"Tumor", {32768, 52428, 32768}},       // Bright red
    {"Lesion", {36044, 49151, 36044}},      // Orange-red
    {"Nodule", {40894, 45874, 34406}},      // Orange

    // Vessels
    {"Artery", {29491, 52428, 32768}},      // Dark red
    {"Vein", {32768, 32768, 45874}},        // Blue
    {"Vessel", {36044, 45874, 38229}},      // Red-orange

    // Other
    {"Air", {58981, 32768, 32768}},         // Light gray
    {"Fat", {52428, 35389, 29491}},         // Yellow
    {"Fluid", {45874, 32768, 49151}},       // Blue
    {"Default", {43690, 32768, 32768}}      // Gray
};

}  // namespace

segment_color get_recommended_segment_color(std::string_view segment_label) noexcept {
    auto it = standard_colors.find(segment_label);
    if (it != standard_colors.end()) {
        return it->second;
    }
    // Return default gray for unknown structures
    return standard_colors.at("Default");
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

constexpr std::array<seg_sop_class_info, 2> seg_sop_classes = {{
    {
        segmentation_storage_uid,
        "Segmentation Storage",
        "Binary or fractional segmentation of medical images",
        false,
        false
    },
    {
        surface_segmentation_storage_uid,
        "Surface Segmentation Storage",
        "3D surface mesh segmentation",
        false,
        true
    }
}};

}  // namespace

std::vector<std::string> get_seg_storage_sop_classes(bool include_surface) {
    std::vector<std::string> result;
    result.reserve(include_surface ? 2 : 1);

    for (const auto& info : seg_sop_classes) {
        if (!info.is_surface || include_surface) {
            result.emplace_back(info.uid);
        }
    }

    return result;
}

const seg_sop_class_info*
get_seg_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(
        seg_sop_classes.begin(),
        seg_sop_classes.end(),
        [uid](const auto& info) { return info.uid == uid; }
    );

    if (it != seg_sop_classes.end()) {
        return &(*it);
    }
    return nullptr;
}

bool is_seg_storage_sop_class(std::string_view uid) noexcept {
    return get_seg_sop_class_info(uid) != nullptr;
}

bool is_surface_segmentation_sop_class(std::string_view uid) noexcept {
    return uid == surface_segmentation_storage_uid;
}

// =============================================================================
// Segment Category Codes
// =============================================================================

namespace {

struct category_code_info {
    std::string_view code;
    std::string_view meaning;
};

constexpr std::array<category_code_info, 7> category_codes = {{
    {"85756007", "Tissue"},                  // SNOMED CT
    {"123037004", "Anatomical Structure"},   // SNOMED CT
    {"260787004", "Physical Object"},        // SNOMED CT
    {"49755003", "Morphologically Abnormal Structure"}, // SNOMED CT
    {"246464006", "Function"},               // SNOMED CT
    {"272737002", "Spatial Concept"},        // SNOMED CT
    {"91720002", "Body Substance"}           // SNOMED CT
}};

}  // namespace

std::string_view get_segment_category_code(segment_category category) noexcept {
    auto index = static_cast<size_t>(category);
    if (index < category_codes.size()) {
        return category_codes[index].code;
    }
    return category_codes[0].code;  // Default to tissue
}

std::string_view get_segment_category_meaning(segment_category category) noexcept {
    auto index = static_cast<size_t>(category);
    if (index < category_codes.size()) {
        return category_codes[index].meaning;
    }
    return category_codes[0].meaning;
}

}  // namespace pacs::services::sop_classes
