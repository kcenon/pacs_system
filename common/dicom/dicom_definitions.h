#pragma once

#include <string>
#include <vector>

namespace pacs {
namespace common {
namespace dicom {

/**
 * @brief Common DICOM definitions and constants
 */

// DICOM network configuration
struct DicomNetworkConfig {
    std::string aeTitle;
    std::string hostname;
    int port;
    int dimseTimeout = 30;
    int acseTimeout = 30;
    int maxPdu = 16384;
};

// Transfer syntax constants
constexpr const char* IMPLICIT_LITTLE_ENDIAN = "1.2.840.10008.1.2";
constexpr const char* EXPLICIT_LITTLE_ENDIAN = "1.2.840.10008.1.2.1";
constexpr const char* EXPLICIT_BIG_ENDIAN = "1.2.840.10008.1.2.2";
constexpr const char* JPEG_BASELINE = "1.2.840.10008.1.2.4.50";
constexpr const char* JPEG_LOSSLESS = "1.2.840.10008.1.2.4.70";

// SOP Class UIDs
constexpr const char* VERIFICATION_SOP_CLASS = "1.2.840.10008.1.1";
constexpr const char* CT_IMAGE_STORAGE = "1.2.840.10008.5.1.4.1.1.2";
constexpr const char* MR_IMAGE_STORAGE = "1.2.840.10008.5.1.4.1.1.4";

} // namespace dicom
} // namespace common
} // namespace pacs