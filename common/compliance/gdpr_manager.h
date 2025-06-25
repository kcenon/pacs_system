/**
 * BSD 3-Clause License
 * Copyright (c) 2024, kcenon
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/result/result.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include <string>
#include <vector>
#include <chrono>

namespace pacs {
namespace common {
namespace compliance {

/**
 * @brief GDPR compliance manager for patient data handling
 */
class GDPRManager {
public:
    /**
     * @brief Data retention policy settings
     */
    struct RetentionPolicy {
        std::chrono::years diagnostic_images{7};     // 7 years for diagnostic images
        std::chrono::years audit_logs{6};           // 6 years for audit logs
        std::chrono::years patient_metadata{10};    // 10 years for patient metadata
        std::chrono::days consent_records{2555};    // 7 years for consent records
    };
    
    /**
     * @brief Patient consent types
     */
    enum class ConsentType {
        DataProcessing,     // General data processing
        ImageStorage,       // Medical image storage
        Research,          // Research purposes
        ThirdPartySharing, // Sharing with third parties
        Marketing          // Marketing communications
    };
    
    /**
     * @brief Initialize GDPR manager
     * @param policy Retention policy settings
     * @return Result indicating success or failure
     */
    static core::Result<void> initialize(const RetentionPolicy& policy);
    
    /**
     * @brief Record patient consent
     * @param patientId Patient identifier
     * @param consentType Type of consent
     * @param granted Whether consent was granted
     * @return Result indicating success or failure
     */
    static core::Result<void> recordConsent(const std::string& patientId, 
                                          ConsentType consentType, 
                                          bool granted);
    
    /**
     * @brief Check if patient has given consent for specific operation
     * @param patientId Patient identifier
     * @param consentType Type of consent to check
     * @return Result with consent status
     */
    static core::Result<bool> hasConsent(const std::string& patientId, 
                                        ConsentType consentType);

private:
    static RetentionPolicy policy_;
    static bool initialized_;
};

} // namespace compliance
} // namespace common
} // namespace pacs