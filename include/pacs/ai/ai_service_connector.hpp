/**
 * @file ai_service_connector.hpp
 * @brief Connector for external AI inference services
 *
 * This file provides the ai_service_connector class for integrating with
 * external AI inference services. It supports sending DICOM studies for
 * AI analysis, tracking job status, and managing inference requests.
 *
 * @see Issue #205 - AI Service Connector Implementation
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_COMMON_SYSTEM
#include <kcenon/common/patterns/result.h>
#endif

namespace pacs::ai {

// =============================================================================
// Result Type (fallback when common_system is unavailable)
// =============================================================================

#ifdef PACS_WITH_COMMON_SYSTEM
template <typename T>
using Result = kcenon::common::Result<T>;

using error_info = kcenon::common::error_info;
#else
/**
 * @brief Simple error info for fallback when common_system is unavailable
 */
struct error_info {
    int code = -1;
    std::string message;
    std::string module;

    error_info() = default;
    explicit error_info(const std::string& msg) : message(msg) {}
    error_info(int c, const std::string& msg, const std::string& mod = "")
        : code(c), message(msg), module(mod) {}
};

/**
 * @brief Simple result type for error handling
 */
template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)), has_value_(true) {}
    Result(const error_info& err) : error_(err), has_value_(false) {}

    [[nodiscard]] bool is_ok() const noexcept { return has_value_; }
    [[nodiscard]] bool is_err() const noexcept { return !has_value_; }
    [[nodiscard]] T& value() & { return data_; }
    [[nodiscard]] const T& value() const& { return data_; }
    [[nodiscard]] T&& value() && { return std::move(data_); }
    [[nodiscard]] const error_info& error() const { return error_; }

private:
    T data_{};
    error_info error_;
    bool has_value_;
};
#endif

// =============================================================================
// Enumerations
// =============================================================================

/**
 * @enum inference_status_code
 * @brief Status codes for AI inference jobs
 */
enum class inference_status_code {
    pending,      ///< Job is queued but not started
    running,      ///< Job is currently processing
    completed,    ///< Job completed successfully
    failed,       ///< Job failed with error
    cancelled,    ///< Job was cancelled
    timeout       ///< Job timed out
};

/**
 * @enum authentication_type
 * @brief Types of authentication for AI services
 */
enum class authentication_type {
    none,         ///< No authentication
    api_key,      ///< API key in header
    bearer_token, ///< OAuth2 bearer token
    basic         ///< HTTP basic authentication
};

// =============================================================================
// Configuration Structures
// =============================================================================

/**
 * @struct ai_service_config
 * @brief Configuration for AI service connection
 */
struct ai_service_config {
    /// Base URL of the AI service (e.g., "https://ai.example.com/v1")
    std::string base_url;

    /// Service name for identification
    std::string service_name{"ai_service"};

    /// Authentication type
    authentication_type auth_type{authentication_type::none};

    /// API key (for api_key auth type)
    std::string api_key;

    /// Username (for basic auth)
    std::string username;

    /// Password (for basic auth)
    std::string password;

    /// Bearer token (for bearer_token auth type)
    std::string bearer_token;

    /// Connection timeout
    std::chrono::milliseconds connection_timeout{30000};

    /// Request timeout for inference operations
    std::chrono::milliseconds request_timeout{300000};  // 5 minutes

    /// Maximum retry attempts on failure
    std::size_t max_retries{3};

    /// Delay between retries (exponential backoff applied)
    std::chrono::milliseconds retry_delay{1000};

    /// Enable TLS certificate verification
    bool verify_ssl{true};

    /// Path to CA certificate bundle (optional)
    std::optional<std::filesystem::path> ca_cert_path;

    /// Status polling interval
    std::chrono::milliseconds polling_interval{5000};
};

/**
 * @struct inference_request
 * @brief Request structure for AI inference
 */
struct inference_request {
    /// Study Instance UID to process
    std::string study_instance_uid;

    /// Series Instance UID (optional, for series-level inference)
    std::optional<std::string> series_instance_uid;

    /// Model ID to use for inference
    std::string model_id;

    /// Custom parameters for the model
    std::map<std::string, std::string> parameters;

    /// Priority level (higher = more urgent)
    int priority{0};

    /// Callback URL for result notification (optional)
    std::optional<std::string> callback_url;

