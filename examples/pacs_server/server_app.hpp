/**
 * @file server_app.hpp
 * @brief PACS Server application class
 *
 * Provides the main server application class that integrates all PACS
 * components including DICOM services, storage, and database.
 *
 * @see Issue #106 - Full PACS Server Sample
 */

#ifndef PACS_EXAMPLE_PACS_SERVER_SERVER_APP_HPP
#define PACS_EXAMPLE_PACS_SERVER_SERVER_APP_HPP

#include "config.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/mpps_scp.hpp"
#include "pacs/services/query_scp.hpp"
#include "pacs/services/retrieve_scp.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/services/worklist_scp.hpp"
#include "pacs/storage/file_storage.hpp"
#include "pacs/storage/index_database.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace pacs::example {

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
 * |  | dicom_server   |   | index_database   |  |
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

    // =========================================================================
    // Service Handlers
    // =========================================================================

    /// Handle incoming C-STORE request
    services::storage_status handle_store(
        const core::dicom_dataset& dataset,
        const std::string& calling_ae,
        const std::string& sop_class_uid,
        const std::string& sop_instance_uid);

    /// Handle C-FIND query
    std::vector<core::dicom_dataset> handle_query(
        services::query_level level,
        const core::dicom_dataset& query_keys,
        const std::string& calling_ae);

    /// Handle C-MOVE/C-GET retrieve
    std::vector<core::dicom_file> handle_retrieve(
        const core::dicom_dataset& query_keys);

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
    std::unique_ptr<network::dicom_server> server_;

    /// File storage
    std::unique_ptr<storage::file_storage> file_storage_;

    /// Index database
    std::unique_ptr<storage::index_database> database_;

    /// Shutdown flag
    std::atomic<bool> shutdown_requested_{false};

    /// Initialization flag
    bool initialized_{false};
};

}  // namespace pacs::example

#endif  // PACS_EXAMPLE_PACS_SERVER_SERVER_APP_HPP
