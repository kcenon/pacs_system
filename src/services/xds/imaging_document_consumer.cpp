// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file imaging_document_consumer.cpp
 * @brief Implementation of IHE XDS-I.b Imaging Document Consumer Actor
 */

#include "pacs/services/xds/imaging_document_consumer.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

#include <algorithm>
#include <sstream>

namespace kcenon::pacs::services::xds {

using namespace kcenon::pacs::core;

// =============================================================================
// KOS-specific DICOM Tags (for reference parsing)
// =============================================================================

namespace consumer_tags {

constexpr dicom_tag current_requested_procedure_evidence_sequence{0x0040, 0xA375};
constexpr dicom_tag referenced_series_sequence{0x0008, 0x1115};
constexpr dicom_tag referenced_sop_sequence{0x0008, 0x1199};
constexpr dicom_tag referenced_sop_instance_uid{0x0008, 0x1155};

}  // namespace consumer_tags

// =============================================================================
// imaging_document_consumer Implementation
// =============================================================================

imaging_document_consumer::imaging_document_consumer(
    const imaging_document_consumer_config& config)
    : config_(config) {}

registry_query_result imaging_document_consumer::query_registry(
    [[maybe_unused]] const registry_query_params& params) const {

    registry_query_result result;

    if (config_.registry_url.empty()) {
        result.error_message = "XDS registry URL not configured";
        return result;
    }

    // NOTE: Actual SOAP request to the XDS registry (ITI-18)
    // would be implemented here using the network layer.
    // The ITI-18 transaction uses SOAP/XML encoding with
    // the AdhocQueryRequest/AdhocQueryResponse pattern.
    result.success = true;

    return result;
}

document_retrieval_result imaging_document_consumer::retrieve_document(
    [[maybe_unused]] const document_reference& doc_ref) const {

    document_retrieval_result result;

    if (config_.repository_url.empty()) {
        result.error_message = "XDS repository URL not configured";
        return result;
    }

    // NOTE: Actual SOAP/MTOM request to the XDS repository (ITI-43)
    // would be implemented here. The response contains the KOS
    // document as an MTOM attachment.
    result.success = true;

    return result;
}

document_retrieval_result imaging_document_consumer::extract_references(
    const core::dicom_dataset& kos_dataset) const {

    document_retrieval_result result;
    result.success = true;

    // Parse Current Requested Procedure Evidence Sequence (0040,A375)
    const auto* evidence_elem = kos_dataset.get(
        consumer_tags::current_requested_procedure_evidence_sequence);

    if (!evidence_elem || !evidence_elem->is_sequence()) {
        result.error_message = "KOS document does not contain evidence sequence";
        result.success = false;
        return result;
    }

    for (const auto& study_item : evidence_elem->sequence_items()) {
        // Extract Study Instance UID
        auto study_uid = study_item.get_string(tags::study_instance_uid);
        if (!study_uid.empty()) {
            if (std::find(result.referenced_study_uids.begin(),
                         result.referenced_study_uids.end(),
                         study_uid) == result.referenced_study_uids.end()) {
                result.referenced_study_uids.push_back(study_uid);
            }
        }

        // Parse Referenced Series Sequence
        const auto* series_elem = study_item.get(
            consumer_tags::referenced_series_sequence);
        if (!series_elem || !series_elem->is_sequence()) {
            continue;
        }

        for (const auto& series_item : series_elem->sequence_items()) {
            auto series_uid = series_item.get_string(tags::series_instance_uid);
            if (!series_uid.empty()) {
                if (std::find(result.referenced_series_uids.begin(),
                             result.referenced_series_uids.end(),
                             series_uid) == result.referenced_series_uids.end()) {
                    result.referenced_series_uids.push_back(series_uid);
                }
            }

            // Parse Referenced SOP Sequence
            const auto* sop_elem = series_item.get(
                consumer_tags::referenced_sop_sequence);
            if (!sop_elem || !sop_elem->is_sequence()) {
                continue;
            }

            for (const auto& sop_item : sop_elem->sequence_items()) {
                auto instance_uid = sop_item.get_string(
                    consumer_tags::referenced_sop_instance_uid);
                if (!instance_uid.empty()) {
                    result.referenced_instance_uids.push_back(instance_uid);
                }
            }
        }
    }

    return result;
}

std::string imaging_document_consumer::build_wado_rs_url(
    const std::string& study_uid,
    const std::string& series_uid,
    const std::string& instance_uid) const {

    std::ostringstream url;
    url << config_.wado_rs_url;

    // Ensure base URL doesn't end with '/'
    if (!config_.wado_rs_url.empty() && config_.wado_rs_url.back() == '/') {
        // Remove the trailing '/' already added
    } else {
        url << '/';
    }

    url << "studies/" << study_uid
        << "/series/" << series_uid
        << "/instances/" << instance_uid;

    return url.str();
}

const imaging_document_consumer_config&
imaging_document_consumer::config() const noexcept {
    return config_;
}

void imaging_document_consumer::set_config(
    const imaging_document_consumer_config& config) {
    config_ = config;
}

}  // namespace kcenon::pacs::services::xds
