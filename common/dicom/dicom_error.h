#pragma once

#include <string>
#include <stdexcept>
#include <optional>

namespace pacs {
namespace common {
namespace dicom {

/**
 * @brief Error code enum for DICOM operations
 */
enum class DicomErrorCode {
    // Success code
    Success = 0,
    
    // General errors
    Unknown = 1,
    InvalidArgument = 2,
    OutOfMemory = 3,
    
    // File operation errors
    FileNotFound = 100,
    FileReadError = 101,
    FileWriteError = 102,
    InvalidDicomFile = 103,
    
    // Network errors
    ConnectionFailed = 200,
    AssociationRejected = 201,
    AssociationAborted = 202,
    NetworkTimeout = 203,
    
    // DICOM protocol errors
    InvalidTransferSyntax = 300,
    UnsupportedSOPClass = 301,
    CStoreFailure = 302,
    CFindFailure = 303,
    CMoveFailure = 304,
    NCreateFailure = 305,
    NSetFailure = 306,
    
    // Conversion errors
    ConversionFailure = 400,
    InvalidTag = 401,
    InvalidVR = 402,
    
    // Operation errors
    NotImplemented = 500,
    OperationCancelled = 501,
    PreconditionFailed = 502
};

/**
 * @brief Exception class for DICOM operations
 * 
 * This exception is thrown when DICOM operations fail and provides
 * detailed information about the error including an error code,
 * message, and optional details.
 */
class DicomException : public std::runtime_error {
public:
    /**
     * @brief Constructor
     * @param code The error code
     * @param message The error message
     * @param details Optional additional details about the error
     */
    DicomException(DicomErrorCode code, const std::string& message, const std::string& details = "");
    
    /**
     * @brief Get the error code
     * @return The error code
     */
    DicomErrorCode getErrorCode() const;
    
    /**
     * @brief Get the error message
     * @return The error message
     */
    const std::string& getMessage() const;
    
    /**
     * @brief Get the error details
     * @return The error details or empty optional if no details
     */
    std::optional<std::string> getDetails() const;
    
    /**
     * @brief Get the complete error description
     * @return A string with code, message, and details if available
     */
    std::string getFullDescription() const;
    
private:
    DicomErrorCode code_;
    std::string message_;
    std::optional<std::string> details_;
};

/**
 * @brief Result class for DICOM operations
 * 
 * This class provides a way to return either a successful result
 * or an error from DICOM operations without throwing exceptions.
 * It's particularly useful for expected error conditions.
 */
template<typename T>
class DicomResult {
public:
    /**
     * @brief Constructor for successful result
     * @param value The successful result value
     */
    DicomResult(const T& value) : value_(value), errorCode_(DicomErrorCode::Success) {}
    
    /**
     * @brief Constructor for successful result
     * @param value The successful result value
     */
    DicomResult(T&& value) : value_(std::move(value)), errorCode_(DicomErrorCode::Success) {}
    
    /**
     * @brief Constructor for error result
     * @param errorCode The error code
     * @param errorMessage The error message
     * @param details Optional additional details about the error
     */
    DicomResult(DicomErrorCode errorCode, std::string errorMessage, std::string details = "")
        : errorCode_(errorCode), errorMessage_(std::move(errorMessage)), errorDetails_(std::move(details)) {}
    
    /**
     * @brief Check if the result is successful
     * @return True if successful, false if error
     */
    bool isSuccess() const {
        return errorCode_ == DicomErrorCode::Success;
    }
    
    /**
     * @brief Check if the result is an error
     * @return True if error, false if successful
     */
    bool isError() const {
        return !isSuccess();
    }
    
    /**
     * @brief Get the value (only valid if isSuccess() is true)
     * @return The value
     * @throws DicomException if the result is an error
     */
    const T& getValue() const {
        if (isError()) {
            throw DicomException(errorCode_, errorMessage_, errorDetails_);
        }
        return value_;
    }
    
    /**
     * @brief Get the value (only valid if isSuccess() is true)
     * @return The value
     * @throws DicomException if the result is an error
     */
    T& getValue() {
        if (isError()) {
            throw DicomException(errorCode_, errorMessage_, errorDetails_);
        }
        return value_;
    }
    
