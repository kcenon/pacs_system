/**
 * @file atna_audit_logger.h
 * @brief IHE ATNA-compliant audit message generator (RFC 3881 XML format)
 *
 * Generates RFC 3881 / DICOM PS3.15 Annex A.5 compliant XML audit messages
 * for security-relevant events in DICOM systems.
 *
 * @see DICOM PS3.15 Annex A.5 — Audit Trail Message Format Profile
 * @see IHE ITI TF-1 Section 9 — Audit Trail and Node Authentication (ATNA)
 * @see RFC 3881 — Security Audit and Access Accountability Message XML
 * @see Issue #817 - ATNA Audit Message Generator
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SECURITY_ATNA_AUDIT_LOGGER_HPP
#define PACS_SECURITY_ATNA_AUDIT_LOGGER_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace kcenon::pacs::security {

// =============================================================================
// RFC 3881 Coded Value
// =============================================================================

/**
 * @brief A coded value with code, code system name, and display name
 *
 * Used for EventID, EventTypeCode, RoleIDCode, and ParticipantObjectIDTypeCode.
 */
struct atna_coded_value {
    /// Code value (e.g., "110114")
    std::string code;

    /// Code system name (e.g., "DCM" for DICOM)
    std::string code_system_name;

    /// Human-readable display name (e.g., "UserAuthentication")
    std::string display_name;
};

// =============================================================================
// Event Action Code (RFC 3881 Section 5.1)
// =============================================================================

/**
 * @brief Action that triggered the audit event
 */
enum class atna_event_action : char {
    create = 'C',
    read = 'R',
    update = 'U',
    delete_action = 'D',
    execute = 'E'
};

// =============================================================================
// Event Outcome Indicator (RFC 3881 Section 5.1)
// =============================================================================

/**
 * @brief Outcome of the audited event
 */
enum class atna_event_outcome : uint8_t {
    success = 0,
    minor_failure = 4,
    serious_failure = 8,
    major_failure = 12
};

// =============================================================================
// Network Access Point Type Code (RFC 3881 Section 5.2)
// =============================================================================

/**
 * @brief Type of network access point identifier
 */
enum class atna_network_access_type : uint8_t {
    machine_name = 1,
    ip_address = 2,
    phone_number = 3
};

// =============================================================================
// Participant Object Type Code (RFC 3881 Section 5.5)
// =============================================================================

/**
 * @brief Type of participant object
 */
enum class atna_object_type : uint8_t {
    person = 1,
    system_object = 2,
    organization = 3,
    other = 4
};

/**
 * @brief Role of the participant object in the event
 */
enum class atna_object_role : uint8_t {
    patient = 1,
    location = 2,
    report = 3,
    resource = 4,
    master_file = 5,
    user = 6,
    list = 7,
    doctor = 8,
    subscriber = 9,
    guarantor = 10,
    security_user_entity = 11,
    security_user_group = 12,
    security_resource = 13,
    security_granularity_def = 14,
    provider = 15,
    data_destination = 16,
    data_repository = 17,
    schedule = 18,
    customer = 19,
    job = 20,
    job_stream = 21,
    table = 22,
    routing_criteria = 23,
    query = 24
};

// =============================================================================
// Active Participant (RFC 3881 Section 5.2)
// =============================================================================

/**
 * @brief An active participant in the audit event
 *
 * Represents a user, process, or system involved in the audited event.
 */
struct atna_active_participant {
    /// User or process identifier
    std::string user_id;

    /// Alternative user ID (e.g., process name)
    std::string alternative_user_id;

    /// Human-readable user name
    std::string user_name;

    /// Whether this participant initiated the event
    bool user_is_requestor{false};

    /// Network access point (hostname or IP)
    std::string network_access_point_id;

    /// Type of network access point
    atna_network_access_type network_access_point_type{
        atna_network_access_type::ip_address};

    /// Role(s) of this participant
    std::vector<atna_coded_value> role_id_codes;
};

// =============================================================================
// Audit Source Identification (RFC 3881 Section 5.4)
// =============================================================================

/**
 * @brief Identifies the audit source system
 */
struct atna_audit_source {
    /// Unique identifier for the audit source
    std::string audit_source_id;

