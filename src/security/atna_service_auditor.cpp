/**
 * @file atna_service_auditor.cpp
 * @brief Implementation of the ATNA service auditor facade
 */

#include "kcenon/pacs/security/atna_service_auditor.h"

namespace kcenon::pacs::security {

// =============================================================================
// Construction
// =============================================================================

atna_service_auditor::atna_service_auditor(
    const syslog_transport_config& config,
    std::string audit_source_id)
    : audit_source_id_(std::move(audit_source_id)),
      transport_(config) {}

atna_service_auditor::atna_service_auditor(
    atna_service_auditor&& other) noexcept
    : audit_source_id_(std::move(other.audit_source_id_)),
      transport_(std::move(other.transport_)),
      enabled_(other.enabled_.load(std::memory_order_relaxed)),
      events_sent_(other.events_sent_.load(std::memory_order_relaxed)),
      events_failed_(other.events_failed_.load(std::memory_order_relaxed)) {}

atna_service_auditor& atna_service_auditor::operator=(
    atna_service_auditor&& other) noexcept {
    if (this != &other) {
        audit_source_id_ = std::move(other.audit_source_id_);
        transport_ = std::move(other.transport_);
        enabled_.store(other.enabled_.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
        events_sent_.store(other.events_sent_.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
        events_failed_.store(
            other.events_failed_.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
    }
    return *this;
}

// =============================================================================
// DICOM Service Audit Methods
// =============================================================================

void atna_service_auditor::audit_instance_stored(
    const std::string& source_ae,
    const std::string& dest_ae,
    const std::string& study_uid,
    const std::string& patient_id,
    bool success) {

    if (!enabled_.load(std::memory_order_relaxed)) {
        return;
    }

    auto outcome = success ? atna_event_outcome::success
                           : atna_event_outcome::serious_failure;

    auto msg = atna_audit_logger::build_dicom_instances_transferred(
        audit_source_id_,
        source_ae,    // source AE
        "",           // source IP (not available at service level)
        dest_ae,      // destination AE
        "",           // destination IP (not available at service level)
        study_uid,
        patient_id,
        true,         // is_import (SCP receiving)
        outcome);

    send_audit(msg);
}

void atna_service_auditor::audit_query(
    const std::string& calling_ae,
    const std::string& called_ae,
    const std::string& query_level,
    bool success) {

    if (!enabled_.load(std::memory_order_relaxed)) {
        return;
    }

    auto outcome = success ? atna_event_outcome::success
                           : atna_event_outcome::serious_failure;

    // Use query level as query_data for the audit trail
    auto msg = atna_audit_logger::build_query(
        audit_source_id_,
        calling_ae,
        "",           // user IP (not available at service level)
        "QueryLevel=" + query_level + " CalledAE=" + called_ae,
        "",           // patient_id (not available from query keys here)
        outcome);

    send_audit(msg);
}

void atna_service_auditor::audit_authentication(
    const std::string& user_id,
    bool is_login,
    bool success) {

    if (!enabled_.load(std::memory_order_relaxed)) {
        return;
    }

    auto outcome = success ? atna_event_outcome::success
                           : atna_event_outcome::serious_failure;

    auto msg = atna_audit_logger::build_user_authentication(
        audit_source_id_,
        user_id,
        "",           // user IP (not available at this level)
        is_login,
        outcome);

    send_audit(msg);
}

void atna_service_auditor::audit_security_alert(
    const std::string& user_id,
    const std::string& alert_description) {

    if (!enabled_.load(std::memory_order_relaxed)) {
        return;
    }

    auto msg = atna_audit_logger::build_security_alert(
        audit_source_id_,
        user_id,
        "",           // user IP (not available at this level)
        alert_description);

    send_audit(msg);
}

// =============================================================================
// IHE XDS-I.b Transaction Audit Methods
// =============================================================================

namespace {

/// Event type codes that identify the specific XDS-I.b transaction
/// within a generic Export / DICOM Instances Transferred event.
const atna_coded_value xds_event_type_iti_41{
    "ITI-41", "IHE Transactions", "Provide and Register Document Set-b"};
const atna_coded_value xds_event_type_rad_68{
    "RAD-68", "IHE Transactions", "Provide and Register Imaging Document Set"};
const atna_coded_value xds_event_type_iti_43{
    "ITI-43", "IHE Transactions", "Retrieve Document Set"};
const atna_coded_value xds_event_type_rad_69{
    "RAD-69", "IHE Transactions", "Retrieve Imaging Document Set"};

/// Object ID type code for SOP Instance UID participant objects
const atna_coded_value sop_instance_uid_type{
    "110181", "DCM", "SOP Instance UID"};

/// Append SOP Instance UID references as participant objects
void append_sop_instance_objects(
    atna_audit_message& msg,
    const std::vector<std::string>& sop_instance_uids) {
    for (const auto& uid : sop_instance_uids) {
        if (uid.empty()) continue;
        atna_participant_object obj;
        obj.object_type = atna_object_type::system_object;
        obj.object_role = atna_object_role::report;
        obj.object_id_type_code = sop_instance_uid_type;
        obj.object_id = uid;
        msg.participant_objects.push_back(std::move(obj));
    }
}

}  // namespace

void atna_service_auditor::audit_xds_provide_and_register(
    xds_source_transaction transaction,
    const std::string& source_ae,
    const std::string& dest_ae,
    const std::string& study_uid,
    const std::string& patient_id,
    const std::vector<std::string>& sop_instance_uids,
    bool success) {

    if (!enabled_.load(std::memory_order_relaxed)) {
        return;
    }

    auto outcome = success ? atna_event_outcome::success
                           : atna_event_outcome::serious_failure;

    // XDS-I.b Provide and Register is an Export event (DCM 110106) —
    // the local actor is sending a document set to a remote registry.
    auto msg = atna_audit_logger::build_export(
        audit_source_id_,
        source_ae,   // user (the local source actor)
        "",          // user IP (not available at service level)
        dest_ae,     // destination identifier (remote registry)
        study_uid,
        patient_id,
        outcome);

    // Tag the event with the specific IHE transaction code so consumers
    // of the audit log can differentiate ITI-41 from RAD-68.
    msg.event_type_codes.push_back(
        transaction == xds_source_transaction::iti_41
            ? xds_event_type_iti_41
            : xds_event_type_rad_68);

    // Add SOP Instance UID references if provided (required for RAD-68,
    // optional but recommended for ITI-41).
    append_sop_instance_objects(msg, sop_instance_uids);

    send_audit(msg);
}

void atna_service_auditor::audit_xds_retrieve(
    xds_consumer_transaction transaction,
    const std::string& source_ae,
    const std::string& dest_ae,
    const std::string& study_uid,
    const std::string& patient_id,
    const std::vector<std::string>& sop_instance_uids,
    bool success) {

    if (!enabled_.load(std::memory_order_relaxed)) {
        return;
    }

    auto outcome = success ? atna_event_outcome::success
                           : atna_event_outcome::serious_failure;

    // XDS-I.b Retrieve Document Set is an Import event
    // (DICOM Instances Transferred with is_import=true) — the local
    // actor is receiving a document set from a remote repository.
    auto msg = atna_audit_logger::build_dicom_instances_transferred(
        audit_source_id_,
        source_ae,
        "",          // source IP (not available at service level)
        dest_ae,
        "",          // destination IP (not available at service level)
        study_uid,
        patient_id,
        true,        // is_import (consumer is receiving)
        outcome);

    // Tag the event with the specific IHE transaction code so consumers
    // of the audit log can differentiate ITI-43 from RAD-69.
    msg.event_type_codes.push_back(
        transaction == xds_consumer_transaction::iti_43
            ? xds_event_type_iti_43
            : xds_event_type_rad_69);

    // Add SOP Instance UID references if provided.
    append_sop_instance_objects(msg, sop_instance_uids);

    send_audit(msg);
}

// =============================================================================
// Enable / Disable
// =============================================================================

void atna_service_auditor::set_enabled(bool enabled) noexcept {
    enabled_.store(enabled, std::memory_order_relaxed);
}

bool atna_service_auditor::is_enabled() const noexcept {
    return enabled_.load(std::memory_order_relaxed);
}

// =============================================================================
// Statistics
// =============================================================================

size_t atna_service_auditor::events_sent() const noexcept {
    return events_sent_.load(std::memory_order_relaxed);
}

size_t atna_service_auditor::events_failed() const noexcept {
    return events_failed_.load(std::memory_order_relaxed);
}

void atna_service_auditor::reset_statistics() noexcept {
    events_sent_.store(0, std::memory_order_relaxed);
    events_failed_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Configuration Access
// =============================================================================

const std::string& atna_service_auditor::audit_source_id() const noexcept {
    return audit_source_id_;
}

const atna_syslog_transport& atna_service_auditor::transport() const noexcept {
    return transport_;
}

void atna_service_auditor::submit_audit_message(
    const atna_audit_message& message) {
    if (!enabled_.load(std::memory_order_relaxed)) {
        return;
    }
    send_audit(message);
}

// =============================================================================
// Private Helpers
// =============================================================================

void atna_service_auditor::send_audit(const atna_audit_message& message) {
    auto xml = atna_audit_logger::to_xml(message);
    auto result = transport_.send(xml);

    if (result.is_ok()) {
        events_sent_.fetch_add(1, std::memory_order_relaxed);
    } else {
        events_failed_.fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace kcenon::pacs::security
