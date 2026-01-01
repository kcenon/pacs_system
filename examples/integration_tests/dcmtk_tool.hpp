/**
 * @file dcmtk_tool.hpp
 * @brief C++ wrapper for DCMTK command-line tools
 *
 * Provides reusable utilities for launching and managing DCMTK CLI tools
 * (echoscu, storescu, findscu, movescu, storescp) within the pacs_system
 * test infrastructure.
 *
 * @see Issue #450 - DCMTK Process Launcher and Test Utilities
 * @see Issue #449 - DCMTK Interoperability Test Automation Epic
 */

#ifndef PACS_INTEGRATION_TESTS_DCMTK_TOOL_HPP
#define PACS_INTEGRATION_TESTS_DCMTK_TOOL_HPP

#include "test_fixtures.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace pacs::integration_test {

// =============================================================================
// DCMTK Result Structure
// =============================================================================

/**
 * @brief Result of a DCMTK tool execution
 */
struct dcmtk_result {
    int exit_code{-1};                       ///< Process exit code
    std::string stdout_output;               ///< Standard output
    std::string stderr_output;               ///< Standard error
    std::chrono::milliseconds duration{0};   ///< Execution duration
    bool timed_out{false};                   ///< Whether the process timed out

    /// @return true if command succeeded (exit code 0)
    [[nodiscard]] bool success() const noexcept { return exit_code == 0; }

    /// @return true if stderr contains output
    [[nodiscard]] bool has_error() const noexcept { return !stderr_output.empty(); }
};

// =============================================================================
// DCMTK Tool Wrapper
// =============================================================================

/**
 * @brief Wrapper class for DCMTK command-line tools
 *
 * Provides static methods to invoke DCMTK CLI tools with standardized
 * timeout handling, error checking, and output parsing.
 */
class dcmtk_tool {
public:
    // -------------------------------------------------------------------------
    // Availability and Version
    // -------------------------------------------------------------------------

    /**
     * @brief Check if DCMTK is available on the system
     * @return true if echoscu command is found in PATH
     */
    static bool is_available() {
        auto result = run_tool("echoscu", {"--version"}, std::chrono::seconds{5});
        return result.exit_code == 0;
    }

    /**
     * @brief Get DCMTK version string
     * @return Version string if available, nullopt otherwise
     */
    static std::optional<std::string> version() {
        auto result = run_tool("echoscu", {"--version"}, std::chrono::seconds{5});
        if (result.exit_code != 0) {
            return std::nullopt;
        }

        // Parse version from output (first line usually contains version)
        auto& output = result.stdout_output.empty()
                           ? result.stderr_output
                           : result.stdout_output;

        std::istringstream stream(output);
        std::string first_line;
        if (std::getline(stream, first_line) && !first_line.empty()) {
            return first_line;
        }
        return std::nullopt;
    }

    // -------------------------------------------------------------------------
    // DICOM SCU Tools
    // -------------------------------------------------------------------------

    /**
     * @brief Run C-ECHO (echoscu) client
     * @param host Server host address
     * @param port Server port
     * @param called_ae Called AE title
     * @param calling_ae Calling AE title (default: ECHOSCU)
     * @param timeout Operation timeout
     * @return DCMTK execution result
     */
    static dcmtk_result echoscu(
        const std::string& host,
        uint16_t port,
        const std::string& called_ae,
        const std::string& calling_ae = "ECHOSCU",
        std::chrono::seconds timeout = std::chrono::seconds{30}) {

        std::vector<std::string> args = {
            "-aec", called_ae,
            "-aet", calling_ae,
            host,
            std::to_string(port)
        };

        return run_tool("echoscu", args, timeout);
    }

    /**
     * @brief Run C-STORE (storescu) client
     * @param host Server host address
     * @param port Server port
     * @param called_ae Called AE title
     * @param files DICOM files to send
     * @param calling_ae Calling AE title (default: STORESCU)
     * @param timeout Operation timeout
     * @return DCMTK execution result
     */
    static dcmtk_result storescu(
        const std::string& host,
        uint16_t port,
        const std::string& called_ae,
        const std::vector<std::filesystem::path>& files,
        const std::string& calling_ae = "STORESCU",
        std::chrono::seconds timeout = std::chrono::seconds{60}) {

        std::vector<std::string> args = {
            "-aec", called_ae,
            "-aet", calling_ae,
            host,
            std::to_string(port)
        };

        for (const auto& file : files) {
            args.push_back(file.string());
        }

        return run_tool("storescu", args, timeout);
    }

