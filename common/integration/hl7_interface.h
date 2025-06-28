/**
 * BSD 3-Clause License
 * Copyright (c) 2024, kcenon
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/result/result.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace pacs {
namespace common {
namespace integration {

/**
 * @brief HL7 v2.x message processor for hospital integration
 */
class HL7Interface {
public:
    /**
     * @brief HL7 message types
     */
    enum class MessageType {
        ADT_A01,  // Admit patient
        ADT_A02,  // Transfer patient
        ADT_A03,  // Discharge patient
        ADT_A04,  // Register patient
        ADT_A08,  // Update patient information
        ORM_O01,  // Order message
        ORU_R01,  // Observation result
        ACK,      // Acknowledgment
        QRY_A19,  // Query
        QCN_J01   // Cancel query
    };
    
    /**
     * @brief Patient information from ADT message
     */
    struct PatientInfo {
        std::string patientId;
        std::string familyName;
        std::string givenName;
        std::string birthDate;
        std::string gender;
        std::string mrn;  // Medical Record Number
        std::string address;
        std::string phoneNumber;
    };
    
    /**
     * @brief Initialize HL7 interface
     * @param listenPort Port to listen for HL7 messages
     * @return Result indicating success or failure
     */
    static core::Result<void> initialize(int listenPort = 8080);
    
    /**
     * @brief Parse ADT message
     * @param message HL7 ADT message
     * @return Result with patient information
     */
    static core::Result<PatientInfo> parseADTMessage(const std::string& message);
    
    /**
     * @brief Send HL7 message
     * @param host Target host
     * @param port Target port
     * @param message HL7 message to send
     * @return Result indicating success or failure
     */
    static core::Result<void> sendMessage(const std::string& host, 
                                        int port, 
                                        const std::string& message);

private:
    static bool listening_;
    static int listenPort_;
};

} // namespace integration
} // namespace common
} // namespace pacs