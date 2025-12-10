/**
 * @file sop_class_registry.cpp
 * @brief Implementation of SOP Class Registry
 */

#include "pacs/services/sop_class_registry.hpp"
#include "pacs/services/sop_classes/dx_storage.hpp"
#include "pacs/services/sop_classes/nm_storage.hpp"
#include "pacs/services/sop_classes/pet_storage.hpp"
#include "pacs/services/sop_classes/rt_storage.hpp"
#include "pacs/services/sop_classes/us_storage.hpp"
#include "pacs/services/sop_classes/xa_storage.hpp"

#include <algorithm>

namespace pacs::services {

// =============================================================================
// Singleton Implementation
// =============================================================================

sop_class_registry& sop_class_registry::instance() {
    static sop_class_registry instance;
    return instance;
}

sop_class_registry::sop_class_registry() {
    register_standard_sop_classes();
}

// =============================================================================
// Query Methods
// =============================================================================

bool sop_class_registry::is_supported(std::string_view uid) const {
    return registry_.find(std::string(uid)) != registry_.end();
}

const sop_class_info* sop_class_registry::get_info(std::string_view uid) const {
    auto it = registry_.find(std::string(uid));
    if (it != registry_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string>
sop_class_registry::get_by_category(sop_class_category category) const {
    std::vector<std::string> result;
    for (const auto& [uid, info] : registry_) {
        if (info.category == category) {
            result.push_back(uid);
        }
    }
    return result;
}

std::vector<std::string>
sop_class_registry::get_by_modality(modality_type modality, bool include_retired) const {
    std::vector<std::string> result;
    for (const auto& [uid, info] : registry_) {
        if (info.category == sop_class_category::storage &&
            info.modality == modality &&
            (include_retired || !info.is_retired)) {
            result.push_back(uid);
        }
    }
    return result;
}

std::vector<std::string>
sop_class_registry::get_all_storage_classes(bool include_retired) const {
    std::vector<std::string> result;
    for (const auto& [uid, info] : registry_) {
        if (info.category == sop_class_category::storage &&
            (include_retired || !info.is_retired)) {
            result.push_back(uid);
        }
    }
    return result;
}

std::vector<std::string> sop_class_registry::get_all() const {
    std::vector<std::string> result;
    result.reserve(registry_.size());
    for (const auto& [uid, info] : registry_) {
        result.push_back(uid);
    }
    return result;
}

bool sop_class_registry::register_sop_class(const sop_class_info& info) {
    auto [it, inserted] = registry_.emplace(
        std::string(info.uid),
        info
    );
    return inserted;
}

// =============================================================================
// Modality Conversion
// =============================================================================

std::string_view sop_class_registry::modality_to_string(modality_type modality) noexcept {
    switch (modality) {
        case modality_type::ct: return "CT";
        case modality_type::mr: return "MR";
        case modality_type::us: return "US";
        case modality_type::xa: return "XA";
        case modality_type::xrf: return "RF";
        case modality_type::cr: return "CR";
        case modality_type::dx: return "DX";
        case modality_type::mg: return "MG";
        case modality_type::nm: return "NM";
        case modality_type::pet: return "PT";
        case modality_type::rt: return "RT";
        case modality_type::sc: return "SC";
        case modality_type::sr: return "SR";
        case modality_type::other: return "OT";
    }
    return "OT";
}

modality_type sop_class_registry::parse_modality(std::string_view modality) noexcept {
    if (modality == "CT") return modality_type::ct;
    if (modality == "MR") return modality_type::mr;
    if (modality == "US") return modality_type::us;
    if (modality == "XA") return modality_type::xa;
    if (modality == "RF" || modality == "XRF") return modality_type::xrf;
    if (modality == "CR") return modality_type::cr;
    if (modality == "DX") return modality_type::dx;
    if (modality == "MG") return modality_type::mg;
    if (modality == "NM") return modality_type::nm;
    if (modality == "PT" || modality == "PET") return modality_type::pet;
    // RT modalities: RTPLAN, RTDOSE, RTSTRUCT, RTIMAGE, RTRECORD
    if (modality == "RT" || modality == "RTPLAN" || modality == "RTDOSE" ||
        modality == "RTSTRUCT" || modality == "RTIMAGE" || modality == "RTRECORD") {
        return modality_type::rt;
    }
    if (modality == "SC") return modality_type::sc;
    if (modality == "SR") return modality_type::sr;
    return modality_type::other;
}

// =============================================================================
// Registration Methods
// =============================================================================

void sop_class_registry::register_standard_sop_classes() {
    register_us_sop_classes();
    register_xa_sop_classes();
    register_dx_sop_classes();
    register_ct_sop_classes();
    register_mr_sop_classes();
    register_pet_sop_classes();
    register_nm_sop_classes();
    register_rt_sop_classes();
    register_other_sop_classes();
}

void sop_class_registry::register_us_sop_classes() {
    // US Image Storage
    registry_.emplace(
        std::string(sop_classes::us_image_storage_uid),
        sop_class_info{
            sop_classes::us_image_storage_uid,
            "US Image Storage",
            sop_class_category::storage,
            modality_type::us,
            false,
            false
        }
    );

    // US Multi-frame Image Storage
    registry_.emplace(
        std::string(sop_classes::us_multiframe_image_storage_uid),
        sop_class_info{
            sop_classes::us_multiframe_image_storage_uid,
            "US Multi-frame Image Storage",
            sop_class_category::storage,
            modality_type::us,
            false,
            true
        }
    );

    // US Image Storage (Retired)
    registry_.emplace(
        std::string(sop_classes::us_image_storage_retired_uid),
        sop_class_info{
            sop_classes::us_image_storage_retired_uid,
            "US Image Storage (Retired)",
            sop_class_category::storage,
            modality_type::us,
            true,
            false
        }
    );

    // US Multi-frame Image Storage (Retired)
    registry_.emplace(
        std::string(sop_classes::us_multiframe_image_storage_retired_uid),
        sop_class_info{
            sop_classes::us_multiframe_image_storage_retired_uid,
            "US Multi-frame Image Storage (Retired)",
            sop_class_category::storage,
            modality_type::us,
            true,
            true
        }
    );
}

void sop_class_registry::register_xa_sop_classes() {
    // XA Image Storage
    registry_.emplace(
        std::string(sop_classes::xa_image_storage_uid),
        sop_class_info{
            sop_classes::xa_image_storage_uid,
            "X-Ray Angiographic Image Storage",
            sop_class_category::storage,
            modality_type::xa,
            false,
            true  // supports multiframe
        }
    );

    // Enhanced XA Image Storage
    registry_.emplace(
        std::string(sop_classes::enhanced_xa_image_storage_uid),
        sop_class_info{
            sop_classes::enhanced_xa_image_storage_uid,
            "Enhanced X-Ray Angiographic Image Storage",
            sop_class_category::storage,
            modality_type::xa,
            false,
            true
        }
    );

    // XRF Image Storage
    registry_.emplace(
        std::string(sop_classes::xrf_image_storage_uid),
        sop_class_info{
            sop_classes::xrf_image_storage_uid,
            "X-Ray Radiofluoroscopic Image Storage",
            sop_class_category::storage,
            modality_type::xrf,
            false,
            true
        }
    );

    // X-Ray 3D Angiographic Image Storage
    registry_.emplace(
        std::string(sop_classes::xray_3d_angiographic_image_storage_uid),
        sop_class_info{
            sop_classes::xray_3d_angiographic_image_storage_uid,
            "X-Ray 3D Angiographic Image Storage",
            sop_class_category::storage,
            modality_type::xa,
            false,
            true
        }
    );

    // X-Ray 3D Craniofacial Image Storage
    registry_.emplace(
        std::string(sop_classes::xray_3d_craniofacial_image_storage_uid),
        sop_class_info{
            sop_classes::xray_3d_craniofacial_image_storage_uid,
            "X-Ray 3D Craniofacial Image Storage",
            sop_class_category::storage,
            modality_type::xa,
            false,
            true
        }
    );
}

void sop_class_registry::register_dx_sop_classes() {
    // Digital X-Ray Image Storage - For Presentation
    registry_.emplace(
        std::string(sop_classes::dx_image_storage_for_presentation_uid),
        sop_class_info{
            sop_classes::dx_image_storage_for_presentation_uid,
            "Digital X-Ray Image Storage - For Presentation",
            sop_class_category::storage,
            modality_type::dx,
            false,
            false
        }
    );

    // Digital X-Ray Image Storage - For Processing
    registry_.emplace(
        std::string(sop_classes::dx_image_storage_for_processing_uid),
        sop_class_info{
            sop_classes::dx_image_storage_for_processing_uid,
            "Digital X-Ray Image Storage - For Processing",
            sop_class_category::storage,
            modality_type::dx,
            false,
            false
        }
    );

    // Digital Mammography X-Ray Image Storage - For Presentation
    registry_.emplace(
        std::string(sop_classes::mammography_image_storage_for_presentation_uid),
        sop_class_info{
            sop_classes::mammography_image_storage_for_presentation_uid,
            "Digital Mammography X-Ray Image Storage - For Presentation",
            sop_class_category::storage,
            modality_type::mg,
            false,
            false
        }
    );

    // Digital Mammography X-Ray Image Storage - For Processing
    registry_.emplace(
        std::string(sop_classes::mammography_image_storage_for_processing_uid),
        sop_class_info{
            sop_classes::mammography_image_storage_for_processing_uid,
            "Digital Mammography X-Ray Image Storage - For Processing",
            sop_class_category::storage,
            modality_type::mg,
            false,
            false
        }
    );

    // Digital Intra-Oral X-Ray Image Storage - For Presentation
    registry_.emplace(
        std::string(sop_classes::intraoral_image_storage_for_presentation_uid),
        sop_class_info{
            sop_classes::intraoral_image_storage_for_presentation_uid,
            "Digital Intra-Oral X-Ray Image Storage - For Presentation",
            sop_class_category::storage,
            modality_type::dx,  // Intra-oral uses DX modality
            false,
            false
        }
    );

    // Digital Intra-Oral X-Ray Image Storage - For Processing
    registry_.emplace(
        std::string(sop_classes::intraoral_image_storage_for_processing_uid),
        sop_class_info{
            sop_classes::intraoral_image_storage_for_processing_uid,
            "Digital Intra-Oral X-Ray Image Storage - For Processing",
            sop_class_category::storage,
            modality_type::dx,  // Intra-oral uses DX modality
            false,
            false
        }
    );
}

void sop_class_registry::register_ct_sop_classes() {
    // CT Image Storage
    registry_.emplace(
        "1.2.840.10008.5.1.4.1.1.2",
        sop_class_info{
            "1.2.840.10008.5.1.4.1.1.2",
            "CT Image Storage",
            sop_class_category::storage,
            modality_type::ct,
            false,
            false
        }
    );

    // Enhanced CT Image Storage
    registry_.emplace(
        "1.2.840.10008.5.1.4.1.1.2.1",
        sop_class_info{
            "1.2.840.10008.5.1.4.1.1.2.1",
            "Enhanced CT Image Storage",
            sop_class_category::storage,
            modality_type::ct,
            false,
            true
        }
    );
}

void sop_class_registry::register_mr_sop_classes() {
    // MR Image Storage
    registry_.emplace(
        "1.2.840.10008.5.1.4.1.1.4",
        sop_class_info{
            "1.2.840.10008.5.1.4.1.1.4",
            "MR Image Storage",
            sop_class_category::storage,
            modality_type::mr,
            false,
            false
        }
    );

    // Enhanced MR Image Storage
    registry_.emplace(
        "1.2.840.10008.5.1.4.1.1.4.1",
        sop_class_info{
            "1.2.840.10008.5.1.4.1.1.4.1",
            "Enhanced MR Image Storage",
            sop_class_category::storage,
            modality_type::mr,
            false,
            true
        }
    );
}

void sop_class_registry::register_pet_sop_classes() {
    // PET Image Storage
    registry_.emplace(
        std::string(sop_classes::pet_image_storage_uid),
        sop_class_info{
            sop_classes::pet_image_storage_uid,
            "PET Image Storage",
            sop_class_category::storage,
            modality_type::pet,
            false,
            false
        }
    );

    // Enhanced PET Image Storage
    registry_.emplace(
        std::string(sop_classes::enhanced_pet_image_storage_uid),
        sop_class_info{
            sop_classes::enhanced_pet_image_storage_uid,
            "Enhanced PET Image Storage",
            sop_class_category::storage,
            modality_type::pet,
            false,
            true  // supports multiframe
        }
    );

    // Legacy Converted Enhanced PET Image Storage
    registry_.emplace(
        std::string(sop_classes::legacy_converted_enhanced_pet_image_storage_uid),
        sop_class_info{
            sop_classes::legacy_converted_enhanced_pet_image_storage_uid,
            "Legacy Converted Enhanced PET Image Storage",
            sop_class_category::storage,
            modality_type::pet,
            false,
            true  // supports multiframe
        }
    );
}

void sop_class_registry::register_nm_sop_classes() {
    // NM Image Storage
    registry_.emplace(
        std::string(sop_classes::nm_image_storage_uid),
        sop_class_info{
            sop_classes::nm_image_storage_uid,
            "NM Image Storage",
            sop_class_category::storage,
            modality_type::nm,
            false,
            true  // supports multiframe (SPECT, dynamic, gated)
        }
    );

    // NM Image Storage (Retired)
    registry_.emplace(
        std::string(sop_classes::nm_image_storage_retired_uid),
        sop_class_info{
            sop_classes::nm_image_storage_retired_uid,
            "NM Image Storage (Retired)",
            sop_class_category::storage,
            modality_type::nm,
            true,  // retired
            true   // supports multiframe
        }
    );
}

void sop_class_registry::register_rt_sop_classes() {
    // RT Plan Storage
    registry_.emplace(
        std::string(sop_classes::rt_plan_storage_uid),
        sop_class_info{
            sop_classes::rt_plan_storage_uid,
            "RT Plan Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            false  // no multiframe
        }
    );

    // RT Dose Storage
    registry_.emplace(
        std::string(sop_classes::rt_dose_storage_uid),
        sop_class_info{
            sop_classes::rt_dose_storage_uid,
            "RT Dose Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            true  // supports multiframe (dose grids)
        }
    );

    // RT Structure Set Storage
    registry_.emplace(
        std::string(sop_classes::rt_structure_set_storage_uid),
        sop_class_info{
            sop_classes::rt_structure_set_storage_uid,
            "RT Structure Set Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            false  // no multiframe
        }
    );

    // RT Image Storage
    registry_.emplace(
        std::string(sop_classes::rt_image_storage_uid),
        sop_class_info{
            sop_classes::rt_image_storage_uid,
            "RT Image Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            false  // typically single frame
        }
    );

    // RT Beams Treatment Record Storage
    registry_.emplace(
        std::string(sop_classes::rt_beams_treatment_record_storage_uid),
        sop_class_info{
            sop_classes::rt_beams_treatment_record_storage_uid,
            "RT Beams Treatment Record Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            false
        }
    );

    // RT Brachy Treatment Record Storage
    registry_.emplace(
        std::string(sop_classes::rt_brachy_treatment_record_storage_uid),
        sop_class_info{
            sop_classes::rt_brachy_treatment_record_storage_uid,
            "RT Brachy Treatment Record Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            false
        }
    );

    // RT Treatment Summary Record Storage
    registry_.emplace(
        std::string(sop_classes::rt_treatment_summary_record_storage_uid),
        sop_class_info{
            sop_classes::rt_treatment_summary_record_storage_uid,
            "RT Treatment Summary Record Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            false
        }
    );

    // RT Ion Plan Storage
    registry_.emplace(
        std::string(sop_classes::rt_ion_plan_storage_uid),
        sop_class_info{
            sop_classes::rt_ion_plan_storage_uid,
            "RT Ion Plan Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            false
        }
    );

    // RT Ion Beams Treatment Record Storage
    registry_.emplace(
        std::string(sop_classes::rt_ion_beams_treatment_record_storage_uid),
        sop_class_info{
            sop_classes::rt_ion_beams_treatment_record_storage_uid,
            "RT Ion Beams Treatment Record Storage",
            sop_class_category::storage,
            modality_type::rt,
            false,
            false
        }
    );
}

void sop_class_registry::register_other_sop_classes() {
    // CR Image Storage
    registry_.emplace(
        "1.2.840.10008.5.1.4.1.1.1",
        sop_class_info{
            "1.2.840.10008.5.1.4.1.1.1",
            "CR Image Storage",
            sop_class_category::storage,
            modality_type::cr,
            false,
            false
        }
    );

    // Note: DX SOP classes are now registered in register_dx_sop_classes()

    // Secondary Capture Image Storage
    registry_.emplace(
        "1.2.840.10008.5.1.4.1.1.7",
        sop_class_info{
            "1.2.840.10008.5.1.4.1.1.7",
            "Secondary Capture Image Storage",
            sop_class_category::storage,
            modality_type::sc,
            false,
            false
        }
    );

    // Verification SOP Class
    registry_.emplace(
        "1.2.840.10008.1.1",
        sop_class_info{
            "1.2.840.10008.1.1",
            "Verification SOP Class",
            sop_class_category::verification,
            modality_type::other,
            false,
            false
        }
    );
}

// =============================================================================
// Convenience Functions
// =============================================================================

bool is_storage_sop_class(std::string_view uid) {
    const auto* info = sop_class_registry::instance().get_info(uid);
    return info != nullptr && info->category == sop_class_category::storage;
}

modality_type get_storage_modality(std::string_view uid) {
    const auto* info = sop_class_registry::instance().get_info(uid);
    if (info != nullptr && info->category == sop_class_category::storage) {
        return info->modality;
    }
    return modality_type::other;
}

std::string_view get_sop_class_name(std::string_view uid) {
    const auto* info = sop_class_registry::instance().get_info(uid);
    if (info != nullptr) {
        return info->name;
    }
    return "Unknown";
}

}  // namespace pacs::services
