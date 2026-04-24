// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file xds_audit_events.cpp
 * @brief Implementation of the XDS.b audit event sinks.
 */

#include "kcenon/pacs/security/xds_audit_events.h"

#include "kcenon/pacs/security/atna_service_auditor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kcenon::pacs::security {

namespace {

class null_sink : public xds_audit_sink {
public:
    void on_iti41_submit(const xds_iti41_event&) override {}
    void on_iti43_retrieve(const xds_iti43_event&) override {}
    void on_iti18_query(const xds_iti18_event&) override {}
};

// Concrete sink that forwards each XDS.b event to an atna_service_auditor.
// The auditor already knows how to turn transaction context into well-formed
// RFC 3881 audit messages with the right DCM event codes (Export for ITI-41,
// DICOM Instances Transferred for ITI-43, Query for ITI-18) and already owns
// the syslog transport plus statistics counters, so this sink is pure glue.
class atna_sink : public xds_audit_sink {
public:
    atna_sink(atna_service_auditor& auditor, std::string local_participant_id)
        : auditor_(&auditor),
          local_participant_id_(std::move(local_participant_id)) {}

    void on_iti41_submit(const xds_iti41_event& e) override {
        const std::string source =
            local_participant_id_.empty() ? e.source_id : local_participant_id_;
        // ITI-41 is a source-side XDS transaction: the existing facade
        // method emits the DCM 110106 Export event tagged with the
        // ITI-41 transaction code. study_uid has no natural value at
        // the XDS layer, so we pass the submission set UID as a stable
        // grouping key that auditors can correlate across multiple
        // documents in one submission.
        auditor_->audit_xds_provide_and_register(
            atna_service_auditor::xds_source_transaction::iti_41,
            source,
            e.destination_endpoint,
            e.submission_set_unique_id,
            e.patient_id,
            e.document_unique_ids,
            e.outcome == atna_event_outcome::success);
    }

    void on_iti43_retrieve(const xds_iti43_event& e) override {
        const std::string dest =
            local_participant_id_.empty() ? e.consumer_id
                                          : local_participant_id_;
        // ITI-43 is a consumer-side XDS transaction: the facade method
        // emits DCM 110104 "DICOM Instances Transferred" with is_import
        // true, tagged with the ITI-43 transaction code. Use the
        // document unique id as the per-transaction object reference.
        std::vector<std::string> refs;
        if (!e.document_unique_id.empty()) {
            refs.push_back(e.document_unique_id);
        }
        auditor_->audit_xds_retrieve(
            atna_service_auditor::xds_consumer_transaction::iti_43,
            e.source_endpoint,
            dest,
            /*study_uid=*/"",
            /*patient_id=*/"",
            refs,
            e.outcome == atna_event_outcome::success);
    }

    void on_iti18_query(const xds_iti18_event& e) override {
        // ITI-18 is a registry metadata query. atna_service_auditor has
        // no XDS-specific query method, so we route through the generic
        // audit_query facade which emits DCM 110112. The query_level
        // string captures the FindDocuments / GetDocuments distinction
        // plus the query keys so an auditor can reconstruct the query
        // context without pulling the wire body.
        std::string query_level;
        if (e.query_kind == xds_iti18_event::kind::find_documents) {
            query_level = "ITI-18 FindDocuments PatientID=" + e.patient_id;
        } else {
            query_level = "ITI-18 GetDocuments UUIDs=";
            bool first = true;
            for (const auto& u : e.document_uuids) {
                if (!first) query_level += ',';
                first = false;
                query_level += u;
            }
        }
        const std::string caller =
            local_participant_id_.empty() ? e.querier_id
                                          : local_participant_id_;
        auditor_->audit_query(
            caller,
            e.registry_endpoint,
            query_level,
            e.outcome == atna_event_outcome::success);
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
