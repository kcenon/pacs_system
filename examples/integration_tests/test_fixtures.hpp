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

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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
    ds.set_string(core::tags::patient_name, encoding::vr_type::PN, "TEST^PATIENT");
    ds.set_string(core::tags::patient_id, encoding::vr_type::LO, "TEST001");
    ds.set_string(core::tags::patient_birth_date, encoding::vr_type::DA, "19800101");
    ds.set_string(core::tags::patient_sex, encoding::vr_type::CS, "M");

    // Study module
    ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI,
                  study_uid.empty() ? generate_uid() : study_uid);
    ds.set_string(core::tags::study_date, encoding::vr_type::DA, "20240101");
    ds.set_string(core::tags::study_time, encoding::vr_type::TM, "120000");
    ds.set_string(core::tags::accession_number, encoding::vr_type::SH, "ACC001");
    ds.set_string(core::tags::study_id, encoding::vr_type::SH, "STUDY001");
    ds.set_string(core::tags::study_description, encoding::vr_type::LO, "Integration Test Study");

    // Series module
    ds.set_string(core::tags::series_instance_uid, encoding::vr_type::UI,
                  series_uid.empty() ? generate_uid() : series_uid);
    ds.set_string(core::tags::modality, encoding::vr_type::CS, "CT");
    ds.set_string(core::tags::series_number, encoding::vr_type::IS, "1");
    ds.set_string(core::tags::series_description, encoding::vr_type::LO, "Test Series");

    // SOP Common module
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");  // CT Image Storage
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI,
                  instance_uid.empty() ? generate_uid() : instance_uid);

    // Image module (minimal)
    ds.set_numeric<uint16_t>(core::tags::rows, encoding::vr_type::US, 64);
    ds.set_numeric<uint16_t>(core::tags::columns, encoding::vr_type::US, 64);
    ds.set_numeric<uint16_t>(core::tags::bits_allocated, encoding::vr_type::US, 16);
    ds.set_numeric<uint16_t>(core::tags::bits_stored, encoding::vr_type::US, 12);
    ds.set_numeric<uint16_t>(core::tags::high_bit, encoding::vr_type::US, 11);
    ds.set_numeric<uint16_t>(core::tags::pixel_representation, encoding::vr_type::US, 0);
    ds.set_numeric<uint16_t>(core::tags::samples_per_pixel, encoding::vr_type::US, 1);
    ds.set_string(core::tags::photometric_interpretation, encoding::vr_type::CS, "MONOCHROME2");

    // Generate minimal pixel data (64x64 16-bit)
    std::vector<uint16_t> pixel_data(64 * 64, 512);
    core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OW);
    pixel_elem.set_value(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(pixel_data.data()),
        pixel_data.size() * sizeof(uint16_t)));
    ds.insert(std::move(pixel_elem));

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
    ds.set_string(core::tags::patient_name, encoding::vr_type::PN, "TEST^MR^PATIENT");
    ds.set_string(core::tags::patient_id, encoding::vr_type::LO, "TESTMR001");
    ds.set_string(core::tags::patient_birth_date, encoding::vr_type::DA, "19900215");
    ds.set_string(core::tags::patient_sex, encoding::vr_type::CS, "F");

    // Study module
    ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI,
                  study_uid.empty() ? generate_uid() : study_uid);
    ds.set_string(core::tags::study_date, encoding::vr_type::DA, "20240115");
    ds.set_string(core::tags::study_time, encoding::vr_type::TM, "140000");
    ds.set_string(core::tags::accession_number, encoding::vr_type::SH, "ACCMR001");
    ds.set_string(core::tags::study_id, encoding::vr_type::SH, "STUDYMR001");
    ds.set_string(core::tags::study_description, encoding::vr_type::LO, "MR Integration Test");

    // Series module
    ds.set_string(core::tags::series_instance_uid, encoding::vr_type::UI, generate_uid());
    ds.set_string(core::tags::modality, encoding::vr_type::CS, "MR");
    ds.set_string(core::tags::series_number, encoding::vr_type::IS, "1");
    ds.set_string(core::tags::series_description, encoding::vr_type::LO, "T1 FLAIR");

    // SOP Common module
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI, "1.2.840.10008.5.1.4.1.1.4");  // MR Image Storage
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image module (minimal)
    ds.set_numeric<uint16_t>(core::tags::rows, encoding::vr_type::US, 64);
    ds.set_numeric<uint16_t>(core::tags::columns, encoding::vr_type::US, 64);
    ds.set_numeric<uint16_t>(core::tags::bits_allocated, encoding::vr_type::US, 16);
    ds.set_numeric<uint16_t>(core::tags::bits_stored, encoding::vr_type::US, 12);
    ds.set_numeric<uint16_t>(core::tags::high_bit, encoding::vr_type::US, 11);
    ds.set_numeric<uint16_t>(core::tags::pixel_representation, encoding::vr_type::US, 0);
    ds.set_numeric<uint16_t>(core::tags::samples_per_pixel, encoding::vr_type::US, 1);
    ds.set_string(core::tags::photometric_interpretation, encoding::vr_type::CS, "MONOCHROME2");

    // Generate minimal pixel data
    std::vector<uint16_t> pixel_data(64 * 64, 256);
    core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OW);
    pixel_elem.set_value(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(pixel_data.data()),
        pixel_data.size() * sizeof(uint16_t)));
    ds.insert(std::move(pixel_elem));

    return ds;
}

