#include "audit_logger.h"
#include "common/logger/logger.h"
#include "common/config/config_manager.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#else
// Simple JSON placeholder
#include <sstream>
class json {
public:
    std::map<std::string, std::string> data;
    std::string current_key;
    
    json() = default;
    
    json& operator[](const std::string& key) {
        current_key = key;
        return *this;
    }
    
    json& operator=(const std::string& value) {
        if (!current_key.empty()) {
            data[current_key] = value;
        }
        return *this;
    }
    
    json& operator=(int value) {
        if (!current_key.empty()) {
            data[current_key] = std::to_string(value);
        }
        return *this;
    }
    
    json& operator=(const char* value) {
        if (!current_key.empty()) {
            data[current_key] = value;
        }
        return *this;
    }
    
    std::string dump(int = -1) const {
        std::stringstream ss;
        ss << "{";
        bool first = true;
        for (const auto& [k, v] : data) {
            if (!first) ss << ",";
            ss << "\"" << k << "\":\"" << v << "\"";
            first = false;
        }
        ss << "}";
        return ss.str();
    }
};
#endif

namespace pacs {
namespace common {
namespace audit {

// Helper function to convert event type to string
std::string eventTypeToString(AuditEventType type) {
    switch (type) {
        case AuditEventType::UserLogin: return "USER_LOGIN";
        case AuditEventType::UserLogout: return "USER_LOGOUT";
        case AuditEventType::LoginFailed: return "LOGIN_FAILED";
        case AuditEventType::PasswordChanged: return "PASSWORD_CHANGED";
        case AuditEventType::AccessGranted: return "ACCESS_GRANTED";
        case AuditEventType::AccessDenied: return "ACCESS_DENIED";
        case AuditEventType::PermissionChanged: return "PERMISSION_CHANGED";
        case AuditEventType::PatientDataAccess: return "PATIENT_DATA_ACCESS";
        case AuditEventType::PatientDataModify: return "PATIENT_DATA_MODIFY";
        case AuditEventType::PatientDataDelete: return "PATIENT_DATA_DELETE";
        case AuditEventType::StudyAccess: return "STUDY_ACCESS";
        case AuditEventType::StudyModify: return "STUDY_MODIFY";
        case AuditEventType::StudyDelete: return "STUDY_DELETE";
        case AuditEventType::ImageAccess: return "IMAGE_ACCESS";
        case AuditEventType::ImageModify: return "IMAGE_MODIFY";
        case AuditEventType::ImageDelete: return "IMAGE_DELETE";
        case AuditEventType::SystemStart: return "SYSTEM_START";
        case AuditEventType::SystemStop: return "SYSTEM_STOP";
        case AuditEventType::ConfigurationChanged: return "CONFIGURATION_CHANGED";
        case AuditEventType::BackupCreated: return "BACKUP_CREATED";
        case AuditEventType::BackupRestored: return "BACKUP_RESTORED";
        case AuditEventType::DicomAssociationOpened: return "DICOM_ASSOCIATION_OPENED";
        case AuditEventType::DicomAssociationClosed: return "DICOM_ASSOCIATION_CLOSED";
        case AuditEventType::DicomStorageReceived: return "DICOM_STORAGE_RECEIVED";
        case AuditEventType::DicomStorageSent: return "DICOM_STORAGE_SENT";
        case AuditEventType::DicomQueryReceived: return "DICOM_QUERY_RECEIVED";
        case AuditEventType::DicomRetrieveRequested: return "DICOM_RETRIEVE_REQUESTED";
        case AuditEventType::SecurityViolation: return "SECURITY_VIOLATION";
        case AuditEventType::InvalidAccess: return "INVALID_ACCESS";
        case AuditEventType::DataExport: return "DATA_EXPORT";
        case AuditEventType::DataImport: return "DATA_IMPORT";
        default: return "UNKNOWN";
    }
}

// FileAuditBackend implementation
FileAuditBackend::FileAuditBackend(const std::string& auditDirectory)
    : auditDirectory_(auditDirectory) {
    // Create audit directory if it doesn't exist
    std::filesystem::create_directories(auditDirectory_);
}

core::Result<void> FileAuditBackend::storeEvent(const AuditEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Get current audit file
        std::string auditFile = getCurrentAuditFile();
        
        // Create JSON object for the event
        json eventJson;
        auto timestamp = std::chrono::system_clock::to_time_t(event.timestamp);
        char timeBuffer[100];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", std::localtime(&timestamp));
        eventJson["timestamp"] = timeBuffer;
        eventJson["timestamp_epoch"] = std::chrono::duration_cast<std::chrono::seconds>(
            event.timestamp.time_since_epoch()).count();
        eventJson["event_type"] = eventTypeToString(event.eventType);
        eventJson["user_id"] = event.userId;
        eventJson["user_role"] = event.userRole;
        eventJson["source_ip"] = event.sourceIp;
        eventJson["target_resource"] = event.targetResource;
        eventJson["action"] = event.action;
        eventJson["outcome"] = event.outcome;
        eventJson["details"] = event.details;
        
        if (!event.patientId.empty()) {
            eventJson["patient_id"] = event.patientId;
        }
        if (!event.studyInstanceUid.empty()) {
            eventJson["study_instance_uid"] = event.studyInstanceUid;
        }
        
        // Append to file
        std::ofstream file(auditFile, std::ios::app);
        if (!file.is_open()) {
            return core::Result<void>::error("Failed to open audit file: " + auditFile);
        }
        
        file << eventJson.dump() << std::endl;
        file.close();
        
        // Check if rotation is needed
        rotateAuditFiles();
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Failed to store audit event: " + std::string(ex.what()));
    }
}

std::vector<AuditEvent> FileAuditBackend::queryEvents(
    const std::chrono::system_clock::time_point& startTime,
    const std::chrono::system_clock::time_point& endTime,
    std::function<bool(const AuditEvent&)> filter) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AuditEvent> results;
    