    /**
     * @brief Run C-FIND (findscu) client
     * @param host Server host address
     * @param port Server port
     * @param called_ae Called AE title
     * @param query_level Query/Retrieve level (PATIENT, STUDY, SERIES, IMAGE)
     * @param keys Query keys as tag-value pairs (e.g., {"PatientID", "TEST001"})
     * @param calling_ae Calling AE title (default: FINDSCU)
     * @param timeout Operation timeout
     * @return DCMTK execution result
     */
    static dcmtk_result findscu(
        const std::string& host,
        uint16_t port,
        const std::string& called_ae,
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& keys,
        const std::string& calling_ae = "FINDSCU",
        std::chrono::seconds timeout = std::chrono::seconds{30}) {

        std::vector<std::string> args = {
            "-aec", called_ae,
            "-aet", calling_ae,
            "-W"  // Use worklist model for MWL or study root for others
        };

        // Set query level
        if (query_level == "PATIENT" || query_level == "STUDY" ||
            query_level == "SERIES" || query_level == "IMAGE") {
            args.push_back("-k");
            args.push_back("QueryRetrieveLevel=" + query_level);
        }

        // Add query keys
        for (const auto& [key, value] : keys) {
            args.push_back("-k");
            args.push_back(key + "=" + value);
        }

        args.push_back(host);
        args.push_back(std::to_string(port));

        return run_tool("findscu", args, timeout);
    }

    /**
     * @brief Run C-MOVE (movescu) client
     * @param host Server host address
     * @param port Server port
     * @param called_ae Called AE title
     * @param dest_ae Destination AE title
     * @param query_level Query/Retrieve level
     * @param keys Query keys as tag-value pairs
     * @param calling_ae Calling AE title (default: MOVESCU)
     * @param timeout Operation timeout
     * @return DCMTK execution result
     */
    static dcmtk_result movescu(
        const std::string& host,
        uint16_t port,
        const std::string& called_ae,
        const std::string& dest_ae,
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& keys,
        const std::string& calling_ae = "MOVESCU",
        std::chrono::seconds timeout = std::chrono::seconds{120}) {

        std::vector<std::string> args = {
            "-aec", called_ae,
            "-aet", calling_ae,
            "-aem", dest_ae,  // Move destination AE title
            "-k", "QueryRetrieveLevel=" + query_level
        };

        // Add query keys
        for (const auto& [key, value] : keys) {
            args.push_back("-k");
            args.push_back(key + "=" + value);
        }

        args.push_back(host);
        args.push_back(std::to_string(port));

        return run_tool("movescu", args, timeout);
    }

    // -------------------------------------------------------------------------
    // DICOM SCP Tools
    // -------------------------------------------------------------------------

    /**
     * @brief Start C-STORE SCP (storescp) server
     * @param port Port to listen on
     * @param ae_title AE title for server
     * @param output_dir Directory to store received files
     * @param startup_timeout Maximum time to wait for server to start
     * @return Background process guard for the server
     */
    static background_process_guard storescp(
        uint16_t port,
        const std::string& ae_title,
        const std::filesystem::path& output_dir,
        std::chrono::seconds startup_timeout = std::chrono::seconds{10}) {

        // Ensure output directory exists
        std::filesystem::create_directories(output_dir);

        std::vector<std::string> args = {
            "-aet", ae_title,
            "-od", output_dir.string(),
            std::to_string(port)
        };

        auto pid = start_tool_background("storescp", args);
        background_process_guard guard(pid);

        // Wait for server to start accepting connections
        if (pid > 0) {
            if (!process_launcher::wait_for_port(port, startup_timeout)) {
                guard.stop();
                return background_process_guard(process_launcher::invalid_pid);
            }
        }

        return guard;
    }

    /**
     * @brief Start C-ECHO SCP (echoscp) server
     * @param port Port to listen on
     * @param ae_title AE title for server
     * @param startup_timeout Maximum time to wait for server to start
     * @return Background process guard for the server
     */
    static background_process_guard echoscp(
        uint16_t port,
        const std::string& ae_title,
        std::chrono::seconds startup_timeout = std::chrono::seconds{10}) {

        std::vector<std::string> args = {
            "-aet", ae_title,
            std::to_string(port)
        };

        auto pid = start_tool_background("echoscp", args);
        background_process_guard guard(pid);

        if (pid > 0) {
            if (!process_launcher::wait_for_port(port, startup_timeout)) {
                guard.stop();
                return background_process_guard(process_launcher::invalid_pid);
            }
        }

        return guard;
    }

private:
    // -------------------------------------------------------------------------
    // Internal Implementation
    // -------------------------------------------------------------------------