    /// Custom metadata to include with request
    std::map<std::string, std::string> metadata;
};

/**
 * @struct inference_status
 * @brief Status information for an inference job
 */
struct inference_status {
    /// Unique job identifier
    std::string job_id;

    /// Current status code
    inference_status_code status{inference_status_code::pending};

    /// Progress percentage (0-100)
    int progress{0};

    /// Human-readable status message
    std::string message;

    /// Error message (if status is failed)
    std::optional<std::string> error_message;

    /// Time when job was created
    std::chrono::system_clock::time_point created_at;

    /// Time when job started processing
    std::optional<std::chrono::system_clock::time_point> started_at;

    /// Time when job completed
    std::optional<std::chrono::system_clock::time_point> completed_at;

    /// Result UIDs (if completed successfully)
    std::vector<std::string> result_uids;
};

/**
 * @struct model_info
 * @brief Information about an available AI model
 */
struct model_info {
    /// Unique model identifier
    std::string model_id;

    /// Human-readable model name
    std::string name;

    /// Model description
    std::string description;

    /// Model version
    std::string version;

    /// Supported modalities (e.g., "CT", "MR", "CR")
    std::vector<std::string> supported_modalities;

    /// Supported SOP classes
    std::vector<std::string> supported_sop_classes;

    /// Output types (e.g., "SR", "SEG", "PR")
    std::vector<std::string> output_types;

    /// Whether model is currently available
    bool available{true};
};

// =============================================================================
// AI Service Connector Class
// =============================================================================

/**
 * @class ai_service_connector
 * @brief Connector for external AI inference services
 *
 * This class provides a unified interface for interacting with external
 * AI inference services, including:
 * - Sending DICOM studies for AI processing
 * - Tracking inference job status
 * - Cancelling running jobs
 * - Listing available AI models
 *
 * The connector uses network_system for HTTP communication and integrates
 * with logger_system for audit logging and monitoring_system for metrics.
 *
 * Thread Safety: All methods are thread-safe.
 *
 * @example
 * @code
 * // Initialize the connector
 * ai_service_config config;
 * config.base_url = "https://ai.hospital.org/api/v1";
 * config.auth_type = authentication_type::api_key;
 * config.api_key = "your-api-key";
 * ai_service_connector::initialize(config);
 *
 * // Request inference
 * inference_request request;
 * request.study_instance_uid = "1.2.3.4.5.6.7.8.9";
 * request.model_id = "chest-xray-detector-v2";
 *
 * auto result = ai_service_connector::request_inference(request);
 * if (result.is_ok()) {
 *     std::string job_id = result.value();
 *
 *     // Poll for status
 *     auto status_result = ai_service_connector::check_status(job_id);
 *     if (status_result.is_ok()) {
 *         auto& status = status_result.value();
 *         if (status.status == inference_status_code::completed) {
 *             // Process results
 *         }
 *     }
 * }
 *
 * // Shutdown
 * ai_service_connector::shutdown();
 * @endcode
 */
class ai_service_connector {
public:
    // =========================================================================
    // Type Aliases
    // =========================================================================

    /// Callback type for status updates
    using status_callback = std::function<void(const inference_status&)>;

    /// Callback type for completion notification
    using completion_callback = std::function<void(const std::string& job_id,
                                                   bool success,
                                                   const std::vector<std::string>& result_uids)>;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the AI service connector
     *
     * Must be called before any other operations. Sets up HTTP client,
     * configures authentication, and validates connection.
     *
     * @param config Configuration options
     * @return Result indicating success or initialization error
     */
    [[nodiscard]] static auto initialize(const ai_service_config& config)
        -> Result<std::monostate>;

    /**
     * @brief Shutdown the AI service connector
     *
     * Cancels pending requests and releases resources.
     */
    static void shutdown();

    /**
     * @brief Check if the connector is initialized
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] static auto is_initialized() noexcept -> bool;

    // =========================================================================
    // Inference Operations
    // =========================================================================

    /**
     * @brief Request AI inference for a study
     *
     * Submits a study for AI processing and returns a job ID for tracking.
     *
     * @param request Inference request parameters
     * @return Result containing job ID on success, or error on failure
     *
     * @note The study must be accessible to the AI service (either via
     *       DICOM C-MOVE or DICOMweb WADO-RS)
     */
    [[nodiscard]] static auto request_inference(const inference_request& request)
        -> Result<std::string>;

