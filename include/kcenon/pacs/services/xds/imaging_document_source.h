// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file imaging_document_source.h
 * @brief IHE XDS-I.b Imaging Document Source Actor
 *
 * Provides the Imaging Document Source actor for the IHE XDS-I.b
 * (Cross-Enterprise Document Sharing for Imaging) integration profile.
 * This actor creates Key Object Selection (KOS) documents from DICOM
 * studies and publishes them to an XDS Document Registry/Repository.
 *
 * @see IHE RAD XDS-I.b Profile
 * @see IHE ITI-41 Provide and Register Document Set-b
 * @see DICOM PS3.3 Key Object Selection Document
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_XDS_IMAGING_DOCUMENT_SOURCE_HPP
#define PACS_SERVICES_XDS_IMAGING_DOCUMENT_SOURCE_HPP

#include "kcenon/pacs/core/dicom_dataset.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::pacs::services::xds {

// =============================================================================
// XDS Document Metadata
// =============================================================================

/**
 * @brief Document entry metadata for XDS registry submission
 *
 * Contains the metadata fields required by the IHE XDS.b
 * Provide and Register Document Set-b transaction (ITI-41).
 */
struct xds_document_entry {
    /// Unique identifier for this document entry
    std::string entry_uuid;

    /// Document unique ID (OID format)
    std::string unique_id;

    /// Patient ID in CX format (ID^^^&OID&ISO)
    std::string patient_id;

    /// Source patient ID (from the originating system)
    std::string source_patient_id;

    /// Document class code (e.g., "Imaging Procedure")
    std::string class_code;
    std::string class_code_scheme;
    std::string class_code_display;

    /// Type code (e.g., "Key Object Selection")
    std::string type_code;
    std::string type_code_scheme;
    std::string type_code_display;

    /// Format code (e.g., "1.2.840.10008.5.1.4.1.1.88.59")
    std::string format_code;
    std::string format_code_scheme;

    /// MIME type of the document
    std::string mime_type{"application/dicom"};

    /// Creation time (DTM format: YYYYMMDDhhmmss)
    std::string creation_time;

    /// Service start/stop time
    std::string service_start_time;
    std::string service_stop_time;

    /// Healthcare facility type
    std::string facility_type_code;
    std::string facility_type_code_scheme;

    /// Practice setting code
    std::string practice_setting_code;
    std::string practice_setting_code_scheme;

    /// Author information
    std::string author_person;
    std::string author_institution;

    /// Title/description
    std::string title;

    /// Language code (e.g., "en-US")
    std::string language_code{"en-US"};

    /// Availability status
    std::string availability_status{"Approved"};
};

/**
 * @brief Submission set metadata for XDS registry
 */
struct xds_submission_set {
    /// Unique identifier for this submission set
    std::string unique_id;

    /// Source ID (OID of the submitting system)
    std::string source_id;

    /// Patient ID
    std::string patient_id;

    /// Content type code
    std::string content_type_code;
    std::string content_type_code_scheme;
    std::string content_type_code_display;

    /// Submission time (DTM format)
    std::string submission_time;

    /// Author information
    std::string author_person;
    std::string author_institution;
};

// =============================================================================
// KOS Document Reference
// =============================================================================

/**
 * @brief Reference to a DICOM instance within a KOS document
 */
struct kos_instance_reference {
    /// Referenced SOP Class UID
    std::string sop_class_uid;

    /// Referenced SOP Instance UID
    std::string sop_instance_uid;

    /// Referenced Series Instance UID
    std::string series_instance_uid;

    /// Study Instance UID
    std::string study_instance_uid;
};

/**
 * @brief Result of a KOS document creation operation
 */
struct kos_creation_result {
    /// Whether the KOS was created successfully
    bool success{false};

    /// The created KOS dataset (if successful)
    std::optional<core::dicom_dataset> kos_dataset;

    /// SOP Instance UID of the created KOS
    std::string kos_instance_uid;

    /// Error message (if failed)
    std::string error_message;

    /// Number of referenced instances
    size_t reference_count{0};
};

// =============================================================================
// Publication Result
// =============================================================================

/**
 * @brief Result of publishing a document to an XDS registry/repository
 */
struct publication_result {
    /// Whether the publication was successful
    bool success{false};

    /// Registry-assigned document entry UUID
    std::string document_entry_uuid;

    /// Error message (if failed)
    std::string error_message;

    /// HTTP status code from the registry (if applicable)
    int http_status{0};
};

// =============================================================================
// Imaging Document Source Configuration
// =============================================================================

/**
 * @brief Configuration for the Imaging Document Source actor
 */
struct imaging_document_source_config {
    /// XDS Registry/Repository endpoint URL
    std::string registry_url;