/**
 * @brief Generate a XA (X-Ray Angiographic) image dataset for testing
 * @param study_uid Study Instance UID (generated if empty)
 * @return DICOM dataset
 */
inline core::dicom_dataset generate_xa_dataset(const std::string& study_uid = "") {
    core::dicom_dataset ds;

    // Patient module
    ds.set_string(core::tags::patient_name, encoding::vr_type::PN, "TEST^XA^PATIENT");
    ds.set_string(core::tags::patient_id, encoding::vr_type::LO, "TESTXA001");
    ds.set_string(core::tags::patient_birth_date, encoding::vr_type::DA, "19750610");
    ds.set_string(core::tags::patient_sex, encoding::vr_type::CS, "F");

    // Study module
    ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI,
                  study_uid.empty() ? generate_uid() : study_uid);
    ds.set_string(core::tags::study_date, encoding::vr_type::DA, "20240220");
    ds.set_string(core::tags::study_time, encoding::vr_type::TM, "103000");
    ds.set_string(core::tags::accession_number, encoding::vr_type::SH, "ACCXA001");
    ds.set_string(core::tags::study_id, encoding::vr_type::SH, "STUDYXA001");
    ds.set_string(core::tags::study_description, encoding::vr_type::LO, "XA Integration Test");

    // Series module
    ds.set_string(core::tags::series_instance_uid, encoding::vr_type::UI, generate_uid());
    ds.set_string(core::tags::modality, encoding::vr_type::CS, "XA");
    ds.set_string(core::tags::series_number, encoding::vr_type::IS, "1");
    ds.set_string(core::tags::series_description, encoding::vr_type::LO, "Coronary Angio");

    // SOP Common module
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI, "1.2.840.10008.5.1.4.1.1.12.1");  // XA Image Storage
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image module
    ds.set_numeric<uint16_t>(core::tags::rows, encoding::vr_type::US, 512);
    ds.set_numeric<uint16_t>(core::tags::columns, encoding::vr_type::US, 512);
    ds.set_numeric<uint16_t>(core::tags::bits_allocated, encoding::vr_type::US, 16);
    ds.set_numeric<uint16_t>(core::tags::bits_stored, encoding::vr_type::US, 12);
    ds.set_numeric<uint16_t>(core::tags::high_bit, encoding::vr_type::US, 11);
    ds.set_numeric<uint16_t>(core::tags::pixel_representation, encoding::vr_type::US, 0);
    ds.set_numeric<uint16_t>(core::tags::samples_per_pixel, encoding::vr_type::US, 1);
    ds.set_string(core::tags::photometric_interpretation, encoding::vr_type::CS, "MONOCHROME2");

    // XA Specific
    // Note: Using raw tag numbers if constants are not defined yet
    // Positioner Primary Angle
    ds.set_string({0x0018, 0x1510}, encoding::vr_type::DS, "0");
    // Positioner Secondary Angle
    ds.set_string({0x0018, 0x1511}, encoding::vr_type::DS, "0");
    // KVP
    ds.set_string({0x0018, 0x0060}, encoding::vr_type::DS, "80");
    // X-Ray Tube Current
    ds.set_numeric<uint16_t>({0x0018, 0x1151}, encoding::vr_type::IS, 500);
    // Exposure Time
    ds.set_numeric<uint16_t>({0x0018, 0x1150}, encoding::vr_type::IS, 100);

    // Generate minimal pixel data
    std::vector<uint16_t> pixel_data(512 * 512, 128);
    core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OW);
    pixel_elem.set_value(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(pixel_data.data()),
        pixel_data.size() * sizeof(uint16_t)));
    ds.insert(std::move(pixel_elem));

    return ds;
}

