// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file submission_set.h
 * @brief XDS.b submission set metadata for ITI-41 Provide and Register
 *        Document Set-b.
 *
 * These are plain data carriers. They mirror the subset of ebRIM/ebRS
 * attributes required for a minimally conforming XDS.b submission set
 * containing one or more documents. Fields are deliberately string-typed
 * because the upstream DICOM dataset and the downstream ebXML registry
 * both canonicalize to strings.
 *
 * @see IHE ITI TF-2a §3.41 - Provide and Register Document Set-b
 * @see Issue #1128
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds {

/**
 * @brief A single XDS document entry plus its binary payload.
 *
 * unique_id is the XDSDocumentEntry.uniqueId classification - typically the
 * OID of the source document (for DICOM KOS this is the SOP Instance UID
 * prefixed with "urn:oid:").
 *
 * mime_type must be an RFC 2046 media type (e.g. "application/dicom",
 * "application/hl7-v3+xml").
 *
 * content holds the raw bytes that become the MTOM-attached part. Ownership
 * is by-copy; document_source::submit does not outlive the call.
 */
struct xds_document {
    std::string entry_uuid;
    std::string unique_id;
    std::string mime_type{"application/dicom"};
    std::string title;
    std::string format_code;
    std::string class_code;
    std::string type_code;
    std::string language_code{"en-US"};
    std::string creation_time;
    std::vector<std::uint8_t> content;
};

/**
 * @brief ebRIM/ebRS-shaped submission set metadata.
 *
 * patient_id is the XDS affinity-domain patient ID in CX form
 * ("id^^^&assigningAuthority&ISO"). source_id is the Document Source actor's
 * OID. submission_set_unique_id is the OID assigned to this submission.
 *
 * All fields are required - validation happens inside document_source::submit
 * and returns a typed error_code on failure rather than throwing.
 */
struct submission_set {
    std::string submission_set_unique_id;
    std::string entry_uuid;
    std::string source_id;
    std::string patient_id;
    std::string content_type_code;
    std::string submission_time;
    std::string author_institution;
    std::string author_person;

    std::vector<xds_document> documents;
};

/**
 * @brief ITI-41 response summary.
 *
 * registry_response_status is the ebRS RegistryResponse/@status value, e.g.
 * "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success" or
 * "...:Failure". Callers should treat any non-Success status as an error;
 * document_source::submit already maps non-Success to
 * error_code::registry_failure_response, so receiving a submit_response
 * implies Success.
 *
 * submission_set_unique_id echoes what was submitted, for logging.
 */
struct submit_response {
    std::string registry_response_status;
    std::string submission_set_unique_id;
};

}  // namespace kcenon::pacs::ihe::xds
