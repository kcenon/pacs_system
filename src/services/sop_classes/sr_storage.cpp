/**
 * @file sr_storage.cpp
 * @brief Implementation of Structured Report Storage SOP Classes
 */

#include "pacs/services/sop_classes/sr_storage.hpp"

#include <algorithm>
#include <array>

namespace pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_sr_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // Deflated Explicit VR Little Endian (good for large SRs)
        "1.2.840.10008.1.2.1.99"
    };
}

// =============================================================================
// SR Document Type
// =============================================================================

sr_document_type get_sr_document_type(std::string_view uid) noexcept {
    if (uid == basic_text_sr_storage_uid) {
        return sr_document_type::basic_text;
    }
    if (uid == enhanced_sr_storage_uid) {
        return sr_document_type::enhanced;
    }
    if (uid == comprehensive_sr_storage_uid) {
        return sr_document_type::comprehensive;
    }
    if (uid == comprehensive_3d_sr_storage_uid) {
        return sr_document_type::comprehensive_3d;
    }
    if (uid == extensible_sr_storage_uid) {
        return sr_document_type::extensible;
    }
    if (uid == key_object_selection_document_storage_uid) {
        return sr_document_type::key_object_selection;
    }
    if (uid == mammography_cad_sr_storage_uid ||
        uid == chest_cad_sr_storage_uid ||
        uid == colon_cad_sr_storage_uid) {
        return sr_document_type::cad;
    }
    if (uid == xray_radiation_dose_sr_storage_uid ||
        uid == radiopharmaceutical_radiation_dose_sr_storage_uid ||
        uid == patient_radiation_dose_sr_storage_uid ||
        uid == enhanced_xray_radiation_dose_sr_storage_uid) {
        return sr_document_type::dose_report;
    }
    return sr_document_type::other;
}

std::string_view to_string(sr_document_type type) noexcept {
    switch (type) {
        case sr_document_type::basic_text:
            return "Basic Text SR";
        case sr_document_type::enhanced:
            return "Enhanced SR";
        case sr_document_type::comprehensive:
            return "Comprehensive SR";
        case sr_document_type::comprehensive_3d:
            return "Comprehensive 3D SR";
        case sr_document_type::extensible:
            return "Extensible SR";
        case sr_document_type::key_object_selection:
            return "Key Object Selection";
        case sr_document_type::cad:
            return "CAD SR";
        case sr_document_type::dose_report:
            return "Dose Report SR";
        case sr_document_type::procedure_log:
            return "Procedure Log SR";
        case sr_document_type::other:
            return "Other SR";
    }
    return "Unknown SR";
}

// =============================================================================
// SR Value Types
// =============================================================================

namespace {

constexpr std::string_view value_type_strings[] = {
    "TEXT",
    "CODE",
    "NUM",
    "DATETIME",
    "DATE",
    "TIME",
    "UIDREF",
    "PNAME",
    "COMPOSITE",
    "IMAGE",
    "WAVEFORM",
    "SCOORD",
    "SCOORD3D",
    "TCOORD",
    "CONTAINER",
    "TABLE"
};

}  // namespace

std::string_view to_string(sr_value_type type) noexcept {
    auto index = static_cast<size_t>(type);
    if (index < std::size(value_type_strings)) {
        return value_type_strings[index];
    }
    return "UNKNOWN";
}

sr_value_type parse_sr_value_type(std::string_view value) noexcept {
    if (value == "TEXT") return sr_value_type::text;
    if (value == "CODE") return sr_value_type::code;
    if (value == "NUM") return sr_value_type::num;
    if (value == "DATETIME") return sr_value_type::datetime;
    if (value == "DATE") return sr_value_type::date;
    if (value == "TIME") return sr_value_type::time;
    if (value == "UIDREF") return sr_value_type::uidref;
    if (value == "PNAME") return sr_value_type::pname;
    if (value == "COMPOSITE") return sr_value_type::composite;
    if (value == "IMAGE") return sr_value_type::image;
    if (value == "WAVEFORM") return sr_value_type::waveform;
    if (value == "SCOORD") return sr_value_type::scoord;
    if (value == "SCOORD3D") return sr_value_type::scoord3d;
    if (value == "TCOORD") return sr_value_type::tcoord;
    if (value == "CONTAINER") return sr_value_type::container;
    if (value == "TABLE") return sr_value_type::table;
    return sr_value_type::unknown;
}

bool is_valid_sr_value_type(std::string_view value) noexcept {
    return parse_sr_value_type(value) != sr_value_type::unknown;
}