/**
 * @brief Generate a worklist item dataset
 * @return DICOM dataset for worklist
 */
inline core::dicom_dataset generate_worklist_item() {
    core::dicom_dataset ds;

    // Patient module
    ds.set_string(core::tags::patient_name, encoding::vr_type::PN, "WORKLIST^PATIENT");
    ds.set_string(core::tags::patient_id, encoding::vr_type::LO, "WL001");
    ds.set_string(core::tags::patient_birth_date, encoding::vr_type::DA, "19850520");
    ds.set_string(core::tags::patient_sex, encoding::vr_type::CS, "M");

    // Scheduled Procedure Step
    ds.set_string(core::tags::scheduled_procedure_step_start_date, encoding::vr_type::DA, "20240201");
    ds.set_string(core::tags::scheduled_procedure_step_start_time, encoding::vr_type::TM, "090000");
    ds.set_string(core::tags::modality, encoding::vr_type::CS, "CT");
    ds.set_string(core::tags::scheduled_station_ae_title, encoding::vr_type::AE, "CT_SCANNER");
    ds.set_string(core::tags::scheduled_procedure_step_description, encoding::vr_type::LO, "CT Chest");

    // Requested Procedure
    ds.set_string(core::tags::requested_procedure_id, encoding::vr_type::SH, "RP001");
    ds.set_string(core::tags::accession_number, encoding::vr_type::SH, "WLACC001");
    ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI, generate_uid());

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

// =============================================================================
// Process Launcher for Binary Integration Tests
// =============================================================================

/**
 * @brief Result of a process execution
 */
struct process_result {
    int exit_code{-1};                      ///< Process exit code
    std::string stdout_output;               ///< Standard output
    std::string stderr_output;               ///< Standard error
    std::chrono::milliseconds duration{0};   ///< Execution duration
    bool timed_out{false};                   ///< Whether the process timed out
};

/**
 * @brief Cross-platform process launcher for binary integration testing
 *
 * Provides utilities to run external processes, capture their output,
 * and manage background processes for integration tests.
 *
 * @see Issue #136 - Binary Integration Test Scripts
 */
class process_launcher {
public:
    /**
     * @brief Run a process and wait for completion
     * @param executable Path to executable
     * @param args Command line arguments
     * @param timeout Maximum execution time
     * @return Process execution result
     */
    static process_result run(
        const std::string& executable,
        const std::vector<std::string>& args = {},
        std::chrono::seconds timeout = std::chrono::seconds{30}) {

        process_result result;
        auto start_time = std::chrono::steady_clock::now();

#ifdef _WIN32
        result = run_windows(executable, args, timeout);
#else
        result = run_posix(executable, args, timeout);
#endif

        auto end_time = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        return result;
    }

