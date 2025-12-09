/**
 * @file pet_iod_validator.cpp
 * @brief Implementation of PET Image IOD Validator
 */

#include "pacs/services/validation/pet_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/pet_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;

// PET-specific DICOM tags
namespace pet_tags {
    // PET Series Module
    constexpr dicom_tag series_type{0x0054, 0x1000};
    constexpr dicom_tag units{0x0054, 0x1001};
    constexpr dicom_tag counts_source{0x0054, 0x1002};
    constexpr dicom_tag series_date{0x0008, 0x0021};
    constexpr dicom_tag series_time{0x0008, 0x0031};

    // PET Image Module
    constexpr dicom_tag image_type{0x0008, 0x0008};
    constexpr dicom_tag frame_reference_time{0x0054, 0x1300};
    constexpr dicom_tag decay_correction{0x0054, 0x1102};
    constexpr dicom_tag decay_factor{0x0054, 0x1321};
    constexpr dicom_tag dose_calibration_factor{0x0054, 0x1322};
    constexpr dicom_tag scatter_fraction_factor{0x0054, 0x1323};
    constexpr dicom_tag rescale_intercept{0x0028, 0x1052};
    constexpr dicom_tag rescale_slope{0x0028, 0x1053};

    // Radiopharmaceutical Information
    constexpr dicom_tag radiopharmaceutical_info_sequence{0x0054, 0x0016};
    constexpr dicom_tag radiopharmaceutical{0x0018, 0x0031};
    constexpr dicom_tag radionuclide_code_sequence{0x0054, 0x0300};
    constexpr dicom_tag radionuclide_total_dose{0x0018, 0x1074};
    constexpr dicom_tag radionuclide_half_life{0x0018, 0x1075};
    constexpr dicom_tag radiopharmaceutical_start_time{0x0018, 0x1072};

    // Frame of Reference
    constexpr dicom_tag frame_of_reference_uid{0x0020, 0x0052};
    constexpr dicom_tag position_reference_indicator{0x0020, 0x1040};

    // Image Plane
    constexpr dicom_tag pixel_spacing{0x0028, 0x0030};
    constexpr dicom_tag image_orientation_patient{0x0020, 0x0037};
    constexpr dicom_tag image_position_patient{0x0020, 0x0032};
    constexpr dicom_tag slice_thickness{0x0018, 0x0050};

    // Reconstruction
    constexpr dicom_tag reconstruction_method{0x0054, 0x1103};
    constexpr dicom_tag number_of_iterations{0x0054, 0x1104};
    constexpr dicom_tag number_of_subsets{0x0054, 0x1105};

    // Corrections
    constexpr dicom_tag attenuation_correction_method{0x0054, 0x1101};
    constexpr dicom_tag scatter_correction_method{0x0054, 0x1105};
    constexpr dicom_tag randoms_correction_method{0x0054, 0x1100};
}

// =============================================================================
// pet_iod_validator Implementation
// =============================================================================

pet_iod_validator::pet_iod_validator(const pet_validation_options& options)
    : options_(options) {}

validation_result pet_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_pet_series_module(dataset, result.findings);
        validate_frame_of_reference_module(dataset, result.findings);
        validate_pet_image_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
        validate_image_plane_module(dataset, result.findings);
    }

    // PET-specific validation
    if (options_.validate_pet_specific) {
        if (options_.validate_radiopharmaceutical) {
            validate_radiopharmaceutical_info(dataset, result.findings);
        }
        validate_suv_parameters(dataset, result.findings);
        validate_reconstruction_info(dataset, result.findings);
        if (options_.validate_corrections) {
            validate_corrections(dataset, result.findings);
        }
    }

    // Check for errors
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool pet_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check only Type 1 required attributes for quick validation

    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is PT
    auto modality = dataset.get_string(tags::modality);
    if (modality != "PT") return false;

    // Frame of Reference Module Type 1
    if (!dataset.contains(pet_tags::frame_of_reference_uid)) return false;

    // Image Pixel Module Type 1
    if (!dataset.contains(tags::samples_per_pixel)) return false;
    if (!dataset.contains(tags::photometric_interpretation)) return false;
    if (!dataset.contains(tags::rows)) return false;
    if (!dataset.contains(tags::columns)) return false;
    if (!dataset.contains(tags::bits_allocated)) return false;
    if (!dataset.contains(tags::bits_stored)) return false;
    if (!dataset.contains(tags::high_bit)) return false;
    if (!dataset.contains(tags::pixel_representation)) return false;
    if (!dataset.contains(tags::pixel_data)) return false;

    // SOP Common Module Type 1
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    return true;
}