// =============================================================================
// SR Relationship Types
// =============================================================================

std::string_view to_string(sr_relationship_type type) noexcept {
    switch (type) {
        case sr_relationship_type::contains:
            return "CONTAINS";
        case sr_relationship_type::has_obs_context:
            return "HAS OBS CONTEXT";
        case sr_relationship_type::has_acq_context:
            return "HAS ACQ CONTEXT";
        case sr_relationship_type::has_concept_mod:
            return "HAS CONCEPT MOD";
        case sr_relationship_type::has_properties:
            return "HAS PROPERTIES";
        case sr_relationship_type::inferred_from:
            return "INFERRED FROM";
        case sr_relationship_type::selected_from:
            return "SELECTED FROM";
        case sr_relationship_type::unknown:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

sr_relationship_type parse_sr_relationship_type(std::string_view value) noexcept {
    if (value == "CONTAINS") return sr_relationship_type::contains;
    if (value == "HAS OBS CONTEXT") return sr_relationship_type::has_obs_context;
    if (value == "HAS ACQ CONTEXT") return sr_relationship_type::has_acq_context;
    if (value == "HAS CONCEPT MOD") return sr_relationship_type::has_concept_mod;
    if (value == "HAS PROPERTIES") return sr_relationship_type::has_properties;
    if (value == "INFERRED FROM") return sr_relationship_type::inferred_from;
    if (value == "SELECTED FROM") return sr_relationship_type::selected_from;
    return sr_relationship_type::unknown;
}

// =============================================================================
// SR Completion and Verification
// =============================================================================

std::string_view to_string(sr_completion_flag flag) noexcept {
    switch (flag) {
        case sr_completion_flag::partial:
            return "PARTIAL";
        case sr_completion_flag::complete:
            return "COMPLETE";
    }
    return "PARTIAL";
}

sr_completion_flag parse_sr_completion_flag(std::string_view value) noexcept {
    if (value == "COMPLETE") {
        return sr_completion_flag::complete;
    }
    return sr_completion_flag::partial;
}

std::string_view to_string(sr_verification_flag flag) noexcept {
    switch (flag) {
        case sr_verification_flag::unverified:
            return "UNVERIFIED";
        case sr_verification_flag::verified:
            return "VERIFIED";
    }
    return "UNVERIFIED";
}

sr_verification_flag parse_sr_verification_flag(std::string_view value) noexcept {
    if (value == "VERIFIED") {
        return sr_verification_flag::verified;
    }
    return sr_verification_flag::unverified;
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

constexpr std::array<sr_sop_class_info, 17> sr_sop_classes = {{
    // Basic SR types
    {
        basic_text_sr_storage_uid,
        "Basic Text SR Storage",
        "Simple text-based structured report",
        sr_document_type::basic_text,
        false,
        false,
        false
    },
    {
        enhanced_sr_storage_uid,
        "Enhanced SR Storage",
        "Enhanced structured report with image references",
        sr_document_type::enhanced,
        false,
        false,
        true
    },
    {
        comprehensive_sr_storage_uid,
        "Comprehensive SR Storage",
        "Comprehensive structured report with spatial coordinates",
        sr_document_type::comprehensive,
        false,
        true,
        true
    },
    {
        comprehensive_3d_sr_storage_uid,
        "Comprehensive 3D SR Storage",
        "Comprehensive structured report with 3D spatial coordinates",
        sr_document_type::comprehensive_3d,
        false,
        true,
        true
    },
    {
        extensible_sr_storage_uid,
        "Extensible SR Storage",
        "Extensible structured report with table support",
        sr_document_type::extensible,
        false,
        true,
        true
    },

    // Key Object Selection
    {
        key_object_selection_document_storage_uid,
        "Key Object Selection Document Storage",
        "Document for marking significant images",
        sr_document_type::key_object_selection,
        false,
        false,
        false
    },

    // CAD SR types
    {
        mammography_cad_sr_storage_uid,
        "Mammography CAD SR Storage",
        "Computer-aided detection results for mammography",
        sr_document_type::cad,
        false,
        true,
        false
    },
    {
        chest_cad_sr_storage_uid,
        "Chest CAD SR Storage",
        "Computer-aided detection results for chest imaging",
        sr_document_type::cad,
        false,
        true,
        false
    },
    {
        colon_cad_sr_storage_uid,
        "Colon CAD SR Storage",
        "Computer-aided detection results for colonography",
        sr_document_type::cad,
        false,
        true,
        false
    },

    // Dose reports
    {
        xray_radiation_dose_sr_storage_uid,
        "X-Ray Radiation Dose SR Storage",
        "Radiation dose structured report for X-ray procedures",
        sr_document_type::dose_report,
        false,
        false,
        false
    },
    {
        radiopharmaceutical_radiation_dose_sr_storage_uid,
        "Radiopharmaceutical Radiation Dose SR Storage",
        "Radiation dose for nuclear medicine procedures",
        sr_document_type::dose_report,
        false,
        false,
        false
    },
    {
        patient_radiation_dose_sr_storage_uid,
        "Patient Radiation Dose SR Storage",
        "Cumulative patient radiation dose report",
        sr_document_type::dose_report,
        false,
        false,
        false
    },
    {
        enhanced_xray_radiation_dose_sr_storage_uid,
        "Enhanced X-Ray Radiation Dose SR Storage",
        "Enhanced radiation dose report with detailed exposure data",
        sr_document_type::dose_report,
        false,
        false,
        false
    },

    // Other specialized SR
    {
        acquisition_context_sr_storage_uid,
        "Acquisition Context SR Storage",
        "Acquisition context documentation",
        sr_document_type::other,
        false,
        false,
        false
    },
    {
        simplified_adult_echo_sr_storage_uid,
        "Simplified Adult Echo SR Storage",
        "Simplified echocardiography structured report",
        sr_document_type::other,
        false,
        false,
        true
    },
    {
        planned_imaging_agent_admin_sr_storage_uid,
        "Planned Imaging Agent Administration SR Storage",
        "Planned contrast agent administration record",
        sr_document_type::other,
        false,
        false,
        false
    },
    {
        performed_imaging_agent_admin_sr_storage_uid,
        "Performed Imaging Agent Administration SR Storage",
        "Performed contrast agent administration record",
        sr_document_type::other,
        false,
        false,
        false
    }
}};

}  // namespace

std::vector<std::string>
get_sr_storage_sop_classes(bool include_cad, bool include_dose) {
    std::vector<std::string> result;
    result.reserve(sr_sop_classes.size());

    for (const auto& info : sr_sop_classes) {
        bool include = true;

        if (!include_cad && info.document_type == sr_document_type::cad) {
            include = false;
        }
        if (!include_dose && info.document_type == sr_document_type::dose_report) {
            include = false;
        }

        if (include) {
            result.emplace_back(info.uid);
        }
    }

    return result;
}

const sr_sop_class_info*
get_sr_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(
        sr_sop_classes.begin(),
        sr_sop_classes.end(),
        [uid](const auto& info) { return info.uid == uid; }
    );

    if (it != sr_sop_classes.end()) {
        return &(*it);
    }
    return nullptr;
}

bool is_sr_storage_sop_class(std::string_view uid) noexcept {
    return get_sr_sop_class_info(uid) != nullptr;
}

bool is_cad_sr_storage_sop_class(std::string_view uid) noexcept {
    const auto* info = get_sr_sop_class_info(uid);
    return info != nullptr && info->document_type == sr_document_type::cad;
}

bool is_dose_sr_storage_sop_class(std::string_view uid) noexcept {
    const auto* info = get_sr_sop_class_info(uid);
    return info != nullptr && info->document_type == sr_document_type::dose_report;
}

bool sr_supports_spatial_coords(std::string_view uid) noexcept {
    const auto* info = get_sr_sop_class_info(uid);
    return info != nullptr && info->supports_spatial_coords;
}

// =============================================================================
// SR Template Identification
// =============================================================================

std::string_view get_recommended_sr_template(std::string_view uid) noexcept {
    if (uid == mammography_cad_sr_storage_uid) {
        return sr_template::mammography_cad_report;
    }
    if (uid == chest_cad_sr_storage_uid) {
        return sr_template::chest_cad_report;
    }
    if (uid == colon_cad_sr_storage_uid) {
        return sr_template::colon_cad_report;
    }
    if (uid == xray_radiation_dose_sr_storage_uid ||
        uid == enhanced_xray_radiation_dose_sr_storage_uid) {
        return sr_template::xray_radiation_dose_report;
    }
    if (uid == key_object_selection_document_storage_uid) {
        return sr_template::key_object_selection;
    }
    if (uid == basic_text_sr_storage_uid ||
        uid == enhanced_sr_storage_uid ||
        uid == comprehensive_sr_storage_uid) {
        return sr_template::basic_diagnostic_imaging_report;
    }
    return "";
}

}  // namespace pacs::services::sop_classes
