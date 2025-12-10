/**
 * @file rt_storage.hpp
 * @brief Radiation Therapy (RT) Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities for Radiation Therapy (RT)
 * object storage including RT Plan, RT Dose, RT Structure Set, RT Image,
 * RT Beams Treatment Record, and related objects.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.19-A.29 - RT IODs
 */

#ifndef PACS_SERVICES_SOP_CLASSES_RT_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_RT_STORAGE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// RT Storage SOP Class UIDs
// =============================================================================

/// @name Primary RT SOP Classes
/// @{

/// RT Plan Storage SOP Class UID
inline constexpr std::string_view rt_plan_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.5";

/// RT Dose Storage SOP Class UID
inline constexpr std::string_view rt_dose_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.2";

/// RT Structure Set Storage SOP Class UID
inline constexpr std::string_view rt_structure_set_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.3";

/// RT Image Storage SOP Class UID
inline constexpr std::string_view rt_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.1";

/// RT Beams Treatment Record Storage SOP Class UID
inline constexpr std::string_view rt_beams_treatment_record_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.4";

/// RT Brachy Treatment Record Storage SOP Class UID
inline constexpr std::string_view rt_brachy_treatment_record_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.6";

/// RT Treatment Summary Record Storage SOP Class UID
inline constexpr std::string_view rt_treatment_summary_record_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.7";

/// RT Ion Plan Storage SOP Class UID
inline constexpr std::string_view rt_ion_plan_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.8";

/// RT Ion Beams Treatment Record Storage SOP Class UID
inline constexpr std::string_view rt_ion_beams_treatment_record_storage_uid =
    "1.2.840.10008.5.1.4.1.1.481.9";

/// @}

// =============================================================================
// RT-Specific Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for RT objects
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * radiation therapy object storage. RT objects typically don't
 * contain pixel data (except RT Image and RT Dose), so compression
 * is less critical.
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_rt_transfer_syntaxes();

// =============================================================================
// RT SOP Class Information
// =============================================================================

/**
 * @brief Information about an RT Storage SOP Class
 */
struct rt_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    bool is_retired;                ///< Whether this SOP class is retired
    bool has_pixel_data;            ///< Whether this SOP class contains pixel data
};

/**
 * @brief Get all RT Storage SOP Class UIDs
 *
 * Returns all RT Storage SOP Class UIDs for comprehensive
 * radiation therapy support.
 *
 * @param include_retired Include retired SOP classes (default: true)
 * @return Vector of RT Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_rt_storage_sop_classes(bool include_retired = true);

/**
 * @brief Get information about a specific RT SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to SOP class info, or nullptr if not an RT storage class
 */
[[nodiscard]] const rt_sop_class_info*
get_rt_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is an RT Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any RT storage SOP class
 */
[[nodiscard]] bool is_rt_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is an RT Plan type
 *
 * @param uid The SOP Class UID to check
 * @return true if this is RT Plan or RT Ion Plan
 */
[[nodiscard]] bool is_rt_plan_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID contains pixel data
 *
 * @param uid The SOP Class UID to check
 * @return true if this RT SOP class contains pixel data
 */
[[nodiscard]] bool rt_sop_class_has_pixel_data(std::string_view uid) noexcept;

// =============================================================================
// RT Plan Type
// =============================================================================

/**
 * @brief RT Plan Intent
 */
enum class rt_plan_intent {
    curative,           ///< CURATIVE - Treatment with curative intent
    palliative,         ///< PALLIATIVE - Treatment for symptom relief
    prophylactic,       ///< PROPHYLACTIC - Preventive treatment
    verification,       ///< VERIFICATION - Plan verification
    machine_qa,         ///< MACHINE_QA - Machine quality assurance
    research,           ///< RESEARCH - Research protocol
    service             ///< SERVICE - Equipment service
};

/**
 * @brief Convert RT plan intent to DICOM string
 * @param intent The plan intent
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_plan_intent intent) noexcept;

/**
 * @brief Parse RT plan intent from DICOM string
 * @param value The DICOM string value
 * @return The plan intent
 */
[[nodiscard]] rt_plan_intent parse_rt_plan_intent(std::string_view value) noexcept;

/**
 * @brief RT Plan Geometry
 */
enum class rt_plan_geometry {
    patient,            ///< PATIENT - Patient-based plan
    treatment_device    ///< TREATMENT_DEVICE - Device-based plan
};

