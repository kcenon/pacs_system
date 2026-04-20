// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file imaging_document_consumer.h
 * @brief IHE XDS-I.b Imaging Document Consumer Actor
 *
 * Provides the Imaging Document Consumer actor for the IHE XDS-I.b
 * integration profile. This actor queries the XDS registry for
 * imaging documents and retrieves referenced images via WADO-RS or C-MOVE.
 *
 * @see IHE RAD XDS-I.b Profile
 * @see IHE ITI-18 Registry Stored Query
 * @see IHE ITI-43 Retrieve Document Set
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_XDS_IMAGING_DOCUMENT_CONSUMER_HPP
#define PACS_SERVICES_XDS_IMAGING_DOCUMENT_CONSUMER_HPP

#include "kcenon/pacs/core/dicom_dataset.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::pacs::services::xds {

// =============================================================================
// Registry Query Types
// =============================================================================

/**
 * @brief Query parameters for XDS registry stored query (ITI-18)
 */
struct registry_query_params {
    /// Patient ID to search for
    std::string patient_id;

    /// Optional: Document class code filter
    std::optional<std::string> class_code;

    /// Optional: Type code filter
    std::optional<std::string> type_code;

    /// Optional: Practice setting code filter
    std::optional<std::string> practice_setting_code;

    /// Optional: Creation time range (from)
    std::optional<std::string> creation_time_from;

    /// Optional: Creation time range (to)
    std::optional<std::string> creation_time_to;

    /// Optional: Document availability status
    std::string status{"Approved"};

    /// Query type identifier
    std::string query_id{"urn:uuid:14d4debf-8f97-4251-9a74-a90016b0af0d"};
};

/**
 * @brief Document reference returned from a registry query
 */
struct document_reference {
    /// Document unique ID
    std::string unique_id;

    /// Repository unique ID (identifies where the document is stored)
    std::string repository_unique_id;

    /// Document entry UUID
    std::string entry_uuid;

    /// Patient ID
    std::string patient_id;

    /// Document class code
    std::string class_code;

    /// Type code
    std::string type_code;

    /// Creation time
    std::string creation_time;

    /// Title
    std::string title;

    /// MIME type
    std::string mime_type;

    /// Availability status
    std::string status;

    /// Size in bytes
    size_t size{0};
};

/**
 * @brief Result of a registry query
 */
struct registry_query_result {
    /// Whether the query succeeded
    bool success{false};

    /// Found document references
    std::vector<document_reference> documents;

    /// Error message (if failed)
    std::string error_message;

    /// HTTP status code from the registry
    int http_status{0};
};

/**
 * @brief Result of a document retrieval
 */
struct document_retrieval_result {
    /// Whether the retrieval succeeded
    bool success{false};

    /// The retrieved KOS dataset
    std::optional<core::dicom_dataset> document;

    /// Instance references extracted from the KOS
    std::vector<std::string> referenced_study_uids;
    std::vector<std::string> referenced_series_uids;
    std::vector<std::string> referenced_instance_uids;

    /// Error message (if failed)
    std::string error_message;

    /// HTTP status code
    int http_status{0};
};

// =============================================================================
// Imaging Document Consumer Configuration
// =============================================================================

/**
 * @brief Configuration for the Imaging Document Consumer actor
 */
struct imaging_document_consumer_config {
    /// XDS Registry endpoint URL for queries (ITI-18)
    std::string registry_url;

    /// XDS Repository endpoint URL for document retrieval (ITI-43)
    std::string repository_url;

    /// WADO-RS base URL for image retrieval (alternative to C-MOVE)
    std::string wado_rs_url;

    /// Connection timeout in milliseconds
    uint32_t timeout_ms{30000};

    /// Maximum number of results to return
    uint32_t max_results{100};
};

// =============================================================================
// Imaging Document Consumer
// =============================================================================

/**
 * @brief IHE XDS-I.b Imaging Document Consumer Actor
 *
 * Queries XDS Document Registries for imaging documents (KOS)
 * and provides the interface for retrieving referenced images
 * via WADO-RS or DICOM C-MOVE.
 *
 * ## Workflow
 *
 * 1. Query the registry for imaging documents using query_registry()
 * 2. Retrieve specific KOS documents using retrieve_document()
 * 3. Extract image references from the KOS document
 * 4. Retrieve images via WADO-RS using build_wado_rs_url()
 *
 * @par Example:
 * @code
 * imaging_document_consumer_config config;
 * config.registry_url = "https://xds-registry.example.com/services/iti18";
 * config.wado_rs_url = "https://pacs.example.com/wado-rs";
 *
 * imaging_document_consumer consumer{config};
 *
 * // Query for patient's imaging documents
 * registry_query_params params;
 * params.patient_id = "12345^^^&1.2.3&ISO";
 *
 * auto query_result = consumer.query_registry(params);
 * if (query_result.success) {
 *     for (const auto& doc : query_result.documents) {
 *         auto retrieval = consumer.retrieve_document(doc);
 *         // Process retrieved KOS document
 *     }
 * }
 * @endcode
 */
class imaging_document_consumer {
public:
    imaging_document_consumer() = default;
    explicit imaging_document_consumer(const imaging_document_consumer_config& config);

    /**
     * @brief Query the XDS registry for imaging documents.
     *
     * Performs an ITI-18 (Registry Stored Query) to find imaging
     * documents matching the specified criteria.
     *
     * @param params Query parameters
     * @return Query result with document references
     */
    [[nodiscard]] registry_query_result query_registry(
        const registry_query_params& params) const;

    /**
     * @brief Retrieve a specific document from the XDS repository.
     *
     * Performs an ITI-43 (Retrieve Document Set) to fetch a
     * KOS document identified by the document reference.
     *
     * @param doc_ref The document reference from a registry query
     * @return Retrieval result with the KOS dataset
     */
    [[nodiscard]] document_retrieval_result retrieve_document(
        const document_reference& doc_ref) const;

    /**
     * @brief Extract image references from a KOS dataset.
     *
     * Parses a KOS (Key Object Selection) document and extracts
     * the referenced SOP Instance UIDs, organized by study and series.
     *
     * @param kos_dataset The KOS dataset to parse
     * @return Retrieval result with extracted references
     */
    [[nodiscard]] document_retrieval_result extract_references(
        const core::dicom_dataset& kos_dataset) const;

    /**
     * @brief Build a WADO-RS URL for retrieving a specific instance.
     *
     * Constructs a WADO-RS retrieve URL based on the configured
     * WADO-RS base URL and the instance identifiers.
     *
     * @param study_uid Study Instance UID
     * @param series_uid Series Instance UID
     * @param instance_uid SOP Instance UID
     * @return The WADO-RS retrieve URL
     */
    [[nodiscard]] std::string build_wado_rs_url(
        const std::string& study_uid,
        const std::string& series_uid,
        const std::string& instance_uid) const;

    /**
     * @brief Get current configuration.
     * @return Reference to configuration
     */
    [[nodiscard]] const imaging_document_consumer_config& config() const noexcept;

    /**
     * @brief Set configuration.
     * @param config New configuration
     */
    void set_config(const imaging_document_consumer_config& config);

private:
    imaging_document_consumer_config config_;
};

}  // namespace kcenon::pacs::services::xds

#endif  // PACS_SERVICES_XDS_IMAGING_DOCUMENT_CONSUMER_HPP
