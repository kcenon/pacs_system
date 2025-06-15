#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

#include "core/result/result.h"
#include "common/security/security_manager.h"

namespace pacs {
namespace common {
namespace audit {

/**
 * @brief Audit event types for HIPAA compliance
 */
enum class AuditEventType {
    // Authentication events
    UserLogin,
    UserLogout,
    LoginFailed,
    PasswordChanged,
    
    // Authorization events
    AccessGranted,
    AccessDenied,
    PermissionChanged,
    
    // Data access events
    PatientDataAccess,
    PatientDataModify,
    PatientDataDelete,
    StudyAccess,
    StudyModify,
    StudyDelete,
    ImageAccess,
    ImageModify,
    ImageDelete,
    
    // System events
    SystemStart,
    SystemStop,
    ConfigurationChanged,
    BackupCreated,
    BackupRestored,
    
    // DICOM events
    DicomAssociationOpened,
    DicomAssociationClosed,
    DicomStorageReceived,
    DicomStorageSent,
    DicomQueryReceived,
    DicomRetrieveRequested,
    
    // Security events
    SecurityViolation,
    InvalidAccess,
    DataExport,
    DataImport
};

/**
 * @brief Audit event structure
 */
struct AuditEvent {
    std::chrono::system_clock::time_point timestamp;
    AuditEventType eventType;
    std::string userId;
    std::string userRole;
    std::string sourceIp;
    std::string targetResource;
    std::string action;
    std::string outcome; // Success, Failure, Warning
    std::string details;
    std::string patientId; // For patient-related events
    std::string studyInstanceUid; // For study-related events
};

/**
 * @brief Interface for audit log storage backends
 */
class AuditStorageBackend {
public:
    virtual ~AuditStorageBackend() = default;
    
    /**
     * @brief Store an audit event
     * @param event The audit event to store
     * @return Result indicating success or failure
     */
    virtual core::Result<void> storeEvent(const AuditEvent& event) = 0;
    
    /**
     * @brief Query audit events
     * @param startTime Start time for the query
     * @param endTime End time for the query
     * @param filter Optional filter function
     * @return Vector of matching audit events
     */
    virtual std::vector<AuditEvent> queryEvents(
        const std::chrono::system_clock::time_point& startTime,
        const std::chrono::system_clock::time_point& endTime,
        std::function<bool(const AuditEvent&)> filter = nullptr) = 0;
};

/**
 * @brief File-based audit storage backend
 */
class FileAuditBackend : public AuditStorageBackend {
public:
    explicit FileAuditBackend(const std::string& auditDirectory);
    
    core::Result<void> storeEvent(const AuditEvent& event) override;
    
    std::vector<AuditEvent> queryEvents(
        const std::chrono::system_clock::time_point& startTime,
        const std::chrono::system_clock::time_point& endTime,
        std::function<bool(const AuditEvent&)> filter = nullptr) override;
        
private:
    std::string auditDirectory_;
    std::mutex mutex_;
    
    std::string getCurrentAuditFile() const;
    void rotateAuditFiles();
};

/**
 * @brief Database-based audit storage backend
 */
class DatabaseAuditBackend : public AuditStorageBackend {
public:
    explicit DatabaseAuditBackend(const std::string& connectionString);
    
    core::Result<void> storeEvent(const AuditEvent& event) override;
    
    std::vector<AuditEvent> queryEvents(
        const std::chrono::system_clock::time_point& startTime,
        const std::chrono::system_clock::time_point& endTime,
        std::function<bool(const AuditEvent&)> filter = nullptr) override;
        
private:
    std::string connectionString_;
    std::mutex mutex_;
};

/**
 * @brief Singleton audit logger for HIPAA compliance
 */
class AuditLogger {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the AuditLogger singleton
     */
    static AuditLogger& getInstance();
    
    /**
     * @brief Initialize the audit logger
     * @param backend The storage backend to use
     * @return Result indicating success or failure
     */
    core::Result<void> initialize(std::unique_ptr<AuditStorageBackend> backend);
    
    /**
     * @brief Shutdown the audit logger
     */
    void shutdown();
    
    /**
     * @brief Log an audit event
     * @param event The audit event to log
     */
    void logEvent(const AuditEvent& event);
    
    /**
     * @brief Log a user login event
     * @param userId User ID
     * @param sourceIp Source IP address
     * @param success Whether login was successful
     */
    void logUserLogin(const std::string& userId, const std::string& sourceIp, bool success);
    