    /**
     * @brief Start a process in the background
     * @param executable Path to executable
     * @param args Command line arguments
     * @return Process ID (pid_t on POSIX, DWORD on Windows), or -1 on error
     */
#ifdef _WIN32
    using pid_type = DWORD;
#else
    using pid_type = pid_t;
#endif

    static pid_type start_background(
        const std::string& executable,
        const std::vector<std::string>& args = {}) {

#ifdef _WIN32
        return start_background_windows(executable, args);
#else
        return start_background_posix(executable, args);
#endif
    }

    /**
     * @brief Stop a background process
     * @param pid Process ID to stop
     * @return true if process was stopped successfully
     */
    static bool stop_background(pid_type pid) {
#ifdef _WIN32
        return stop_background_windows(pid);
#else
        return stop_background_posix(pid);
#endif
    }

    /**
     * @brief Check if a process is still running
     * @param pid Process ID to check
     * @return true if process is running
     */
    static bool is_running(pid_type pid) {
#ifdef _WIN32
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (process == nullptr) {
            return false;
        }
        DWORD exit_code;
        if (GetExitCodeProcess(process, &exit_code)) {
            CloseHandle(process);
            return exit_code == STILL_ACTIVE;
        }
        CloseHandle(process);
        return false;
#else
        return kill(pid, 0) == 0;
#endif
    }

    /**
     * @brief Wait for a port to be listening
     * @param port Port number to check
     * @param timeout Maximum wait time
     * @param host Host to check (default: localhost)
     * @return true if port is listening before timeout
     */
    static bool wait_for_port(
        uint16_t port,
        std::chrono::seconds timeout = std::chrono::seconds{10},
        const std::string& host = "127.0.0.1") {

        auto start = std::chrono::steady_clock::now();
        auto interval = std::chrono::milliseconds{100};

        while (true) {
            if (is_port_listening(port, host)) {
                return true;
            }

            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= timeout) {
                return false;
            }

            std::this_thread::sleep_for(interval);
        }
    }

    /**
     * @brief Check if a port is currently listening
     * @param port Port number
     * @param host Host address
     * @return true if port is accepting connections
     */
    static bool is_port_listening(uint16_t port, const std::string& host = "127.0.0.1") {
#ifdef _WIN32
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            return false;
        }

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        // Set non-blocking
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(host.c_str());

        int result = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);

        timeval tv{0, 100000};  // 100ms timeout
        result = select(0, nullptr, &writefds, nullptr, &tv);

        closesocket(sock);
        WSACleanup();

        return result > 0;
#else
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return false;
        }

        // Set socket to non-blocking
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            close(sock);
            return false;
        }

        int result = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        if (result == 0) {
            close(sock);
            return true;
        }

        if (errno == EINPROGRESS) {
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(sock, &writefds);

            timeval tv{0, 100000};  // 100ms timeout
            result = select(sock + 1, nullptr, &writefds, nullptr, &tv);

            if (result > 0) {
                int error = 0;
                socklen_t len = sizeof(error);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
                close(sock);
                return error == 0;
            }
        }

        close(sock);
        return false;
#endif
    }