const pet_validation_options& pet_iod_validator::options() const noexcept {
    return options_;
}

void pet_iod_validator::set_options(const pet_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void pet_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Patient Module - All attributes are Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName", findings);
        check_type2_attribute(dataset, tags::patient_id, "PatientID", findings);
        check_type2_attribute(dataset, tags::patient_birth_date, "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex", findings);
    }
}

void pet_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid, "StudyInstanceUID", findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate", findings);
        check_type2_attribute(dataset, tags::study_time, "StudyTime", findings);
        check_type2_attribute(dataset, tags::referring_physician_name, "ReferringPhysicianName", findings);
        check_type2_attribute(dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number, "AccessionNumber", findings);
    }
}

void pet_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        // Special check: Modality must be "PT"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void pet_iod_validator::validate_pet_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // PET Series Module Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, pet_tags::series_type, "SeriesType", findings);
        check_type1_attribute(dataset, pet_tags::units, "Units", findings);
        check_type1_attribute(dataset, pet_tags::counts_source, "CountsSource", findings);
    }

    // Validate Units value
    if (dataset.contains(pet_tags::units)) {
        auto units = dataset.get_string(pet_tags::units);
        if (units != "CNTS" && units != "BQML" && units != "GML" &&
            units != "PROPCNTS" && units != "NONE") {
            findings.push_back({
                validation_severity::warning,
                pet_tags::units,
                "Unusual Units value for PET: " + units +
                " (expected CNTS, BQML, or GML)",
                "PT-WARN-001"
            });
        }
    }

    // Validate SeriesType
    if (dataset.contains(pet_tags::series_type)) {
        auto series_type = dataset.get_string(pet_tags::series_type);
        // SeriesType should be like "STATIC\IMAGE" or "WHOLE BODY\IMAGE"
        if (series_type.find('\\') == std::string::npos) {
            findings.push_back({
                validation_severity::info,
                pet_tags::series_type,
                "SeriesType should contain backslash separator (e.g., STATIC\\IMAGE)",
                "PT-INFO-001"
            });
        }
    }
}

void pet_iod_validator::validate_frame_of_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, pet_tags::frame_of_reference_uid,
                              "FrameOfReferenceUID", findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, pet_tags::position_reference_indicator,
                              "PositionReferenceIndicator", findings);
    }
}

void pet_iod_validator::validate_pet_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, pet_tags::image_type, "ImageType", findings);
        check_type1_attribute(dataset, pet_tags::frame_reference_time,
                              "FrameReferenceTime", findings);
    }

    // Type 1C - Decay Correction (required if Units = BQML)
    if (options_.check_conditional) {
        if (dataset.contains(pet_tags::units)) {
            auto units = dataset.get_string(pet_tags::units);
            if (units == "BQML") {
                if (!dataset.contains(pet_tags::decay_correction)) {
                    findings.push_back({
                        validation_severity::error,
                        pet_tags::decay_correction,
                        "DecayCorrection is required when Units = BQML",
                        "PT-ERR-001"
                    });
                }
            }
        }
    }

    // Validate DecayCorrection value
    if (dataset.contains(pet_tags::decay_correction)) {
        auto decay = dataset.get_string(pet_tags::decay_correction);
        if (decay != "NONE" && decay != "START" && decay != "ADMIN") {
            findings.push_back({
                validation_severity::warning,
                pet_tags::decay_correction,
                "Invalid DecayCorrection value: " + decay +
                " (expected NONE, START, or ADMIN)",
                "PT-WARN-002"
            });
        }
    }
}