    /**
     * @brief Check the status of an inference job
     *
     * @param job_id The job identifier returned from request_inference
     * @return Result containing current status on success
     */
    [[nodiscard]] static auto check_status(const std::string& job_id)
        -> Result<inference_status>;

    /**
     * @brief Cancel an inference job
     *
     * Attempts to cancel a pending or running job. Jobs that have
     * already completed cannot be cancelled.
     *
     * @param job_id The job identifier to cancel
     * @return Result indicating success or failure
     */
    [[nodiscard]] static auto cancel(const std::string& job_id)
        -> Result<std::monostate>;

    /**
     * @brief Wait for a job to complete
     *
     * Blocks until the job completes, fails, or times out.
     *
     * @param job_id The job identifier to wait for
     * @param timeout Maximum time to wait
     * @param status_callback Optional callback for status updates
     * @return Result containing final status on completion
     */
    [[nodiscard]] static auto wait_for_completion(
        const std::string& job_id,
        std::chrono::milliseconds timeout = std::chrono::minutes{30},
        status_callback callback = nullptr)
        -> Result<inference_status>;

    /**
     * @brief List active inference jobs
     *
     * Returns all jobs that are currently pending or running.
     *
     * @return Result containing list of active job statuses
     */
    [[nodiscard]] static auto list_active_jobs()
        -> Result<std::vector<inference_status>>;

    // =========================================================================
    // Model Management
    // =========================================================================

    /**
     * @brief List available AI models
     *
     * @return Result containing list of available models
     */
    [[nodiscard]] static auto list_models()
        -> Result<std::vector<model_info>>;

    /**
     * @brief Get information about a specific model
     *
     * @param model_id The model identifier
     * @return Result containing model information
     */
    [[nodiscard]] static auto get_model_info(const std::string& model_id)
        -> Result<model_info>;

    // =========================================================================
    // Health Check
    // =========================================================================

    /**
     * @brief Check AI service health
     *
     * @return true if the service is healthy and accessible
     */
    [[nodiscard]] static auto check_health() -> bool;

    /**
     * @brief Get current latency to the AI service
     *
     * @return Round-trip time to the service, or nullopt if unavailable
     */
    [[nodiscard]] static auto get_latency()
        -> std::optional<std::chrono::milliseconds>;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get the current configuration
     * @return Current AI service configuration
     */
    [[nodiscard]] static auto get_config() -> const ai_service_config&;

    /**
     * @brief Update authentication credentials
     *
     * @param auth_type New authentication type
     * @param credentials New credentials (API key, token, or username:password)
     * @return Result indicating success or failure
     */
    [[nodiscard]] static auto update_credentials(
        authentication_type auth_type,
        const std::string& credentials)
        -> Result<std::monostate>;

private:
    // Prevent instantiation
    ai_service_connector() = delete;
    ~ai_service_connector() = delete;
    ai_service_connector(const ai_service_connector&) = delete;
    ai_service_connector& operator=(const ai_service_connector&) = delete;

    // PIMPL implementation
    class impl;
    static std::unique_ptr<impl> pimpl_;
};

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * @brief Convert inference status code to string
 * @param status The status code
 * @return Human-readable status string
 */
[[nodiscard]] inline auto to_string(inference_status_code status) -> std::string {
    switch (status) {
        case inference_status_code::pending:
            return "pending";
        case inference_status_code::running:
            return "running";
        case inference_status_code::completed:
            return "completed";
        case inference_status_code::failed:
            return "failed";
        case inference_status_code::cancelled:
            return "cancelled";
        case inference_status_code::timeout:
            return "timeout";
        default:
            return "unknown";
    }
}

/**
 * @brief Convert authentication type to string
 * @param type The authentication type
 * @return Human-readable authentication type string
 */
[[nodiscard]] inline auto to_string(authentication_type type) -> std::string {
    switch (type) {
        case authentication_type::none:
            return "none";
        case authentication_type::api_key:
            return "api_key";
        case authentication_type::bearer_token:
            return "bearer_token";
        case authentication_type::basic:
            return "basic";
        default:
            return "unknown";
    }
}

}  // namespace pacs::ai
