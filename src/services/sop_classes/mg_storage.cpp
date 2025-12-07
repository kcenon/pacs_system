/**
 * @file mg_storage.cpp
 * @brief Implementation of Digital Mammography X-Ray Image Storage utilities
 */

#include "pacs/services/sop_classes/mg_storage.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace pacs::services::sop_classes {

// =============================================================================
// Breast Laterality Implementation
// =============================================================================

std::string_view to_string(breast_laterality laterality) noexcept {
    switch (laterality) {
        case breast_laterality::left:       return "L";
        case breast_laterality::right:      return "R";
        case breast_laterality::bilateral:  return "B";
        case breast_laterality::unknown:    return "";
    }
    return "";
}

breast_laterality parse_breast_laterality(std::string_view value) noexcept {
    if (value.empty()) {
        return breast_laterality::unknown;
    }

    // Handle single character codes
    if (value.size() == 1) {
        switch (std::toupper(static_cast<unsigned char>(value[0]))) {
            case 'L': return breast_laterality::left;
            case 'R': return breast_laterality::right;
            case 'B': return breast_laterality::bilateral;
        }
    }

    // Handle full words (case-insensitive)
    std::string lower_value;
    lower_value.reserve(value.size());
    for (char c : value) {
        lower_value += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower_value == "left") return breast_laterality::left;
    if (lower_value == "right") return breast_laterality::right;
    if (lower_value == "bilateral" || lower_value == "both") {
        return breast_laterality::bilateral;
    }

    return breast_laterality::unknown;
}

bool is_valid_breast_laterality(std::string_view value) noexcept {
    return value == "L" || value == "R" || value == "B";
}

// =============================================================================
// Mammography View Position Implementation
// =============================================================================

std::string_view to_string(mg_view_position position) noexcept {
    switch (position) {
        case mg_view_position::cc:       return "CC";
        case mg_view_position::mlo:      return "MLO";
        case mg_view_position::ml:       return "ML";
        case mg_view_position::lm:       return "LM";
        case mg_view_position::xccl:     return "XCCL";
        case mg_view_position::xccm:     return "XCCM";
        case mg_view_position::fb:       return "FB";
        case mg_view_position::sio:      return "SIO";
        case mg_view_position::iso:      return "ISO";
        case mg_view_position::cv:       return "CV";
        case mg_view_position::at:       return "AT";
        case mg_view_position::spot:     return "SPOT";
        case mg_view_position::mag:      return "MAG";
        case mg_view_position::spot_mag: return "SPOT MAG";
        case mg_view_position::rl:       return "RL";
        case mg_view_position::rm:       return "RM";
        case mg_view_position::rs:       return "RS";
        case mg_view_position::ri:       return "RI";
        case mg_view_position::tangen:   return "TAN";
        case mg_view_position::implant:  return "ID";
        case mg_view_position::id:       return "ID";
        case mg_view_position::other:    return "";
    }
    return "";
}

