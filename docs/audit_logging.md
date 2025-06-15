# PACS System Audit Logging Guide

## Overview

The PACS system includes comprehensive audit logging functionality to meet HIPAA compliance requirements and provide complete visibility into system access and operations.

## Features

- **HIPAA Compliant**: Meets all HIPAA audit logging requirements
- **Comprehensive Event Tracking**: Logs all security-relevant events
- **Tamper-Resistant**: Audit logs are append-only and protected
- **Automated Rotation**: Automatic log rotation and archival
- **Query and Reporting**: Built-in query and report generation
- **Real-time Processing**: Events are logged in near real-time

## Audit Event Types

### Authentication Events
- `USER_LOGIN`: Successful user login
- `USER_LOGOUT`: User logout
- `LOGIN_FAILED`: Failed login attempt
- `PASSWORD_CHANGED`: Password modification

### Authorization Events
- `ACCESS_GRANTED`: Access to resource granted
- `ACCESS_DENIED`: Access to resource denied
- `PERMISSION_CHANGED`: User permissions modified

### Patient Data Events
- `PATIENT_DATA_ACCESS`: Patient record viewed
- `PATIENT_DATA_MODIFY`: Patient record modified
- `PATIENT_DATA_DELETE`: Patient record deleted

### Study/Image Events
- `STUDY_ACCESS`: Study viewed or retrieved
- `STUDY_MODIFY`: Study metadata modified
- `STUDY_DELETE`: Study deleted
- `IMAGE_ACCESS`: Image viewed
- `IMAGE_MODIFY`: Image modified
- `IMAGE_DELETE`: Image deleted

### DICOM Events
- `DICOM_ASSOCIATION_OPENED`: DICOM connection established
- `DICOM_ASSOCIATION_CLOSED`: DICOM connection closed
- `DICOM_STORAGE_RECEIVED`: DICOM image received
- `DICOM_STORAGE_SENT`: DICOM image sent
- `DICOM_QUERY_RECEIVED`: DICOM query received
- `DICOM_RETRIEVE_REQUESTED`: DICOM retrieve requested

### Security Events
- `SECURITY_VIOLATION`: Security policy violation
- `INVALID_ACCESS`: Invalid access attempt
- `DATA_EXPORT`: Data exported from system
- `DATA_IMPORT`: Data imported to system

## Configuration

### Basic Configuration
```json
{
  "audit": {
    "enabled": true,
    "backend": "file",
    "directory": "./data/audit"
  }
}
```

### Advanced Configuration
```json
{
  "audit": {
    "enabled": true,
    "backend": "file",
    "directory": "./data/audit",
    "rotation": {
      "max_file_size_mb": 100,
      "max_files": 30,
      "compress_old_files": true
    },
    "retention": {
      "days": 365,
      "archive_old_files": true,
      "archive_directory": "./data/audit/archive"
    }
  }
}
```

## Usage

### Automatic Logging

Many events are automatically logged by the system:
- All authentication attempts
- DICOM associations and transfers
- System startup and shutdown

### Manual Logging

For custom events, use the audit logging API:

```cpp
#include "common/audit/audit_logger.h"

// Log patient data access
AUDIT_LOG_PATIENT_ACCESS(userId, patientId, "view", "Success");

// Log study access
AUDIT_LOG_STUDY_ACCESS(userId, studyUid, "retrieve", "Success");

// Log security violation
AUDIT_LOG_SECURITY_VIOLATION(userId, sourceIp, "Unauthorized access attempt");
```

### Scoped Audit Logging

For operations that may succeed or fail:

```cpp
{
    ScopedAuditLog audit(userId, "Study: " + studyUid, "export", 
                         AuditEventType::DataExport);
    
    // Perform operation
    auto result = exportStudy(studyUid);
    
    if (result.isOk()) {
        audit.setOutcome("Success");
        audit.addDetail("format", "DICOMDIR");
        audit.addDetail("destination", exportPath);
    } else {
        audit.setOutcome("Failure");
        audit.addDetail("error", result.getError());
    }
    
    // Audit event is automatically logged when 'audit' goes out of scope
}
```

## Audit Log Format

Audit logs are stored in JSON format for easy parsing and analysis:

```json
{
  "timestamp": "2024-01-15 14:23:45",
  "timestamp_epoch": 1705330425,
  "event_type": "PATIENT_DATA_ACCESS",
  "user_id": "john.doe",
  "user_role": "Radiologist",
  "source_ip": "192.168.1.100",
  "target_resource": "Patient: 12345",
  "action": "view",
  "outcome": "Success",
  "details": "Viewed patient demographics",
  "patient_id": "12345"
}
```

## Querying Audit Logs

### Programmatic Query

```cpp
auto& auditLogger = AuditLogger::getInstance();

// Query events for the last 24 hours
auto endTime = std::chrono::system_clock::now();
auto startTime = endTime - std::chrono::hours(24);

// Get all events
auto events = auditLogger.queryEvents(startTime, endTime);

// Filter events
auto loginEvents = auditLogger.queryEvents(startTime, endTime,
    [](const AuditEvent& event) {
        return event.eventType == AuditEventType::UserLogin ||
               event.eventType == AuditEventType::LoginFailed;
    });
```

### Generate Reports

```cpp
// Generate audit report
auto result = auditLogger.generateReport(startTime, endTime, 
                                          "./reports/audit_report.txt");
```

## File Management

### Log Rotation

Logs are automatically rotated when they reach the configured size limit:
- Default: 100MB per file
- Rotated files are named: `audit_YYYYMMDD.log.HHMMSS`

### Retention Policy

- Default retention: 365 days
- Old logs can be automatically archived or deleted
- Archived logs can be compressed to save space

### File Permissions

Ensure proper file permissions on audit logs:
```bash
chmod 640 ./data/audit/*.log
chown pacs:pacs ./data/audit/*.log
```

## Best Practices

1. **Regular Review**: Review audit logs regularly for suspicious activity
2. **Backup Audit Logs**: Include audit logs in backup procedures
3. **Monitor Disk Space**: Ensure adequate disk space for audit logs
4. **Time Synchronization**: Keep system time synchronized (NTP)
5. **Access Control**: Restrict access to audit logs to authorized personnel only
6. **Integrity Checking**: Implement file integrity monitoring for audit logs

## Compliance

### HIPAA Requirements Met

- ✓ User identification
- ✓ Date and time of access
- ✓ Type of action performed
- ✓ Patient identification
- ✓ Source of access (IP address)
- ✓ Success or failure indication

### Audit Log Protection

- Append-only format prevents tampering
- File permissions restrict access
- Optional encryption for archived logs
- Integrity verification available

## Troubleshooting

### Audit Logging Not Working

1. Check if audit logging is enabled in configuration
2. Verify audit directory exists and is writable
3. Check disk space availability
4. Review system logs for errors

### Missing Events

1. Ensure AuditLogger is initialized at startup
2. Check if specific event types are being logged
3. Verify system time is correct

### Performance Impact

1. Use asynchronous logging (default)
2. Adjust rotation settings if needed
3. Consider database backend for high-volume systems

## Integration with Monitoring Systems

Audit logs can be integrated with external monitoring systems:

1. **Syslog**: Forward audit events to syslog
2. **SIEM**: Export logs to Security Information and Event Management systems
3. **Log Analytics**: Use log analysis tools for pattern detection
4. **Alerting**: Set up alerts for critical security events