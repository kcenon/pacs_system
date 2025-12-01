/**
 * @file test_fixtures.hpp
 * @brief Common test fixtures and utilities for integration tests
 *
 * Provides reusable test fixtures, DICOM data generators, and utility
 * functions for integration testing of PACS system components.
 *
 * @see Issue #111 - Integration Test Suite
 */

#ifndef PACS_INTEGRATION_TESTS_TEST_FIXTURES_HPP
#define PACS_INTEGRATION_TESTS_TEST_FIXTURES_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"
#include "pacs/services/verification_scp.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace pacs::integration_test {

// =============================================================================
// Constants
// =============================================================================

/// Default test port range start (use high ports to avoid conflicts)
constexpr uint16_t default_test_port = 41104;

/// Default timeout for test operations
constexpr auto default_timeout = std::chrono::milliseconds{5000};

/// Default AE titles
constexpr const char* test_scp_ae_title = "TEST_SCP";
constexpr const char* test_scu_ae_title = "TEST_SCU";

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Generate a unique UID for testing
 * @param root Root UID prefix
 * @return Unique UID string
 */
inline std::string generate_uid(const std::string& root = "1.2.826.0.1.3680043.9.9999") {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return root + "." + std::to_string(timestamp) + "." + std::to_string(++counter);
}

/**
 * @brief Find an available port for testing
 * @param start Starting port number
 * @return Available port number
 */
inline uint16_t find_available_port(uint16_t start = default_test_port) {
    // Simple increment strategy - in practice, could test with socket binding
    static std::atomic<uint16_t> port_offset{0};
    return start + (port_offset++ % 100);
}

/**
 * @brief Wait for a condition with timeout
 * @param condition Condition function
 * @param timeout Maximum wait time
 * @param interval Check interval
 * @return true if condition became true before timeout
 */
template <typename Func>
bool wait_for(
    Func&& condition,
    std::chrono::milliseconds timeout = default_timeout,
    std::chrono::milliseconds interval = std::chrono::milliseconds{50}) {

    auto start = std::chrono::steady_clock::now();
    while (!condition()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout) {
            return false;
        }
        std::this_thread::sleep_for(interval);
    }
    return true;
}

// =============================================================================
// DICOM Dataset Generators
// =============================================================================

/**
 * @brief Generate a minimal CT image dataset for testing
 * @param study_uid Study Instance UID (generated if empty)
 * @param series_uid Series Instance UID (generated if empty)
 * @param instance_uid SOP Instance UID (generated if empty)
 * @return DICOM dataset
 */
inline core::dicom_dataset generate_ct_dataset(
    const std::string& study_uid = "",
    const std::string& series_uid = "",
    const std::string& instance_uid = "") {

    core::dicom_dataset ds;

    // Patient module
    ds.set_string(core::tags::patient_name, "TEST^PATIENT");
    ds.set_string(core::tags::patient_id, "TEST001");
    ds.set_string(core::tags::patient_birth_date, "19800101");
    ds.set_string(core::tags::patient_sex, "M");

    // Study module
    ds.set_string(core::tags::study_instance_uid,
                  study_uid.empty() ? generate_uid() : study_uid);
    ds.set_string(core::tags::study_date, "20240101");
    ds.set_string(core::tags::study_time, "120000");
    ds.set_string(core::tags::accession_number, "ACC001");
    ds.set_string(core::tags::study_id, "STUDY001");
    ds.set_string(core::tags::study_description, "Integration Test Study");

    // Series module
    ds.set_string(core::tags::series_instance_uid,
                  series_uid.empty() ? generate_uid() : series_uid);
    ds.set_string(core::tags::modality, "CT");
    ds.set_string(core::tags::series_number, "1");
    ds.set_string(core::tags::series_description, "Test Series");

    // SOP Common module
    ds.set_string(core::tags::sop_class_uid, "1.2.840.10008.5.1.4.1.1.2");  // CT Image Storage
    ds.set_string(core::tags::sop_instance_uid,
                  instance_uid.empty() ? generate_uid() : instance_uid);

    // Image module (minimal)
    ds.set_uint16(core::tags::rows, 64);
    ds.set_uint16(core::tags::columns, 64);
    ds.set_uint16(core::tags::bits_allocated, 16);
    ds.set_uint16(core::tags::bits_stored, 12);
    ds.set_uint16(core::tags::high_bit, 11);
    ds.set_uint16(core::tags::pixel_representation, 0);
    ds.set_uint16(core::tags::samples_per_pixel, 1);
    ds.set_string(core::tags::photometric_interpretation, "MONOCHROME2");

    // Generate minimal pixel data (64x64 16-bit)
    std::vector<uint16_t> pixel_data(64 * 64, 512);
    ds.set_pixel_data(reinterpret_cast<const uint8_t*>(pixel_data.data()),
                      pixel_data.size() * sizeof(uint16_t));

    return ds;
}