mg_view_position parse_mg_view_position(std::string_view value) noexcept {
    if (value.empty()) {
        return mg_view_position::other;
    }

    // Convert to uppercase for comparison
    std::string upper_value;
    upper_value.reserve(value.size());
    for (char c : value) {
        upper_value += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Standard views
    if (upper_value == "CC") return mg_view_position::cc;
    if (upper_value == "MLO") return mg_view_position::mlo;
    if (upper_value == "ML") return mg_view_position::ml;
    if (upper_value == "LM") return mg_view_position::lm;

    // Extended CC views
    if (upper_value == "XCCL") return mg_view_position::xccl;
    if (upper_value == "XCCM") return mg_view_position::xccm;

    // Other standard views
    if (upper_value == "FB") return mg_view_position::fb;
    if (upper_value == "SIO") return mg_view_position::sio;
    if (upper_value == "ISO") return mg_view_position::iso;
    if (upper_value == "CV") return mg_view_position::cv;
    if (upper_value == "AT") return mg_view_position::at;

    // Spot/magnification views
    if (upper_value == "SPOT") return mg_view_position::spot;
    if (upper_value == "MAG") return mg_view_position::mag;
    if (upper_value == "SPOT MAG" || upper_value == "SPOTMAG") {
        return mg_view_position::spot_mag;
    }

    // Rolled views
    if (upper_value == "RL") return mg_view_position::rl;
    if (upper_value == "RM") return mg_view_position::rm;
    if (upper_value == "RS") return mg_view_position::rs;
    if (upper_value == "RI") return mg_view_position::ri;

    // Specialized views
    if (upper_value == "TAN" || upper_value == "TANGENTIAL") {
        return mg_view_position::tangen;
    }
    if (upper_value == "ID" || upper_value == "IMPLANT DISPLACED" ||
        upper_value == "IMPLANTDISPLACED") {
        return mg_view_position::implant;
    }

    return mg_view_position::other;
}

bool is_screening_view(mg_view_position position) noexcept {
    return position == mg_view_position::cc || position == mg_view_position::mlo;
}

bool is_magnification_view(mg_view_position position) noexcept {
    return position == mg_view_position::mag ||
           position == mg_view_position::spot_mag;
}

bool is_spot_compression_view(mg_view_position position) noexcept {
    return position == mg_view_position::spot ||
           position == mg_view_position::spot_mag;
}

std::vector<std::string_view> get_valid_mg_view_positions() noexcept {
    return {
        "CC", "MLO", "ML", "LM", "XCCL", "XCCM", "FB", "SIO", "ISO",
        "CV", "AT", "SPOT", "MAG", "SPOT MAG", "RL", "RM", "RS", "RI",
        "TAN", "ID"
    };
}

// =============================================================================
// Compression Force Validation
// =============================================================================

bool is_valid_compression_force(double force_n) noexcept {
    // Typical mammography compression force: 50-200 N
    // Extended range to account for variations: 20-300 N
    return force_n >= 20.0 && force_n <= 300.0;
}

std::pair<double, double> get_typical_compression_force_range() noexcept {
    return {50.0, 200.0};  // Newtons
}

bool is_valid_compressed_breast_thickness(double thickness_mm) noexcept {
    // Typical compressed breast thickness: 20-100 mm
    // Extended range: 10-150 mm
    return thickness_mm >= 10.0 && thickness_mm <= 150.0;
}

// =============================================================================
// Image Type Implementation
// =============================================================================

std::string_view to_string(mg_image_type type) noexcept {
    switch (type) {
        case mg_image_type::for_presentation: return "FOR PRESENTATION";
        case mg_image_type::for_processing:   return "FOR PROCESSING";
    }
    return "";
}

// =============================================================================
// CAD Processing Status Implementation
// =============================================================================

std::string_view to_string(cad_processing_status status) noexcept {
    switch (status) {
        case cad_processing_status::not_processed:
            return "NOT PROCESSED";
        case cad_processing_status::processed_no_findings:
            return "PROCESSED - NO FINDINGS";
        case cad_processing_status::processed_findings:
            return "PROCESSED - FINDINGS";
        case cad_processing_status::processing_failed:
            return "PROCESSING FAILED";
        case cad_processing_status::pending:
            return "PENDING";
    }
    return "";
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

// Static array of mammography SOP class information
constexpr std::array<mg_sop_class_info, 5> mg_sop_classes = {{
    {
        mg_image_storage_for_presentation_uid,
        "Digital Mammography X-Ray Image Storage - For Presentation",
        "Standard 2D mammography images ready for display",
        mg_image_type::for_presentation,
        false,  // not tomosynthesis
        false   // no multiframe
    },
    {
        mg_image_storage_for_processing_uid,
        "Digital Mammography X-Ray Image Storage - For Processing",
        "Raw 2D mammography images requiring processing",
        mg_image_type::for_processing,
        false,  // not tomosynthesis
        false   // no multiframe
    },
    {
        breast_tomosynthesis_image_storage_uid,
        "Breast Tomosynthesis Image Storage",
        "3D breast tomosynthesis image set",
        mg_image_type::for_presentation,
        true,   // is tomosynthesis
        true    // multiframe
    },
    {
        breast_projection_image_storage_for_presentation_uid,
        "Breast Projection X-Ray Image Storage - For Presentation",
        "Synthesized 2D from tomosynthesis - for display",
        mg_image_type::for_presentation,
        true,   // related to tomosynthesis
        false   // no multiframe
    },
    {
        breast_projection_image_storage_for_processing_uid,
        "Breast Projection X-Ray Image Storage - For Processing",
        "Synthesized 2D from tomosynthesis - raw",
        mg_image_type::for_processing,
        true,   // related to tomosynthesis
        false   // no multiframe
    }
}};

}  // namespace

std::vector<std::string> get_mg_storage_sop_classes(bool include_tomosynthesis) {
    std::vector<std::string> result;
    result.reserve(mg_sop_classes.size());

    for (const auto& info : mg_sop_classes) {
        if (!include_tomosynthesis && info.is_tomosynthesis) {
            continue;
        }
        result.emplace_back(info.uid);
    }

    return result;
}

const mg_sop_class_info* get_mg_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(mg_sop_classes.begin(), mg_sop_classes.end(),
        [uid](const mg_sop_class_info& info) {
            return info.uid == uid;
        });

    if (it != mg_sop_classes.end()) {
        return &(*it);
    }
    return nullptr;
}

