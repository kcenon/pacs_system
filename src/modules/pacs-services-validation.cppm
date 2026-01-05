// BSD 3-Clause License
// Copyright (c) 2025, kcenon
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
 * - RT: Radiation Therapy
 * - SEG: Segmentation
 * - SR: Structured Reporting
 * - US: Ultrasound
 * - XA: X-Ray Angiography
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
#include <pacs/services/validation/dx_iod_validator.hpp>
#include <pacs/services/validation/mg_iod_validator.hpp>
#include <pacs/services/validation/nm_iod_validator.hpp>
#include <pacs/services/validation/pet_iod_validator.hpp>
#include <pacs/services/validation/rt_iod_validator.hpp>
#include <pacs/services/validation/seg_iod_validator.hpp>
#include <pacs/services/validation/sr_iod_validator.hpp>
#include <pacs/services/validation/us_iod_validator.hpp>
#include <pacs/services/validation/xa_iod_validator.hpp>

export module kcenon.pacs:services_validation;

// ============================================================================
// Re-export pacs::services::validation namespace
// ============================================================================

export namespace pacs::services::validation {

// ==========================================================================
// Common Validation Types
// ==========================================================================

// Validation result types
using pacs::services::validation::validation_result;
using pacs::services::validation::validation_issue;
using pacs::services::validation::validation_severity;

// ==========================================================================
// Ultrasound (US) IOD Validator
// ==========================================================================

using pacs::services::validation::us_validation_options;
using pacs::services::validation::us_iod_validator;

// ==========================================================================
// Digital X-Ray (DX) IOD Validator
// ==========================================================================

using pacs::services::validation::dx_validation_options;
using pacs::services::validation::dx_iod_validator;

// ==========================================================================
// Mammography (MG) IOD Validator
// ==========================================================================

using pacs::services::validation::mg_validation_options;
using pacs::services::validation::mg_iod_validator;

// ==========================================================================
// Nuclear Medicine (NM) IOD Validator
// ==========================================================================

using pacs::services::validation::nm_validation_options;
using pacs::services::validation::nm_iod_validator;

// ==========================================================================
// Positron Emission Tomography (PET) IOD Validator
// ==========================================================================

using pacs::services::validation::pet_validation_options;
using pacs::services::validation::pet_iod_validator;

// ==========================================================================
// Radiation Therapy (RT) IOD Validator
// ==========================================================================

using pacs::services::validation::rt_validation_options;
using pacs::services::validation::rt_iod_validator;

// ==========================================================================
// Segmentation (SEG) IOD Validator
// ==========================================================================

using pacs::services::validation::seg_validation_options;
using pacs::services::validation::seg_iod_validator;

// ==========================================================================
// Structured Reporting (SR) IOD Validator
// ==========================================================================

using pacs::services::validation::sr_validation_options;
using pacs::services::validation::sr_iod_validator;

// ==========================================================================
// X-Ray Angiography (XA) IOD Validator
// ==========================================================================

using pacs::services::validation::xa_validation_options;
using pacs::services::validation::xa_iod_validator;

} // namespace pacs::services::validation