    /**
     * @brief Find the full path to a DCMTK tool
     * @param tool_name Name of the tool (e.g., "echoscu")
     * @return Full path to the tool, or just the tool name if not found
     */
    static std::string find_tool_path(const std::string& tool_name) {
        // First, check common installation paths
        std::vector<std::string> search_paths = {
            "/usr/local/bin",
            "/usr/bin",
            "/opt/homebrew/bin",  // macOS Homebrew (Apple Silicon)
            "/opt/local/bin"      // MacPorts
        };

        for (const auto& path : search_paths) {
            std::filesystem::path full_path = std::filesystem::path(path) / tool_name;
            if (std::filesystem::exists(full_path)) {
                return full_path.string();
            }
        }

        // Fall back to relying on PATH
        return tool_name;
    }

    /**
     * @brief Run a DCMTK tool and capture output
     * @param tool_name Name of the tool
     * @param args Command-line arguments
     * @param timeout Maximum execution time
     * @return DCMTK execution result
     */
    static dcmtk_result run_tool(
        const std::string& tool_name,
        const std::vector<std::string>& args,
        std::chrono::seconds timeout) {

        dcmtk_result result;

        std::string tool_path = find_tool_path(tool_name);
        auto process_res = process_launcher::run(tool_path, args, timeout);

        result.exit_code = process_res.exit_code;
        result.stdout_output = std::move(process_res.stdout_output);
        result.stderr_output = std::move(process_res.stderr_output);
        result.duration = process_res.duration;
        result.timed_out = process_res.timed_out;

        return result;
    }

    /**
     * @brief Start a DCMTK tool in background
     * @param tool_name Name of the tool
     * @param args Command-line arguments
     * @return Process ID
     */
    static process_launcher::pid_type start_tool_background(
        const std::string& tool_name,
        const std::vector<std::string>& args) {

        std::string tool_path = find_tool_path(tool_name);
        return process_launcher::start_background(tool_path, args);
    }
};

// =============================================================================
// DCMTK Server Guard
// =============================================================================

/**
 * @brief RAII guard for DCMTK server processes
 *
 * Provides lifecycle management for DCMTK server processes with
 * automatic cleanup on destruction.
 */
class dcmtk_server_guard {
public:
    /**
     * @brief Construct a server guard
     * @param tool_name Name of the DCMTK server tool
     * @param port Port the server is listening on
     * @param args Command-line arguments
     */
    dcmtk_server_guard(
        const std::string& tool_name,
        uint16_t port,
        const std::vector<std::string>& args)
        : port_(port) {

        // Build full command path
        std::string tool_path = find_tool_path(tool_name);
        auto pid = process_launcher::start_background(tool_path, args);
        process_.set_pid(pid);
    }

    ~dcmtk_server_guard() {
        stop();
    }

    // Non-copyable
    dcmtk_server_guard(const dcmtk_server_guard&) = delete;
    dcmtk_server_guard& operator=(const dcmtk_server_guard&) = delete;

    // Movable
    dcmtk_server_guard(dcmtk_server_guard&& other) noexcept
        : process_(std::move(other.process_))
        , port_(other.port_) {
        other.port_ = 0;
    }

    dcmtk_server_guard& operator=(dcmtk_server_guard&& other) noexcept {
        if (this != &other) {
            stop();
            process_ = std::move(other.process_);
            port_ = other.port_;
            other.port_ = 0;
        }
        return *this;
    }

    /**
     * @brief Wait for the server to be ready (accepting connections)
     * @param timeout Maximum wait time
     * @return true if server is ready
     */
    [[nodiscard]] bool wait_for_ready(
        std::chrono::seconds timeout = std::chrono::seconds{10}) const {

        return process_launcher::wait_for_port(port_, timeout);
    }

    /**
     * @brief Stop the server
     */
    void stop() {
        process_.stop();
    }

    /// @return true if server process is running
    [[nodiscard]] bool is_running() const {
        return process_.is_running();
    }

    /// @return Server port
    [[nodiscard]] uint16_t port() const noexcept { return port_; }

    /// @return Process ID
    [[nodiscard]] process_launcher::pid_type pid() const noexcept {
        return process_.pid();
    }

private:
    static std::string find_tool_path(const std::string& tool_name) {
        std::vector<std::string> search_paths = {
            "/usr/local/bin",
            "/usr/bin",
            "/opt/homebrew/bin",
            "/opt/local/bin"
        };

        for (const auto& path : search_paths) {
            std::filesystem::path full_path = std::filesystem::path(path) / tool_name;
            if (std::filesystem::exists(full_path)) {
                return full_path.string();
            }
        }

        return tool_name;
    }

    background_process_guard process_;
    uint16_t port_{0};
};

}  // namespace pacs::integration_test

#endif  // PACS_INTEGRATION_TESTS_DCMTK_TOOL_HPP
