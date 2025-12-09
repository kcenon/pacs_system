/**
 * @file rt_storage.cpp
 * @brief Implementation of Radiation Therapy Storage SOP Classes
 */

#include "pacs/services/sop_classes/rt_storage.hpp"

#include <algorithm>
#include <array>

namespace pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_rt_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // JPEG Lossless (for RT Image and RT Dose with pixel data)
        "1.2.840.10008.1.2.4.70",
        // JPEG 2000 Lossless (better compression for dose grids)
        "1.2.840.10008.1.2.4.90",
        // RLE Lossless
        "1.2.840.10008.1.2.5"
    };
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

// Static array of RT SOP class information
constexpr std::array<rt_sop_class_info, 10> rt_sop_classes = {{
    {
        rt_plan_storage_uid,
        "RT Plan Storage",
        "Radiation therapy treatment plan including beams and fractions",
        false,
        false   // no pixel data
    },
    {
        rt_dose_storage_uid,
        "RT Dose Storage",
        "Radiation dose distribution (dose grid)",
        false,
        true    // has pixel data (dose grid)
    },
    {
        rt_structure_set_storage_uid,
        "RT Structure Set Storage",
        "Radiation therapy structure set (ROI contours)",
        false,
        false   // no pixel data
    },
    {
        rt_image_storage_uid,
        "RT Image Storage",
        "Radiation therapy images (portal, DRR, etc.)",
        false,
        true    // has pixel data
    },
    {
        rt_beams_treatment_record_storage_uid,
        "RT Beams Treatment Record Storage",
        "Treatment delivery record for external beam therapy",
        false,
        false   // no pixel data
    },
    {
        rt_brachy_treatment_record_storage_uid,
        "RT Brachy Treatment Record Storage",
        "Treatment delivery record for brachytherapy",
        false,
        false   // no pixel data
    },
    {
        rt_treatment_summary_record_storage_uid,
        "RT Treatment Summary Record Storage",
        "Summary of treatment delivery",
        false,
        false   // no pixel data
    },
    {
        rt_ion_plan_storage_uid,
        "RT Ion Plan Storage",
        "Ion beam (proton, carbon) treatment plan",
        false,
        false   // no pixel data
    },
    {
        rt_ion_beams_treatment_record_storage_uid,
        "RT Ion Beams Treatment Record Storage",
        "Treatment delivery record for ion beam therapy",
        false,
        false   // no pixel data
    },
    // Additional slot for future RT SOP classes
    {
        "",
        "",
        "",
        true,
        false
    }
}};

// Number of valid RT SOP classes (excluding placeholder)
constexpr size_t rt_sop_class_count = 9;

}  // namespace

std::vector<std::string> get_rt_storage_sop_classes(bool include_retired) {
    std::vector<std::string> result;
    result.reserve(rt_sop_class_count);

    for (size_t i = 0; i < rt_sop_class_count; ++i) {
        const auto& info = rt_sop_classes[i];
        if (!info.is_retired || include_retired) {
            result.emplace_back(info.uid);
        }
    }

    return result;
}

const rt_sop_class_info*
get_rt_sop_class_info(std::string_view uid) noexcept {
    for (size_t i = 0; i < rt_sop_class_count; ++i) {
        if (rt_sop_classes[i].uid == uid) {
            return &rt_sop_classes[i];
        }
    }
    return nullptr;
}

bool is_rt_storage_sop_class(std::string_view uid) noexcept {
    return get_rt_sop_class_info(uid) != nullptr;
}

bool is_rt_plan_sop_class(std::string_view uid) noexcept {
    return uid == rt_plan_storage_uid || uid == rt_ion_plan_storage_uid;
}

bool rt_sop_class_has_pixel_data(std::string_view uid) noexcept {
    const auto* info = get_rt_sop_class_info(uid);
    return info != nullptr && info->has_pixel_data;
}

// =============================================================================
// RT Plan Intent Conversion
// =============================================================================

std::string_view to_string(rt_plan_intent intent) noexcept {
    switch (intent) {
        case rt_plan_intent::curative:
            return "CURATIVE";
        case rt_plan_intent::palliative:
            return "PALLIATIVE";
        case rt_plan_intent::prophylactic:
            return "PROPHYLACTIC";
        case rt_plan_intent::verification:
            return "VERIFICATION";
        case rt_plan_intent::machine_qa:
            return "MACHINE_QA";
        case rt_plan_intent::research:
            return "RESEARCH";
        case rt_plan_intent::service:
            return "SERVICE";
    }
    return "CURATIVE";
}

