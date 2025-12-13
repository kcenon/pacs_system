/**
 * @file ai_service_connector_test.cpp
 * @brief Unit tests for ai_service_connector
 */

#include <pacs/ai/ai_service_connector.hpp>
#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/monitoring_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

using namespace pacs::ai;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

/**
 * @brief RAII wrapper for AI service connector initialization/shutdown
 */
class ai_connector_test_fixture {
public:
    explicit ai_connector_test_fixture(const ai_service_config& config = default_config()) {
        // Initialize dependencies
        pacs::integration::logger_config log_config;
        log_config.enable_console = false;
        log_config.enable_file = false;
        pacs::integration::logger_adapter::initialize(log_config);

        pacs::integration::monitoring_config mon_config;
        mon_config.enable_metrics = true;
        pacs::integration::monitoring_adapter::initialize(mon_config);

        // Initialize AI service connector
        (void)ai_service_connector::initialize(config);
    }

    ~ai_connector_test_fixture() {
        ai_service_connector::shutdown();
        pacs::integration::monitoring_adapter::shutdown();
        pacs::integration::logger_adapter::shutdown();
    }

    ai_connector_test_fixture(const ai_connector_test_fixture&) = delete;
    ai_connector_test_fixture& operator=(const ai_connector_test_fixture&) = delete;

    static ai_service_config default_config() {
        ai_service_config config;
        config.base_url = "http://localhost:8080/api/v1";
        config.service_name = "test_ai_service";
        config.auth_type = authentication_type::none;
        config.connection_timeout = std::chrono::seconds(5);
        config.request_timeout = std::chrono::seconds(30);
        return config;
    }
};

}  // namespace

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_CASE("ai_service_connector initialization and shutdown",
          "[ai_service_connector][init]") {
    SECTION("Basic initialization") {
        pacs::integration::logger_config log_config;
        log_config.enable_console = false;
        log_config.enable_file = false;
        pacs::integration::logger_adapter::initialize(log_config);

        pacs::integration::monitoring_config mon_config;
        pacs::integration::monitoring_adapter::initialize(mon_config);

        ai_service_config config;
        config.base_url = "http://localhost:8080/api/v1";
        config.service_name = "test_service";

        auto result = ai_service_connector::initialize(config);
        REQUIRE(result.is_ok());
        REQUIRE(ai_service_connector::is_initialized());

        ai_service_connector::shutdown();
        REQUIRE_FALSE(ai_service_connector::is_initialized());

        pacs::integration::monitoring_adapter::shutdown();
        pacs::integration::logger_adapter::shutdown();
    }

    SECTION("Initialization without base URL fails") {
        pacs::integration::logger_config log_config;
        log_config.enable_console = false;
        pacs::integration::logger_adapter::initialize(log_config);

        pacs::integration::monitoring_config mon_config;
        pacs::integration::monitoring_adapter::initialize(mon_config);

        ai_service_config config;
        // base_url is empty

        auto result = ai_service_connector::initialize(config);
        REQUIRE(result.is_err());
        REQUIRE_FALSE(ai_service_connector::is_initialized());

        pacs::integration::monitoring_adapter::shutdown();
        pacs::integration::logger_adapter::shutdown();
    }

    SECTION("Shutdown without initialization is safe") {
        ai_service_connector::shutdown();
        REQUIRE_FALSE(ai_service_connector::is_initialized());
    }

    SECTION("Configuration is preserved") {
        pacs::integration::logger_config log_config;
        log_config.enable_console = false;
        pacs::integration::logger_adapter::initialize(log_config);

        pacs::integration::monitoring_config mon_config;
        pacs::integration::monitoring_adapter::initialize(mon_config);

        ai_service_config config;
        config.base_url = "https://ai.example.com/api";
        config.service_name = "test_ai";
        config.auth_type = authentication_type::api_key;
        config.api_key = "test-key-123";
        config.connection_timeout = std::chrono::seconds(10);
        config.max_retries = 5;

        auto result = ai_service_connector::initialize(config);
        REQUIRE(result.is_ok());

        auto& stored_config = ai_service_connector::get_config();
        REQUIRE(stored_config.base_url == "https://ai.example.com/api");
        REQUIRE(stored_config.service_name == "test_ai");
        REQUIRE(stored_config.auth_type == authentication_type::api_key);
        REQUIRE(stored_config.api_key == "test-key-123");
        REQUIRE(stored_config.connection_timeout == std::chrono::seconds(10));
        REQUIRE(stored_config.max_retries == 5);

        ai_service_connector::shutdown();
        pacs::integration::monitoring_adapter::shutdown();
        pacs::integration::logger_adapter::shutdown();
    }
}

