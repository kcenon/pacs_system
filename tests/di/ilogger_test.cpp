/**
 * @file ilogger_test.cpp
 * @brief Unit tests for ILogger interface and implementations
 *
 * @see Issue #309 - Full Adoption of ILogger Interface
 * @see Issue #347 - Add ILogger Unit Tests
 */

#include <pacs/di/ilogger.hpp>
#include <pacs/di/service_registration.hpp>
#include <pacs/services/verification_scp.hpp>
#include <pacs/services/storage_scp.hpp>
#include <pacs/services/storage_scu.hpp>
#include <pacs/services/query_scp.hpp>
#include <pacs/services/retrieve_scp.hpp>
#include <pacs/services/worklist_scp.hpp>
#include <pacs/services/mpps_scp.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <vector>

using namespace pacs::di;
using namespace pacs::services;
using namespace kcenon::common::di;

// =============================================================================
// Mock Logger for Testing
// =============================================================================

namespace {

/**
 * @brief Mock logger that records all log calls for verification
 */
class MockLogger final : public ILogger {
public:
    MockLogger() = default;
    ~MockLogger() override = default;

    void trace(std::string_view message) override {
        trace_count_.fetch_add(1, std::memory_order_relaxed);
        last_message_ = std::string(message);
    }

    void debug(std::string_view message) override {
        debug_count_.fetch_add(1, std::memory_order_relaxed);
        last_message_ = std::string(message);
    }

    void info(std::string_view message) override {
        info_count_.fetch_add(1, std::memory_order_relaxed);
        last_message_ = std::string(message);
    }

    void warn(std::string_view message) override {
        warn_count_.fetch_add(1, std::memory_order_relaxed);
        last_message_ = std::string(message);
    }

    void error(std::string_view message) override {
        error_count_.fetch_add(1, std::memory_order_relaxed);
        last_message_ = std::string(message);
    }

    void fatal(std::string_view message) override {
        fatal_count_.fetch_add(1, std::memory_order_relaxed);
        last_message_ = std::string(message);
    }

    [[nodiscard]] bool is_enabled(pacs::integration::log_level level) const noexcept override {
        return level >= enabled_level_;
    }

