/**
 * @file server_app.h
 * @brief PACS Server application class
 *
 * Provides the main server application class that integrates all PACS
 * components including DICOM services, storage, and database.
 *
 * @see Issue #106 - Full PACS Server Sample
 */

#ifndef PACS_EXAMPLE_PACS_SERVER_SERVER_APP_HPP
#define PACS_EXAMPLE_PACS_SERVER_SERVER_APP_HPP

#include "config.h"

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/network/v2/dicom_server_v2.h"
#include "kcenon/pacs/network/server_config.h"
#include "kcenon/pacs/services/event_sender.h"
#include "kcenon/pacs/services/mpps_scp.h"
#include "kcenon/pacs/services/print_scp.h"
#include "kcenon/pacs/services/query_scp.h"
#include "kcenon/pacs/services/retrieve_scp.h"
#include "kcenon/pacs/services/storage_commitment_scp.h"
#include "kcenon/pacs/services/storage_scp.h"
#include "kcenon/pacs/services/ups/ups_index.h"
#include "kcenon/pacs/services/verification_scp.h"
#include "kcenon/pacs/services/worklist_scp.h"
#include "kcenon/pacs/storage/audit_record.h"
#include "kcenon/pacs/storage/file_storage.h"
#include "kcenon/pacs/storage/index_database.h"
#ifdef PACS_SERVER_WITH_HTTP
#include "kcenon/pacs/web/rest_server.h"
#endif

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

namespace kcenon::pacs::example {

/**
 * @brief Complete PACS server application
 *
 * Integrates all PACS components into a single, easy-to-use application:
 * - DICOM network server with all SCP services
 * - File-based storage with hierarchical organization
 * - SQLite database for metadata indexing
 * - Graceful shutdown handling
 *
 * ## Architecture
 *
 * ```
 * +---------------------------------------------+
 * |              pacs_server_app                |
 * +---------------------------------------------+
 * |                                             |
 * |  +----------------+   +------------------+  |
 * |  | dicom_server_v2|   | index_database   |  |
 * |  +-------+--------+   +--------+---------+  |
 * |          |                     |            |
 * |  +-------v--------+   +--------v---------+  |
 * |  | SCP Services   |   | file_storage     |  |
 * |  | - Verification |   |                  |  |
 * |  | - Storage      |   |                  |  |
 * |  | - Query        |   |                  |  |
 * |  | - Retrieve     |   |                  |  |
 * |  | - Worklist     |   |                  |  |
 * |  | - MPPS         |   |                  |  |
 * |  +----------------+   +------------------+  |
 * +---------------------------------------------+
 * ```
 *
 * @example Usage
 * @code
 * pacs_server_config config;
 * config.server.port = 11112;
 * config.server.ae_title = "MY_PACS";
 * config.storage.directory = "/data/dicom";
 *
 * pacs_server_app app{config};
 *
 * if (!app.initialize()) {
 *     std::cerr << "Failed to initialize\n";
 *     return 1;
 * }
 *
 * if (!app.start()) {
 *     std::cerr << "Failed to start\n";
 *     return 1;
 * }
 *
 * // Wait for shutdown signal
 * app.wait_for_shutdown();
 * @endcode
 */
class pacs_server_app {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct server application with configuration
     * @param config Server configuration
     */
    explicit pacs_server_app(const pacs_server_config& config);

    /**
     * @brief Destructor - stops server if running
     */
    ~pacs_server_app();