rt_plan_intent parse_rt_plan_intent(std::string_view value) noexcept {
    if (value == "CURATIVE") {
        return rt_plan_intent::curative;
    }
    if (value == "PALLIATIVE") {
        return rt_plan_intent::palliative;
    }
    if (value == "PROPHYLACTIC") {
        return rt_plan_intent::prophylactic;
    }
    if (value == "VERIFICATION") {
        return rt_plan_intent::verification;
    }
    if (value == "MACHINE_QA") {
        return rt_plan_intent::machine_qa;
    }
    if (value == "RESEARCH") {
        return rt_plan_intent::research;
    }
    if (value == "SERVICE") {
        return rt_plan_intent::service;
    }
    return rt_plan_intent::curative;
}

// =============================================================================
// RT Plan Geometry Conversion
// =============================================================================

std::string_view to_string(rt_plan_geometry geometry) noexcept {
    switch (geometry) {
        case rt_plan_geometry::patient:
            return "PATIENT";
        case rt_plan_geometry::treatment_device:
            return "TREATMENT_DEVICE";
    }
    return "PATIENT";
}

rt_plan_geometry parse_rt_plan_geometry(std::string_view value) noexcept {
    if (value == "TREATMENT_DEVICE") {
        return rt_plan_geometry::treatment_device;
    }
    return rt_plan_geometry::patient;
}

// =============================================================================
// RT Dose Type Conversion
// =============================================================================

std::string_view to_string(rt_dose_type type) noexcept {
    switch (type) {
        case rt_dose_type::physical:
            return "PHYSICAL";
        case rt_dose_type::effective:
            return "EFFECTIVE";
        case rt_dose_type::error:
            return "ERROR";
    }
    return "PHYSICAL";
}

rt_dose_type parse_rt_dose_type(std::string_view value) noexcept {
    if (value == "EFFECTIVE") {
        return rt_dose_type::effective;
    }
    if (value == "ERROR") {
        return rt_dose_type::error;
    }
    return rt_dose_type::physical;
}

// =============================================================================
// RT Dose Summation Type Conversion
// =============================================================================

std::string_view to_string(rt_dose_summation_type type) noexcept {
    switch (type) {
        case rt_dose_summation_type::plan:
            return "PLAN";
        case rt_dose_summation_type::multi_plan:
            return "MULTI_PLAN";
        case rt_dose_summation_type::fraction:
            return "FRACTION";
        case rt_dose_summation_type::beam:
            return "BEAM";
        case rt_dose_summation_type::brachy:
            return "BRACHY";
        case rt_dose_summation_type::fraction_session:
            return "FRACTION_SESSION";
        case rt_dose_summation_type::beam_session:
            return "BEAM_SESSION";
        case rt_dose_summation_type::brachy_session:
            return "BRACHY_SESSION";
        case rt_dose_summation_type::control_point:
            return "CONTROL_POINT";
        case rt_dose_summation_type::record:
            return "RECORD";
    }
    return "PLAN";
}

rt_dose_summation_type parse_rt_dose_summation_type(std::string_view value) noexcept {
    if (value == "PLAN") {
        return rt_dose_summation_type::plan;
    }
    if (value == "MULTI_PLAN") {
        return rt_dose_summation_type::multi_plan;
    }
    if (value == "FRACTION") {
        return rt_dose_summation_type::fraction;
    }
    if (value == "BEAM") {
        return rt_dose_summation_type::beam;
    }
    if (value == "BRACHY") {
        return rt_dose_summation_type::brachy;
    }
    if (value == "FRACTION_SESSION") {
        return rt_dose_summation_type::fraction_session;
    }
    if (value == "BEAM_SESSION") {
        return rt_dose_summation_type::beam_session;
    }
    if (value == "BRACHY_SESSION") {
        return rt_dose_summation_type::brachy_session;
    }
    if (value == "CONTROL_POINT") {
        return rt_dose_summation_type::control_point;
    }
    if (value == "RECORD") {
        return rt_dose_summation_type::record;
    }
    return rt_dose_summation_type::plan;
}

// =============================================================================
// RT Dose Units Conversion
// =============================================================================

std::string_view to_string(rt_dose_units units) noexcept {
    switch (units) {
        case rt_dose_units::gy:
            return "GY";
        case rt_dose_units::relative:
            return "RELATIVE";
    }
    return "GY";
}

rt_dose_units parse_rt_dose_units(std::string_view value) noexcept {
    if (value == "RELATIVE") {
        return rt_dose_units::relative;
    }
    return rt_dose_units::gy;
}

// =============================================================================
// RT ROI Interpreted Type Conversion
// =============================================================================

