/**
 * @file logger_adapter_test.cpp
 * @brief Unit tests for logger_adapter
 */

#include <pacs/integration/logger_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace pacs::integration;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

/**
 * @brief Create a temporary directory for test logs
 */
auto create_temp_log_directory() -> std::filesystem::path {
    auto temp_dir = std::filesystem::temp_directory_path() / "pacs_logger_test";
    std::filesystem::create_directories(temp_dir);
    return temp_dir;
}

/**
 * @brief Clean up temporary log directory
 */
void cleanup_temp_directory(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        std::filesystem::remove_all(path);
    }
}

/**
 * @brief Read file contents as string
 */
auto read_file_contents(const std::filesystem::path& path) -> std::string {
    std::ifstream file(path);
    if (!file) {
        return "";
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

/**
 * @brief RAII wrapper for logger initialization/shutdown
 */
class logger_test_fixture {
public:
    explicit logger_test_fixture(const logger_config& config) : log_dir_(config.log_directory) {
        logger_adapter::initialize(config);
    }

    ~logger_test_fixture() {
        logger_adapter::shutdown();
        cleanup_temp_directory(log_dir_);
    }

    logger_test_fixture(const logger_test_fixture&) = delete;
    logger_test_fixture& operator=(const logger_test_fixture&) = delete;

private:
    std::filesystem::path log_dir_;
};

}  // namespace

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_CASE("logger_adapter initialization and shutdown", "[logger_adapter][init]") {
    auto temp_dir = create_temp_log_directory();

    SECTION("Basic initialization") {
        logger_config config;
        config.log_directory = temp_dir;
        config.enable_console = false;
        config.enable_file = true;
        config.enable_audit_log = true;

        logger_adapter::initialize(config);
        REQUIRE(logger_adapter::is_initialized());

        logger_adapter::shutdown();
        REQUIRE_FALSE(logger_adapter::is_initialized());
    }

    SECTION("Multiple initialization calls are safe") {
        logger_config config;
        config.log_directory = temp_dir;
        config.enable_console = false;

        logger_adapter::initialize(config);
        logger_adapter::initialize(config);  // Should not crash
        REQUIRE(logger_adapter::is_initialized());

        logger_adapter::shutdown();
    }

    SECTION("Shutdown without initialization is safe") {
        logger_adapter::shutdown();  // Should not crash
        REQUIRE_FALSE(logger_adapter::is_initialized());
    }

    cleanup_temp_directory(temp_dir);
}

// =============================================================================
// Standard Logging Tests
// =============================================================================

TEST_CASE("logger_adapter standard logging", "[logger_adapter][logging]") {
    auto temp_dir = create_temp_log_directory();
    logger_config config;
    config.log_directory = temp_dir;
    config.enable_console = false;
    config.enable_file = true;
    config.enable_audit_log = false;
    config.min_level = log_level::trace;

    logger_test_fixture fixture(config);

    SECTION("Log at different levels") {
        logger_adapter::trace("Trace message: {}", 1);
        logger_adapter::debug("Debug message: {}", 2);
        logger_adapter::info("Info message: {}", 3);
        logger_adapter::warn("Warn message: {}", 4);
        logger_adapter::error("Error message: {}", 5);

        logger_adapter::flush();

        // Give async logger time to write
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    SECTION("Log level filtering") {
        logger_adapter::set_min_level(log_level::warn);
        REQUIRE(logger_adapter::get_min_level() == log_level::warn);

        REQUIRE_FALSE(logger_adapter::is_level_enabled(log_level::trace));
        REQUIRE_FALSE(logger_adapter::is_level_enabled(log_level::debug));
        REQUIRE_FALSE(logger_adapter::is_level_enabled(log_level::info));
        REQUIRE(logger_adapter::is_level_enabled(log_level::warn));
        REQUIRE(logger_adapter::is_level_enabled(log_level::error));
        REQUIRE(logger_adapter::is_level_enabled(log_level::fatal));
    }

    SECTION("Log with source location") {
        logger_adapter::log(log_level::info, "Test message with location",
                           __FILE__, __LINE__, __FUNCTION__);
        logger_adapter::flush();
    }
}

// =============================================================================
// DICOM Audit Logging Tests
// =============================================================================

TEST_CASE("logger_adapter DICOM audit logging", "[logger_adapter][audit]") {
    auto temp_dir = create_temp_log_directory();
    logger_config config;
    config.log_directory = temp_dir;
    config.enable_console = false;
    config.enable_file = false;
    config.enable_audit_log = true;
    config.audit_log_format = "json";

    logger_test_fixture fixture(config);

    SECTION("Log association established") {
        logger_adapter::log_association_established(
            "MODALITY1", "PACS_SERVER", "192.168.1.100");

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("ASSOCIATION_ESTABLISHED") != std::string::npos);
        REQUIRE(content.find("MODALITY1") != std::string::npos);
        REQUIRE(content.find("PACS_SERVER") != std::string::npos);
        REQUIRE(content.find("192.168.1.100") != std::string::npos);
    }

    SECTION("Log association released") {
        logger_adapter::log_association_released("MODALITY1", "PACS_SERVER");

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("ASSOCIATION_RELEASED") != std::string::npos);
    }

    SECTION("Log C-STORE received - success") {
        logger_adapter::log_c_store_received(
            "MODALITY1", "12345", "1.2.3.4", "1.2.3.4.5",
            storage_status::success);

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("C-STORE") != std::string::npos);
        REQUIRE(content.find("success") != std::string::npos);
        REQUIRE(content.find("12345") != std::string::npos);
        REQUIRE(content.find("1.2.3.4.5") != std::string::npos);
    }

    SECTION("Log C-STORE received - failure") {
        logger_adapter::log_c_store_received(
            "MODALITY1", "12345", "1.2.3.4", "1.2.3.4.5",
            storage_status::out_of_resources);

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("C-STORE") != std::string::npos);
        REQUIRE(content.find("failure") != std::string::npos);
        REQUIRE(content.find("OutOfResources") != std::string::npos);
    }

    SECTION("Log C-FIND executed") {
        logger_adapter::log_c_find_executed(
            "WORKSTATION1", query_level::study, 42);

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("C-FIND") != std::string::npos);
        REQUIRE(content.find("STUDY") != std::string::npos);
        REQUIRE(content.find("42") != std::string::npos);
    }

    SECTION("Log C-MOVE executed - success") {
        logger_adapter::log_c_move_executed(
            "WORKSTATION1", "ARCHIVE_PACS", "1.2.3.4", 10,
            move_status::success);

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("C-MOVE") != std::string::npos);
        REQUIRE(content.find("success") != std::string::npos);
        REQUIRE(content.find("ARCHIVE_PACS") != std::string::npos);
    }

    SECTION("Log C-MOVE executed - failure") {
        logger_adapter::log_c_move_executed(
            "WORKSTATION1", "UNKNOWN_DEST", "1.2.3.4", 0,
            move_status::refused_move_destination_unknown);

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("C-MOVE") != std::string::npos);
        REQUIRE(content.find("failure") != std::string::npos);
        REQUIRE(content.find("RefusedMoveDestinationUnknown") != std::string::npos);
    }
}