void pet_iod_validator::validate_image_pixel_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::samples_per_pixel, "SamplesPerPixel", findings);
        check_type1_attribute(dataset, tags::photometric_interpretation,
                              "PhotometricInterpretation", findings);
        check_type1_attribute(dataset, tags::rows, "Rows", findings);
        check_type1_attribute(dataset, tags::columns, "Columns", findings);
        check_type1_attribute(dataset, tags::bits_allocated, "BitsAllocated", findings);
        check_type1_attribute(dataset, tags::bits_stored, "BitsStored", findings);
        check_type1_attribute(dataset, tags::high_bit, "HighBit", findings);
        check_type1_attribute(dataset, tags::pixel_representation, "PixelRepresentation", findings);
        check_type1_attribute(dataset, tags::pixel_data, "PixelData", findings);
    }

    // Validate pixel data consistency
    if (options_.validate_pixel_data) {
        check_pixel_data_consistency(dataset, findings);
    }
}

void pet_iod_validator::validate_image_plane_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, pet_tags::pixel_spacing, "PixelSpacing", findings);
        check_type1_attribute(dataset, pet_tags::image_orientation_patient,
                              "ImageOrientationPatient", findings);
        check_type1_attribute(dataset, pet_tags::image_position_patient,
                              "ImagePositionPatient", findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, pet_tags::slice_thickness, "SliceThickness", findings);
    }
}

void pet_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a PET storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_pet_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized PET Storage SOP Class: " + sop_class,
                "PT-ERR-002"
            });
        }
    }
}

// =============================================================================
// PET-Specific Validation
// =============================================================================

void pet_iod_validator::validate_radiopharmaceutical_info(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Radiopharmaceutical Information Sequence is Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, pet_tags::radiopharmaceutical_info_sequence,
                              "RadiopharmaceuticalInformationSequence", findings);
    }

    // If sequence is present, validate its contents
    if (dataset.contains(pet_tags::radiopharmaceutical_info_sequence)) {
        // For quantitative PET (BQML), dose and half-life are critical
        if (dataset.contains(pet_tags::units)) {
            auto units = dataset.get_string(pet_tags::units);
            if (units == "BQML" || units == "GML") {
                // Check for required dose information
                if (!dataset.contains(pet_tags::radionuclide_total_dose)) {
                    findings.push_back({
                        validation_severity::warning,
                        pet_tags::radionuclide_total_dose,
                        "RadionuclideTotalDose recommended for quantitative PET",
                        "PT-WARN-003"
                    });
                }
                if (!dataset.contains(pet_tags::radionuclide_half_life)) {
                    findings.push_back({
                        validation_severity::warning,
                        pet_tags::radionuclide_half_life,
                        "RadionuclideHalfLife recommended for quantitative PET",
                        "PT-WARN-004"
                    });
                }
            }
        }
    }
}

void pet_iod_validator::validate_suv_parameters(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // SUV calculation requires specific parameters
    if (dataset.contains(pet_tags::units)) {
        auto units = dataset.get_string(pet_tags::units);

        // For SUV calculation, rescale parameters are important
        if (units == "BQML" || units == "GML") {
            if (!dataset.contains(pet_tags::rescale_slope)) {
                findings.push_back({
                    validation_severity::info,
                    pet_tags::rescale_slope,
                    "RescaleSlope recommended for accurate SUV calculation",
                    "PT-INFO-002"
                });
            }
        }

        // Decay factor for quantitative analysis
        if (units == "BQML") {
            if (!dataset.contains(pet_tags::decay_factor)) {
                findings.push_back({
                    validation_severity::info,
                    pet_tags::decay_factor,
                    "DecayFactor recommended for quantitative analysis",
                    "PT-INFO-003"
                });
            }
        }
    }
}

