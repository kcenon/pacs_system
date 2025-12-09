/**
 * @file nm_storage.cpp
 * @brief Implementation of Nuclear Medicine Image Storage SOP Classes
 */

#include "pacs/services/sop_classes/nm_storage.hpp"

#include <algorithm>
#include <array>

namespace pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_nm_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // JPEG Lossless (for diagnostic quality - important for quantitative data)
        "1.2.840.10008.1.2.4.70",
        // JPEG 2000 Lossless (better compression, preserves count values)
        "1.2.840.10008.1.2.4.90",
        // RLE Lossless
        "1.2.840.10008.1.2.5"
    };
}

// =============================================================================
// Photometric Interpretation
// =============================================================================

std::string_view to_string(nm_photometric_interpretation interp) noexcept {
    switch (interp) {
        case nm_photometric_interpretation::monochrome2:
            return "MONOCHROME2";
        case nm_photometric_interpretation::palette_color:
            return "PALETTE COLOR";
    }
    return "MONOCHROME2";
}

nm_photometric_interpretation
parse_nm_photometric_interpretation(std::string_view value) noexcept {
    if (value == "PALETTE COLOR") {
        return nm_photometric_interpretation::palette_color;
    }
    return nm_photometric_interpretation::monochrome2;
}

bool is_valid_nm_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME2" || value == "PALETTE COLOR";
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

// Static array of NM SOP class information
constexpr std::array<nm_sop_class_info, 2> nm_sop_classes = {{
    {
        nm_image_storage_uid,
        "NM Image Storage",
        "Nuclear medicine planar, SPECT, and gated images",
        false,
        true   // supports multiframe (dynamic, gated, SPECT)
    },
    {
        nm_image_storage_retired_uid,
        "NM Image Storage (Retired)",
        "Legacy nuclear medicine image storage",
        true,
        true
    }
}};

}  // namespace

std::vector<std::string> get_nm_storage_sop_classes(bool include_retired) {
    std::vector<std::string> result;
    result.reserve(nm_sop_classes.size());

    for (const auto& info : nm_sop_classes) {
        if (!info.is_retired || include_retired) {
            result.emplace_back(info.uid);
        }
    }

    return result;
}

const nm_sop_class_info*
get_nm_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(
        nm_sop_classes.begin(),
        nm_sop_classes.end(),
        [uid](const auto& info) { return info.uid == uid; }
    );

    if (it != nm_sop_classes.end()) {
        return &(*it);
    }
    return nullptr;
}

bool is_nm_storage_sop_class(std::string_view uid) noexcept {
    return get_nm_sop_class_info(uid) != nullptr;
}

bool is_nm_multiframe_sop_class(std::string_view uid) noexcept {
    const auto* info = get_nm_sop_class_info(uid);
    return info != nullptr && info->supports_multiframe;
}

// =============================================================================
// Type of Data Conversion
// =============================================================================

std::string_view to_string(nm_type_of_data type) noexcept {
    switch (type) {
        case nm_type_of_data::static_image:
            return "STATIC";
        case nm_type_of_data::dynamic:
            return "DYNAMIC";
        case nm_type_of_data::gated:
            return "GATED";
        case nm_type_of_data::whole_body:
            return "WHOLE BODY";
        case nm_type_of_data::recon_tomo:
            return "RECON TOMO";
        case nm_type_of_data::recon_gated_tomo:
            return "RECON GATED TOMO";
        case nm_type_of_data::tomo:
            return "TOMO";
        case nm_type_of_data::gated_tomo:
            return "GATED TOMO";
    }
    return "STATIC";
}