/**
 * @brief Convert RT plan geometry to DICOM string
 * @param geometry The plan geometry
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_plan_geometry geometry) noexcept;

/**
 * @brief Parse RT plan geometry from DICOM string
 * @param value The DICOM string value
 * @return The plan geometry
 */
[[nodiscard]] rt_plan_geometry parse_rt_plan_geometry(std::string_view value) noexcept;

// =============================================================================
// RT Dose Type
// =============================================================================

/**
 * @brief RT Dose Type
 */
enum class rt_dose_type {
    physical,           ///< PHYSICAL - Physical dose
    effective,          ///< EFFECTIVE - Effective dose (RBE weighted)
    error               ///< ERROR - Dose error/uncertainty
};

/**
 * @brief Convert RT dose type to DICOM string
 * @param type The dose type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_dose_type type) noexcept;

/**
 * @brief Parse RT dose type from DICOM string
 * @param value The DICOM string value
 * @return The dose type
 */
[[nodiscard]] rt_dose_type parse_rt_dose_type(std::string_view value) noexcept;

/**
 * @brief RT Dose Summation Type
 */
enum class rt_dose_summation_type {
    plan,               ///< PLAN - Single plan dose
    multi_plan,         ///< MULTI_PLAN - Multi-plan sum
    fraction,           ///< FRACTION - Single fraction dose
    beam,               ///< BEAM - Single beam dose
    brachy,             ///< BRACHY - Brachytherapy dose
    fraction_session,   ///< FRACTION_SESSION - Single fraction session dose
    beam_session,       ///< BEAM_SESSION - Single beam session dose
    brachy_session,     ///< BRACHY_SESSION - Brachytherapy session dose
    control_point,      ///< CONTROL_POINT - Single control point dose
    record              ///< RECORD - Treatment record dose
};

/**
 * @brief Convert RT dose summation type to DICOM string
 * @param type The dose summation type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_dose_summation_type type) noexcept;

/**
 * @brief Parse RT dose summation type from DICOM string
 * @param value The DICOM string value
 * @return The dose summation type
 */
[[nodiscard]] rt_dose_summation_type parse_rt_dose_summation_type(std::string_view value) noexcept;

/**
 * @brief RT Dose Units
 */
enum class rt_dose_units {
    gy,                 ///< GY - Gray (absorbed dose)
    relative            ///< RELATIVE - Relative dose
};

/**
 * @brief Convert RT dose units to DICOM string
 * @param units The dose units
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_dose_units units) noexcept;

/**
 * @brief Parse RT dose units from DICOM string
 * @param value The DICOM string value
 * @return The dose units
 */
[[nodiscard]] rt_dose_units parse_rt_dose_units(std::string_view value) noexcept;

// =============================================================================
// RT Structure Set
// =============================================================================

/**
 * @brief RT ROI Interpreted Type
 */
enum class rt_roi_interpreted_type {
    external,           ///< EXTERNAL - External patient surface
    ptv,                ///< PTV - Planning Target Volume
    ctv,                ///< CTV - Clinical Target Volume
    gtv,                ///< GTV - Gross Tumor Volume
    organ,              ///< ORGAN - Organ at risk
    avoidance,          ///< AVOIDANCE - Avoidance structure
    treated_volume,     ///< TREATED_VOLUME - Treated volume
    irrad_volume,       ///< IRRAD_VOLUME - Irradiated volume
    bolus,              ///< BOLUS - Bolus material
    brachy_channel,     ///< BRACHY_CHANNEL - Brachytherapy channel
    brachy_accessory,   ///< BRACHY_ACCESSORY - Brachytherapy accessory
    brachy_src_appl,    ///< BRACHY_SRC_APPL - Brachytherapy source applicator
    brachy_chnl_shld,   ///< BRACHY_CHNL_SHLD - Brachytherapy channel shield
    support,            ///< SUPPORT - Patient support structure
    fixation,           ///< FIXATION - Patient fixation device
    dose_region,        ///< DOSE_REGION - Dose reference region
    contrast_agent,     ///< CONTRAST_AGENT - Contrast agent region
    cavity,             ///< CAVITY - Cavity structure
    marker,             ///< MARKER - Marker structure
    registration,       ///< REGISTRATION - Registration structure
    isocenter,          ///< ISOCENTER - Isocenter point
    control_point       ///< CONTROL - Control point marker
};