/**
 * @brief Generate a MR image dataset for testing
 * @param study_uid Study Instance UID (generated if empty)
 * @return DICOM dataset
 */
inline core::dicom_dataset generate_mr_dataset(const std::string& study_uid = "") {
    core::dicom_dataset ds;

    // Patient module
    ds.set_string(core::tags::patient_name, "TEST^MR^PATIENT");
    ds.set_string(core::tags::patient_id, "TESTMR001");
    ds.set_string(core::tags::patient_birth_date, "19900215");
    ds.set_string(core::tags::patient_sex, "F");

    // Study module
    ds.set_string(core::tags::study_instance_uid,
                  study_uid.empty() ? generate_uid() : study_uid);
    ds.set_string(core::tags::study_date, "20240115");
    ds.set_string(core::tags::study_time, "140000");
    ds.set_string(core::tags::accession_number, "ACCMR001");
    ds.set_string(core::tags::study_id, "STUDYMR001");
    ds.set_string(core::tags::study_description, "MR Integration Test");

    // Series module
    ds.set_string(core::tags::series_instance_uid, generate_uid());
    ds.set_string(core::tags::modality, "MR");
    ds.set_string(core::tags::series_number, "1");
    ds.set_string(core::tags::series_description, "T1 FLAIR");

    // SOP Common module
    ds.set_string(core::tags::sop_class_uid, "1.2.840.10008.5.1.4.1.1.4");  // MR Image Storage
    ds.set_string(core::tags::sop_instance_uid, generate_uid());

    // Image module (minimal)
    ds.set_uint16(core::tags::rows, 64);
    ds.set_uint16(core::tags::columns, 64);
    ds.set_uint16(core::tags::bits_allocated, 16);
    ds.set_uint16(core::tags::bits_stored, 12);
    ds.set_uint16(core::tags::high_bit, 11);
    ds.set_uint16(core::tags::pixel_representation, 0);
    ds.set_uint16(core::tags::samples_per_pixel, 1);
    ds.set_string(core::tags::photometric_interpretation, "MONOCHROME2");

    // Generate minimal pixel data
    std::vector<uint16_t> pixel_data(64 * 64, 256);
    ds.set_pixel_data(reinterpret_cast<const uint8_t*>(pixel_data.data()),
                      pixel_data.size() * sizeof(uint16_t));

    return ds;
}

/**
 * @brief Generate a worklist item dataset
 * @return DICOM dataset for worklist
 */
inline core::dicom_dataset generate_worklist_item() {
    core::dicom_dataset ds;

    // Patient module
    ds.set_string(core::tags::patient_name, "WORKLIST^PATIENT");
    ds.set_string(core::tags::patient_id, "WL001");
    ds.set_string(core::tags::patient_birth_date, "19850520");
    ds.set_string(core::tags::patient_sex, "M");

    // Scheduled Procedure Step
    ds.set_string(core::tags::scheduled_procedure_step_start_date, "20240201");
    ds.set_string(core::tags::scheduled_procedure_step_start_time, "090000");
    ds.set_string(core::tags::modality, "CT");
    ds.set_string(core::tags::scheduled_station_ae_title, "CT_SCANNER");
    ds.set_string(core::tags::scheduled_procedure_step_description, "CT Chest");

    // Requested Procedure
    ds.set_string(core::tags::requested_procedure_id, "RP001");
    ds.set_string(core::tags::accession_number, "WLACC001");
    ds.set_string(core::tags::study_instance_uid, generate_uid());

    return ds;
}

// =============================================================================
// Test Server Fixture
// =============================================================================

/**
 * @brief RAII wrapper for a test DICOM server
 *
 * Provides automatic server lifecycle management for tests.
 * Server is started on construction and stopped on destruction.
 */
class test_server {
public:
    /**
     * @brief Construct and start a test server
     * @param port Port to listen on (0 = auto-select)
     * @param ae_title AE title for server
     */
    explicit test_server(
        uint16_t port = 0,
        const std::string& ae_title = test_scp_ae_title)
        : port_(port == 0 ? find_available_port() : port)
        , ae_title_(ae_title) {

        network::server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 20;
        config.idle_timeout = std::chrono::seconds{60};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.1";
        config.implementation_version_name = "TEST_SCP";

        server_ = std::make_unique<network::dicom_server>(config);
    }