    /// Enterprise site identifier (optional)
    std::string audit_enterprise_site_id;

    /// Audit source type codes (optional)
    std::vector<uint8_t> audit_source_type_codes;
};

// =============================================================================
// Participant Object Detail (RFC 3881 Section 5.5)
// =============================================================================

/**
 * @brief Additional detail about a participant object
 */
struct atna_object_detail {
    /// Detail type
    std::string type;

    /// Detail value
    std::string value;
};

// =============================================================================
// Participant Object Identification (RFC 3881 Section 5.5)
// =============================================================================

/**
 * @brief An object (patient, study, query) involved in the event
 */
struct atna_participant_object {
    /// Object type
    atna_object_type object_type{atna_object_type::system_object};

    /// Role of the object in the event
    atna_object_role object_role{atna_object_role::resource};

    /// Object ID type code
    atna_coded_value object_id_type_code;

    /// Object identifier (e.g., Patient ID, Study UID)
    std::string object_id;

    /// Human-readable object name
    std::string object_name;

    /// Query data (base64 encoded, for query events)
    std::string object_query;

    /// Additional details
    std::vector<atna_object_detail> object_details;
};

// =============================================================================
// Audit Message (RFC 3881 Section 5)
// =============================================================================

/**
 * @brief Complete RFC 3881 audit message
 */
struct atna_audit_message {
    // -- Event Identification --

    /// Event ID coded value (e.g., DCM 110114 = UserAuthentication)
    atna_coded_value event_id;

    /// Event type codes for sub-classification
    std::vector<atna_coded_value> event_type_codes;

    /// Action that triggered the event
    atna_event_action event_action{atna_event_action::execute};

    /// When the event occurred
    std::chrono::system_clock::time_point event_date_time;

    /// Outcome of the event
    atna_event_outcome event_outcome{atna_event_outcome::success};

    // -- Participants --

    /// Active participants (users/processes)
    std::vector<atna_active_participant> active_participants;

    /// Audit source identification
    atna_audit_source audit_source;

    /// Participant objects (patients/studies/queries)
    std::vector<atna_participant_object> participant_objects;
};

// =============================================================================
// Well-Known DCM Event IDs (DICOM PS3.15 A.5.3)
// =============================================================================

namespace atna_event_ids {

/// Application Activity (110100) — Start/Stop
inline const atna_coded_value application_activity{
    "110100", "DCM", "Application Activity"};

/// Audit Log Used (110101)
inline const atna_coded_value audit_log_used{
    "110101", "DCM", "Audit Log Used"};

/// Begin Transferring DICOM Instances (110102)
inline const atna_coded_value begin_transferring{
    "110102", "DCM", "Begin Transferring DICOM Instances"};

/// DICOM Instances Accessed (110103)
inline const atna_coded_value dicom_instances_accessed{
    "110103", "DCM", "DICOM Instances Accessed"};

/// DICOM Instances Transferred (110104)
inline const atna_coded_value dicom_instances_transferred{
    "110104", "DCM", "DICOM Instances Transferred"};

/// DICOM Study Deleted (110105)
inline const atna_coded_value dicom_study_deleted{
    "110105", "DCM", "DICOM Study Deleted"};

/// Export (110106)
inline const atna_coded_value data_export{
    "110106", "DCM", "Export"};

/// Import (110107)
inline const atna_coded_value data_import{
    "110107", "DCM", "Import"};

/// Network Entry (110108)
inline const atna_coded_value network_entry{
    "110108", "DCM", "Network Entry"};

/// Order Record (110109)
inline const atna_coded_value order_record{
    "110109", "DCM", "Order Record"};

/// Patient Record (110110)
inline const atna_coded_value patient_record{
    "110110", "DCM", "Patient Record"};

/// Procedure Record (110111)
inline const atna_coded_value procedure_record{
    "110111", "DCM", "Procedure Record"};

/// Query (110112)
inline const atna_coded_value query{
    "110112", "DCM", "Query"};

/// Security Alert (110113)
inline const atna_coded_value security_alert{
    "110113", "DCM", "Security Alert"};

/// User Authentication (110114)
inline const atna_coded_value user_authentication{
    "110114", "DCM", "User Authentication"};

}  // namespace atna_event_ids

