/**
 * @file xa_storage.cpp
 * @brief Implementation of X-Ray Angiographic Image Storage SOP Classes
 */

#include "pacs/services/sop_classes/xa_storage.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_xa_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // JPEG Lossless (for diagnostic quality - future support)
        "1.2.840.10008.1.2.4.70",
        // RLE Lossless (for multi-frame - future support)
        "1.2.840.10008.1.2.5"
    };
}

// =============================================================================
// Photometric Interpretation
// =============================================================================

std::string_view to_string(xa_photometric_interpretation interp) noexcept {
    switch (interp) {
        case xa_photometric_interpretation::monochrome1:
            return "MONOCHROME1";
        case xa_photometric_interpretation::monochrome2:
            return "MONOCHROME2";
    }
    return "MONOCHROME2";
}

xa_photometric_interpretation
parse_xa_photometric_interpretation(std::string_view value) noexcept {
    if (value == "MONOCHROME1") {
        return xa_photometric_interpretation::monochrome1;
    }
    // Default to MONOCHROME2 for unknown or MONOCHROME2 values
    return xa_photometric_interpretation::monochrome2;
}

bool is_valid_xa_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME1" || value == "MONOCHROME2";
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

// Static array of XA SOP class information
constexpr std::array<xa_sop_class_info, 5> xa_sop_classes = {{
    {
        xa_image_storage_uid,
        "XA Image Storage",
        "Standard X-Ray Angiographic images (single/multi-frame)",
        false,  // not enhanced
        false,  // not 3D
        true    // supports multiframe
    },
    {
        enhanced_xa_image_storage_uid,
        "Enhanced XA Image Storage",
        "Enhanced X-Ray Angiographic IOD with extended attributes",
        true,   // enhanced
        false,  // not 3D
        true    // supports multiframe
    },
    {
        xrf_image_storage_uid,
        "XRF Image Storage",
        "X-Ray Radiofluoroscopic images for fluoroscopy procedures",
        false,  // not enhanced
        false,  // not 3D
        true    // supports multiframe
    },
    {
        xray_3d_angiographic_image_storage_uid,
        "X-Ray 3D Angiographic Image Storage",
        "3D rotational angiography reconstructions",
        true,   // enhanced (by nature)
        true,   // 3D
        true    // supports multiframe
    },
    {
        xray_3d_craniofacial_image_storage_uid,
        "X-Ray 3D Craniofacial Image Storage",
        "3D craniofacial imaging (cone beam CT)",
        true,   // enhanced (by nature)
        true,   // 3D
        true    // supports multiframe
    }
}};

}  // namespace

std::vector<std::string> get_xa_storage_sop_classes(bool include_3d) {
    std::vector<std::string> result;
    result.reserve(include_3d ? 5 : 3);

    for (const auto& info : xa_sop_classes) {
        if (!info.is_3d || include_3d) {
            result.emplace_back(info.uid);
        }
    }

    return result;
}

const xa_sop_class_info*
get_xa_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(
        xa_sop_classes.begin(),
        xa_sop_classes.end(),
        [uid](const auto& info) { return info.uid == uid; }
    );

    if (it != xa_sop_classes.end()) {
        return &(*it);
    }
    return nullptr;
}

bool is_xa_storage_sop_class(std::string_view uid) noexcept {
    return get_xa_sop_class_info(uid) != nullptr;
}

bool is_xa_multiframe_sop_class(std::string_view uid) noexcept {
    const auto* info = get_xa_sop_class_info(uid);
    return info != nullptr && info->supports_multiframe;
}

bool is_enhanced_xa_sop_class(std::string_view uid) noexcept {
    const auto* info = get_xa_sop_class_info(uid);
    return info != nullptr && info->is_enhanced;
}

bool is_xa_3d_sop_class(std::string_view uid) noexcept {
    const auto* info = get_xa_sop_class_info(uid);
    return info != nullptr && info->is_3d;
}

// =============================================================================
// Positioner Information
// =============================================================================

std::string_view to_string(xa_positioner_motion motion) noexcept {
    switch (motion) {
        case xa_positioner_motion::stationary:
            return "STATIONARY";
        case xa_positioner_motion::dynamic:
            return "DYNAMIC";
    }
    return "STATIONARY";
}

bool xa_positioner_angles::is_valid() const noexcept {
    // Typical clinical range: primary angle -90 to +90 (LAO/RAO)
    // Secondary angle: -45 to +45 (Cranial/Caudal)
    return primary_angle >= -180.0 && primary_angle <= 180.0 &&
           secondary_angle >= -90.0 && secondary_angle <= 90.0;
}

// =============================================================================
// Calibration Information
// =============================================================================

double xa_calibration_data::magnification_factor() const noexcept {
    if (distance_source_to_patient <= 0.0 ||
        distance_source_to_detector <= 0.0) {
        return 0.0;
    }
    return distance_source_to_detector / distance_source_to_patient;
}

double xa_calibration_data::isocenter_pixel_spacing() const noexcept {
    double mag = magnification_factor();
    if (mag <= 0.0 || imager_pixel_spacing[0] <= 0.0) {
        return 0.0;
    }
    // Pixel spacing at isocenter = detector pixel spacing / magnification
    return imager_pixel_spacing[0] / mag;
}

bool xa_calibration_data::is_valid() const noexcept {
    return imager_pixel_spacing[0] > 0.0 &&
           imager_pixel_spacing[1] > 0.0 &&
           distance_source_to_detector > 0.0 &&
           distance_source_to_patient > 0.0 &&
           distance_source_to_detector > distance_source_to_patient;
}

}  // namespace pacs::services::sop_classes
