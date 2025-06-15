/**
 * @file api_version.h
 * @brief API versioning and compatibility management
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#pragma once

#include <string>
#include <tuple>

namespace pacs {

/**
 * @brief PACS System API version information
 * 
 * Version format: MAJOR.MINOR.PATCH
 * - MAJOR: Incompatible API changes
 * - MINOR: Backwards-compatible functionality additions
 * - PATCH: Backwards-compatible bug fixes
 */
class ApiVersion {
public:
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;
    
    static constexpr const char* VERSION_STRING = "1.0.0";
    static constexpr const char* BUILD_DATE = __DATE__;
    static constexpr const char* BUILD_TIME = __TIME__;
    
    /**
     * @brief Get version as tuple for comparison
     */
    static std::tuple<int, int, int> getVersion() {
        return std::make_tuple(MAJOR, MINOR, PATCH);
    }
    
    /**
     * @brief Get full version string
     */
    static std::string getVersionString() {
        return std::string(VERSION_STRING) + " (" + BUILD_DATE + " " + BUILD_TIME + ")";
    }
    
    /**
     * @brief Check if current version is compatible with required version
     * @param requiredMajor Required major version
     * @param requiredMinor Required minor version
     * @return true if compatible
     */
    static bool isCompatible(int requiredMajor, int requiredMinor = 0) {
        // Major version must match exactly
        if (MAJOR != requiredMajor) {
            return false;
        }
        
        // Minor version must be greater or equal
        return MINOR >= requiredMinor;
    }
    
    /**
     * @brief Get API capabilities for this version
     */
    static uint32_t getCapabilities() {
        uint32_t caps = 0;
        
        // Version 1.0 capabilities
        caps |= CAP_STORAGE_SCP;
        caps |= CAP_QUERY_RETRIEVE;
        caps |= CAP_WORKLIST;
        caps |= CAP_MPPS;
        caps |= CAP_ENCRYPTION;
        caps |= CAP_AUDIT_LOGGING;
        caps |= CAP_CONNECTION_POOLING;
        caps |= CAP_POSTGRESQL;
        
        return caps;
    }
    
    // Capability flags
    static constexpr uint32_t CAP_STORAGE_SCP = 1 << 0;
    static constexpr uint32_t CAP_QUERY_RETRIEVE = 1 << 1;
    static constexpr uint32_t CAP_WORKLIST = 1 << 2;
    static constexpr uint32_t CAP_MPPS = 1 << 3;
    static constexpr uint32_t CAP_ENCRYPTION = 1 << 4;
    static constexpr uint32_t CAP_AUDIT_LOGGING = 1 << 5;
    static constexpr uint32_t CAP_CONNECTION_POOLING = 1 << 6;
    static constexpr uint32_t CAP_POSTGRESQL = 1 << 7;
    static constexpr uint32_t CAP_MONITORING = 1 << 8;
    static constexpr uint32_t CAP_CIRCUIT_BREAKER = 1 << 9;
};

/**
 * @brief Macro for checking API version at compile time
 */
#define PACS_API_VERSION_CHECK(major, minor) \
    static_assert(pacs::ApiVersion::MAJOR == major && \
                  pacs::ApiVersion::MINOR >= minor, \
                  "PACS API version mismatch")

/**
 * @brief Deprecated attribute for marking deprecated APIs
 */
#if defined(__GNUC__) || defined(__clang__)
    #define PACS_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
    #define PACS_DEPRECATED(msg) __declspec(deprecated(msg))
#else
    #define PACS_DEPRECATED(msg)
#endif

/**
 * @brief API version compatibility checker
 */
class ApiCompatibility {
public:
    /**
     * @brief Check if a feature is available
     */
    static bool hasFeature(uint32_t capability) {
        return (ApiVersion::getCapabilities() & capability) != 0;
    }
    
    /**
     * @brief Get minimum required version for a feature
     */
    static std::tuple<int, int, int> getFeatureVersion(uint32_t capability) {
        // Map capabilities to their introduction version
        switch (capability) {
            case ApiVersion::CAP_STORAGE_SCP:
            case ApiVersion::CAP_QUERY_RETRIEVE:
            case ApiVersion::CAP_WORKLIST:
            case ApiVersion::CAP_MPPS:
                return std::make_tuple(1, 0, 0);
                
            case ApiVersion::CAP_ENCRYPTION:
            case ApiVersion::CAP_AUDIT_LOGGING:
            case ApiVersion::CAP_CONNECTION_POOLING:
            case ApiVersion::CAP_POSTGRESQL:
            case ApiVersion::CAP_CIRCUIT_BREAKER:
                return std::make_tuple(1, 0, 0);
                
            case ApiVersion::CAP_MONITORING:
                return std::make_tuple(1, 1, 0);  // Future feature
                
            default:
                return std::make_tuple(999, 999, 999);  // Unknown
        }
    }
};

} // namespace pacs