// =============================================================================
// Inference Request Tests
// =============================================================================

TEST_CASE("ai_service_connector inference requests",
          "[ai_service_connector][inference]") {
    ai_connector_test_fixture fixture;

    SECTION("Valid inference request succeeds") {
        inference_request request;
        request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
        request.model_id = "chest-xray-detector";

        auto result = ai_service_connector::request_inference(request);
        REQUIRE(result.is_ok());
        REQUIRE_FALSE(result.value().empty());
    }

    SECTION("Request with series UID succeeds") {
        inference_request request;
        request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
        request.series_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1.1";
        request.model_id = "lung-nodule-detector";

        auto result = ai_service_connector::request_inference(request);
        REQUIRE(result.is_ok());
    }

    SECTION("Request with parameters succeeds") {
        inference_request request;
        request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
        request.model_id = "segmentation-model";
        request.parameters["threshold"] = "0.5";
        request.parameters["output_format"] = "SEG";
        request.priority = 5;

        auto result = ai_service_connector::request_inference(request);
        REQUIRE(result.is_ok());
    }

    SECTION("Request without study UID fails") {
        inference_request request;
        request.model_id = "test-model";
        // study_instance_uid is empty

        auto result = ai_service_connector::request_inference(request);
        REQUIRE(result.is_err());
    }

    SECTION("Request without model ID fails") {
        inference_request request;
        request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
        // model_id is empty

        auto result = ai_service_connector::request_inference(request);
        REQUIRE(result.is_err());
    }
}

// =============================================================================
// Status Checking Tests
// =============================================================================

TEST_CASE("ai_service_connector status checking",
          "[ai_service_connector][status]") {
    ai_connector_test_fixture fixture;

    SECTION("Check status of submitted job") {
        inference_request request;
        request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
        request.model_id = "test-model";

        auto submit_result = ai_service_connector::request_inference(request);
        REQUIRE(submit_result.is_ok());

        auto job_id = submit_result.value();
        auto status_result = ai_service_connector::check_status(job_id);
        REQUIRE(status_result.is_ok());

        auto& status = status_result.value();
        REQUIRE(status.job_id == job_id);
    }

    SECTION("Status contains valid fields") {
        inference_request request;
        request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
        request.model_id = "test-model";

        auto submit_result = ai_service_connector::request_inference(request);
        REQUIRE(submit_result.is_ok());

        auto status_result = ai_service_connector::check_status(submit_result.value());
        REQUIRE(status_result.is_ok());

        auto& status = status_result.value();
        // Progress should be between 0 and 100
        REQUIRE(status.progress >= 0);
        REQUIRE(status.progress <= 100);
    }
}

// =============================================================================
// Job Cancellation Tests
// =============================================================================

TEST_CASE("ai_service_connector job cancellation",
          "[ai_service_connector][cancel]") {
    ai_connector_test_fixture fixture;

    SECTION("Cancel submitted job") {
        inference_request request;
        request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
        request.model_id = "test-model";

        auto submit_result = ai_service_connector::request_inference(request);
        REQUIRE(submit_result.is_ok());

        auto job_id = submit_result.value();
        auto cancel_result = ai_service_connector::cancel(job_id);
        REQUIRE(cancel_result.is_ok());
    }
}

// =============================================================================
// Active Jobs Tests
// =============================================================================

TEST_CASE("ai_service_connector active jobs listing",
          "[ai_service_connector][jobs]") {
    ai_connector_test_fixture fixture;

    SECTION("List active jobs") {
        // Submit a few jobs
        for (int i = 0; i < 3; ++i) {
            inference_request request;
            request.study_instance_uid = "1.2.840." + std::to_string(i);
            request.model_id = "test-model";
            (void)ai_service_connector::request_inference(request);
        }

        auto result = ai_service_connector::list_active_jobs();
        REQUIRE(result.is_ok());
        // Should have some active jobs
    }
}

