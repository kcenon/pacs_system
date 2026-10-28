// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-services-validation.cppm
 * @brief C++20 module partition for IOD validators.
 *
 * This module partition exports IOD (Information Object Definition)
 * validators for various DICOM modalities:
 * - DX: Digital X-Ray
 * - MG: Mammography
 * - NM: Nuclear Medicine
 * - PET: Positron Emission Tomography
 * - RT: Radiation Therapy (Plan, Dose, Structure Set)
 * - SEG: Segmentation (binary/fractional, heightmap, label map)
 * - SR: Structured Reporting
 * - US: Ultrasound
 * - XA: X-Ray Angiography
 * - CT and CT image processing
 * - MR: Magnetic Resonance
 * - Ophthalmic photography
 * - Parametric map
 * - Waveform presentation state
 * - WSI: Whole Slide Microscopy
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// PACS validation headers
#include <kcenon/pacs/services/validation/ct_iod_validator.h>
#include <kcenon/pacs/services/validation/ct_processing_iod_validator.h>
#include <kcenon/pacs/services/validation/dx_iod_validator.h>
#include <kcenon/pacs/services/validation/heightmap_seg_iod_validator.h>
#include <kcenon/pacs/services/validation/label_map_seg_iod_validator.h>
#include <kcenon/pacs/services/validation/mg_iod_validator.h>
#include <kcenon/pacs/services/validation/mr_iod_validator.h>
#include <kcenon/pacs/services/validation/nm_iod_validator.h>
#include <kcenon/pacs/services/validation/ophthalmic_iod_validator.h>
#include <kcenon/pacs/services/validation/parametric_map_iod_validator.h>
#include <kcenon/pacs/services/validation/pet_iod_validator.h>
#include <kcenon/pacs/services/validation/rt_iod_validator.h>
#include <kcenon/pacs/services/validation/seg_iod_validator.h>
#include <kcenon/pacs/services/validation/sr_iod_validator.h>
#include <kcenon/pacs/services/validation/us_iod_validator.h>
#include <kcenon/pacs/services/validation/waveform_ps_iod_validator.h>
#include <kcenon/pacs/services/validation/wsi_iod_validator.h>
#include <kcenon/pacs/services/validation/xa_iod_validator.h>

export module kcenon.pacs:services_validation;

// ============================================================================
// Re-export kcenon::pacs::services::validation namespace
// ============================================================================
// The per-modality tag constants (ct_tags, mr_tags, xa_tags, ...) are not
// exported; use kcenon::pacs::core::tags or the validator headers.