/**
 * @brief Convert RT ROI interpreted type to DICOM string
 * @param type The ROI interpreted type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_roi_interpreted_type type) noexcept;

/**
 * @brief Parse RT ROI interpreted type from DICOM string
 * @param value The DICOM string value
 * @return The ROI interpreted type
 */
[[nodiscard]] rt_roi_interpreted_type parse_rt_roi_interpreted_type(std::string_view value) noexcept;

/**
 * @brief RT ROI Generation Algorithm
 */
enum class rt_roi_generation_algorithm {
    automatic,          ///< AUTOMATIC - Automated segmentation
    semiautomatic,      ///< SEMIAUTOMATIC - Semi-automated with user input
    manual              ///< MANUAL - Manual contouring
};

/**
 * @brief Convert RT ROI generation algorithm to DICOM string
 * @param algorithm The generation algorithm
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_roi_generation_algorithm algorithm) noexcept;

/**
 * @brief Parse RT ROI generation algorithm from DICOM string
 * @param value The DICOM string value
 * @return The generation algorithm
 */
[[nodiscard]] rt_roi_generation_algorithm
parse_rt_roi_generation_algorithm(std::string_view value) noexcept;

// =============================================================================
// RT Beam Information
// =============================================================================

/**
 * @brief RT Beam Type
 */
enum class rt_beam_type {
    static_beam,        ///< STATIC - Static beam
    dynamic             ///< DYNAMIC - Dynamic beam (IMRT, VMAT)
};

/**
 * @brief Convert RT beam type to DICOM string
 * @param type The beam type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_beam_type type) noexcept;

/**
 * @brief Parse RT beam type from DICOM string
 * @param value The DICOM string value
 * @return The beam type
 */
[[nodiscard]] rt_beam_type parse_rt_beam_type(std::string_view value) noexcept;

/**
 * @brief RT Radiation Type
 */
enum class rt_radiation_type {
    photon,             ///< PHOTON - X-ray photons
    electron,           ///< ELECTRON - Electrons
    neutron,            ///< NEUTRON - Neutrons
    proton,             ///< PROTON - Protons
    ion                 ///< ION - Heavy ions (carbon, etc.)
};

/**
 * @brief Convert RT radiation type to DICOM string
 * @param type The radiation type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_radiation_type type) noexcept;

/**
 * @brief Parse RT radiation type from DICOM string
 * @param value The DICOM string value
 * @return The radiation type
 */
[[nodiscard]] rt_radiation_type parse_rt_radiation_type(std::string_view value) noexcept;

/**
 * @brief RT Treatment Delivery Type
 */
enum class rt_treatment_delivery_type {
    treatment,          ///< TREATMENT - Actual treatment
    open_portfilm,      ///< OPEN_PORTFILM - Open field portal image
    trmt_portfilm,      ///< TRMT_PORTFILM - Treatment field portal image
    continuation,       ///< CONTINUATION - Continuation of interrupted treatment
    setup               ///< SETUP - Setup verification
};

/**
 * @brief Convert RT treatment delivery type to DICOM string
 * @param type The treatment delivery type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_treatment_delivery_type type) noexcept;

/**
 * @brief Parse RT treatment delivery type from DICOM string
 * @param value The DICOM string value
 * @return The treatment delivery type
 */
[[nodiscard]] rt_treatment_delivery_type
parse_rt_treatment_delivery_type(std::string_view value) noexcept;

// =============================================================================
// RT Image Information
// =============================================================================

/**
 * @brief RT Image Type values (as used in Image Type attribute)
 */
enum class rt_image_plane {
    axial,              ///< AXIAL - Axial plane
    localizer,          ///< LOCALIZER - Localizer/scout image
    drr,                ///< DRR - Digitally Reconstructed Radiograph
    portal,             ///< PORTAL - Portal image
    fluence             ///< FLUENCE - Fluence map
};

/**
 * @brief Convert RT image plane to DICOM string
 * @param plane The image plane type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(rt_image_plane plane) noexcept;

/**
 * @brief Parse RT image plane from DICOM string
 * @param value The DICOM string value
 * @return The image plane type
 */
[[nodiscard]] rt_image_plane parse_rt_image_plane(std::string_view value) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_RT_STORAGE_HPP