    // Test accessors
    [[nodiscard]] size_t trace_count() const noexcept {
        return trace_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t debug_count() const noexcept {
        return debug_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t info_count() const noexcept {
        return info_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t warn_count() const noexcept {
        return warn_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t error_count() const noexcept {
        return error_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t fatal_count() const noexcept {
        return fatal_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] size_t total_count() const noexcept {
        return trace_count() + debug_count() + info_count() +
               warn_count() + error_count() + fatal_count();
    }

    [[nodiscard]] const std::string& last_message() const noexcept {
        return last_message_;
    }

    void set_enabled_level(pacs::integration::log_level level) noexcept {
        enabled_level_ = level;
    }

    void reset() noexcept {
        trace_count_.store(0, std::memory_order_relaxed);
        debug_count_.store(0, std::memory_order_relaxed);
        info_count_.store(0, std::memory_order_relaxed);
        warn_count_.store(0, std::memory_order_relaxed);
        error_count_.store(0, std::memory_order_relaxed);
        fatal_count_.store(0, std::memory_order_relaxed);
        last_message_.clear();
    }

private:
    std::atomic<size_t> trace_count_{0};
    std::atomic<size_t> debug_count_{0};
    std::atomic<size_t> info_count_{0};
    std::atomic<size_t> warn_count_{0};
    std::atomic<size_t> error_count_{0};
    std::atomic<size_t> fatal_count_{0};
    std::string last_message_;
    pacs::integration::log_level enabled_level_ = pacs::integration::log_level::trace;
};

}  // namespace

// =============================================================================
// NullLogger Tests
// =============================================================================

TEST_CASE("NullLogger is a no-op implementation", "[di][logger][null]") {
    NullLogger logger;

    SECTION("all log methods are safe to call") {
        // Should not throw or crash
        logger.trace("trace message");
        logger.debug("debug message");
        logger.info("info message");
        logger.warn("warn message");
        logger.error("error message");
        logger.fatal("fatal message");
    }

    SECTION("is_enabled always returns false") {
        CHECK_FALSE(logger.is_enabled(pacs::integration::log_level::trace));
        CHECK_FALSE(logger.is_enabled(pacs::integration::log_level::debug));
        CHECK_FALSE(logger.is_enabled(pacs::integration::log_level::info));
        CHECK_FALSE(logger.is_enabled(pacs::integration::log_level::warn));
        CHECK_FALSE(logger.is_enabled(pacs::integration::log_level::error));
        CHECK_FALSE(logger.is_enabled(pacs::integration::log_level::fatal));
    }

    SECTION("formatted logging methods are safe") {
        // Should not throw or crash even with format arguments
        logger.trace_fmt("value: {}", 42);
        logger.debug_fmt("value: {}", 3.14);
        logger.info_fmt("value: {}", "test");
        logger.warn_fmt("values: {} {}", 1, 2);
        logger.error_fmt("error: {}", "failure");
        logger.fatal_fmt("fatal: {}", "crash");
    }
}

// =============================================================================
// null_logger() Singleton Tests
// =============================================================================

TEST_CASE("null_logger() returns singleton instance", "[di][logger][null]") {
    SECTION("returns non-null pointer") {
        auto logger = null_logger();
        REQUIRE(logger != nullptr);
    }

    SECTION("returns same instance on multiple calls") {
        auto logger1 = null_logger();
        auto logger2 = null_logger();
        auto logger3 = null_logger();

        CHECK(logger1.get() == logger2.get());
        CHECK(logger2.get() == logger3.get());
    }

    SECTION("instance is NullLogger") {
        auto logger = null_logger();
        // Verify behavior matches NullLogger
        CHECK_FALSE(logger->is_enabled(pacs::integration::log_level::info));
    }
}

// =============================================================================
// LoggerService Tests
// =============================================================================

TEST_CASE("LoggerService delegates to logger_adapter", "[di][logger][service]") {
    LoggerService service;

    SECTION("all log methods are safe to call") {
        // These delegate to logger_adapter - should not crash
        service.trace("trace message");
        service.debug("debug message");
        service.info("info message");
        service.warn("warn message");
        service.error("error message");
        service.fatal("fatal message");
    }

    SECTION("is_enabled delegates correctly") {
        // Should return based on logger_adapter's min level
        // Note: actual level depends on logger_adapter initialization state
        (void)service.is_enabled(pacs::integration::log_level::info);
    }

    SECTION("formatted logging methods work") {
        service.trace_fmt("value: {}", 42);
        service.debug_fmt("value: {}", 3.14);
        service.info_fmt("value: {}", "test");
        service.warn_fmt("values: {} {}", 1, 2);
        service.error_fmt("error: {}", "failure");
        service.fatal_fmt("fatal: {}", "crash");
    }
}

// =============================================================================
// ILogger Formatted Logging Tests
// =============================================================================

TEST_CASE("ILogger formatted logging with MockLogger", "[di][logger][format]") {
    auto mock = std::make_shared<MockLogger>();
    ILogger* logger = mock.get();

    SECTION("trace_fmt formats and logs correctly") {
        logger->trace_fmt("value: {} and {}", 42, "test");
        CHECK(mock->trace_count() == 1);
        CHECK(mock->last_message() == "value: 42 and test");
    }

    SECTION("debug_fmt formats and logs correctly") {
        logger->debug_fmt("pi is {:.2f}", 3.14159);
        CHECK(mock->debug_count() == 1);
    }

    SECTION("info_fmt formats and logs correctly") {
        logger->info_fmt("count: {}", 100);
        CHECK(mock->info_count() == 1);
        CHECK(mock->last_message() == "count: 100");
    }

    SECTION("warn_fmt formats and logs correctly") {
        logger->warn_fmt("warning {}", "message");
        CHECK(mock->warn_count() == 1);
    }

    SECTION("error_fmt formats and logs correctly") {
        logger->error_fmt("error code: {:#x}", 255);
        CHECK(mock->error_count() == 1);
    }

    SECTION("fatal_fmt formats and logs correctly") {
        logger->fatal_fmt("fatal: {}", "shutdown");
        CHECK(mock->fatal_count() == 1);
    }

    SECTION("formatted logging respects is_enabled") {
        mock->set_enabled_level(pacs::integration::log_level::warn);

        // These should not log because level is below warn
        logger->trace_fmt("skip: {}", 1);
        logger->debug_fmt("skip: {}", 2);
        logger->info_fmt("skip: {}", 3);

        // These should log because level is at or above warn
        logger->warn_fmt("log: {}", 4);
        logger->error_fmt("log: {}", 5);
        logger->fatal_fmt("log: {}", 6);

        CHECK(mock->trace_count() == 0);
        CHECK(mock->debug_count() == 0);
        CHECK(mock->info_count() == 0);
        CHECK(mock->warn_count() == 1);
        CHECK(mock->error_count() == 1);
        CHECK(mock->fatal_count() == 1);
    }
}

// =============================================================================
// SCP Service Logger Injection Tests
// =============================================================================

TEST_CASE("verification_scp logger injection", "[di][logger][services]") {
    SECTION("default construction uses null_logger") {
        verification_scp scp;
        REQUIRE(scp.logger() != nullptr);
        // Verify it behaves like NullLogger
        CHECK_FALSE(scp.logger()->is_enabled(pacs::integration::log_level::info));
    }

    SECTION("custom logger can be injected via constructor") {
        auto mock = std::make_shared<MockLogger>();
        verification_scp scp(mock);

        CHECK(scp.logger().get() == mock.get());
    }

    SECTION("set_logger replaces the logger") {
        verification_scp scp;
        auto mock = std::make_shared<MockLogger>();

        scp.set_logger(mock);
        CHECK(scp.logger().get() == mock.get());
    }

    SECTION("set_logger(nullptr) uses null_logger") {
        auto mock = std::make_shared<MockLogger>();
        verification_scp scp(mock);

        scp.set_logger(nullptr);
        REQUIRE(scp.logger() != nullptr);
        CHECK_FALSE(scp.logger()->is_enabled(pacs::integration::log_level::info));
    }
}

TEST_CASE("storage_scp logger injection", "[di][logger][services]") {
    SECTION("default construction uses null_logger") {
        storage_scp scp;
        REQUIRE(scp.logger() != nullptr);
    }

    SECTION("custom logger can be injected via constructor") {
        auto mock = std::make_shared<MockLogger>();
        storage_scp scp(mock);

        CHECK(scp.logger().get() == mock.get());
    }

    SECTION("config constructor with logger") {
        auto mock = std::make_shared<MockLogger>();
        storage_scp_config config;
        storage_scp scp(config, mock);

        CHECK(scp.logger().get() == mock.get());
    }
}

TEST_CASE("query_scp logger injection", "[di][logger][services]") {
    SECTION("default construction uses null_logger") {
        query_scp scp;
        REQUIRE(scp.logger() != nullptr);
    }

    SECTION("custom logger can be injected") {
        auto mock = std::make_shared<MockLogger>();
        query_scp scp(mock);

        CHECK(scp.logger().get() == mock.get());
    }
}

TEST_CASE("retrieve_scp logger injection", "[di][logger][services]") {
    SECTION("default construction uses null_logger") {
        retrieve_scp scp;
        REQUIRE(scp.logger() != nullptr);
    }

    SECTION("custom logger can be injected") {
        auto mock = std::make_shared<MockLogger>();
        retrieve_scp scp(mock);

        CHECK(scp.logger().get() == mock.get());
    }
}

TEST_CASE("worklist_scp logger injection", "[di][logger][services]") {
    SECTION("default construction uses null_logger") {
        worklist_scp scp;
        REQUIRE(scp.logger() != nullptr);
    }

    SECTION("custom logger can be injected") {
        auto mock = std::make_shared<MockLogger>();
        worklist_scp scp(mock);

        CHECK(scp.logger().get() == mock.get());
    }
}

TEST_CASE("mpps_scp logger injection", "[di][logger][services]") {
    SECTION("default construction uses null_logger") {
        mpps_scp scp;
        REQUIRE(scp.logger() != nullptr);
    }

    SECTION("custom logger can be injected") {
        auto mock = std::make_shared<MockLogger>();
        mpps_scp scp(mock);

        CHECK(scp.logger().get() == mock.get());
    }
}

// =============================================================================
// Storage SCU Logger Injection Tests
// =============================================================================

TEST_CASE("storage_scu logger injection", "[di][logger][services]") {
    SECTION("default construction uses null_logger") {
        storage_scu scu;
        // storage_scu doesn't expose logger() publicly, but should not crash
        // Verify default construction works
        CHECK(scu.images_sent() == 0);
        CHECK(scu.failures() == 0);
    }

    SECTION("custom logger can be injected via constructor") {
        auto mock = std::make_shared<MockLogger>();
        storage_scu scu(mock);

        CHECK(scu.images_sent() == 0);
    }

    SECTION("config constructor with logger") {
        auto mock = std::make_shared<MockLogger>();
        storage_scu_config config;
        storage_scu scu(config, mock);

        CHECK(scu.images_sent() == 0);
    }
}

// =============================================================================
// Service Registration Tests
// =============================================================================

TEST_CASE("ILogger service registration", "[di][logger][registration]") {
    service_container container;

    SECTION("register_services registers ILogger by default") {
        auto result = register_services(container);
        REQUIRE(result.is_ok());

        REQUIRE(container.is_registered<ILogger>());
    }

    SECTION("ILogger can be resolved after registration") {
        auto result = register_services(container);
        REQUIRE(result.is_ok());

        auto logger_result = container.resolve<ILogger>();
        REQUIRE(logger_result.is_ok());
        REQUIRE(logger_result.value() != nullptr);
    }

    SECTION("enable_logger = false skips ILogger registration") {
        registration_config config;
        config.enable_logger = false;

        auto result = register_services(container, config);
        REQUIRE(result.is_ok());

        REQUIRE_FALSE(container.is_registered<ILogger>());
    }

    SECTION("singleton lifetime returns same instance") {
        registration_config config;
        config.use_singletons = true;
        config.enable_logger = true;

        auto result = register_services(container, config);
        REQUIRE(result.is_ok());

        auto logger1 = container.resolve<ILogger>().value();
        auto logger2 = container.resolve<ILogger>().value();
        CHECK(logger1.get() == logger2.get());
    }

    SECTION("transient lifetime returns different instances") {
        registration_config config;
        config.use_singletons = false;
        config.enable_logger = true;

        auto result = register_services(container, config);
        REQUIRE(result.is_ok());

        auto logger1 = container.resolve<ILogger>().value();
        auto logger2 = container.resolve<ILogger>().value();
        CHECK(logger1.get() != logger2.get());
    }
}

TEST_CASE("register_logger_instance registers custom logger", "[di][logger][registration]") {
    service_container container;

    auto mock = std::make_shared<MockLogger>();
    auto result = register_logger_instance(container, mock);
    REQUIRE(result.is_ok());

    auto resolved = container.resolve<ILogger>().value();
    CHECK(resolved.get() == mock.get());
}

TEST_CASE("register_logger registers custom factory", "[di][logger][registration]") {
    service_container container;

    size_t factory_call_count = 0;
    auto result = register_logger<MockLogger>(
        container,
        [&](IServiceContainer&) -> std::shared_ptr<MockLogger> {
            ++factory_call_count;
            return std::make_shared<MockLogger>();
        },
        service_lifetime::transient
    );
    REQUIRE(result.is_ok());

    // Each resolve should call factory
    (void)container.resolve<ILogger>();
    (void)container.resolve<ILogger>();

    CHECK(factory_call_count == 2);
}

// =============================================================================
// scp_service Base Class Tests
// =============================================================================

TEST_CASE("scp_service logger accessor is const-correct", "[di][logger][scp]") {
    auto mock = std::make_shared<MockLogger>();
    const verification_scp const_scp(mock);

    // Should compile - logger() should be callable on const instance
    auto& logger = const_scp.logger();
    CHECK(logger.get() == mock.get());
}

TEST_CASE("scp_service polymorphic logger access", "[di][logger][scp]") {
    auto mock = std::make_shared<MockLogger>();

    // Create via base pointer
    std::unique_ptr<scp_service> base_ptr = std::make_unique<verification_scp>(mock);

    // Logger should be accessible through base class
    CHECK(base_ptr->logger().get() == mock.get());

    // set_logger should work through base class
    auto mock2 = std::make_shared<MockLogger>();
    base_ptr->set_logger(mock2);
    CHECK(base_ptr->logger().get() == mock2.get());
}