    /**
     * @brief Get the error code
     * @return The error code
     */
    DicomErrorCode getErrorCode() const {
        return errorCode_;
    }
    
    /**
     * @brief Get the error message
     * @return The error message or empty string if successful
     */
    const std::string& getErrorMessage() const {
        return errorMessage_;
    }
    
    /**
     * @brief Get the error details
     * @return The error details or empty string if successful or no details
     */
    const std::string& getErrorDetails() const {
        return errorDetails_;
    }
    
private:
    T value_ = {};
    DicomErrorCode errorCode_;
    std::string errorMessage_;
    std::string errorDetails_;
};

/**
 * @brief Specialization of DicomResult for void return type
 * 
 * This specialization is used for operations that don't return a value
 * but still need to indicate success or failure.
 */
template<>
class DicomResult<void> {
public:
    /**
     * @brief Constructor for successful result
     */
    DicomResult() : errorCode_(DicomErrorCode::Success) {}
    
    /**
     * @brief Constructor for error result
     * @param errorCode The error code
     * @param errorMessage The error message
     * @param details Optional additional details about the error
     */
    DicomResult(DicomErrorCode errorCode, std::string errorMessage, std::string details = "")
        : errorCode_(errorCode), errorMessage_(std::move(errorMessage)), errorDetails_(std::move(details)) {}
    
    /**
     * @brief Check if the result is successful
     * @return True if successful, false if error
     */
    bool isSuccess() const {
        return errorCode_ == DicomErrorCode::Success;
    }
    
    /**
     * @brief Check if the result is an error
     * @return True if error, false if successful
     */
    bool isError() const {
        return !isSuccess();
    }
    
    /**
     * @brief Get the error code
     * @return The error code
     */
    DicomErrorCode getErrorCode() const {
        return errorCode_;
    }
    
    /**
     * @brief Get the error message
     * @return The error message or empty string if successful
     */
    const std::string& getErrorMessage() const {
        return errorMessage_;
    }
    
    /**
     * @brief Get the error details
     * @return The error details or empty string if successful or no details
     */
    const std::string& getErrorDetails() const {
        return errorDetails_;
    }
    
private:
    DicomErrorCode errorCode_;
    std::string errorMessage_;
    std::string errorDetails_;
};

// Type alias for void result
using DicomVoidResult = DicomResult<void>;

/**
 * @brief Convert a DCMTK error condition to a DicomErrorCode
 * @param condition The DCMTK error condition
 * @return The corresponding DicomErrorCode
 */
DicomErrorCode convertDcmtkError(unsigned int condition);

/**
 * @brief Convert a DCMTK error condition to a DicomVoidResult
 * @param condition The DCMTK error condition
 * @param operationName The name of the operation for the error message
 * @return DicomVoidResult containing success or the error
 */
DicomVoidResult makeDcmtkResult(unsigned int condition, const std::string& operationName);

/**
 * @brief Make a successful DicomVoidResult
 * @return Successful DicomVoidResult
 */
inline DicomVoidResult makeSuccessResult() {
    return DicomVoidResult();
}

/**
 * @brief Make an error DicomVoidResult
 * @param errorCode The error code
 * @param errorMessage The error message
 * @param details Optional additional details about the error
 * @return Error DicomVoidResult
 */
inline DicomVoidResult makeErrorResult(DicomErrorCode errorCode, const std::string& errorMessage, 
                                     const std::string& details = "") {
    return DicomVoidResult(errorCode, errorMessage, details);
}

/**
 * @brief Make a successful DicomResult
 * @param value The successful result value
 * @return Successful DicomResult containing the value
 */
template<typename T>
inline DicomResult<T> makeSuccessResult(T value) {
    return DicomResult<T>(std::move(value));
}

/**
 * @brief Make an error DicomResult
 * @param errorCode The error code
 * @param errorMessage The error message
 * @param details Optional additional details about the error
 * @return Error DicomResult
 */
template<typename T>
inline DicomResult<T> makeErrorResult(DicomErrorCode errorCode, const std::string& errorMessage, 
                                    const std::string& details = "") {
    return DicomResult<T>(errorCode, errorMessage, details);
}

} // namespace dicom
} // namespace common
} // namespace pacs