std::string_view to_string(rt_roi_interpreted_type type) noexcept {
    switch (type) {
        case rt_roi_interpreted_type::external:
            return "EXTERNAL";
        case rt_roi_interpreted_type::ptv:
            return "PTV";
        case rt_roi_interpreted_type::ctv:
            return "CTV";
        case rt_roi_interpreted_type::gtv:
            return "GTV";
        case rt_roi_interpreted_type::organ:
            return "ORGAN";
        case rt_roi_interpreted_type::avoidance:
            return "AVOIDANCE";
        case rt_roi_interpreted_type::treated_volume:
            return "TREATED_VOLUME";
        case rt_roi_interpreted_type::irrad_volume:
            return "IRRAD_VOLUME";
        case rt_roi_interpreted_type::bolus:
            return "BOLUS";
        case rt_roi_interpreted_type::brachy_channel:
            return "BRACHY_CHANNEL";
        case rt_roi_interpreted_type::brachy_accessory:
            return "BRACHY_ACCESSORY";
        case rt_roi_interpreted_type::brachy_src_appl:
            return "BRACHY_SRC_APPL";
        case rt_roi_interpreted_type::brachy_chnl_shld:
            return "BRACHY_CHNL_SHLD";
        case rt_roi_interpreted_type::support:
            return "SUPPORT";
        case rt_roi_interpreted_type::fixation:
            return "FIXATION";
        case rt_roi_interpreted_type::dose_region:
            return "DOSE_REGION";
        case rt_roi_interpreted_type::contrast_agent:
            return "CONTRAST_AGENT";
        case rt_roi_interpreted_type::cavity:
            return "CAVITY";
        case rt_roi_interpreted_type::marker:
            return "MARKER";
        case rt_roi_interpreted_type::registration:
            return "REGISTRATION";
        case rt_roi_interpreted_type::isocenter:
            return "ISOCENTER";
        case rt_roi_interpreted_type::control_point:
            return "CONTROL";
    }
    return "ORGAN";
}

rt_roi_interpreted_type parse_rt_roi_interpreted_type(std::string_view value) noexcept {
    if (value == "EXTERNAL") {
        return rt_roi_interpreted_type::external;
    }
    if (value == "PTV") {
        return rt_roi_interpreted_type::ptv;
    }
    if (value == "CTV") {
        return rt_roi_interpreted_type::ctv;
    }
    if (value == "GTV") {
        return rt_roi_interpreted_type::gtv;
    }
    if (value == "ORGAN") {
        return rt_roi_interpreted_type::organ;
    }
    if (value == "AVOIDANCE") {
        return rt_roi_interpreted_type::avoidance;
    }
    if (value == "TREATED_VOLUME") {
        return rt_roi_interpreted_type::treated_volume;
    }
    if (value == "IRRAD_VOLUME") {
        return rt_roi_interpreted_type::irrad_volume;
    }
    if (value == "BOLUS") {
        return rt_roi_interpreted_type::bolus;
    }
    if (value == "BRACHY_CHANNEL") {
        return rt_roi_interpreted_type::brachy_channel;
    }
    if (value == "BRACHY_ACCESSORY") {
        return rt_roi_interpreted_type::brachy_accessory;
    }
    if (value == "BRACHY_SRC_APPL") {
        return rt_roi_interpreted_type::brachy_src_appl;
    }
    if (value == "BRACHY_CHNL_SHLD") {
        return rt_roi_interpreted_type::brachy_chnl_shld;
    }
    if (value == "SUPPORT") {
        return rt_roi_interpreted_type::support;
    }
    if (value == "FIXATION") {
        return rt_roi_interpreted_type::fixation;
    }
    if (value == "DOSE_REGION") {
        return rt_roi_interpreted_type::dose_region;
    }
    if (value == "CONTRAST_AGENT") {
        return rt_roi_interpreted_type::contrast_agent;
    }
    if (value == "CAVITY") {
        return rt_roi_interpreted_type::cavity;
    }
    if (value == "MARKER") {
        return rt_roi_interpreted_type::marker;
    }
    if (value == "REGISTRATION") {
        return rt_roi_interpreted_type::registration;
    }
    if (value == "ISOCENTER") {
        return rt_roi_interpreted_type::isocenter;
    }
    if (value == "CONTROL") {
        return rt_roi_interpreted_type::control_point;
    }
    return rt_roi_interpreted_type::organ;
}

// =============================================================================
// RT ROI Generation Algorithm Conversion
// =============================================================================

