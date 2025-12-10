/**
 * @file pet_storage.cpp
 * @brief Implementation of PET Image Storage SOP Classes
 */

#include "pacs/services/sop_classes/pet_storage.hpp"

#include <algorithm>
#include <array>

namespace pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_pet_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // JPEG Lossless (for diagnostic quality - recommended for quantitative data)
        "1.2.840.10008.1.2.4.70",
        // JPEG 2000 Lossless (better compression, preserves quantitative values)
        "1.2.840.10008.1.2.4.90",
        // RLE Lossless
        "1.2.840.10008.1.2.5"
    };
}

// =============================================================================
// Photometric Interpretation
// =============================================================================

std::string_view to_string(pet_photometric_interpretation interp) noexcept {
    switch (interp) {
        case pet_photometric_interpretation::monochrome2:
            return "MONOCHROME2";
    }
    return "MONOCHROME2";
}

pet_photometric_interpretation
parse_pet_photometric_interpretation([[maybe_unused]] std::string_view value) noexcept {
    // PET images are always MONOCHROME2
    return pet_photometric_interpretation::monochrome2;
}

bool is_valid_pet_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME2";
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

// Static array of PET SOP class information
constexpr std::array<pet_sop_class_info, 3> pet_sop_classes = {{
    {
        pet_image_storage_uid,
        "PET Image Storage",
        "Standard PET image storage",
        false,
        false,  // single-frame
        false   // not enhanced
    },
    {
        enhanced_pet_image_storage_uid,
        "Enhanced PET Image Storage",
        "Enhanced PET with multi-frame and extended attributes",
        false,
        true,   // supports multiframe
        true    // enhanced
    },
    {
        legacy_converted_enhanced_pet_image_storage_uid,
        "Legacy Converted Enhanced PET Image Storage",
        "Legacy PET converted to enhanced format",
        false,
        true,   // supports multiframe
        true    // enhanced
    }
}};

}  // namespace

std::vector<std::string> get_pet_storage_sop_classes(bool include_retired) {
    std::vector<std::string> result;
    result.reserve(pet_sop_classes.size());

    for (const auto& info : pet_sop_classes) {
        if (!info.is_retired || include_retired) {
            result.emplace_back(info.uid);
        }
    }

    return result;
}

const pet_sop_class_info*
get_pet_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(
        pet_sop_classes.begin(),
        pet_sop_classes.end(),
        [uid](const auto& info) { return info.uid == uid; }
    );

    if (it != pet_sop_classes.end()) {
        return &(*it);
    }
    return nullptr;
}

bool is_pet_storage_sop_class(std::string_view uid) noexcept {
    return get_pet_sop_class_info(uid) != nullptr;
}

bool is_enhanced_pet_sop_class(std::string_view uid) noexcept {
    const auto* info = get_pet_sop_class_info(uid);
    return info != nullptr && info->is_enhanced;
}

// =============================================================================
// Reconstruction Type Conversion
// =============================================================================

std::string_view to_string(pet_reconstruction_type recon) noexcept {
    switch (recon) {
        case pet_reconstruction_type::fbp:
            return "FBP";
        case pet_reconstruction_type::osem:
            return "OSEM";
        case pet_reconstruction_type::mlem:
            return "MLEM";
        case pet_reconstruction_type::tof_osem:
            return "TOF-OSEM";
        case pet_reconstruction_type::psf_osem:
            return "PSF-OSEM";
        case pet_reconstruction_type::other:
            return "OTHER";
    }
    return "OTHER";
}

pet_reconstruction_type
parse_pet_reconstruction_type(std::string_view value) noexcept {
    if (value == "FBP" || value == "FILTERED BACK PROJECTION") {
        return pet_reconstruction_type::fbp;
    }
    if (value == "OSEM" || value == "3D-OSEM") {
        return pet_reconstruction_type::osem;
    }
    if (value == "MLEM") {
        return pet_reconstruction_type::mlem;
    }
    if (value.find("TOF") != std::string_view::npos) {
        return pet_reconstruction_type::tof_osem;
    }
    if (value.find("PSF") != std::string_view::npos) {
        return pet_reconstruction_type::psf_osem;
    }
    return pet_reconstruction_type::other;
}

// =============================================================================
// PET Units Conversion
// =============================================================================

std::string_view to_string(pet_units units) noexcept {
    switch (units) {
        case pet_units::cnts:
            return "CNTS";
        case pet_units::bqml:
            return "BQML";
        case pet_units::gml:
            return "GML";
        case pet_units::suv_bw:
            return "SUV";
        case pet_units::suv_lbm:
            return "SUL";
        case pet_units::suv_bsa:
            return "SUV_BSA";
        case pet_units::percent_id_gram:
            return "%ID/G";
        case pet_units::other:
            return "OTHER";
    }
    return "OTHER";
}

pet_units parse_pet_units(std::string_view value) noexcept {
    if (value == "CNTS") {
        return pet_units::cnts;
    }
    if (value == "BQML" || value == "BQ/ML") {
        return pet_units::bqml;
    }
    if (value == "GML" || value == "G/ML") {
        return pet_units::gml;
    }
    if (value == "SUV" || value == "SUV_BW") {
        return pet_units::suv_bw;
    }
    if (value == "SUL" || value == "SUV_LBM") {
        return pet_units::suv_lbm;
    }
    if (value == "SUV_BSA") {
        return pet_units::suv_bsa;
    }
    if (value == "%ID/G" || value == "PERCENT_ID_GRAM") {
        return pet_units::percent_id_gram;
    }
    return pet_units::other;
}

// =============================================================================
// Radiotracer Conversion
// =============================================================================

std::string_view to_string(pet_radiotracer tracer) noexcept {
    switch (tracer) {
        case pet_radiotracer::fdg:
            return "18F-FDG";
        case pet_radiotracer::naf:
            return "18F-NaF";
        case pet_radiotracer::flt:
            return "18F-FLT";
        case pet_radiotracer::fdopa:
            return "18F-FDOPA";
        case pet_radiotracer::ammonia:
            return "13N-Ammonia";
        case pet_radiotracer::rubidium:
            return "82Rb";
        case pet_radiotracer::gallium_dotatate:
            return "68Ga-DOTATATE";
        case pet_radiotracer::psma:
            return "PSMA";
        case pet_radiotracer::other:
            return "Other";
    }
    return "Other";
}

}  // namespace pacs::services::sop_classes