    /// Source system OID (used as sourceId in submissions)
    std::string source_oid;

    /// Assigning authority OID for patient IDs
    std::string assigning_authority_oid;

    /// Default facility type code
    std::string facility_type_code{"Radiology"};
    std::string facility_type_code_scheme{"2.16.840.1.113883.5.11"};

    /// Default practice setting code
    std::string practice_setting_code{"Radiology"};
    std::string practice_setting_code_scheme{"2.16.840.1.113883.6.96"};

    /// Connection timeout in milliseconds
    uint32_t timeout_ms{30000};

    /// Whether to include all instances or just key instances
    bool include_all_instances{true};
};

// =============================================================================
// Imaging Document Source
// =============================================================================

/**
 * @brief IHE XDS-I.b Imaging Document Source Actor
 *
 * Creates KOS (Key Object Selection) documents from DICOM studies
 * and provides the interface for publishing them to an XDS
 * Document Registry/Repository via the ITI-41 transaction.
 *
 * ## Workflow
 *
 * 1. When a study is received/completed, call create_kos_document()
 * 2. Optionally customize the document metadata
 * 3. Call publish_document() to submit to the XDS registry
 *
 * @par Example:
 * @code
 * imaging_document_source_config config;
 * config.registry_url = "https://xds-registry.example.com/services/iti41";
 * config.source_oid = "1.2.3.4.5.6.7.8.9";
 *
 * imaging_document_source source{config};
 *
 * // Create KOS from study instances
 * auto kos_result = source.create_kos_document(
 *     "1.2.840.10008.1.2.3.4",  // Study Instance UID
 *     instances,                  // Instance references
 *     patient_dataset             // Patient demographics
 * );
 *
 * // Build submission and publish
 * auto pub_result = source.publish_document(
 *     kos_result.kos_dataset.value(),
 *     document_entry
 * );
 * @endcode
 */
class imaging_document_source {
public:
    imaging_document_source() = default;
    explicit imaging_document_source(const imaging_document_source_config& config);

    /**
     * @brief Create a KOS document from a set of DICOM instance references.
     *
     * Generates a Key Object Selection Document (SOP Class 1.2.840.10008.5.1.4.1.1.88.59)
     * containing references to the specified instances, following TID 2010.
     *
     * @param study_instance_uid The Study Instance UID
     * @param references Vector of instance references to include
     * @param patient_demographics Optional patient demographics dataset
     * @return KOS creation result with the generated dataset
     */
    [[nodiscard]] kos_creation_result create_kos_document(
        const std::string& study_instance_uid,
        const std::vector<kos_instance_reference>& references,
        const std::optional<core::dicom_dataset>& patient_demographics = std::nullopt) const;

    /**
     * @brief Build XDS document entry metadata from a KOS dataset.
     *
     * Extracts patient and study metadata from the KOS dataset
     * and populates an xds_document_entry structure suitable for
     * registry submission.
     *
     * @param kos_dataset The KOS dataset to extract metadata from
     * @return Populated document entry metadata
     */
    [[nodiscard]] xds_document_entry build_document_entry(
        const core::dicom_dataset& kos_dataset) const;

    /**
     * @brief Build XDS submission set metadata.
     *
     * Creates a submission set entry for ITI-41 Provide and Register
     * Document Set-b transaction.
     *
     * @param patient_id Patient ID for the submission
     * @return Populated submission set metadata
     */
    [[nodiscard]] xds_submission_set build_submission_set(
        const std::string& patient_id) const;

    /**
     * @brief Publish a KOS document to the XDS registry/repository.
     *
     * Performs the ITI-41 (Provide and Register Document Set-b) transaction
     * to publish the KOS document and its metadata to the configured
     * XDS registry/repository.
     *
     * @param kos_dataset The KOS dataset to publish
     * @param entry The document entry metadata
     * @return Publication result
     */
    [[nodiscard]] publication_result publish_document(
        const core::dicom_dataset& kos_dataset,
        const xds_document_entry& entry) const;

    /**
     * @brief Get current configuration.
     * @return Reference to configuration
     */
    [[nodiscard]] const imaging_document_source_config& config() const noexcept;

    /**
     * @brief Set configuration.
     * @param config New configuration
     */
    void set_config(const imaging_document_source_config& config);

private:
    imaging_document_source_config config_;

    /// Generate a new UID for KOS instances
    [[nodiscard]] std::string generate_uid() const;

    /// Build the Current Requested Procedure Evidence Sequence
    void build_evidence_sequence(
        core::dicom_dataset& kos_dataset,
        const std::vector<kos_instance_reference>& references) const;
};

}  // namespace kcenon::pacs::services::xds

#endif  // PACS_SERVICES_XDS_IMAGING_DOCUMENT_SOURCE_HPP