std::string_view to_string(rt_roi_generation_algorithm algorithm) noexcept {
    switch (algorithm) {
        case rt_roi_generation_algorithm::automatic:
            return "AUTOMATIC";
        case rt_roi_generation_algorithm::semiautomatic:
            return "SEMIAUTOMATIC";
        case rt_roi_generation_algorithm::manual:
            return "MANUAL";
    }
    return "MANUAL";
}

rt_roi_generation_algorithm
parse_rt_roi_generation_algorithm(std::string_view value) noexcept {
    if (value == "AUTOMATIC") {
        return rt_roi_generation_algorithm::automatic;
    }
    if (value == "SEMIAUTOMATIC") {
        return rt_roi_generation_algorithm::semiautomatic;
    }
    return rt_roi_generation_algorithm::manual;
}

// =============================================================================
// RT Beam Type Conversion
// =============================================================================

std::string_view to_string(rt_beam_type type) noexcept {
    switch (type) {
        case rt_beam_type::static_beam:
            return "STATIC";
        case rt_beam_type::dynamic:
            return "DYNAMIC";
    }
    return "STATIC";
}

rt_beam_type parse_rt_beam_type(std::string_view value) noexcept {
    if (value == "DYNAMIC") {
        return rt_beam_type::dynamic;
    }
    return rt_beam_type::static_beam;
}

// =============================================================================
// RT Radiation Type Conversion
// =============================================================================

std::string_view to_string(rt_radiation_type type) noexcept {
    switch (type) {
        case rt_radiation_type::photon:
            return "PHOTON";
        case rt_radiation_type::electron:
            return "ELECTRON";
        case rt_radiation_type::neutron:
            return "NEUTRON";
        case rt_radiation_type::proton:
            return "PROTON";
        case rt_radiation_type::ion:
            return "ION";
    }
    return "PHOTON";
}

rt_radiation_type parse_rt_radiation_type(std::string_view value) noexcept {
    if (value == "PHOTON") {
        return rt_radiation_type::photon;
    }
    if (value == "ELECTRON") {
        return rt_radiation_type::electron;
    }
    if (value == "NEUTRON") {
        return rt_radiation_type::neutron;
    }
    if (value == "PROTON") {
        return rt_radiation_type::proton;
    }
    if (value == "ION") {
        return rt_radiation_type::ion;
    }
    return rt_radiation_type::photon;
}

// =============================================================================
// RT Treatment Delivery Type Conversion
// =============================================================================

std::string_view to_string(rt_treatment_delivery_type type) noexcept {
    switch (type) {
        case rt_treatment_delivery_type::treatment:
            return "TREATMENT";
        case rt_treatment_delivery_type::open_portfilm:
            return "OPEN_PORTFILM";
        case rt_treatment_delivery_type::trmt_portfilm:
            return "TRMT_PORTFILM";
        case rt_treatment_delivery_type::continuation:
            return "CONTINUATION";
        case rt_treatment_delivery_type::setup:
            return "SETUP";
    }
    return "TREATMENT";
}

rt_treatment_delivery_type
parse_rt_treatment_delivery_type(std::string_view value) noexcept {
    if (value == "TREATMENT") {
        return rt_treatment_delivery_type::treatment;
    }
    if (value == "OPEN_PORTFILM") {
        return rt_treatment_delivery_type::open_portfilm;
    }
    if (value == "TRMT_PORTFILM") {
        return rt_treatment_delivery_type::trmt_portfilm;
    }
    if (value == "CONTINUATION") {
        return rt_treatment_delivery_type::continuation;
    }
    if (value == "SETUP") {
        return rt_treatment_delivery_type::setup;
    }
    return rt_treatment_delivery_type::treatment;
}

// =============================================================================
// RT Image Plane Conversion
// =============================================================================

std::string_view to_string(rt_image_plane plane) noexcept {
    switch (plane) {
        case rt_image_plane::axial:
            return "AXIAL";
        case rt_image_plane::localizer:
            return "LOCALIZER";
        case rt_image_plane::drr:
            return "DRR";
        case rt_image_plane::portal:
            return "PORTAL";
        case rt_image_plane::fluence:
            return "FLUENCE";
    }
    return "PORTAL";
}

rt_image_plane parse_rt_image_plane(std::string_view value) noexcept {
    if (value == "AXIAL") {
        return rt_image_plane::axial;
    }
    if (value == "LOCALIZER") {
        return rt_image_plane::localizer;
    }
    if (value == "DRR") {
        return rt_image_plane::drr;
    }
    if (value == "PORTAL") {
        return rt_image_plane::portal;
    }
    if (value == "FLUENCE") {
        return rt_image_plane::fluence;
    }
    return rt_image_plane::portal;
}

}  // namespace pacs::services::sop_classes
