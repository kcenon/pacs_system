/**
 * @file dx_storage.cpp
 * @brief Implementation of Digital X-Ray (DX) Image Storage SOP Classes
 */

#include "pacs/services/sop_classes/dx_storage.hpp"

#include <algorithm>
#include <array>

namespace pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_dx_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // JPEG Lossless (for diagnostic quality - most important for DX)
        "1.2.840.10008.1.2.4.70",
        // JPEG 2000 Lossless (better compression, good for large DX images)
        "1.2.840.10008.1.2.4.90",
        // JPEG 2000 Lossy (for archival with acceptable quality loss)
        "1.2.840.10008.1.2.4.91",
        // RLE Lossless (simple lossless compression)
        "1.2.840.10008.1.2.5"
    };
}

// =============================================================================
// Photometric Interpretation
// =============================================================================

std::string_view to_string(dx_photometric_interpretation interp) noexcept {
    switch (interp) {
        case dx_photometric_interpretation::monochrome1:
            return "MONOCHROME1";
        case dx_photometric_interpretation::monochrome2:
            return "MONOCHROME2";
    }
    return "MONOCHROME2";
}

dx_photometric_interpretation
parse_dx_photometric_interpretation(std::string_view value) noexcept {
    if (value == "MONOCHROME1") {
        return dx_photometric_interpretation::monochrome1;
    }
    // Default to MONOCHROME2 for DX
    return dx_photometric_interpretation::monochrome2;
}

bool is_valid_dx_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME1" || value == "MONOCHROME2";
}

// =============================================================================
// Image Type
// =============================================================================

std::string_view to_string(dx_image_type type) noexcept {
    switch (type) {
        case dx_image_type::for_presentation:
            return "FOR PRESENTATION";
        case dx_image_type::for_processing:
            return "FOR PROCESSING";
    }
    return "FOR PRESENTATION";
}

// =============================================================================
// View Position
// =============================================================================

std::string_view to_string(dx_view_position position) noexcept {
    switch (position) {
        case dx_view_position::ap:
            return "AP";
        case dx_view_position::pa:
            return "PA";
        case dx_view_position::lateral:
            return "LATERAL";
        case dx_view_position::oblique:
            return "OBLIQUE";
        case dx_view_position::other:
            return "";
    }
    return "";
}

dx_view_position parse_view_position(std::string_view value) noexcept {
    if (value == "AP") return dx_view_position::ap;
    if (value == "PA") return dx_view_position::pa;
    if (value == "LATERAL" || value == "LAT" ||
        value == "LL" || value == "RL") {
        return dx_view_position::lateral;
    }
    if (value == "OBLIQUE" || value == "OBL" ||
        value == "LAO" || value == "RAO" ||
        value == "LPO" || value == "RPO") {
        return dx_view_position::oblique;
    }
    return dx_view_position::other;
}

// =============================================================================
// Detector Type
// =============================================================================

std::string_view to_string(dx_detector_type type) noexcept {
    switch (type) {
        case dx_detector_type::direct:
            return "DIRECT";
        case dx_detector_type::indirect:
            return "INDIRECT";
        case dx_detector_type::storage:
            return "STORAGE";
        case dx_detector_type::film:
            return "FILM";
    }
    return "DIRECT";
}

dx_detector_type parse_detector_type(std::string_view value) noexcept {
    if (value == "DIRECT") return dx_detector_type::direct;
    if (value == "INDIRECT") return dx_detector_type::indirect;
    if (value == "STORAGE") return dx_detector_type::storage;
    if (value == "FILM") return dx_detector_type::film;
    // Default to direct for modern DR systems
    return dx_detector_type::direct;
}

// =============================================================================
// Body Part
// =============================================================================

std::string_view to_string(dx_body_part part) noexcept {
    switch (part) {
        case dx_body_part::chest:       return "CHEST";
        case dx_body_part::abdomen:     return "ABDOMEN";
        case dx_body_part::pelvis:      return "PELVIS";
        case dx_body_part::spine:       return "SPINE";
        case dx_body_part::skull:       return "SKULL";
        case dx_body_part::hand:        return "HAND";
        case dx_body_part::foot:        return "FOOT";
        case dx_body_part::knee:        return "KNEE";
        case dx_body_part::elbow:       return "ELBOW";
        case dx_body_part::shoulder:    return "SHOULDER";
        case dx_body_part::hip:         return "HIP";
        case dx_body_part::wrist:       return "WRIST";
        case dx_body_part::ankle:       return "ANKLE";
        case dx_body_part::extremity:   return "EXTREMITY";
        case dx_body_part::breast:      return "BREAST";
        case dx_body_part::other:       return "";
    }
    return "";
}