export namespace kcenon::pacs::services::validation {

// ==========================================================================
// Common Validation Types
// ==========================================================================

using kcenon::pacs::services::validation::validation_severity;
using kcenon::pacs::services::validation::validation_finding;
using kcenon::pacs::services::validation::validation_result;

// ==========================================================================
// Ultrasound (US) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::us_validation_options;
using kcenon::pacs::services::validation::us_iod_validator;
using kcenon::pacs::services::validation::validate_us_iod;
using kcenon::pacs::services::validation::is_valid_us_dataset;

// ==========================================================================
// Digital X-Ray (DX) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::dx_validation_options;
using kcenon::pacs::services::validation::dx_iod_validator;
using kcenon::pacs::services::validation::validate_dx_iod;
using kcenon::pacs::services::validation::is_valid_dx_dataset;
using kcenon::pacs::services::validation::is_for_presentation_dx;
using kcenon::pacs::services::validation::is_for_processing_dx;

// ==========================================================================
// Mammography (MG) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::mg_validation_options;
using kcenon::pacs::services::validation::mg_iod_validator;
using kcenon::pacs::services::validation::validate_mg_iod;
using kcenon::pacs::services::validation::is_valid_mg_dataset;
using kcenon::pacs::services::validation::is_for_presentation_mg;
using kcenon::pacs::services::validation::is_for_processing_mg;
using kcenon::pacs::services::validation::has_breast_implant;
using kcenon::pacs::services::validation::is_screening_mammogram;

// ==========================================================================
// Nuclear Medicine (NM) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::nm_validation_options;
using kcenon::pacs::services::validation::nm_iod_validator;
using kcenon::pacs::services::validation::validate_nm_iod;
using kcenon::pacs::services::validation::is_valid_nm_dataset;

// ==========================================================================
// Positron Emission Tomography (PET) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::pet_validation_options;
using kcenon::pacs::services::validation::pet_iod_validator;
using kcenon::pacs::services::validation::validate_pet_iod;
using kcenon::pacs::services::validation::is_valid_pet_dataset;

// ==========================================================================
// Radiation Therapy (RT) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::rt_validation_options;
using kcenon::pacs::services::validation::rt_plan_iod_validator;
using kcenon::pacs::services::validation::rt_dose_iod_validator;
using kcenon::pacs::services::validation::rt_structure_set_iod_validator;
using kcenon::pacs::services::validation::rt_iod_validator;
using kcenon::pacs::services::validation::validate_rt_plan_iod;
using kcenon::pacs::services::validation::validate_rt_dose_iod;
using kcenon::pacs::services::validation::validate_rt_structure_set_iod;
using kcenon::pacs::services::validation::validate_rt_iod;
using kcenon::pacs::services::validation::is_valid_rt_plan_dataset;
using kcenon::pacs::services::validation::is_valid_rt_dose_dataset;
using kcenon::pacs::services::validation::is_valid_rt_structure_set_dataset;
using kcenon::pacs::services::validation::is_valid_rt_dataset;

// ==========================================================================
// Segmentation (SEG) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::seg_validation_options;
using kcenon::pacs::services::validation::seg_iod_validator;
using kcenon::pacs::services::validation::validate_seg_iod;
using kcenon::pacs::services::validation::is_valid_seg_dataset;
using kcenon::pacs::services::validation::is_binary_segmentation;
using kcenon::pacs::services::validation::is_fractional_segmentation;
using kcenon::pacs::services::validation::get_segment_count;

// ==========================================================================
// Structured Reporting (SR) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::sr_validation_options;
using kcenon::pacs::services::validation::sr_iod_validator;
using kcenon::pacs::services::validation::validate_sr_iod;
using kcenon::pacs::services::validation::is_valid_sr_dataset;
using kcenon::pacs::services::validation::is_sr_complete;
using kcenon::pacs::services::validation::is_sr_verified;
using kcenon::pacs::services::validation::get_content_item_count;
using kcenon::pacs::services::validation::get_sr_document_title;

// ==========================================================================
// X-Ray Angiography (XA) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::xa_validation_options;
using kcenon::pacs::services::validation::xa_iod_validator;
using kcenon::pacs::services::validation::validate_xa_iod;
using kcenon::pacs::services::validation::is_valid_xa_dataset;
using kcenon::pacs::services::validation::has_qca_calibration;

// ==========================================================================
// Computed Tomography (CT) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::ct_validation_options;
using kcenon::pacs::services::validation::ct_iod_validator;
using kcenon::pacs::services::validation::validate_ct_iod;
using kcenon::pacs::services::validation::is_valid_ct_dataset;

// ==========================================================================
// CT Image Processing IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::ct_processing_validation_options;
using kcenon::pacs::services::validation::ct_processing_iod_validator;
using kcenon::pacs::services::validation::validate_ct_processing_iod;
using kcenon::pacs::services::validation::is_valid_ct_processing_dataset;

// ==========================================================================
// Magnetic Resonance (MR) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::mr_validation_options;
using kcenon::pacs::services::validation::mr_iod_validator;
using kcenon::pacs::services::validation::validate_mr_iod;
using kcenon::pacs::services::validation::is_valid_mr_dataset;

// ==========================================================================
// Heightmap Segmentation IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::heightmap_seg_validation_options;
using kcenon::pacs::services::validation::heightmap_seg_iod_validator;
using kcenon::pacs::services::validation::validate_heightmap_seg_iod;
using kcenon::pacs::services::validation::is_valid_heightmap_seg_dataset;
using kcenon::pacs::services::validation::is_heightmap_segmentation;

// ==========================================================================
// Label Map Segmentation IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::label_map_seg_validation_options;
using kcenon::pacs::services::validation::label_map_seg_iod_validator;
using kcenon::pacs::services::validation::validate_label_map_seg_iod;
using kcenon::pacs::services::validation::is_valid_label_map_seg_dataset;
using kcenon::pacs::services::validation::is_label_map_segmentation;

// ==========================================================================
// Ophthalmic Photography IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::ophthalmic_validation_options;
using kcenon::pacs::services::validation::ophthalmic_iod_validator;
using kcenon::pacs::services::validation::validate_ophthalmic_iod;
using kcenon::pacs::services::validation::is_valid_ophthalmic_dataset;

// ==========================================================================
// Parametric Map IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::pmap_validation_options;
using kcenon::pacs::services::validation::parametric_map_iod_validator;
using kcenon::pacs::services::validation::validate_parametric_map_iod;
using kcenon::pacs::services::validation::is_valid_parametric_map_dataset;

// ==========================================================================
// Waveform Presentation State IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::waveform_ps_validation_options;
using kcenon::pacs::services::validation::waveform_ps_iod_validator;

// ==========================================================================
// Whole Slide Microscopy (WSI) IOD Validator
// ==========================================================================

using kcenon::pacs::services::validation::wsi_validation_options;
using kcenon::pacs::services::validation::wsi_iod_validator;
using kcenon::pacs::services::validation::validate_wsi_iod;
using kcenon::pacs::services::validation::is_valid_wsi_dataset;

} // namespace kcenon::pacs::services::validation
