/**
 * @file atna_service_auditor.cpp
 * @brief Implementation of the ATNA service auditor facade
 */

#include "pacs/security/atna_service_auditor.hpp"

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