// =============================================================================
// Security Event Tests
// =============================================================================

TEST_CASE("logger_adapter security event logging", "[logger_adapter][security]") {
    auto temp_dir = create_temp_log_directory();
    logger_config config;
    config.log_directory = temp_dir;
    config.enable_console = false;
    config.enable_file = false;
    config.enable_audit_log = true;

    logger_test_fixture fixture(config);

    SECTION("Log authentication success") {
        logger_adapter::log_security_event(
            security_event_type::authentication_success,
            "User authenticated successfully",
            "admin_user");

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("SECURITY") != std::string::npos);
        REQUIRE(content.find("authentication_success") != std::string::npos);
        REQUIRE(content.find("admin_user") != std::string::npos);
    }

    SECTION("Log authentication failure") {
        logger_adapter::log_security_event(
            security_event_type::authentication_failure,
            "Invalid AE title: UNKNOWN",
            "192.168.1.50");

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("authentication_failure") != std::string::npos);
    }

    SECTION("Log access denied") {
        logger_adapter::log_security_event(
            security_event_type::access_denied,
            "Access to patient data denied",
            "unauthorized_user");

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("access_denied") != std::string::npos);
    }

    SECTION("Log configuration change") {
        logger_adapter::log_security_event(
            security_event_type::configuration_change,
            "Storage path changed to /new/path");

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("configuration_change") != std::string::npos);
    }

    SECTION("Log data export") {
        logger_adapter::log_security_event(
            security_event_type::data_export,
            "Study 1.2.3.4 exported to USB drive",
            "technician1");

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("data_export") != std::string::npos);
    }
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("logger_adapter configuration", "[logger_adapter][config]") {
    auto temp_dir = create_temp_log_directory();

    SECTION("Get configuration after initialization") {
        logger_config config;
        config.log_directory = temp_dir;
        config.min_level = log_level::debug;
        config.enable_console = false;
        config.enable_file = true;
        config.enable_audit_log = true;
        config.max_file_size_mb = 50;
        config.max_files = 5;

        logger_test_fixture fixture(config);

        auto& retrieved_config = logger_adapter::get_config();
        REQUIRE(retrieved_config.min_level == log_level::debug);
        REQUIRE(retrieved_config.enable_file == true);
        REQUIRE(retrieved_config.enable_audit_log == true);
        REQUIRE(retrieved_config.max_file_size_mb == 50);
        REQUIRE(retrieved_config.max_files == 5);
    }

    SECTION("Set and get minimum log level") {
        logger_config config;
        config.log_directory = temp_dir;
        config.enable_console = false;
        config.min_level = log_level::info;

        logger_test_fixture fixture(config);

        REQUIRE(logger_adapter::get_min_level() == log_level::info);

        logger_adapter::set_min_level(log_level::error);
        REQUIRE(logger_adapter::get_min_level() == log_level::error);

        logger_adapter::set_min_level(log_level::trace);
        REQUIRE(logger_adapter::get_min_level() == log_level::trace);
    }
}

