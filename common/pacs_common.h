#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace pacs {
namespace common {

/**
 * @brief Common constants used throughout the PACS system
 */
class Constants {
public:
    static constexpr const char* DEFAULT_AE_TITLE = "PACS";
    static constexpr int DEFAULT_PORT = 11112;
    static constexpr const char* DEFAULT_HOST = "localhost";
    static constexpr int DEFAULT_TIMEOUT = 30;  // seconds
};

/**
 * @brief Enum representing different DICOM service types
 */
enum class ServiceType {
    MPPS,
    STORAGE,
    WORKLIST,
    QUERY_RETRIEVE
};

/**
 * @brief Enum representing the role of the service (SCP or SCU)
 */
enum class ServiceRole {
    SCP,  // Service Class Provider
    SCU   // Service Class User
};

/**
 * @brief Configuration class for DICOM services
 */
class ServiceConfig {
public:
    ServiceConfig() = default;
    
    std::string aeTitle = Constants::DEFAULT_AE_TITLE;
    std::string peerAETitle = Constants::DEFAULT_AE_TITLE;
    std::string peerHost = Constants::DEFAULT_HOST;
    int peerPort = Constants::DEFAULT_PORT;
    int localPort = Constants::DEFAULT_PORT;
    int timeout = Constants::DEFAULT_TIMEOUT;
    bool enableTLS = false;
    std::string certificateFile;
    std::string privateKeyFile;
    ServiceType serviceType;
    ServiceRole serviceRole;
};

} // namespace common
} // namespace pacs