// =============================================================================
// Well-Known Event Type Codes
// =============================================================================

namespace atna_event_types {

/// Application Start
inline const atna_coded_value application_start{
    "110120", "DCM", "Application Start"};

/// Application Stop
inline const atna_coded_value application_stop{
    "110121", "DCM", "Application Stop"};

/// Login
inline const atna_coded_value login{
    "110122", "DCM", "Login"};

/// Logout
inline const atna_coded_value logout{
    "110123", "DCM", "Logout"};

}  // namespace atna_event_types

// =============================================================================
// Well-Known Role ID Codes
// =============================================================================

namespace atna_role_ids {

/// Application (110150)
inline const atna_coded_value application{
    "110150", "DCM", "Application"};

/// Application Launcher (110151)
inline const atna_coded_value application_launcher{
    "110151", "DCM", "Application Launcher"};

/// Destination Role ID (110152)
inline const atna_coded_value destination{
    "110152", "DCM", "Destination Role ID"};

/// Source Role ID (110153)
inline const atna_coded_value source{
    "110153", "DCM", "Source Role ID"};

/// Destination Media (110154)
inline const atna_coded_value destination_media{
    "110154", "DCM", "Destination Media"};

/// Source Media (110155)
inline const atna_coded_value source_media{
    "110155", "DCM", "Source Media"};

}  // namespace atna_role_ids

// =============================================================================
// Well-Known Participant Object ID Type Codes
// =============================================================================

namespace atna_object_id_types {

/// Patient Number (RFC 3881 defined)
inline const atna_coded_value patient_number{
    "2", "RFC-3881", "Patient Number"};

/// Study Instance UID
inline const atna_coded_value study_instance_uid{
    "110180", "DCM", "Study Instance UID"};

/// SOP Class UID
inline const atna_coded_value sop_class_uid{
    "110181", "DCM", "SOP Class UID"};

/// Node ID
inline const atna_coded_value node_id{
    "110182", "DCM", "Node ID"};

}  // namespace atna_object_id_types

// =============================================================================
// ATNA Audit Logger
// =============================================================================

/**
 * @brief RFC 3881 XML audit message generator
 *
 * Generates well-formed XML audit messages compliant with
 * DICOM PS3.15 Annex A.5 / RFC 3881.
 */
class atna_audit_logger {
public:
    // =========================================================================
    // XML Generation
    // =========================================================================

    /**
     * @brief Serialize an audit message to RFC 3881 XML
     *
     * @param message The audit message to serialize
     * @return Well-formed XML string
     */
    [[nodiscard]] static std::string to_xml(const atna_audit_message& message);

    // =========================================================================
    // Event Factory Methods
    // =========================================================================

    /**
     * @brief Build Application Activity audit message (start/stop)
     *
     * @param source_id Audit source identifier
     * @param app_name Application name
     * @param is_start true for application start, false for stop
     * @param outcome Event outcome
     * @return Complete audit message
     */
    [[nodiscard]] static atna_audit_message build_application_activity(
        const std::string& source_id,
        const std::string& app_name,
        bool is_start,
        atna_event_outcome outcome = atna_event_outcome::success);

    /**
     * @brief Build DICOM Instances Accessed audit message (C-FIND, QIDO-RS)
     *
     * @param source_id Audit source identifier
     * @param user_id User/AE Title that performed the access
     * @param user_ip Source IP address
     * @param study_uid Study Instance UID accessed
     * @param patient_id Patient ID (if available)
     * @param outcome Event outcome
     * @return Complete audit message
     */
    [[nodiscard]] static atna_audit_message build_dicom_instances_accessed(
        const std::string& source_id,
        const std::string& user_id,
        const std::string& user_ip,
        const std::string& study_uid,
        const std::string& patient_id = "",
        atna_event_outcome outcome = atna_event_outcome::success);