void pet_iod_validator::validate_reconstruction_info(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Reconstruction Method is informational but important for interpretation
    if (!dataset.contains(pet_tags::reconstruction_method)) {
        findings.push_back({
            validation_severity::info,
            pet_tags::reconstruction_method,
            "ReconstructionMethod not specified - helps with interpretation",
            "PT-INFO-004"
        });
    }
}

void pet_iod_validator::validate_corrections(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Attenuation Correction Method is Type 3 but important
    if (!dataset.contains(pet_tags::attenuation_correction_method)) {
        findings.push_back({
            validation_severity::info,
            pet_tags::attenuation_correction_method,
            "AttenuationCorrectionMethod not specified",
            "PT-INFO-005"
        });
    }

    // For quantitative PET, corrections are critical
    if (dataset.contains(pet_tags::units)) {
        auto units = dataset.get_string(pet_tags::units);
        if (units == "BQML" || units == "GML") {
            // Randoms correction
            if (!dataset.contains(pet_tags::randoms_correction_method)) {
                findings.push_back({
                    validation_severity::info,
                    pet_tags::randoms_correction_method,
                    "RandomsCorrectionMethod recommended for quantitative PET",
                    "PT-INFO-006"
                });
            }
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void pet_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::error,
            tag,
            std::string("Type 1 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "PT-TYPE1-MISSING"
        });
    } else {
        // Type 1 must have a value (cannot be empty)
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back({
                validation_severity::error,
                tag,
                std::string("Type 1 attribute has empty value: ") +
                std::string(name) + " (" + tag.to_string() + ")",
                "PT-TYPE1-EMPTY"
            });
        }
    }
}

void pet_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    // Type 2 must be present but can be empty
    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::warning,
            tag,
            std::string("Type 2 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "PT-TYPE2-MISSING"
        });
    }
}

void pet_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "PT") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'PT' for PET images, found: " + modality,
            "PT-ERR-003"
        });
    }
}

void pet_iod_validator::check_pixel_data_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Check BitsStored <= BitsAllocated
    auto bits_allocated = dataset.get_numeric<uint16_t>(tags::bits_allocated);
    auto bits_stored = dataset.get_numeric<uint16_t>(tags::bits_stored);
    auto high_bit = dataset.get_numeric<uint16_t>(tags::high_bit);

    if (bits_allocated && bits_stored) {
        if (*bits_stored > *bits_allocated) {
            findings.push_back({
                validation_severity::error,
                tags::bits_stored,
                "BitsStored (" + std::to_string(*bits_stored) +
                ") exceeds BitsAllocated (" + std::to_string(*bits_allocated) + ")",
                "PT-ERR-004"
            });
        }
    }

    // Check HighBit == BitsStored - 1
    if (bits_stored && high_bit) {
        if (*high_bit != *bits_stored - 1) {
            findings.push_back({
                validation_severity::warning,
                tags::high_bit,
                "HighBit (" + std::to_string(*high_bit) +
                ") should typically be BitsStored - 1 (" +
                std::to_string(*bits_stored - 1) + ")",
                "PT-WARN-005"
            });
        }
    }

    // Check photometric interpretation is valid for PET
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric = dataset.get_string(tags::photometric_interpretation);
        if (!sop_classes::is_valid_pet_photometric(photometric)) {
            findings.push_back({
                validation_severity::warning,
                tags::photometric_interpretation,
                "Unusual photometric interpretation for PET: " + photometric +
                " (expected MONOCHROME2)",
                "PT-WARN-006"
            });
        }
    }

    // PET images should be grayscale (SamplesPerPixel = 1)
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::warning,
            tags::samples_per_pixel,
            "PET images should have SamplesPerPixel = 1, found: " +
            std::to_string(*samples),
            "PT-WARN-007"
        });
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_pet_iod(const dicom_dataset& dataset) {
    pet_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_pet_dataset(const dicom_dataset& dataset) {
    pet_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace pacs::services::validation
