#include "dicom_error.h"

#ifndef DCMTK_NOT_AVAILABLE
#include "dcmtk/config/osconfig.h"
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
    OFCondition cond;
    cond.code(condition);
    
    // Check for specific DCMTK errors and map them to our error codes
    if (cond == EC_MemoryExhausted) {
        return DicomErrorCode::OutOfMemory;
    }
    else if (cond == EC_InvalidTag) {
        return DicomErrorCode::InvalidTag;
    }
    else if (cond == EC_InvalidVR) {
        return DicomErrorCode::InvalidVR;
    }
    else if (cond == EC_ItemEnd) {
        return DicomErrorCode::InvalidDicomFile;
    }
    else if (cond == EC_ItemNotFound) {
        return DicomErrorCode::InvalidDicomFile;
    }
    else if (cond == EC_InvalidStream) {
        return DicomErrorCode::FileReadError;
    }
    else if (cond == EC_StreamNotifyClient) {
        return DicomErrorCode::FileWriteError;
    }
    else if (cond == EC_WrongStreamMode) {
        return DicomErrorCode::FileReadError;
    }
    else if (cond == EC_DoubleTag) {
        return DicomErrorCode::InvalidDicomFile;
    }
    else if (cond == EC_InvalidBasicOffsetTable) {
        return DicomErrorCode::InvalidDicomFile;
    }
    else if (cond == EC_InvalidFilename) {
        return DicomErrorCode::FileNotFound;
    }
    else if (cond == DIMSE_BADDATA) {
        return DicomErrorCode::InvalidDicomFile;
    }
    else if (cond == DIMSE_ILLEGALASSOCIATION) {
        return DicomErrorCode::AssociationRejected;
    }
    else if (cond == DIMSE_ASSOCIATIONABORTED) {
        return DicomErrorCode::AssociationAborted;
    }
    else if (cond == DIMSE_READPDVFAILED) {
        return DicomErrorCode::NetworkTimeout;
    }
    else if (cond == DIMSE_NOVALIDPRESENTATIONCONTEXTID) {
        return DicomErrorCode::UnsupportedSOPClass;
    }
    else if (cond == DIMSE_NULLKEY) {
        return DicomErrorCode::InvalidArgument;
    }
    
    // Default case
    return DicomErrorCode::Unknown;
#endif
}

DicomVoidResult makeDcmtkResult(unsigned int condition, const std::string& operationName) {
#ifdef DCMTK_NOT_AVAILABLE
    return DicomVoidResult(DicomErrorCode::NotImplemented, "DCMTK not available");
#else
    // If condition is good, return success
    OFCondition cond;
    cond.code(condition);
    
    if (cond.good()) {
        return DicomVoidResult();
    }
    
    // Otherwise, convert the error
    DicomErrorCode errorCode = convertDcmtkError(condition);
    
    std::string errorMsg = operationName;
    errorMsg += " failed: ";
    errorMsg += cond.text();
    
    return DicomVoidResult(errorCode, errorMsg);
#endif
}

} // namespace dicom
} // namespace common
} // namespace pacs