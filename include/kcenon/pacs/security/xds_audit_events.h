// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file xds_audit_events.h
 * @brief ATNA audit event helpers for the IHE XDS.b actors (ITI-18/41/43).
 *
 * Provides a small sink interface that the three XDS.b actors
 * (document_source, document_consumer, registry_query) call on every
 * transaction attempt - success or failure. The sink abstracts the
 * transport so the actors can remain decoupled from syslog / file /
 * in-memory delivery and so tests can observe emission without a network.
 *
 * Mapping of IHE transactions to RFC 3881 / DICOM PS3.15 event codes:
 *
 *   ITI-41 Provide and Register  -> DCM 110106 "Export"
 *                                    EventActionCode = 'C' (Create)
 *                                    Event type code "ITI-41"
 *
 *   ITI-43 Retrieve Document Set -> DCM 110104 "DICOM Instances Transferred"
 *                                    EventActionCode = 'R' (Read)
 *                                    is_import=true, event type code "ITI-43"
 *
 *   ITI-18 RegistryStoredQuery   -> DCM 110112 "Query"
 *                                    EventActionCode = 'E' (Execute)
 *                                    Event type code "ITI-18"
 *
 * Success paths record atna_event_outcome::success.
 * All error paths (metadata validation, signing, transport, parse,
 * registry-reported Failure, signature verification) record
 * atna_event_outcome::serious_failure.
 *
 * @see IHE ITI TF-1 Section 9 - Audit Trail and Node Authentication
 * @see RFC 3881 - Security Audit and Access Accountability Message XML
 * @see DICOM PS3.15 Annex A.5 - Audit Trail Message Format Profile
 * @see Issue #1131
 */

#ifndef PACS_SECURITY_XDS_AUDIT_EVENTS_HPP
#define PACS_SECURITY_XDS_AUDIT_EVENTS_HPP

#include "kcenon/pacs/security/atna_audit_logger.h"

#include <memory>
#include <string>
#include <vector>

namespace kcenon::pacs::security {

class atna_service_auditor;

/**
 * @brief Context captured for an ITI-41 Provide and Register attempt.
 *
 * All string fields are empty when unavailable; the sink implementation
 * decides whether to emit a placeholder or omit the field from the
 * generated audit message. source_id identifies the local Document
 * Source actor and destination_endpoint is the registry URL.
 */
struct xds_iti41_event {
    std::string source_id;
    std::string destination_endpoint;
    std::string patient_id;
    std::string submission_set_unique_id;
    std::vector<std::string> document_unique_ids;
    atna_event_outcome outcome{atna_event_outcome::success};
    std::string failure_description;
};

/**
 * @brief Context captured for an ITI-43 Retrieve Document Set attempt.
 */
struct xds_iti43_event {
    std::string consumer_id;
    std::string source_endpoint;
    std::string document_unique_id;
    std::string repository_unique_id;
    atna_event_outcome outcome{atna_event_outcome::success};
    std::string failure_description;
};

/**
 * @brief Context captured for an ITI-18 Registry Stored Query attempt.
 *
 * query_kind distinguishes FindDocuments from GetDocuments. When the
 * query is FindDocuments, patient_id is populated; for GetDocuments it
 * is empty and document_uuids carries the requested entry UUIDs.
 */
struct xds_iti18_event {
    enum class kind : std::uint8_t {
        find_documents,
        get_documents
    };

    std::string querier_id;
    std::string registry_endpoint;
    kind query_kind{kind::find_documents};
    std::string patient_id;
    std::vector<std::string> document_uuids;
    atna_event_outcome outcome{atna_event_outcome::success};
    std::string failure_description;
};

/**
 * @brief Abstract sink that receives one call per XDS.b transaction
 *        attempt, regardless of outcome.
 *
 * Implementations map the event to an atna_audit_message and forward it
 * to a transport. The default (null) implementation is available via
 * make_null_xds_audit_sink() and is used when an actor is constructed
 * without a sink - keeping the new feature opt-in and preserving the
 * pre-audit-wiring semantics for existing callers.
 *
 * Thread safety: implementations must be safe for concurrent calls from
 * multiple actor instances. The stock atna_sink satisfies this because
 * atna_service_auditor uses atomic counters and a thread-safe transport.
 */
class xds_audit_sink {
public:
    virtual ~xds_audit_sink() = default;