    /**
     * @brief Build DICOM Instances Transferred audit message (C-STORE, C-MOVE)
     *
     * @param source_id Audit source identifier
     * @param source_ae Source AE Title
     * @param source_ip Source IP address
     * @param dest_ae Destination AE Title
     * @param dest_ip Destination IP address
     * @param study_uid Study Instance UID
     * @param patient_id Patient ID (if available)
     * @param is_import true if receiving (import), false if sending (export)
     * @param outcome Event outcome
     * @return Complete audit message
     */
    [[nodiscard]] static atna_audit_message build_dicom_instances_transferred(
        const std::string& source_id,
        const std::string& source_ae,
        const std::string& source_ip,
        const std::string& dest_ae,
        const std::string& dest_ip,
        const std::string& study_uid,
        const std::string& patient_id = "",
        bool is_import = true,
        atna_event_outcome outcome = atna_event_outcome::success);

    /**
     * @brief Build Study Deleted audit message
     *
     * @param source_id Audit source identifier
     * @param user_id User/AE Title that deleted the study
     * @param user_ip Source IP address
     * @param study_uid Deleted Study Instance UID
     * @param patient_id Patient ID
     * @param outcome Event outcome
     * @return Complete audit message
     */
    [[nodiscard]] static atna_audit_message build_study_deleted(
        const std::string& source_id,
        const std::string& user_id,
        const std::string& user_ip,
        const std::string& study_uid,
        const std::string& patient_id = "",
        atna_event_outcome outcome = atna_event_outcome::success);

    /**
     * @brief Build Security Alert audit message
     *
     * @param source_id Audit source identifier
     * @param user_id User/system that triggered the alert
     * @param user_ip Source IP address
     * @param alert_description Description of the security alert
     * @param outcome Event outcome
     * @return Complete audit message
     */
    [[nodiscard]] static atna_audit_message build_security_alert(
        const std::string& source_id,
        const std::string& user_id,
        const std::string& user_ip,
        const std::string& alert_description,
        atna_event_outcome outcome = atna_event_outcome::serious_failure);

    /**
     * @brief Build User Authentication audit message (login/logout)
     *
     * @param source_id Audit source identifier
     * @param user_id User that authenticated
     * @param user_ip Source IP address
     * @param is_login true for login, false for logout
     * @param outcome Event outcome (success/failure)
     * @return Complete audit message
     */
    [[nodiscard]] static atna_audit_message build_user_authentication(
        const std::string& source_id,
        const std::string& user_id,
        const std::string& user_ip,
        bool is_login,
        atna_event_outcome outcome = atna_event_outcome::success);

    /**
     * @brief Build Query audit message (C-FIND, QIDO-RS)
     *
     * @param source_id Audit source identifier
     * @param user_id User/AE Title that performed the query
     * @param user_ip Source IP address
     * @param query_data The query parameters (will be encoded)
     * @param patient_id Patient ID from query (if available)
     * @param outcome Event outcome
     * @return Complete audit message
     */
    [[nodiscard]] static atna_audit_message build_query(
        const std::string& source_id,
        const std::string& user_id,
        const std::string& user_ip,
        const std::string& query_data,
        const std::string& patient_id = "",
        atna_event_outcome outcome = atna_event_outcome::success);

    /**
     * @brief Build Export audit message (media/network export)
     *
     * @param source_id Audit source identifier
     * @param user_id User/AE Title that exported data
     * @param user_ip Source IP address
     * @param dest_id Destination identifier
     * @param study_uid Study Instance UID
     * @param patient_id Patient ID
     * @param outcome Event outcome
     * @return Complete audit message
     */
    [[nodiscard]] static atna_audit_message build_export(
        const std::string& source_id,
        const std::string& user_id,
        const std::string& user_ip,
        const std::string& dest_id,
        const std::string& study_uid,
        const std::string& patient_id = "",
        atna_event_outcome outcome = atna_event_outcome::success);

private:
    // =========================================================================
    // XML Helpers
    // =========================================================================

    [[nodiscard]] static std::string xml_escape(const std::string& str);

    [[nodiscard]] static std::string format_datetime(
        std::chrono::system_clock::time_point tp);

    [[nodiscard]] static std::string format_coded_value_attrs(
        const atna_coded_value& cv);
};

}  // namespace kcenon::pacs::security

#endif  // PACS_SECURITY_ATNA_AUDIT_LOGGER_HPP