private:
#ifndef _WIN32
    // POSIX implementation
    static process_result run_posix(
        const std::string& executable,
        const std::vector<std::string>& args,
        std::chrono::seconds timeout) {

        process_result result;

        // Create pipes for stdout and stderr
        int stdout_pipe[2];
        int stderr_pipe[2];

        if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
            result.exit_code = -1;
            result.stderr_output = "Failed to create pipes";
            return result;
        }

        pid_t pid = fork();

        if (pid < 0) {
            result.exit_code = -1;
            result.stderr_output = "Failed to fork";
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
            return result;
        }

        if (pid == 0) {
            // Child process
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);

            dup2(stdout_pipe[1], STDOUT_FILENO);
            dup2(stderr_pipe[1], STDERR_FILENO);

            close(stdout_pipe[1]);
            close(stderr_pipe[1]);

            // Build argv
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(executable.c_str()));
            for (const auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            execv(executable.c_str(), argv.data());
            _exit(127);  // execv failed
        }

        // Parent process
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Set pipes to non-blocking
        fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

        auto start = std::chrono::steady_clock::now();
        int status = 0;
        bool child_exited = false;

        std::array<char, 4096> buffer{};

        while (!child_exited) {
            // Check timeout
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= timeout) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                result.timed_out = true;
                result.exit_code = -1;
                break;
            }

            // Check if child has exited
            pid_t wait_result = waitpid(pid, &status, WNOHANG);
            if (wait_result > 0) {
                child_exited = true;
                if (WIFEXITED(status)) {
                    result.exit_code = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    result.exit_code = -WTERMSIG(status);
                }
            } else if (wait_result < 0) {
                break;
            }

            // Read available output
            ssize_t n;
            while ((n = read(stdout_pipe[0], buffer.data(), buffer.size() - 1)) > 0) {
                buffer[static_cast<size_t>(n)] = '\0';
                result.stdout_output += buffer.data();
            }
            while ((n = read(stderr_pipe[0], buffer.data(), buffer.size() - 1)) > 0) {
                buffer[static_cast<size_t>(n)] = '\0';
                result.stderr_output += buffer.data();
            }

            if (!child_exited) {
                std::this_thread::sleep_for(std::chrono::milliseconds{10});
            }
        }

        // Read any remaining output
        ssize_t n;
        while ((n = read(stdout_pipe[0], buffer.data(), buffer.size() - 1)) > 0) {
            buffer[static_cast<size_t>(n)] = '\0';
            result.stdout_output += buffer.data();
        }
        while ((n = read(stderr_pipe[0], buffer.data(), buffer.size() - 1)) > 0) {
            buffer[static_cast<size_t>(n)] = '\0';
            result.stderr_output += buffer.data();
        }

        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        return result;
    }

    static pid_t start_background_posix(
        const std::string& executable,
        const std::vector<std::string>& args) {

        pid_t pid = fork();

        if (pid < 0) {
            return -1;
        }

        if (pid == 0) {
            // Child process
            // Redirect stdout and stderr to /dev/null
            int null_fd = open("/dev/null", O_RDWR);
            if (null_fd >= 0) {
                dup2(null_fd, STDOUT_FILENO);
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);
            }

            // Create new session to detach from terminal
            setsid();

            // Build argv
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(executable.c_str()));
            for (const auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            execv(executable.c_str(), argv.data());
            _exit(127);
        }

        return pid;
    }

    static bool stop_background_posix(pid_t pid) {
        if (pid <= 0) {
            return false;
        }

        // Send SIGTERM first for graceful shutdown
        if (kill(pid, SIGTERM) != 0) {
            return errno == ESRCH;  // Process already gone
        }

        // Wait for process to terminate (up to 5 seconds)
        for (int i = 0; i < 50; ++i) {
            int status;
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result > 0 || (result < 0 && errno == ECHILD)) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }

        // Force kill if still running
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);

        return true;
    }