dx_body_part parse_body_part(std::string_view value) noexcept {
    if (value == "CHEST") return dx_body_part::chest;
    if (value == "ABDOMEN") return dx_body_part::abdomen;
    if (value == "PELVIS") return dx_body_part::pelvis;
    if (value == "SPINE" || value == "CSPINE" || value == "TSPINE" ||
        value == "LSPINE" || value == "SSPINE") {
        return dx_body_part::spine;
    }
    if (value == "SKULL" || value == "HEAD") return dx_body_part::skull;
    if (value == "HAND" || value == "FINGER") return dx_body_part::hand;
    if (value == "FOOT" || value == "TOE") return dx_body_part::foot;
    if (value == "KNEE") return dx_body_part::knee;
    if (value == "ELBOW") return dx_body_part::elbow;
    if (value == "SHOULDER") return dx_body_part::shoulder;
    if (value == "HIP") return dx_body_part::hip;
    if (value == "WRIST") return dx_body_part::wrist;
    if (value == "ANKLE") return dx_body_part::ankle;
    if (value == "EXTREMITY" || value == "ARM" || value == "LEG") {
        return dx_body_part::extremity;
    }
    if (value == "BREAST") return dx_body_part::breast;
    return dx_body_part::other;
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

// Static array of DX SOP class information
constexpr std::array<dx_sop_class_info, 6> dx_sop_classes = {{
    {
        dx_image_storage_for_presentation_uid,
        "Digital X-Ray Image Storage - For Presentation",
        "Processed digital radiography images ready for clinical review",
        dx_image_type::for_presentation,
        false,
        false
    },
    {
        dx_image_storage_for_processing_uid,
        "Digital X-Ray Image Storage - For Processing",
        "Raw digital radiography data requiring further processing",
        dx_image_type::for_processing,
        false,
        false
    },
    {
        mammography_image_storage_for_presentation_uid,
        "Digital Mammography X-Ray Image Storage - For Presentation",
        "Processed digital mammography images ready for clinical review",
        dx_image_type::for_presentation,
        true,
        false
    },
    {
        mammography_image_storage_for_processing_uid,
        "Digital Mammography X-Ray Image Storage - For Processing",
        "Raw digital mammography data requiring further processing",
        dx_image_type::for_processing,
        true,
        false
    },
    {
        intraoral_image_storage_for_presentation_uid,
        "Digital Intra-Oral X-Ray Image Storage - For Presentation",
        "Processed dental intra-oral images ready for clinical review",
        dx_image_type::for_presentation,
        false,
        true
    },
    {
        intraoral_image_storage_for_processing_uid,
        "Digital Intra-Oral X-Ray Image Storage - For Processing",
        "Raw dental intra-oral data requiring further processing",
        dx_image_type::for_processing,
        false,
        true
    }
}};

}  // namespace

std::vector<std::string> get_dx_storage_sop_classes(bool include_mammography,
                                                     bool include_intraoral) {
    std::vector<std::string> result;
    result.reserve(6);

    for (const auto& info : dx_sop_classes) {
        // Always include general DX
        if (!info.is_mammography && !info.is_intraoral) {
            result.emplace_back(info.uid);
        }
        // Include mammography if requested
        else if (info.is_mammography && include_mammography) {
            result.emplace_back(info.uid);
        }
        // Include intra-oral if requested
        else if (info.is_intraoral && include_intraoral) {
            result.emplace_back(info.uid);
        }
    }

    return result;
}

const dx_sop_class_info*
get_dx_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(
        dx_sop_classes.begin(),
        dx_sop_classes.end(),
        [uid](const auto& info) { return info.uid == uid; }
    );

    if (it != dx_sop_classes.end()) {
        return &(*it);
    }
    return nullptr;
}

bool is_dx_storage_sop_class(std::string_view uid) noexcept {
    return get_dx_sop_class_info(uid) != nullptr;
}

bool is_dx_for_processing_sop_class(std::string_view uid) noexcept {
    const auto* info = get_dx_sop_class_info(uid);
    return info != nullptr && info->image_type == dx_image_type::for_processing;
}

bool is_dx_for_presentation_sop_class(std::string_view uid) noexcept {
    const auto* info = get_dx_sop_class_info(uid);
    return info != nullptr && info->image_type == dx_image_type::for_presentation;
}

bool is_mammography_sop_class(std::string_view uid) noexcept {
    const auto* info = get_dx_sop_class_info(uid);
    return info != nullptr && info->is_mammography;
}

}  // namespace pacs::services::sop_classes
