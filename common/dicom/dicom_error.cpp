#include "dicom_error.h"

#ifndef USE_DCMTK_PLACEHOLDER
#include "dcmtk/dcmdata/dcerror.h"
#include "dcmtk/dcmnet/diutil.h"
#endif

#include <sstream>

namespace pacs {
namespace common {
namespace dicom {

DicomException::DicomException(DicomErrorCode code, const std::string& message, const std::string& details)
    : std::runtime_error(message), code_(code), message_(message) {
    if (!details.empty()) {
        details_ = details;
    }
}

DicomErrorCode DicomException::getErrorCode() const {
    return code_;
}

const std::string& DicomException::getMessage() const {
    return message_;
}

std::optional<std::string> DicomException::getDetails() const {
    return details_;
}

std::string DicomException::getFullDescription() const {
    std::ostringstream oss;
    oss << "Error " << static_cast<int>(code_) << ": " << message_;
    
    if (details_) {
        oss << " (" << *details_ << ")";
    }
    
    return oss.str();
}

DicomErrorCode convertDcmtkError(unsigned int condition) {
#ifdef DCMTK_NOT_AVAILABLE
    return DicomErrorCode::Unknown;
#else
    // Convert DCMTK error codes to DicomErrorCode
    // In newer DCMTK, we can't directly construct OFCondition from unsigned int
    // For now, return Unknown for all DCMTK errors
    return DicomErrorCode::Unknown;
#endif
}

DicomVoidResult makeDcmtkResult(unsigned int condition, const std::string& operationName) {
#ifdef DCMTK_NOT_AVAILABLE
    return DicomVoidResult(DicomErrorCode::NotImplemented, "DCMTK not available");
#else
    // Check if condition indicates success (typically 0)
    if (condition == 0) {
        return DicomVoidResult();
    }
    
    // Otherwise, convert the error
    DicomErrorCode errorCode = convertDcmtkError(condition);
    
    std::string errorMsg = operationName;
    errorMsg += " failed";
    
    return DicomVoidResult(errorCode, errorMsg);
#endif
}

} // namespace dicom
} // namespace common
} // namespace pacs