    /**
     * @brief Log a user logout event
     * @param userId User ID
     * @param sourceIp Source IP address
     */
    void logUserLogout(const std::string& userId, const std::string& sourceIp);
    
    /**
     * @brief Log a patient data access event
     * @param userId User ID
     * @param patientId Patient ID
     * @param action Action performed (view, modify, delete)
     * @param outcome Outcome (success, failure)
     */
    void logPatientDataAccess(const std::string& userId, const std::string& patientId,
                              const std::string& action, const std::string& outcome);
    
    /**
     * @brief Log a study access event
     * @param userId User ID
     * @param studyInstanceUid Study Instance UID
     * @param action Action performed (view, retrieve, modify, delete)
     * @param outcome Outcome (success, failure)
     */
    void logStudyAccess(const std::string& userId, const std::string& studyInstanceUid,
                        const std::string& action, const std::string& outcome);
    
    /**
     * @brief Log a DICOM association event
     * @param remoteAeTitle Remote AE title
     * @param remoteIp Remote IP address
     * @param eventType Association opened or closed
     */
    void logDicomAssociation(const std::string& remoteAeTitle, const std::string& remoteIp,
                             AuditEventType eventType);
    
    /**
     * @brief Log a DICOM storage event
     * @param userId User ID (if available)
     * @param studyInstanceUid Study Instance UID
     * @param sopInstanceUid SOP Instance UID
     * @param eventType Storage received or sent
     */
    void logDicomStorage(const std::string& userId, const std::string& studyInstanceUid,
                         const std::string& sopInstanceUid, AuditEventType eventType);
    
    /**
     * @brief Log a security violation
     * @param userId User ID (if available)
     * @param sourceIp Source IP address
     * @param violation Description of the violation
     */
    void logSecurityViolation(const std::string& userId, const std::string& sourceIp,
                              const std::string& violation);
    
    /**
     * @brief Query audit events
     * @param startTime Start time for the query
     * @param endTime End time for the query
     * @param filter Optional filter function
     * @return Vector of matching audit events
     */
    std::vector<AuditEvent> queryEvents(
        const std::chrono::system_clock::time_point& startTime,
        const std::chrono::system_clock::time_point& endTime,
        std::function<bool(const AuditEvent&)> filter = nullptr);
    
    /**
     * @brief Generate audit report
     * @param startTime Start time for the report
     * @param endTime End time for the report
     * @param reportPath Path to save the report
     * @return Result indicating success or failure
     */
    core::Result<void> generateReport(
        const std::chrono::system_clock::time_point& startTime,
        const std::chrono::system_clock::time_point& endTime,
        const std::string& reportPath);

private:
    AuditLogger() = default;
    ~AuditLogger();
    
    // Delete copy and move constructors and assignment operators
    AuditLogger(const AuditLogger&) = delete;
    AuditLogger& operator=(const AuditLogger&) = delete;
    AuditLogger(AuditLogger&&) = delete;
    AuditLogger& operator=(AuditLogger&&) = delete;
    
    void processEventQueue();
    std::string getUserRole(const std::string& userId);
    
    std::unique_ptr<AuditStorageBackend> backend_;
    std::queue<AuditEvent> eventQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::thread processingThread_;
    std::atomic<bool> running_{false};
    bool initialized_{false};
};

/**
 * @brief RAII helper for audit logging
 */
class ScopedAuditLog {
public:
    ScopedAuditLog(const std::string& userId, const std::string& resource,
                   const std::string& action, AuditEventType eventType);
    ~ScopedAuditLog();
    
    void setOutcome(const std::string& outcome);
    void addDetail(const std::string& key, const std::string& value);
    
private:
    AuditEvent event_;
    bool logged_{false};
};

// Convenience macros for audit logging
#define AUDIT_LOG_LOGIN(userId, sourceIp, success) \
    AuditLogger::getInstance().logUserLogin(userId, sourceIp, success)

#define AUDIT_LOG_PATIENT_ACCESS(userId, patientId, action, outcome) \
    AuditLogger::getInstance().logPatientDataAccess(userId, patientId, action, outcome)

#define AUDIT_LOG_STUDY_ACCESS(userId, studyUid, action, outcome) \
    AuditLogger::getInstance().logStudyAccess(userId, studyUid, action, outcome)

#define AUDIT_LOG_SECURITY_VIOLATION(userId, sourceIp, violation) \
    AuditLogger::getInstance().logSecurityViolation(userId, sourceIp, violation)

} // namespace audit
} // namespace common
} // namespace pacs