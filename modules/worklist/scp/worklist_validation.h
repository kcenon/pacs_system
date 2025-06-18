/**
 * BSD 3-Clause License
 * Copyright (c) 2024, kcenon
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "dcmtk/dcmdata/dcdatset.h"
#include "core/result/result.h"
#include <string>
#include <vector>

namespace pacs {
namespace worklist {
namespace scp {

/**
 * @brief Validation rules for worklist items
 */
class WorklistValidator {
public:
    /**
     * @brief Validate a worklist item dataset
     * @param dataset The DICOM dataset to validate
     * @return Result indicating validation success or failure
     */
    static core::Result<void> validateWorklistItem(const DcmDataset* dataset);
    
    /**
     * @brief Validate required tags for scheduled procedure step
     * @param dataset The DICOM dataset to validate
     * @return Result indicating validation success or failure
     */
    static core::Result<void> validateScheduledProcedureStep(const DcmDataset* dataset);
    
    /**
     * @brief Validate patient information completeness
     * @param dataset The DICOM dataset to validate
     * @return Result indicating validation success or failure
     */
    static core::Result<void> validatePatientInfo(const DcmDataset* dataset);

private:
    /**
     * @brief Check if required tag exists and has valid value
     * @param dataset The DICOM dataset
     * @param tag The DICOM tag to check
     * @param tagName The human-readable tag name for error messages
     * @return Result indicating presence and validity
     */
    static core::Result<void> checkRequiredTag(const DcmDataset* dataset, 
                                             const DcmTagKey& tag, 
                                             const std::string& tagName);
    
    /**
     * @brief Validate date format (YYYYMMDD)
     * @param dateString The date string to validate
     * @return True if valid date format, false otherwise
     */
    static bool isValidDate(const std::string& dateString);
    
    /**
     * @brief Validate time format (HHMMSS)
     * @param timeString The time string to validate
     * @return True if valid time format, false otherwise
     */
    static bool isValidTime(const std::string& timeString);
    
    /**
     * @brief Validate modality code
     * @param modality The modality string to validate
     * @return True if valid modality, false otherwise
     */
    static bool isValidModality(const std::string& modality);
};

} // namespace scp
} // namespace worklist
} // namespace pacs