    // Non-copyable, non-movable
    pacs_server_app(const pacs_server_app&) = delete;
    pacs_server_app& operator=(const pacs_server_app&) = delete;
    pacs_server_app(pacs_server_app&&) = delete;
    pacs_server_app& operator=(pacs_server_app&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Initialize all components
     *
     * Sets up storage, database, and DICOM services.
     * Must be called before start().
     *
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool initialize();

    /**
     * @brief Start the DICOM server
     *
     * Begins accepting connections on the configured port.
     * initialize() must be called before start().
     *
     * @return true if server started successfully
     */
    [[nodiscard]] bool start();

    /**
     * @brief Stop the server gracefully
     *
     * Stops accepting new connections and waits for active
     * associations to complete.
     */
    void stop();

    /**
     * @brief Wait for server shutdown
     *
     * Blocks until the server is stopped.
     */
    void wait_for_shutdown();

    /**
     * @brief Request shutdown
     *
     * Thread-safe method to request server shutdown.
     * Typically called from signal handler.
     */
    void request_shutdown();

    // =========================================================================
    // Status Queries
    // =========================================================================

    /**
     * @brief Check if server is running
     * @return true if server is accepting connections
     */
    [[nodiscard]] bool is_running() const noexcept;

    /**
     * @brief Get current server statistics
     */
    void print_statistics() const;

private:
    // =========================================================================
    // Private Methods
    // =========================================================================

    /// Set up file storage
    [[nodiscard]] bool setup_storage();

    /// Set up database
    [[nodiscard]] bool setup_database();

    /// Set up DICOM services
    [[nodiscard]] bool setup_services();

    /// Set up DICOM server
    [[nodiscard]] bool setup_server();

    /// Set up the HTTP API, when a port is configured
    [[nodiscard]] bool setup_http();

    // =========================================================================
    // Service Handlers
    // =========================================================================

    /// One worklist item as a C-FIND response, or nothing when the keys do not match it
    [[nodiscard]] std::optional<core::dicom_dataset> worklist_row(
        const storage::worklist_item& item, const core::dicom_dataset& keys) const;

    /// Why an instance cannot be committed, or nothing when it is held
    [[nodiscard]] std::optional<services::commitment_failure_reason> verify_commitment(
        const services::sop_reference& reference) const;

    /// Write a print job to the spool directory
    [[nodiscard]] network::Result<std::monostate> spool_print_job(const services::print_job& job);

    /// Handle incoming C-STORE request
    services::storage_status handle_store(
        const core::dicom_dataset& dataset,
        const std::string& calling_ae,
        const std::string& sop_class_uid,
        const std::string& sop_instance_uid,
        const encoding::transfer_syntax& syntax);

    /// Add a row to the audit trail. A failed write to the trail is reported and does not fail the operation.
    void record_audit(storage::audit_event_type type, bool succeeded, std::string_view source_ae,
                      std::string_view target_ae, std::string patient_id, std::string study_uid,
                      std::string message);

    /// Handle worklist query
    std::vector<core::dicom_dataset> handle_worklist_query(
        const core::dicom_dataset& query_keys,
        const std::string& calling_ae);

    /// Handle MPPS N-CREATE
    network::Result<std::monostate> handle_mpps_create(
        const services::mpps_instance& instance);

    /// Handle MPPS N-SET
    network::Result<std::monostate> handle_mpps_set(
        const std::string& sop_instance_uid,
        const core::dicom_dataset& modifications,
        services::mpps_status new_status);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Server configuration
    pacs_server_config config_;

    /// DICOM server
    std::shared_ptr<network::v2::dicom_server_v2> server_;

    /// File storage
    std::shared_ptr<storage::file_storage> file_storage_;

    /// Index database
    std::shared_ptr<storage::index_database> database_;

    /// Sends N-EVENT-REPORTs on associations this server opens
    std::shared_ptr<services::event_sender> events_;

    /// UPS workitems; declared after the database and the sender it uses
    std::unique_ptr<services::ups_index> ups_index_;

#ifdef PACS_SERVER_WITH_HTTP
    /// REST and DICOMweb, on the same database and storage as the DICOM services
    std::unique_ptr<web::rest_server> http_server_;
#endif

    /// Shutdown flag
    std::atomic<bool> shutdown_requested_{false};

    /// Initialization flag
    bool initialized_{false};
};

}  // namespace kcenon::pacs::example

#endif  // PACS_EXAMPLE_PACS_SERVER_SERVER_APP_HPP