    virtual void on_iti41_submit(const xds_iti41_event& e) = 0;
    virtual void on_iti43_retrieve(const xds_iti43_event& e) = 0;
    virtual void on_iti18_query(const xds_iti18_event& e) = 0;
};

/**
 * @brief Build a sink that discards every event.
 *
 * Used as the default when an actor is constructed without an explicit
 * sink. Making the absence of a sink still produce a valid (no-op)
 * object lets the actors call through unconditionally and avoids a
 * null-check on every transaction.
 */
[[nodiscard]] std::shared_ptr<xds_audit_sink> make_null_xds_audit_sink();

/**
 * @brief Build a sink that forwards every event to the given
 *        atna_service_auditor.
 *
 * The auditor instance must outlive the sink. The sink holds a raw
 * pointer (not a shared ownership) because atna_service_auditor is
 * typically a process-wide singleton owned by the service composition
 * root - extending its ownership to every XDS actor would tangle the
 * lifetime graph for no gain.
 *
 * @param auditor ATNA auditor that will emit the generated messages.
 *                Must not be null.
 * @param local_participant_id User/AE identifier to record in the
 *                ActiveParticipant block for the local actor
 *                (typically the Document Source / Consumer OID).
 */
[[nodiscard]] std::shared_ptr<xds_audit_sink> make_atna_xds_audit_sink(
    atna_service_auditor& auditor, std::string local_participant_id);

/**
 * @brief Build a sink that captures every event into an in-memory
 *        vector for inspection.
 *
 * Used by the integration test harness to assert that every success
 * AND failure path emits exactly one event with the expected outcome
 * and context fields. The returned sink exposes an events() accessor
 * on the derived type; use std::dynamic_pointer_cast<recording_sink>
 * to access it (header declares the recording type below so tests
 * don't have to reach into implementation headers).
 *
 * @see recording_xds_audit_sink
 */
class recording_xds_audit_sink : public xds_audit_sink {
public:
    struct captured_event {
        enum class kind : std::uint8_t { iti41, iti43, iti18 };
        kind k{kind::iti41};
        xds_iti41_event iti41;
        xds_iti43_event iti43;
        xds_iti18_event iti18;
    };

    void on_iti41_submit(const xds_iti41_event& e) override;
    void on_iti43_retrieve(const xds_iti43_event& e) override;
    void on_iti18_query(const xds_iti18_event& e) override;

    [[nodiscard]] const std::vector<captured_event>& events() const noexcept;
    void clear() noexcept;

private:
    std::vector<captured_event> events_;
};

// =============================================================================
// Message-Build Helpers (testing surface)
// =============================================================================

/**
 * @brief Build the RFC 3881 audit message for a given XDS.b event.
 *
 * Exposed so the integration tests can assert the exact event code /
 * action / EventTypeCode against the IHE ATNA checklist without
 * needing to spin up a syslog transport. Production code emits these
 * via the atna_sink + atna_service_auditor::submit_audit_message path
 * instead of calling these helpers directly.
 *
 * @param audit_source_id Value for AuditSourceIdentification/@code.
 * @param event Transaction context captured by the XDS.b actor.
 * @return Fully populated atna_audit_message ready for
 *         atna_audit_logger::to_xml or submit_audit_message.
 * @see Issue #1131
 */
[[nodiscard]] atna_audit_message build_iti41_audit_message(
    const std::string& audit_source_id, const xds_iti41_event& event,
    const std::string& local_participant_id = "");
[[nodiscard]] atna_audit_message build_iti43_audit_message(
    const std::string& audit_source_id, const xds_iti43_event& event,
    const std::string& local_participant_id = "");
[[nodiscard]] atna_audit_message build_iti18_audit_message(
    const std::string& audit_source_id, const xds_iti18_event& event,
    const std::string& local_participant_id = "");

}  // namespace kcenon::pacs::security

#endif  // PACS_SECURITY_XDS_AUDIT_EVENTS_HPP