    // TODO: Implement file-based query
    // This would involve reading audit files and parsing JSON events
    
    return results;
}

std::string FileAuditBackend::getCurrentAuditFile() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream filename;
    filename << auditDirectory_ << "/audit_";
    
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d", std::localtime(&timestamp));
    filename << buffer << ".log";
    
    return filename.str();
}

void FileAuditBackend::rotateAuditFiles() {
    // Check file size and rotate if needed
    std::string currentFile = getCurrentAuditFile();
    
    try {
        auto fileSize = std::filesystem::file_size(currentFile);
        const size_t maxFileSize = 100 * 1024 * 1024; // 100MB
        
        if (fileSize > maxFileSize) {
            // Rename current file with timestamp
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::system_clock::to_time_t(now);
            
            std::stringstream rotatedFilename;
            rotatedFilename << currentFile << ".";
            
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%H%M%S", std::localtime(&timestamp));
            rotatedFilename << buffer;
            
            std::filesystem::rename(currentFile, rotatedFilename.str());
            
            logger::logInfo("Rotated audit file: {} -> {}", currentFile, rotatedFilename.str());
        }
    }
    catch (const std::exception& ex) {
        logger::logError("Failed to rotate audit file: {}", ex.what());
    }
}

// DatabaseAuditBackend implementation
DatabaseAuditBackend::DatabaseAuditBackend(const std::string& connectionString)
    : connectionString_(connectionString) {
    // TODO: Initialize database connection
}

core::Result<void> DatabaseAuditBackend::storeEvent(const AuditEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // TODO: Implement database storage
    return core::Result<void>::error("Database audit backend not implemented");
}

std::vector<AuditEvent> DatabaseAuditBackend::queryEvents(
    const std::chrono::system_clock::time_point& startTime,
    const std::chrono::system_clock::time_point& endTime,
    std::function<bool(const AuditEvent&)> filter) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AuditEvent> results;
    
    // TODO: Implement database query
    
    return results;
}

// AuditLogger implementation
AuditLogger& AuditLogger::getInstance() {
    static AuditLogger instance;
    return instance;
}

AuditLogger::~AuditLogger() {
    shutdown();
}

core::Result<void> AuditLogger::initialize(std::unique_ptr<AuditStorageBackend> backend) {
    if (initialized_) {
        return core::Result<void>::error("AuditLogger already initialized");
    }
    
    if (!backend) {
        return core::Result<void>::error("Invalid audit storage backend");
    }
    
    backend_ = std::move(backend);
    running_ = true;
    
    // Start processing thread
    processingThread_ = std::thread(&AuditLogger::processEventQueue, this);
    
    // Log system start event
    AuditEvent startEvent;
    startEvent.timestamp = std::chrono::system_clock::now();
    startEvent.eventType = AuditEventType::SystemStart;
    startEvent.userId = "SYSTEM";
    startEvent.userRole = "SYSTEM";
    startEvent.action = "System startup";
    startEvent.outcome = "Success";
    startEvent.details = "PACS system started";
    
    logEvent(startEvent);
    
    initialized_ = true;
    logger::logInfo("Audit logger initialized");
    
    return core::Result<void>::ok();
}