// =============================================================================
// Health Check Tests
// =============================================================================

TEST_CASE("ai_service_connector health check",
          "[ai_service_connector][health]") {
    ai_connector_test_fixture fixture;

    SECTION("Check health returns result") {
        // Health check should work (even if service is mocked)
        auto healthy = ai_service_connector::check_health();
        // Just verify it doesn't throw
        (void)healthy;
    }

    SECTION("Get latency returns result") {
        auto latency = ai_service_connector::get_latency();
        // Latency might be null if service is unavailable
        // Just verify it doesn't throw
        (void)latency;
    }
}

// =============================================================================
// Credential Update Tests
// =============================================================================

TEST_CASE("ai_service_connector credential update",
          "[ai_service_connector][credentials]") {
    ai_connector_test_fixture fixture;

    SECTION("Update to API key authentication") {
        auto result = ai_service_connector::update_credentials(
            authentication_type::api_key,
            "new-api-key-12345");
        REQUIRE(result.is_ok());

        auto& config = ai_service_connector::get_config();
        REQUIRE(config.auth_type == authentication_type::api_key);
    }

    SECTION("Update to bearer token authentication") {
        auto result = ai_service_connector::update_credentials(
            authentication_type::bearer_token,
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...");
        REQUIRE(result.is_ok());

        auto& config = ai_service_connector::get_config();
        REQUIRE(config.auth_type == authentication_type::bearer_token);
    }

    SECTION("Update to basic authentication") {
        auto result = ai_service_connector::update_credentials(
            authentication_type::basic,
            "username:password");
        REQUIRE(result.is_ok());

        auto& config = ai_service_connector::get_config();
        REQUIRE(config.auth_type == authentication_type::basic);
    }

    SECTION("Invalid basic auth format fails") {
        auto result = ai_service_connector::update_credentials(
            authentication_type::basic,
            "invalid-format-no-colon");
        REQUIRE(result.is_err());
    }
}

// =============================================================================
// Helper Function Tests
// =============================================================================

TEST_CASE("ai_service_connector helper functions",
          "[ai_service_connector][helpers]") {
    SECTION("inference_status_code to_string") {
        REQUIRE(to_string(inference_status_code::pending) == "pending");
        REQUIRE(to_string(inference_status_code::running) == "running");
        REQUIRE(to_string(inference_status_code::completed) == "completed");
        REQUIRE(to_string(inference_status_code::failed) == "failed");
        REQUIRE(to_string(inference_status_code::cancelled) == "cancelled");
        REQUIRE(to_string(inference_status_code::timeout) == "timeout");
    }

    SECTION("authentication_type to_string") {
        REQUIRE(to_string(authentication_type::none) == "none");
        REQUIRE(to_string(authentication_type::api_key) == "api_key");
        REQUIRE(to_string(authentication_type::bearer_token) == "bearer_token");
        REQUIRE(to_string(authentication_type::basic) == "basic");
    }
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_CASE("ai_service_connector thread safety",
          "[ai_service_connector][threading]") {
    ai_connector_test_fixture fixture;

    SECTION("Concurrent inference requests") {
        constexpr int num_threads = 4;
        constexpr int requests_per_thread = 10;

        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < requests_per_thread; ++i) {
                    inference_request request;
                    request.study_instance_uid = "1.2.840." +
                        std::to_string(t) + "." + std::to_string(i);
                    request.model_id = "test-model";

                    auto result = ai_service_connector::request_inference(request);
                    if (result.is_ok()) {
                        success_count++;
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        REQUIRE(success_count == num_threads * requests_per_thread);
    }

    SECTION("Concurrent status checks") {
        // Submit some jobs first
        std::vector<std::string> job_ids;
        for (int i = 0; i < 5; ++i) {
            inference_request request;
            request.study_instance_uid = "1.2.840." + std::to_string(i);
            request.model_id = "test-model";

            auto result = ai_service_connector::request_inference(request);
            if (result.is_ok()) {
                job_ids.push_back(result.value());
            }
        }

        constexpr int num_threads = 4;
        std::vector<std::thread> threads;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                for (const auto& job_id : job_ids) {
                    auto result = ai_service_connector::check_status(job_id);
                    // Just verify no crashes
                    (void)result;
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        REQUIRE(true);  // No crashes means success
    }
}