bool is_mg_storage_sop_class(std::string_view uid) noexcept {
    return get_mg_sop_class_info(uid) != nullptr;
}

bool is_breast_tomosynthesis_sop_class(std::string_view uid) noexcept {
    auto info = get_mg_sop_class_info(uid);
    return info && info->is_tomosynthesis;
}

bool is_mg_for_processing_sop_class(std::string_view uid) noexcept {
    auto info = get_mg_sop_class_info(uid);
    return info && info->image_type == mg_image_type::for_processing;
}

bool is_mg_for_presentation_sop_class(std::string_view uid) noexcept {
    auto info = get_mg_sop_class_info(uid);
    return info && info->image_type == mg_image_type::for_presentation;
}

// =============================================================================
// Transfer Syntax Recommendations
// =============================================================================

std::vector<std::string> get_mg_transfer_syntaxes() {
    return {
        // JPEG 2000 Lossless - excellent for high-resolution mammography
        "1.2.840.10008.1.2.4.90",
        // JPEG Lossless - widely supported
        "1.2.840.10008.1.2.4.70",
        // JPEG 2000 Lossy - good compression for storage
        "1.2.840.10008.1.2.4.91",
        // Explicit VR Little Endian - universal compatibility
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian - legacy support
        "1.2.840.10008.1.2"
    };
}

// =============================================================================
// Utility Functions
// =============================================================================

bool is_valid_laterality_view_combination(
    breast_laterality laterality,
    mg_view_position view) noexcept {

    // Bilateral laterality should only be used for comparison views
    // or special study types, not for standard single-breast views
    if (laterality == breast_laterality::bilateral) {
        // CV (cleavage view) can legitimately be bilateral
        if (view == mg_view_position::cv) {
            return true;
        }
        // Most standard views should specify L or R
        return false;
    }

    // Unknown laterality with a specific view is problematic
    if (laterality == breast_laterality::unknown) {
        // Allow if view is also unspecified
        return view == mg_view_position::other;
    }

    // Left or Right with any view is valid
    return true;
}

std::vector<std::pair<breast_laterality, mg_view_position>>
get_standard_screening_views() noexcept {
    return {
        {breast_laterality::right, mg_view_position::cc},
        {breast_laterality::left, mg_view_position::cc},
        {breast_laterality::right, mg_view_position::mlo},
        {breast_laterality::left, mg_view_position::mlo}
    };
}

std::string create_mg_image_type(
    bool is_original,
    bool is_primary,
    mg_image_type type) {

    std::string result;

    // Value 1: ORIGINAL or DERIVED
    result += is_original ? "ORIGINAL" : "DERIVED";
    result += "\\";

    // Value 2: PRIMARY or SECONDARY
    result += is_primary ? "PRIMARY" : "SECONDARY";
    result += "\\";

    // Value 3: Image flavor - empty for mammography typically
    result += "\\";

    // Value 4: Derived Pixel Contrast - specific for For Presentation/Processing
    if (type == mg_image_type::for_presentation) {
        result += "NONE";  // or specific processing type
    } else {
        result += "";  // empty for processing images
    }

    return result;
}

}  // namespace pacs::services::sop_classes