// =============================================================================
// Audit Log Format Tests
// =============================================================================

TEST_CASE("logger_adapter audit log JSON format", "[logger_adapter][audit][json]") {
    auto temp_dir = create_temp_log_directory();
    logger_config config;
    config.log_directory = temp_dir;
    config.enable_console = false;
    config.enable_file = false;
    config.enable_audit_log = true;
    config.audit_log_format = "json";

    logger_test_fixture fixture(config);

    logger_adapter::log_c_store_received(
        "TEST_AE", "PATIENT001", "1.2.840.10008.1", "1.2.840.10008.1.1",
        storage_status::success);

    logger_adapter::flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto audit_path = temp_dir / "audit.json";
    auto content = read_file_contents(audit_path);

    // Verify JSON structure
    REQUIRE(content.find("{") != std::string::npos);
    REQUIRE(content.find("}") != std::string::npos);
    REQUIRE(content.find("\"timestamp\"") != std::string::npos);
    REQUIRE(content.find("\"event_type\"") != std::string::npos);
    REQUIRE(content.find("\"outcome\"") != std::string::npos);

    // Verify ISO8601 timestamp format (YYYY-MM-DDTHH:MM:SS)
    REQUIRE(content.find("T") != std::string::npos);  // ISO8601 separator
}

// =============================================================================
// Empty/Null Value Tests
// =============================================================================

TEST_CASE("logger_adapter handles empty values", "[logger_adapter][empty]") {
    auto temp_dir = create_temp_log_directory();
    logger_config config;
    config.log_directory = temp_dir;
    config.enable_console = false;
    config.enable_audit_log = true;

    logger_test_fixture fixture(config);

    SECTION("Security event with empty user_id") {
        logger_adapter::log_security_event(
            security_event_type::authentication_failure,
            "Failed login attempt",
            "");  // Empty user_id

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("authentication_failure") != std::string::npos);
        // user_id should not be present when empty
    }

    SECTION("C-FIND with zero matches") {
        logger_adapter::log_c_find_executed(
            "TEST_AE", query_level::patient, 0);

        logger_adapter::flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto audit_path = temp_dir / "audit.json";
        auto content = read_file_contents(audit_path);

        REQUIRE(content.find("C-FIND") != std::string::npos);
        REQUIRE(content.find("\"matches_returned\":\"0\"") != std::string::npos);
    }
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_CASE("logger_adapter thread safety", "[logger_adapter][thread]") {
    auto temp_dir = create_temp_log_directory();
    logger_config config;
    config.log_directory = temp_dir;
    config.enable_console = false;
    config.enable_file = true;
    config.enable_audit_log = true;
    config.async_mode = true;

    logger_test_fixture fixture(config);

    constexpr int kNumThreads = 4;
    constexpr int kMessagesPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                logger_adapter::info("Thread {} message {}", t, i);
                logger_adapter::log_c_find_executed(
                    "THREAD_" + std::to_string(t),
                    query_level::study,
                    static_cast<std::size_t>(i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    logger_adapter::flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify no crashes and audit log was created
    auto audit_path = temp_dir / "audit.json";
    REQUIRE(std::filesystem::exists(audit_path));
}
