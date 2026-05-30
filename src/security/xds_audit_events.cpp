// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file xds_audit_events.cpp
 * @brief Implementation of the XDS.b audit event sinks.
 *
 * RFC 3881 / DICOM PS3.15 A.5 / IHE ITI TF-2 event code mapping used
 * here (aligned with the reviewer's Task-2 checklist):
 *
 *   ITI-18 RegistryStoredQuery
 *     EventID         DCM 110112 Query
 *     EventAction     E (Execute)
 *     EventTypeCode   urn:ihe:iti:2007:RegistryStoredQuery
 *                     (code system "IHETransactions")
 *
 *   ITI-41 Provide and Register Document Set-b (Source-side)
 *     EventID         DCM 110106 Export
 *     EventAction     C (Create)
 *     EventTypeCode   ITI-41 (code system "IHETransactions")
 *
 *   ITI-43 Retrieve Document Set (Consumer-side)
 *     EventID         DCM 110107 Import
 *     EventAction     R (Read)
 *     EventTypeCode   ITI-43 (code system "IHETransactions")
 *
 * Messages are built by free functions (build_iti18/41/43_audit_message)
 * so the integration tests can assert the exact codes without spinning
 * up a syslog transport. The atna_sink delegates to those builders and
 * routes the result through atna_service_auditor::submit_audit_message
 * so emission stays serialized and the enabled-flag / statistics path
 * is shared with every other audit method (reviewer cross-cut §3).
 */

#include "kcenon/pacs/security/xds_audit_events.h"

#include "kcenon/pacs/security/atna_service_auditor.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kcenon::pacs::security {

namespace {

// -----------------------------------------------------------------------------
// RFC 3881 coded-value constants
// -----------------------------------------------------------------------------

// EventID values the reviewer's Task-2 checklist pins per transaction.
// DCM 110107 Import is not in atna_event_ids (only Export 110106 is), so
// both are declared here to keep the vocabulary in one place.
const atna_coded_value kEventIdExport{
    "110106", "DCM", "Export"};
const atna_coded_value kEventIdImport{
    "110107", "DCM", "Import"};
const atna_coded_value kEventIdQuery{
    "110112", "DCM", "Query"};

// EventTypeCodes - code system "IHETransactions" per IHE ITI TF-2a
// Audit Trail Profile. The ITI-18 code uses the full URN form the
// reviewer specified; ITI-41/43 use the short transaction label that
// matches the existing atna_service_auditor convention.
const atna_coded_value kEventTypeIti18{
    "urn:ihe:iti:2007:RegistryStoredQuery",
    "IHETransactions",
    "Registry Stored Query"};
const atna_coded_value kEventTypeIti41{
    "ITI-41", "IHETransactions",
    "Provide and Register Document Set-b"};
const atna_coded_value kEventTypeIti43{
    "ITI-43", "IHETransactions", "Retrieve Document Set"};

// Object ID type codes for participant objects.
const atna_coded_value kXdsDocumentUniqueIdType{
    "urn:ihe:iti:xds:2013:uniqueId", "IHE XDS Metadata",
    "XDSDocumentEntry.uniqueId"};
const atna_coded_value kXdsEntryUuidType{
    "urn:ihe:iti:xds:2013:entryUUID", "IHE XDS Metadata",
    "XDSDocumentEntry.entryUUID"};
const atna_coded_value kXdsSubmissionSetUidType{
    "urn:ihe:iti:xds:2013:submissionSet.uniqueId", "IHE XDS Metadata",
    "XDSSubmissionSet.uniqueId"};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

atna_active_participant make_participant(const std::string& user_id,
                                         bool is_requestor,
                                         const atna_coded_value& role) {
    atna_active_participant p;
    p.user_id = user_id;
    p.user_is_requestor = is_requestor;
    p.network_access_point_type = atna_network_access_type::machine_name;
    p.role_id_codes.push_back(role);
    return p;
}

void attach_failure_detail(atna_audit_message& msg,
                           const std::string& description) {
    if (description.empty()) return;
    atna_object_detail d;
    d.type = "FailureDescription";
    d.value = description;
    if (msg.participant_objects.empty()) {
        atna_participant_object synth;
        synth.object_type = atna_object_type::system_object;
        synth.object_role = atna_object_role::report;
        synth.object_id_type_code = kXdsDocumentUniqueIdType;
        synth.object_id = "(no object context)";
        synth.object_details.push_back(std::move(d));
        msg.participant_objects.push_back(std::move(synth));
    } else {
        msg.participant_objects.back().object_details.push_back(std::move(d));
    }
}

}  // namespace

// -----------------------------------------------------------------------------
// Message builders (public, testing surface)
// -----------------------------------------------------------------------------

atna_audit_message build_iti41_audit_message(
    const std::string& audit_source_id, const xds_iti41_event& e,
    const std::string& local_participant_id) {
    atna_audit_message msg;
    msg.event_id = kEventIdExport;
    msg.event_action = atna_event_action::create;  // 'C'
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = e.outcome;
    msg.event_type_codes.push_back(kEventTypeIti41);
    msg.audit_source.audit_source_id = audit_source_id;

    const std::string source =
        local_participant_id.empty() ? e.source_id : local_participant_id;
    msg.active_participants.push_back(
        make_participant(source, true, atna_role_ids::source));
    msg.active_participants.push_back(make_participant(
        e.destination_endpoint, false, atna_role_ids::destination));

    if (!e.submission_set_unique_id.empty()) {
        atna_participant_object ss;
        ss.object_type = atna_object_type::system_object;
        ss.object_role = atna_object_role::report;
        ss.object_id_type_code = kXdsSubmissionSetUidType;
        ss.object_id = e.submission_set_unique_id;
        msg.participant_objects.push_back(std::move(ss));
    }
    for (const auto& uid : e.document_unique_ids) {
        if (uid.empty()) continue;
        atna_participant_object d;
        d.object_type = atna_object_type::system_object;
        d.object_role = atna_object_role::report;
        d.object_id_type_code = kXdsDocumentUniqueIdType;
        d.object_id = uid;
        msg.participant_objects.push_back(std::move(d));
    }
    if (!e.patient_id.empty()) {
        atna_participant_object p;
        p.object_type = atna_object_type::person;
        p.object_role = atna_object_role::patient;
        p.object_id_type_code = atna_object_id_types::patient_number;
        p.object_id = e.patient_id;
        msg.participant_objects.push_back(std::move(p));
    }
    attach_failure_detail(msg, e.failure_description);
    return msg;
}

atna_audit_message build_iti43_audit_message(
    const std::string& audit_source_id, const xds_iti43_event& e,
    const std::string& local_participant_id) {
    atna_audit_message msg;
    msg.event_id = kEventIdImport;
    msg.event_action = atna_event_action::read;  // 'R'
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = e.outcome;
    msg.event_type_codes.push_back(kEventTypeIti43);
    msg.audit_source.audit_source_id = audit_source_id;

    const std::string consumer =
        local_participant_id.empty() ? e.consumer_id : local_participant_id;
    // Consumer side: the local actor is the requestor pulling from a
    // remote repository. Source role = remote repository (not
    // requestor), destination role = local consumer (requestor).
    msg.active_participants.push_back(
        make_participant(e.source_endpoint, false, atna_role_ids::source));
    msg.active_participants.push_back(
        make_participant(consumer, true, atna_role_ids::destination));

    if (!e.document_unique_id.empty()) {
        atna_participant_object d;
        d.object_type = atna_object_type::system_object;
        d.object_role = atna_object_role::report;
        d.object_id_type_code = kXdsDocumentUniqueIdType;
        d.object_id = e.document_unique_id;
        msg.participant_objects.push_back(std::move(d));
    }
    if (!e.repository_unique_id.empty()) {
        atna_participant_object r;
        r.object_type = atna_object_type::system_object;
        r.object_role = atna_object_role::resource;
        r.object_id_type_code = atna_object_id_types::node_id;
        r.object_id = e.repository_unique_id;
        msg.participant_objects.push_back(std::move(r));
    }
    attach_failure_detail(msg, e.failure_description);
    return msg;
}

atna_audit_message build_iti18_audit_message(
    const std::string& audit_source_id, const xds_iti18_event& e,
    const std::string& local_participant_id) {
    atna_audit_message msg;
    msg.event_id = kEventIdQuery;
    msg.event_action = atna_event_action::execute;  // 'E'
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = e.outcome;
    msg.event_type_codes.push_back(kEventTypeIti18);
    msg.audit_source.audit_source_id = audit_source_id;

    const std::string caller =
        local_participant_id.empty() ? e.querier_id : local_participant_id;
    msg.active_participants.push_back(
        make_participant(caller, true, atna_role_ids::source));
    msg.active_participants.push_back(make_participant(
        e.registry_endpoint, false, atna_role_ids::destination));

    if (e.query_kind == xds_iti18_event::kind::find_documents &&
        !e.patient_id.empty()) {
        atna_participant_object p;
        p.object_type = atna_object_type::person;
        p.object_role = atna_object_role::patient;
        p.object_id_type_code = atna_object_id_types::patient_number;
        p.object_id = e.patient_id;
        msg.participant_objects.push_back(std::move(p));
    }
    for (const auto& u : e.document_uuids) {
        if (u.empty()) continue;
        atna_participant_object obj;
        obj.object_type = atna_object_type::system_object;
        obj.object_role = atna_object_role::query;
        obj.object_id_type_code = kXdsEntryUuidType;
        obj.object_id = u;
        msg.participant_objects.push_back(std::move(obj));
    }
    attach_failure_detail(msg, e.failure_description);
    return msg;
}

// -----------------------------------------------------------------------------
// Sinks
// -----------------------------------------------------------------------------

namespace {

class null_sink : public xds_audit_sink {
public:
    void on_iti41_submit(const xds_iti41_event&) override {}
    void on_iti43_retrieve(const xds_iti43_event&) override {}
    void on_iti18_query(const xds_iti18_event&) override {}
};

class atna_sink : public xds_audit_sink {
public:
    atna_sink(atna_service_auditor& auditor, std::string local_participant_id)
        : auditor_(&auditor),
          local_participant_id_(std::move(local_participant_id)) {}

    void on_iti41_submit(const xds_iti41_event& e) override {
        auditor_->submit_audit_message(build_iti41_audit_message(
            auditor_->audit_source_id(), e, local_participant_id_));
    }
    void on_iti43_retrieve(const xds_iti43_event& e) override {
        auditor_->submit_audit_message(build_iti43_audit_message(
            auditor_->audit_source_id(), e, local_participant_id_));
    }
    void on_iti18_query(const xds_iti18_event& e) override {
        auditor_->submit_audit_message(build_iti18_audit_message(
            auditor_->audit_source_id(), e, local_participant_id_));
    }

private:
    atna_service_auditor* auditor_;
    std::string local_participant_id_;
};

}  // namespace

// -----------------------------------------------------------------------------
// Factories
// -----------------------------------------------------------------------------

std::shared_ptr<xds_audit_sink> make_null_xds_audit_sink() {
    return std::make_shared<null_sink>();
}

std::shared_ptr<xds_audit_sink> make_atna_xds_audit_sink(
    atna_service_auditor& auditor, std::string local_participant_id) {
    return std::make_shared<atna_sink>(auditor,
                                       std::move(local_participant_id));
}

// -----------------------------------------------------------------------------
// recording_xds_audit_sink
// -----------------------------------------------------------------------------

void recording_xds_audit_sink::on_iti41_submit(const xds_iti41_event& e) {
    captured_event c;
    c.k = captured_event::kind::iti41;
    c.iti41 = e;
    events_.push_back(std::move(c));
}

void recording_xds_audit_sink::on_iti43_retrieve(const xds_iti43_event& e) {
    captured_event c;
    c.k = captured_event::kind::iti43;
    c.iti43 = e;
    events_.push_back(std::move(c));
}

void recording_xds_audit_sink::on_iti18_query(const xds_iti18_event& e) {
    captured_event c;
    c.k = captured_event::kind::iti18;
    c.iti18 = e;
    events_.push_back(std::move(c));
}

const std::vector<recording_xds_audit_sink::captured_event>&
recording_xds_audit_sink::events() const noexcept {
    return events_;
}

void recording_xds_audit_sink::clear() noexcept {
    events_.clear();
}

}  // namespace kcenon::pacs::security