nm_type_of_data parse_nm_type_of_data(std::string_view value) noexcept {
    if (value == "STATIC") {
        return nm_type_of_data::static_image;
    }
    if (value == "DYNAMIC") {
        return nm_type_of_data::dynamic;
    }
    if (value == "GATED") {
        return nm_type_of_data::gated;
    }
    if (value == "WHOLE BODY") {
        return nm_type_of_data::whole_body;
    }
    if (value == "RECON TOMO") {
        return nm_type_of_data::recon_tomo;
    }
    if (value == "RECON GATED TOMO") {
        return nm_type_of_data::recon_gated_tomo;
    }
    if (value == "TOMO") {
        return nm_type_of_data::tomo;
    }
    if (value == "GATED TOMO") {
        return nm_type_of_data::gated_tomo;
    }
    return nm_type_of_data::static_image;
}

// =============================================================================
// Collimator Type Conversion
// =============================================================================

std::string_view to_string(nm_collimator_type collimator) noexcept {
    switch (collimator) {
        case nm_collimator_type::parallel:
            return "PARA";
        case nm_collimator_type::fan_beam:
            return "FANB";
        case nm_collimator_type::cone_beam:
            return "CONE";
        case nm_collimator_type::pinhole:
            return "PINH";
        case nm_collimator_type::diverging:
            return "DIVG";
        case nm_collimator_type::converging:
            return "CVGB";
        case nm_collimator_type::none:
            return "NONE";
    }
    return "PARA";
}

nm_collimator_type parse_nm_collimator_type(std::string_view value) noexcept {
    if (value == "PARA" || value == "PARALLEL") {
        return nm_collimator_type::parallel;
    }
    if (value == "FANB" || value == "FAN BEAM") {
        return nm_collimator_type::fan_beam;
    }
    if (value == "CONE" || value == "CONE BEAM") {
        return nm_collimator_type::cone_beam;
    }
    if (value == "PINH" || value == "PINHOLE") {
        return nm_collimator_type::pinhole;
    }
    if (value == "DIVG" || value == "DIVERGING") {
        return nm_collimator_type::diverging;
    }
    if (value == "CVGB" || value == "CONVERGING") {
        return nm_collimator_type::converging;
    }
    if (value == "NONE") {
        return nm_collimator_type::none;
    }
    return nm_collimator_type::parallel;
}

// =============================================================================
// Radioisotope Conversion
// =============================================================================

std::string_view to_string(nm_radioisotope isotope) noexcept {
    switch (isotope) {
        case nm_radioisotope::tc99m:
            return "Tc-99m";
        case nm_radioisotope::i131:
            return "I-131";
        case nm_radioisotope::i123:
            return "I-123";
        case nm_radioisotope::tl201:
            return "Tl-201";
        case nm_radioisotope::ga67:
            return "Ga-67";
        case nm_radioisotope::in111:
            return "In-111";
        case nm_radioisotope::f18:
            return "F-18";
        case nm_radioisotope::other:
            return "Other";
    }
    return "Other";
}

double get_primary_energy_kev(nm_radioisotope isotope) noexcept {
    switch (isotope) {
        case nm_radioisotope::tc99m:
            return 140.0;
        case nm_radioisotope::i131:
            return 364.0;
        case nm_radioisotope::i123:
            return 159.0;
        case nm_radioisotope::tl201:
            return 71.0;  // Primary peak (also 167 keV)
        case nm_radioisotope::ga67:
            return 93.0;  // Primary peak (also 185, 300 keV)
        case nm_radioisotope::in111:
            return 171.0; // Primary peak (also 245 keV)
        case nm_radioisotope::f18:
            return 511.0; // Annihilation photons
        case nm_radioisotope::other:
            return 0.0;
    }
    return 0.0;
}

// =============================================================================
// Whole Body Technique Conversion
// =============================================================================

std::string_view to_string(nm_whole_body_technique technique) noexcept {
    switch (technique) {
        case nm_whole_body_technique::single_pass:
            return "1PASS";
        case nm_whole_body_technique::multi_pass:
            return "2PASS";
        case nm_whole_body_technique::stepping:
            return "STEP";
    }
    return "1PASS";
}

}  // namespace pacs::services::sop_classes