    ~test_server() {
        stop();
    }

    // Non-copyable, non-movable
    test_server(const test_server&) = delete;
    test_server& operator=(const test_server&) = delete;
    test_server(test_server&&) = delete;
    test_server& operator=(test_server&&) = delete;

    /**
     * @brief Register a service provider
     * @param service Service provider to register
     */
    template <typename Service>
    void register_service(std::shared_ptr<Service> service) {
        server_->register_service(std::move(service));
    }

    /**
     * @brief Start the server
     * @return true if server started successfully
     */
    [[nodiscard]] bool start() {
        auto result = server_->start();
        if (result.is_ok()) {
            running_ = true;
            // Give server time to start accepting connections
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
        return result.is_ok();
    }

    /**
     * @brief Stop the server
     */
    void stop() {
        if (running_) {
            server_->stop();
            running_ = false;
        }
    }

    /// @return Server port
    [[nodiscard]] uint16_t port() const noexcept { return port_; }

    /// @return Server AE title
    [[nodiscard]] const std::string& ae_title() const noexcept { return ae_title_; }

    /// @return true if server is running
    [[nodiscard]] bool is_running() const noexcept { return running_; }

    /// @return Reference to underlying server
    [[nodiscard]] network::dicom_server& server() { return *server_; }

private:
    uint16_t port_;
    std::string ae_title_;
    std::unique_ptr<network::dicom_server> server_;
    bool running_{false};
};

// =============================================================================
// Test Association Helper
// =============================================================================

/**
 * @brief Helper for establishing test associations
 */
class test_association {
public:
    /**
     * @brief Connect to a test server
     * @param host Server host
     * @param port Server port
     * @param called_ae Called AE title
     * @param calling_ae Calling AE title
     * @param sop_classes SOP classes to propose
     * @return Result with association or error
     */
    static network::Result<network::association> connect(
        const std::string& host,
        uint16_t port,
        const std::string& called_ae,
        const std::string& calling_ae = test_scu_ae_title,
        const std::vector<std::string>& sop_classes = {"1.2.840.10008.1.1"}) {

        network::association_config config;
        config.calling_ae_title = calling_ae;
        config.called_ae_title = called_ae;
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.2";
        config.implementation_version_name = "TEST_SCU";

        uint8_t context_id = 1;
        for (const auto& sop_class : sop_classes) {
            config.proposed_contexts.push_back({
                context_id,
                sop_class,
                {
                    "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
                    "1.2.840.10008.1.2"     // Implicit VR Little Endian
                }
            });
            context_id += 2;
        }

        return network::association::connect(host, port, config, default_timeout);
    }
};

// =============================================================================
// Test Data Directory
// =============================================================================

/**
 * @brief RAII wrapper for temporary test directory
 */
class test_directory {
public:
    explicit test_directory(const std::string& prefix = "pacs_test_") {
        auto temp_base = std::filesystem::temp_directory_path();
        path_ = temp_base / (prefix + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path_);
    }

    ~test_directory() {
        if (std::filesystem::exists(path_)) {
            std::filesystem::remove_all(path_);
        }
    }

    // Non-copyable, non-movable
    test_directory(const test_directory&) = delete;
    test_directory& operator=(const test_directory&) = delete;
    test_directory(test_directory&&) = delete;
    test_directory& operator=(test_directory&&) = delete;

    /// @return Path to test directory
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    /// @return Path as string
    [[nodiscard]] std::string string() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

// =============================================================================
// Test Result Counters
// =============================================================================

/**
 * @brief Thread-safe test result counter
 */
class test_counter {
public:
    void increment_success() { ++success_; }
    void increment_failure() { ++failure_; }
    void increment_warning() { ++warning_; }

    [[nodiscard]] size_t success() const noexcept { return success_.load(); }
    [[nodiscard]] size_t failure() const noexcept { return failure_.load(); }
    [[nodiscard]] size_t warning() const noexcept { return warning_.load(); }
    [[nodiscard]] size_t total() const noexcept {
        return success_.load() + failure_.load() + warning_.load();
    }

    void reset() {
        success_ = 0;
        failure_ = 0;
        warning_ = 0;
    }

private:
    std::atomic<size_t> success_{0};
    std::atomic<size_t> failure_{0};
    std::atomic<size_t> warning_{0};
};

}  // namespace pacs::integration_test

#endif  // PACS_INTEGRATION_TESTS_TEST_FIXTURES_HPP
