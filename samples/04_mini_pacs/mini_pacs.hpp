/**
 * @file mini_pacs.hpp
 * @brief Mini PACS class integrating all DICOM services
 *
 * This file provides a complete Mini PACS implementation that combines:
 * - Verification SCP (C-ECHO)
 * - Storage SCP (C-STORE)
 * - Query SCP (C-FIND at Patient/Study/Series/Image levels)
 * - Retrieve SCP (C-MOVE/C-GET)
 * - Modality Worklist SCP (MWL C-FIND)
 * - MPPS SCP (N-CREATE/N-SET)
 *
 * @see DICOM PS3.4 - Service Class Specifications
 * @see DICOM PS3.7 - Message Exchange
 */

#pragma once

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_file.hpp>
#include <pacs/network/dicom_server.hpp>
#include <pacs/network/server_config.hpp>
#include <pacs/services/verification_scp.hpp>
#include <pacs/services/storage_scp.hpp>
#include <pacs/services/query_scp.hpp>
#include <pacs/services/retrieve_scp.hpp>
#include <pacs/services/worklist_scp.hpp>
#include <pacs/services/mpps_scp.hpp>
#include <pacs/storage/file_storage.hpp>
#include <pacs/storage/index_database.hpp>

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pacs::samples {

// =============================================================================
// Configuration
// =============================================================================

/**
 * @brief Configuration options for Mini PACS
 *
 * Provides all necessary settings for server operation including
 * network configuration, storage paths, and feature toggles.
 */
struct mini_pacs_config {
    /// Application Entity title (max 16 characters)
    std::string ae_title{"MINI_PACS"};

    /// TCP port to listen on
    uint16_t port{11112};

    /// Root path for DICOM file storage
    std::filesystem::path storage_path{"./pacs_data"};

    /// Maximum concurrent associations
    size_t max_associations{50};

    /// Enable Modality Worklist service
    bool enable_worklist{true};

    /// Enable MPPS service
    bool enable_mpps{true};

    /// Enable verbose logging
    bool verbose_logging{false};
};

// =============================================================================
// Statistics
// =============================================================================

/**
 * @brief Runtime statistics for Mini PACS operations
 *
 * Thread-safe counters tracking service usage and performance.
 */
struct mini_pacs_statistics {
    /// Total associations established since start
    std::atomic<int> associations_total{0};

    /// Currently active associations
    std::atomic<int> associations_active{0};

    /// C-ECHO operations processed
    std::atomic<int> c_echo_count{0};

    /// C-STORE operations processed
    std::atomic<int> c_store_count{0};

    /// C-FIND operations processed
    std::atomic<int> c_find_count{0};

    /// C-MOVE operations processed
    std::atomic<int> c_move_count{0};

    /// C-GET operations processed
    std::atomic<int> c_get_count{0};

    /// MWL queries processed
    std::atomic<int> mwl_count{0};

    /// MPPS N-CREATE processed
    std::atomic<int> mpps_create_count{0};

    /// MPPS N-SET processed
    std::atomic<int> mpps_set_count{0};

    /// Total bytes received
    std::atomic<size_t> bytes_received{0};
};

// =============================================================================
// Worklist Item
// =============================================================================

/**
 * @brief Worklist item for MWL queries
 *
 * Represents a scheduled procedure step that can be queried by modalities.
 */
struct worklist_entry {
    std::string patient_id;
    std::string patient_name;
    std::string patient_birth_date;
    std::string patient_sex;
    std::string study_uid;
    std::string accession_number;
    std::string modality;
    std::string scheduled_station_ae;
    std::string scheduled_date;
    std::string scheduled_time;
    std::string step_id;
    std::string procedure_description;
    std::string referring_physician;
};

// =============================================================================
// MPPS Instance
// =============================================================================

/**
 * @brief MPPS instance for tracking procedure progress
 */
struct mpps_entry {
    std::string sop_instance_uid;
    std::string status;  // "IN PROGRESS", "COMPLETED", "DISCONTINUED"
    std::string station_ae;
    core::dicom_dataset data;
};

// =============================================================================
// Mini PACS Class
// =============================================================================

/**
 * @brief Complete Mini PACS server integrating all DICOM services
 *
 * The mini_pacs class provides a fully functional PACS server with:
 * - Image storage and retrieval
 * - Patient/Study/Series level queries
 * - Modality worklist management
 * - MPPS procedure tracking
 *
 * ## Architecture
 *
 * ```
 * ┌─────────────────────────────────────────────────────────┐
 * │                      MiniPACS                           │
 * │                                                         │
 * │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
 * │  │  Storage    │  │   Index     │  │   Server    │     │
 * │  │  Backend    │  │  Database   │  │   Config    │     │
 * │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘     │
 * │         │                │                │            │
 * │  ┌──────▼────────────────▼────────────────▼──────┐     │
 * │  │                DICOM Server                   │     │
 * │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐         │     │
 * │  │  │Verify   │ │Storage  │ │Query    │         │     │
 * │  │  │SCP      │ │SCP      │ │SCP      │         │     │
 * │  │  └─────────┘ └─────────┘ └─────────┘         │     │
 * │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐         │     │
 * │  │  │Retrieve │ │Worklist │ │MPPS     │         │     │
 * │  │  │SCP      │ │SCP      │ │SCP      │         │     │
 * │  │  └─────────┘ └─────────┘ └─────────┘         │     │
 * │  └───────────────────────────────────────────────┘     │
 * └─────────────────────────────────────────────────────────┘
 * ```
 *
 * @example Basic Usage
 * @code
 * mini_pacs_config config;
 * config.ae_title = "MY_PACS";
 * config.port = 11112;
 *
 * mini_pacs pacs(config);
 *
 * if (!pacs.start()) {
 *     std::cerr << "Failed to start PACS\n";
 *     return 1;
 * }
 *
 * pacs.wait();  // Block until shutdown
 * @endcode
 */
class mini_pacs {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct Mini PACS with configuration
     * @param config Configuration options
     */
    explicit mini_pacs(const mini_pacs_config& config);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~mini_pacs();

    // Non-copyable, non-movable
    mini_pacs(const mini_pacs&) = delete;
    mini_pacs& operator=(const mini_pacs&) = delete;
    mini_pacs(mini_pacs&&) = delete;
    mini_pacs& operator=(mini_pacs&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Start the PACS server
     * @return true if started successfully
     */
    [[nodiscard]] bool start();

    /**
     * @brief Stop the PACS server
     */
    void stop();

    /**
     * @brief Block until server shutdown
     */
    void wait();

    /**
     * @brief Check if server is running
     * @return true if running
     */
    [[nodiscard]] bool is_running() const noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get current statistics
     * @return Reference to statistics structure
     */
    [[nodiscard]] const mini_pacs_statistics& statistics() const noexcept;

    // =========================================================================
    // Worklist Management
    // =========================================================================

    /**
     * @brief Add a worklist item for MWL queries
     * @param item The worklist entry to add
     */
    void add_worklist_item(const worklist_entry& item);

    /**
     * @brief Clear all worklist items
     */
    void clear_worklist();

    /**
     * @brief Get current worklist items
     * @return Vector of worklist entries
     */
    [[nodiscard]] std::vector<worklist_entry> get_worklist_items() const;

    // =========================================================================
    // Configuration Access
    // =========================================================================

    /**
     * @brief Get the configuration
     * @return Reference to configuration
     */
    [[nodiscard]] const mini_pacs_config& config() const noexcept;

private:
    // =========================================================================
    // Initialization
    // =========================================================================

    void initialize_storage();
    void initialize_services();
    void configure_server();
    void setup_event_handlers();

    // =========================================================================
    // Storage Handlers
    // =========================================================================

    services::storage_status handle_store(
        const core::dicom_dataset& dataset,
        const std::string& calling_ae,
        const std::string& sop_class_uid,
        const std::string& sop_instance_uid);

    bool update_index(
        const core::dicom_dataset& dataset,
        const std::filesystem::path& file_path);

    // =========================================================================
    // Query Handlers
    // =========================================================================

    std::vector<core::dicom_dataset> handle_query(
        services::query_level level,
        const core::dicom_dataset& query_keys,
        const std::string& calling_ae);

    // =========================================================================
    // Retrieve Handlers
    // =========================================================================

    std::vector<core::dicom_file> handle_retrieve(
        const core::dicom_dataset& query_keys);

    std::optional<std::pair<std::string, uint16_t>> resolve_destination(
        const std::string& ae_title);

    // =========================================================================
    // Worklist Handlers
    // =========================================================================

    std::vector<core::dicom_dataset> handle_worklist_query(
        const core::dicom_dataset& query_keys,
        const std::string& calling_ae);

    core::dicom_dataset create_worklist_response(const worklist_entry& item);

    // =========================================================================
    // MPPS Handlers
    // =========================================================================

    network::Result<std::monostate> handle_mpps_create(
        const services::mpps_instance& instance);

    network::Result<std::monostate> handle_mpps_set(
        const std::string& sop_instance_uid,
        const core::dicom_dataset& modifications,
        services::mpps_status new_status);

    // =========================================================================
    // Member Variables
    // =========================================================================

    mini_pacs_config config_;
    mini_pacs_statistics stats_;

    // Storage components
    std::shared_ptr<storage::file_storage> file_storage_;
    std::shared_ptr<storage::index_database> index_db_;

    // Service providers
    std::shared_ptr<services::verification_scp> verification_scp_;
    std::shared_ptr<services::storage_scp> storage_scp_;
    std::shared_ptr<services::query_scp> query_scp_;
    std::shared_ptr<services::retrieve_scp> retrieve_scp_;
    std::shared_ptr<services::worklist_scp> worklist_scp_;
    std::shared_ptr<services::mpps_scp> mpps_scp_;

    // Server
    std::unique_ptr<network::dicom_server> server_;
    std::atomic<bool> running_{false};

    // Worklist items (in-memory for this sample)
    mutable std::mutex worklist_mutex_;
    std::vector<worklist_entry> worklist_items_;

    // MPPS instances (in-memory for this sample)
    mutable std::mutex mpps_mutex_;
    std::vector<mpps_entry> mpps_instances_;
};

}  // namespace pacs::samples