#else
    // Windows implementation
    static process_result run_windows(
        const std::string& executable,
        const std::vector<std::string>& args,
        std::chrono::seconds timeout) {

        process_result result;

        // Build command line
        std::ostringstream cmd;
        cmd << "\"" << executable << "\"";
        for (const auto& arg : args) {
            cmd << " \"" << arg << "\"";
        }
        std::string cmd_line = cmd.str();

        // Create pipes for stdout and stderr
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE stdout_read, stdout_write;
        HANDLE stderr_read, stderr_write;

        if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
            !CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
            result.exit_code = -1;
            result.stderr_output = "Failed to create pipes";
            return result;
        }

        SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = stdout_write;
        si.hStdError = stderr_write;
        si.hStdInput = nullptr;

        PROCESS_INFORMATION pi{};

        if (!CreateProcessA(
                nullptr,
                const_cast<char*>(cmd_line.c_str()),
                nullptr,
                nullptr,
                TRUE,
                0,
                nullptr,
                nullptr,
                &si,
                &pi)) {
            result.exit_code = -1;
            result.stderr_output = "Failed to create process";
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            CloseHandle(stderr_read);
            CloseHandle(stderr_write);
            return result;
        }

        CloseHandle(stdout_write);
        CloseHandle(stderr_write);

        // Wait for process with timeout
        DWORD timeout_ms = static_cast<DWORD>(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
        DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms);

        if (wait_result == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, 1);
            result.timed_out = true;
            result.exit_code = -1;
        } else {
            DWORD exit_code;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            result.exit_code = static_cast<int>(exit_code);
        }

        // Read output
        char buffer[4096];
        DWORD bytes_read;

        while (ReadFile(stdout_read, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            result.stdout_output += buffer;
        }

        while (ReadFile(stderr_read, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            result.stderr_output += buffer;
        }

        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return result;
    }

    static DWORD start_background_windows(
        const std::string& executable,
        const std::vector<std::string>& args) {

        std::ostringstream cmd;
        cmd << "\"" << executable << "\"";
        for (const auto& arg : args) {
            cmd << " \"" << arg << "\"";
        }
        std::string cmd_line = cmd.str();

        STARTUPINFOA si{};
        si.cb = sizeof(si);

        PROCESS_INFORMATION pi{};

        if (!CreateProcessA(
                nullptr,
                const_cast<char*>(cmd_line.c_str()),
                nullptr,
                nullptr,
                FALSE,
                DETACHED_PROCESS,
                nullptr,
                nullptr,
                &si,
                &pi)) {
            return 0;
        }

        CloseHandle(pi.hThread);
        DWORD pid = pi.dwProcessId;
        CloseHandle(pi.hProcess);

        return pid;
    }

    static bool stop_background_windows(DWORD pid) {
        if (pid == 0) {
            return false;
        }

        HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (process == nullptr) {
            return GetLastError() == ERROR_INVALID_PARAMETER;  // Process already gone
        }

        TerminateProcess(process, 0);
        WaitForSingleObject(process, 5000);
        CloseHandle(process);

        return true;
    }
#endif
};

/**
 * @brief RAII wrapper for a background process
 *
 * Automatically stops the process when the guard goes out of scope.
 */
class background_process_guard {
public:
    explicit background_process_guard(process_launcher::pid_type pid = -1) : pid_(pid) {}

    ~background_process_guard() {
        stop();
    }

    // Non-copyable
    background_process_guard(const background_process_guard&) = delete;
    background_process_guard& operator=(const background_process_guard&) = delete;

    // Movable
    background_process_guard(background_process_guard&& other) noexcept
        : pid_(other.pid_) {
        other.pid_ = -1;
    }

    background_process_guard& operator=(background_process_guard&& other) noexcept {
        if (this != &other) {
            stop();
            pid_ = other.pid_;
            other.pid_ = -1;
        }
        return *this;
    }

    /// @brief Set the process ID
    void set_pid(process_launcher::pid_type pid) { pid_ = pid; }

    /// @return Process ID
    [[nodiscard]] process_launcher::pid_type pid() const noexcept { return pid_; }

    /// @brief Check if process is running
    [[nodiscard]] bool is_running() const {
        return pid_ > 0 && process_launcher::is_running(pid_);
    }

    /// @brief Stop the process
    void stop() {
        if (pid_ > 0) {
            process_launcher::stop_background(pid_);
            pid_ = -1;
        }
    }

    /// @brief Release ownership without stopping
    process_launcher::pid_type release() {
        auto p = pid_;
        pid_ = -1;
        return p;
    }

private:
    process_launcher::pid_type pid_{-1};
};

}  // namespace pacs::integration_test

#endif  // PACS_INTEGRATION_TESTS_TEST_FIXTURES_HPP