void AuditLogger::shutdown() {
    if (!initialized_) {
        return;
    }
    
    // Log system stop event
    AuditEvent stopEvent;
    stopEvent.timestamp = std::chrono::system_clock::now();
    stopEvent.eventType = AuditEventType::SystemStop;
    stopEvent.userId = "SYSTEM";
    stopEvent.userRole = "SYSTEM";
    stopEvent.action = "System shutdown";
    stopEvent.outcome = "Success";
    stopEvent.details = "PACS system stopped";
    
    logEvent(stopEvent);
    
    // Stop processing thread
    running_ = false;
    queueCondition_.notify_all();
    
    if (processingThread_.joinable()) {
        processingThread_.join();
    }
    
    initialized_ = false;
    logger::logInfo("Audit logger shutdown");
}

void AuditLogger::logEvent(const AuditEvent& event) {
    if (!initialized_) {
        logger::logWarning("Audit logger not initialized, dropping event");
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.push(event);
    }
    
    queueCondition_.notify_one();
}

void AuditLogger::logUserLogin(const std::string& userId, const std::string& sourceIp, bool success) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.eventType = success ? AuditEventType::UserLogin : AuditEventType::LoginFailed;
    event.userId = userId;
    event.userRole = getUserRole(userId);
    event.sourceIp = sourceIp;
    event.action = "User login";
    event.outcome = success ? "Success" : "Failure";
    event.details = success ? "User logged in successfully" : "Login attempt failed";
    
    logEvent(event);
}

void AuditLogger::logUserLogout(const std::string& userId, const std::string& sourceIp) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.eventType = AuditEventType::UserLogout;
    event.userId = userId;
    event.userRole = getUserRole(userId);
    event.sourceIp = sourceIp;
    event.action = "User logout";
    event.outcome = "Success";
    event.details = "User logged out";
    
    logEvent(event);
}

void AuditLogger::logPatientDataAccess(const std::string& userId, const std::string& patientId,
                                       const std::string& action, const std::string& outcome) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    
    if (action == "view") {
        event.eventType = AuditEventType::PatientDataAccess;
    } else if (action == "modify") {
        event.eventType = AuditEventType::PatientDataModify;
    } else if (action == "delete") {
        event.eventType = AuditEventType::PatientDataDelete;
    }
    
    event.userId = userId;
    event.userRole = getUserRole(userId);
    event.patientId = patientId;
    event.targetResource = "Patient: " + patientId;
    event.action = action;
    event.outcome = outcome;
    
    logEvent(event);
}

void AuditLogger::logStudyAccess(const std::string& userId, const std::string& studyInstanceUid,
                                 const std::string& action, const std::string& outcome) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    
    if (action == "view" || action == "retrieve") {
        event.eventType = AuditEventType::StudyAccess;
    } else if (action == "modify") {
        event.eventType = AuditEventType::StudyModify;
    } else if (action == "delete") {
        event.eventType = AuditEventType::StudyDelete;
    }
    
    event.userId = userId;
    event.userRole = getUserRole(userId);
    event.studyInstanceUid = studyInstanceUid;
    event.targetResource = "Study: " + studyInstanceUid;
    event.action = action;
    event.outcome = outcome;
    
    logEvent(event);
}

void AuditLogger::logDicomAssociation(const std::string& remoteAeTitle, const std::string& remoteIp,
                                      AuditEventType eventType) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.eventType = eventType;
    event.userId = remoteAeTitle;
    event.userRole = "DICOM_NODE";
    event.sourceIp = remoteIp;
    event.action = (eventType == AuditEventType::DicomAssociationOpened) ? 
                   "DICOM association opened" : "DICOM association closed";
    event.outcome = "Success";
    
    logEvent(event);
}

void AuditLogger::logDicomStorage(const std::string& userId, const std::string& studyInstanceUid,
                                  const std::string& sopInstanceUid, AuditEventType eventType) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.eventType = eventType;
    event.userId = userId.empty() ? "DICOM_NODE" : userId;
    event.userRole = userId.empty() ? "DICOM_NODE" : getUserRole(userId);
    event.studyInstanceUid = studyInstanceUid;
    event.targetResource = "SOP Instance: " + sopInstanceUid;
    event.action = (eventType == AuditEventType::DicomStorageReceived) ? 
                   "DICOM storage received" : "DICOM storage sent";
    event.outcome = "Success";
    
    logEvent(event);
}

void AuditLogger::logSecurityViolation(const std::string& userId, const std::string& sourceIp,
                                       const std::string& violation) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.eventType = AuditEventType::SecurityViolation;
    event.userId = userId.empty() ? "UNKNOWN" : userId;
    event.userRole = userId.empty() ? "UNKNOWN" : getUserRole(userId);
    event.sourceIp = sourceIp;
    event.action = "Security violation detected";
    event.outcome = "Failure";
    event.details = violation;
    
    logEvent(event);
    
    // Also log as error
    logger::logError("SECURITY VIOLATION: User={}, IP={}, Details={}", userId, sourceIp, violation);
}

std::vector<AuditEvent> AuditLogger::queryEvents(
    const std::chrono::system_clock::time_point& startTime,
    const std::chrono::system_clock::time_point& endTime,
    std::function<bool(const AuditEvent&)> filter) {
    
    if (!backend_) {
        return {};
    }
    
    return backend_->queryEvents(startTime, endTime, filter);
}

core::Result<void> AuditLogger::generateReport(
    const std::chrono::system_clock::time_point& startTime,
    const std::chrono::system_clock::time_point& endTime,
    const std::string& reportPath) {
    
    try {
        auto events = queryEvents(startTime, endTime);
        
        // Create report
        std::ofstream report(reportPath);
        if (!report.is_open()) {
            return core::Result<void>::error("Failed to create report file");
        }
        
        // Write report header
        auto startTimeT = std::chrono::system_clock::to_time_t(startTime);
        auto endTimeT = std::chrono::system_clock::to_time_t(endTime);
        
        report << "PACS Audit Report\n";
        report << "=================\n\n";
        char startBuffer[64], endBuffer[64];
        std::strftime(startBuffer, sizeof(startBuffer), "%Y-%m-%d %H:%M:%S", std::localtime(&startTimeT));
        std::strftime(endBuffer, sizeof(endBuffer), "%Y-%m-%d %H:%M:%S", std::localtime(&endTimeT));
        
        report << "Period: " << startBuffer;
        report << " to " << endBuffer << "\n";
        report << "Total Events: " << events.size() << "\n\n";
        
        // Group events by type
        std::map<AuditEventType, int> eventCounts;
        for (const auto& event : events) {
            eventCounts[event.eventType]++;
        }
        
        report << "Event Summary:\n";
        report << "--------------\n";
        for (const auto& [type, count] : eventCounts) {
            report << eventTypeToString(type) << ": " << count << "\n";
        }
        report << "\n";
        
        // Write detailed events
        report << "Detailed Events:\n";
        report << "----------------\n";
        for (const auto& event : events) {
            auto eventTimeT = std::chrono::system_clock::to_time_t(event.timestamp);
            char timeBuffer[64];
            std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", std::localtime(&eventTimeT));
            report << timeBuffer << " ";
            report << "[" << eventTypeToString(event.eventType) << "] ";
            report << "User: " << event.userId << " ";
            report << "Action: " << event.action << " ";
            report << "Outcome: " << event.outcome << "\n";
            
            if (!event.details.empty()) {
                report << "  Details: " << event.details << "\n";
            }
            report << "\n";
        }
        
        report.close();
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Failed to generate report: " + std::string(ex.what()));
    }
}

void AuditLogger::processEventQueue() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        // Wait for events or shutdown
        queueCondition_.wait(lock, [this] {
            return !eventQueue_.empty() || !running_;
        });
        
        // Process all queued events
        while (!eventQueue_.empty()) {
            AuditEvent event = eventQueue_.front();
            eventQueue_.pop();
            
            lock.unlock();
            
            // Store event
            auto result = backend_->storeEvent(event);
            if (!result.isOk()) {
                logger::logError("Failed to store audit event: {}", result.error());
            }
            
            lock.lock();
        }
    }
}

std::string AuditLogger::getUserRole(const std::string& userId) {
    // Get user role from security manager
    auto& securityManager = security::SecurityManager::getInstance();
    
    if (securityManager.userHasRole(userId, security::UserRole::Admin)) {
        return "Admin";
    } else if (securityManager.userHasRole(userId, security::UserRole::Operator)) {
        return "Operator";
    } else if (securityManager.userHasRole(userId, security::UserRole::Viewer)) {
        return "Viewer";
    } else {
        return "User";
    }
}

// ScopedAuditLog implementation
ScopedAuditLog::ScopedAuditLog(const std::string& userId, const std::string& resource,
                               const std::string& action, AuditEventType eventType) {
    event_.timestamp = std::chrono::system_clock::now();
    event_.eventType = eventType;
    event_.userId = userId;
    // Get user role from security manager or default
    event_.userRole = "user";  // TODO: Get from security manager
    event_.targetResource = resource;
    event_.action = action;
    event_.outcome = "In Progress";
}

ScopedAuditLog::~ScopedAuditLog() {
    if (!logged_) {
        // If not explicitly set, assume success
        if (event_.outcome == "In Progress") {
            event_.outcome = "Success";
        }
        AuditLogger::getInstance().logEvent(event_);
    }
}

void ScopedAuditLog::setOutcome(const std::string& outcome) {
    event_.outcome = outcome;
}

void ScopedAuditLog::addDetail(const std::string& key, const std::string& value) {
    if (!event_.details.empty()) {
        event_.details += ", ";
    }
    event_.details += key + ": " + value;
}

} // namespace audit
} // namespace common
} // namespace pacs