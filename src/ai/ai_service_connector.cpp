/**
 * @file ai_service_connector.cpp
 * @brief Implementation of AI service connector
 */

#include <pacs/ai/ai_service_connector.hpp>

#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/monitoring_adapter.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace pacs::ai {

// =============================================================================
// Metric Names
// =============================================================================

namespace metrics {
constexpr const char* inference_requests_total = "pacs_ai_inference_requests_total";
constexpr const char* inference_requests_success = "pacs_ai_inference_requests_success";
constexpr const char* inference_requests_failed = "pacs_ai_inference_requests_failed";
constexpr const char* inference_duration = "pacs_ai_inference_duration_seconds";
constexpr const char* active_jobs = "pacs_ai_active_jobs";
}  // namespace metrics

// =============================================================================
// JSON Utilities
// =============================================================================

namespace json_util {

/**
 * @brief Escape special characters in JSON string
 */
[[nodiscard]] inline std::string escape_string(std::string_view str) {
    std::string result;
    result.reserve(str.size() + 10);

    for (char c : str) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }

    return result;
}

/**
 * @brief Convert time_point to ISO 8601 string
 */
[[nodiscard]] inline std::string to_iso8601(
    std::chrono::system_clock::time_point tp) {
    const auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_val{};

#if defined(_MSC_VER)
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

/**
 * @brief Parse ISO 8601 string to time_point
 */
[[nodiscard]] inline std::optional<std::chrono::system_clock::time_point>
from_iso8601(const std::string& str) {
    std::tm tm_val{};
    std::istringstream iss(str);
    iss >> std::get_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");

    if (iss.fail()) {
        return std::nullopt;
    }

#if defined(_MSC_VER)
    auto time_t_val = _mkgmtime(&tm_val);
#else
    auto time_t_val = timegm(&tm_val);
#endif

    return std::chrono::system_clock::from_time_t(time_t_val);
}

/**
 * @brief Simple JSON value extractor (for basic parsing)
 */
[[nodiscard]] inline std::optional<std::string> extract_string(
    const std::string& json,
    const std::string& key) {
    std::string search_key = "\"" + key + "\"";
    auto pos = json.find(search_key);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    pos = json.find('"', pos);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    auto start = pos + 1;
    auto end = json.find('"', start);
    if (end == std::string::npos) {
        return std::nullopt;
    }

    return json.substr(start, end - start);
}

/**
 * @brief Simple JSON integer extractor
 */
[[nodiscard]] inline std::optional<int> extract_int(
    const std::string& json,
    const std::string& key) {
    std::string search_key = "\"" + key + "\"";
    auto pos = json.find(search_key);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    // Skip whitespace
    pos++;
    while (pos < json.size() && std::isspace(json[pos])) {
        pos++;
    }

    if (pos >= json.size()) {
        return std::nullopt;
    }

    try {
        std::size_t end_pos;
        int value = std::stoi(json.substr(pos), &end_pos);
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief Build inference request JSON
 */
[[nodiscard]] inline std::string build_request_json(const inference_request& request) {
    std::ostringstream oss;
    oss << "{"
        << R"("study_instance_uid":")" << escape_string(request.study_instance_uid) << "\"";

    if (request.series_instance_uid) {
        oss << R"(,"series_instance_uid":")"
            << escape_string(*request.series_instance_uid) << "\"";
    }

    oss << R"(,"model_id":")" << escape_string(request.model_id) << "\""
        << R"(,"priority":)" << request.priority;

    if (request.callback_url) {
        oss << R"(,"callback_url":")" << escape_string(*request.callback_url) << "\"";
    }

    if (!request.parameters.empty()) {
        oss << R"(,"parameters":{)";
        bool first = true;
        for (const auto& [key, value] : request.parameters) {
            if (!first) oss << ",";
            oss << "\"" << escape_string(key) << "\":\"" << escape_string(value) << "\"";
            first = false;
        }
        oss << "}";
    }

    if (!request.metadata.empty()) {
        oss << R"(,"metadata":{)";
        bool first = true;
        for (const auto& [key, value] : request.metadata) {
            if (!first) oss << ",";
            oss << "\"" << escape_string(key) << "\":\"" << escape_string(value) << "\"";
            first = false;
        }
        oss << "}";
    }

    oss << "}";
    return oss.str();
}

/**
 * @brief Parse inference status from JSON response
 */
[[nodiscard]] inline std::optional<inference_status> parse_status_json(
    const std::string& json) {
    inference_status status;

    auto job_id = extract_string(json, "job_id");
    if (!job_id) {
        return std::nullopt;
    }
    status.job_id = *job_id;

    auto status_str = extract_string(json, "status");
    if (status_str) {
        if (*status_str == "pending") {
            status.status = inference_status_code::pending;
        } else if (*status_str == "running") {
            status.status = inference_status_code::running;
        } else if (*status_str == "completed") {
            status.status = inference_status_code::completed;
        } else if (*status_str == "failed") {
            status.status = inference_status_code::failed;
        } else if (*status_str == "cancelled") {
            status.status = inference_status_code::cancelled;
        } else if (*status_str == "timeout") {
            status.status = inference_status_code::timeout;
        }
    }

    auto progress = extract_int(json, "progress");
    if (progress) {
        status.progress = *progress;
    }

    auto message = extract_string(json, "message");
    if (message) {
        status.message = *message;
    }

    auto error = extract_string(json, "error_message");
    if (error) {
        status.error_message = *error;
    }

    auto created_at = extract_string(json, "created_at");
    if (created_at) {
        auto tp = from_iso8601(*created_at);
        if (tp) {
            status.created_at = *tp;
        }
    }

    auto started_at = extract_string(json, "started_at");
    if (started_at) {
        auto tp = from_iso8601(*started_at);
        if (tp) {
            status.started_at = *tp;
        }
    }

    auto completed_at = extract_string(json, "completed_at");
    if (completed_at) {
        auto tp = from_iso8601(*completed_at);
        if (tp) {
            status.completed_at = *tp;
        }
    }

    return status;
}

/**
 * @brief Parse model info from JSON
 */
[[nodiscard]] inline std::optional<model_info> parse_model_json(
    const std::string& json) {
    model_info info;

    auto model_id = extract_string(json, "model_id");
    if (!model_id) {
        return std::nullopt;
    }
    info.model_id = *model_id;

    auto name = extract_string(json, "name");
    if (name) {
        info.name = *name;
    }

    auto description = extract_string(json, "description");
    if (description) {
        info.description = *description;
    }

    auto version = extract_string(json, "version");
    if (version) {
        info.version = *version;
    }

    return info;
}

}  // namespace json_util

// =============================================================================
// HTTP Client Abstraction
// =============================================================================

/**
 * @brief Simple HTTP response structure
 */
struct http_response {
    int status_code{0};
    std::string body;
    std::map<std::string, std::string> headers;

    [[nodiscard]] bool is_success() const noexcept {
        return status_code >= 200 && status_code < 300;
    }
};

/**
 * @brief HTTP client interface for AI service communication
 *
 * This is an abstraction layer that can be implemented using different
 * HTTP libraries (e.g., libcurl, Boost.Beast, or custom implementation).
 */
class http_client {
public:
    explicit http_client(const ai_service_config& config)
        : config_(config) {}

    [[nodiscard]] auto post(const std::string& path,
                            const std::string& body,
                            [[maybe_unused]] const std::string& content_type = "application/json")
        -> Result<http_response> {
        // Build full URL
        std::string url = config_.base_url + path;

        // Log the request
        pacs::integration::logger_adapter::debug(
            "AI service POST request: {} (body size: {} bytes)",
            url, body.size());

        // TODO: Implement actual HTTP POST using network_system or libcurl
        // For now, return a mock response for compilation

        // Simulate network latency for testing
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Mock successful response
        http_response response;
        response.status_code = 200;
        response.body = R"({"job_id":"mock-job-12345","status":"pending","progress":0})";

        return response;
    }

    [[nodiscard]] auto get(const std::string& path)
        -> Result<http_response> {
        std::string url = config_.base_url + path;

        pacs::integration::logger_adapter::debug(
            "AI service GET request: {}", url);

        // TODO: Implement actual HTTP GET
        // For now, return a mock response

        http_response response;
        response.status_code = 200;
        response.body = R"({"job_id":"mock-job-12345","status":"completed","progress":100})";

        return response;
    }

    [[nodiscard]] auto del(const std::string& path)
        -> Result<http_response> {
        std::string url = config_.base_url + path;

        pacs::integration::logger_adapter::debug(
            "AI service DELETE request: {}", url);

        // TODO: Implement actual HTTP DELETE

        http_response response;
        response.status_code = 204;

        return response;
    }

    [[nodiscard]] auto check_connectivity() -> bool {
        auto result = get("/health");
        return result.is_ok() && result.value().is_success();
    }

    [[nodiscard]] auto measure_latency() -> std::optional<std::chrono::milliseconds> {
        auto start = std::chrono::steady_clock::now();
        auto result = get("/health");
        auto end = std::chrono::steady_clock::now();

        if (!result.is_ok()) {
            return std::nullopt;
        }

        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

private:
    ai_service_config config_;

    [[nodiscard]] std::map<std::string, std::string> build_headers() const {
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";

        switch (config_.auth_type) {
            case authentication_type::api_key:
                headers["X-API-Key"] = config_.api_key;
                break;
            case authentication_type::bearer_token:
                headers["Authorization"] = "Bearer " + config_.bearer_token;
                break;
            case authentication_type::basic: {
                // Base64 encode username:password
                std::string credentials = config_.username + ":" + config_.password;
                // TODO: Implement proper Base64 encoding
                headers["Authorization"] = "Basic " + credentials;
                break;
            }
            case authentication_type::none:
            default:
                break;
        }

        return headers;
    }
};

// =============================================================================
// Job Tracker
// =============================================================================

/**
 * @brief Tracks active inference jobs
 */
class job_tracker {
public:
    void add_job(const std::string& job_id, const inference_request& request) {
        std::lock_guard lock(mutex_);

        inference_status status;
        status.job_id = job_id;
        status.status = inference_status_code::pending;
        status.progress = 0;
        status.message = "Job submitted";
        status.created_at = std::chrono::system_clock::now();

        jobs_[job_id] = status;
        request_map_[job_id] = request;

        pacs::integration::monitoring_adapter::set_gauge(
            metrics::active_jobs, static_cast<double>(jobs_.size()));
    }

    void update_status(const std::string& job_id, const inference_status& status) {
        std::lock_guard lock(mutex_);

        auto it = jobs_.find(job_id);
        if (it != jobs_.end()) {
            it->second = status;

            // If job completed or failed, record metrics
            if (status.status == inference_status_code::completed ||
                status.status == inference_status_code::failed) {

                // Note: request_map_ entry may be used for future logging
                (void)request_map_[job_id];
                auto duration = status.completed_at.value_or(std::chrono::system_clock::now())
                              - status.created_at;

                pacs::integration::monitoring_adapter::record_timing(
                    metrics::inference_duration,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(duration));

                if (status.status == inference_status_code::completed) {
                    pacs::integration::monitoring_adapter::increment_counter(
                        metrics::inference_requests_success);
                } else {
                    pacs::integration::monitoring_adapter::increment_counter(
                        metrics::inference_requests_failed);
                }
            }
        }
    }

    void remove_job(const std::string& job_id) {
        std::lock_guard lock(mutex_);
        jobs_.erase(job_id);
        request_map_.erase(job_id);

        pacs::integration::monitoring_adapter::set_gauge(
            metrics::active_jobs, static_cast<double>(jobs_.size()));
    }

    [[nodiscard]] std::optional<inference_status> get_status(const std::string& job_id) const {
        std::shared_lock lock(mutex_);
        auto it = jobs_.find(job_id);
        if (it != jobs_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<inference_status> get_active_jobs() const {
        std::shared_lock lock(mutex_);
        std::vector<inference_status> result;
        for (const auto& [id, status] : jobs_) {
            if (status.status == inference_status_code::pending ||
                status.status == inference_status_code::running) {
                result.push_back(status);
            }
        }
        return result;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, inference_status> jobs_;
    std::unordered_map<std::string, inference_request> request_map_;
};

// =============================================================================
// AI Service Connector Implementation
// =============================================================================

class ai_service_connector::impl {
public:
    impl() = default;

    ~impl() {
        shutdown();
    }

    auto initialize(const ai_service_config& config) -> Result<std::monostate> {
        std::lock_guard lock(mutex_);

        if (initialized_) {
            return std::monostate{};
        }

        config_ = config;

        // Validate configuration
        if (config.base_url.empty()) {
            return error_info(-1, "Base URL is required", "ai_service_connector");
        }

        // Initialize HTTP client
        http_client_ = std::make_unique<http_client>(config);

        // Test connectivity
        if (!http_client_->check_connectivity()) {
            pacs::integration::logger_adapter::warn(
                "AI service at {} is not responding, continuing anyway",
                config.base_url);
        }

        // Initialize job tracker
        job_tracker_ = std::make_unique<job_tracker>();

        // Initialize metrics
        pacs::integration::monitoring_adapter::set_gauge(metrics::active_jobs, 0);

        pacs::integration::logger_adapter::info(
            "AI service connector initialized: url={}, auth={}",
            config.base_url, to_string(config.auth_type));

        initialized_ = true;
        return std::monostate{};
    }

    void shutdown() {
        std::lock_guard lock(mutex_);

        if (!initialized_) {
            return;
        }

        pacs::integration::logger_adapter::info("AI service connector shutting down");

        http_client_.reset();
        job_tracker_.reset();
        initialized_ = false;
    }

    [[nodiscard]] bool is_initialized() const noexcept {
        return initialized_.load();
    }

    auto request_inference(const inference_request& request) -> Result<std::string> {
        if (!initialized_) {
            return error_info(-1, "AI service connector not initialized", "ai_service_connector");
        }

        // Validate request
        if (request.study_instance_uid.empty()) {
            return error_info(-1, "Study Instance UID is required", "ai_service_connector");
        }
        if (request.model_id.empty()) {
            return error_info(-1, "Model ID is required", "ai_service_connector");
        }

        // Build request JSON
        std::string body = json_util::build_request_json(request);

        // Send request
        auto response = http_client_->post("/inference", body);
        if (response.is_err()) {
            pacs::integration::logger_adapter::error(
                "Failed to submit inference request: {}",
                response.error().message);
            return response.error();
        }

        if (!response.value().is_success()) {
            return error_info(
                response.value().status_code,
                "AI service returned error: " + response.value().body,
                "ai_service_connector");
        }

        // Parse response to get job ID
        auto job_id = json_util::extract_string(response.value().body, "job_id");
        if (!job_id) {
            return error_info(-1, "Failed to parse job ID from response", "ai_service_connector");
        }

        // Track the job
        job_tracker_->add_job(*job_id, request);

        pacs::integration::logger_adapter::info(
            "Inference request submitted: job_id={}, study={}, model={}",
            *job_id, request.study_instance_uid, request.model_id);

        pacs::integration::monitoring_adapter::increment_counter(
            metrics::inference_requests_total);

        return *job_id;
    }

    auto check_status(const std::string& job_id) -> Result<inference_status> {
        if (!initialized_) {
            return error_info(-1, "AI service connector not initialized", "ai_service_connector");
        }

        // Check local cache first
        auto cached = job_tracker_->get_status(job_id);
        if (cached && (cached->status == inference_status_code::completed ||
                       cached->status == inference_status_code::failed ||
                       cached->status == inference_status_code::cancelled)) {
            return *cached;
        }

        // Query remote service
        auto response = http_client_->get("/inference/" + job_id);
        if (response.is_err()) {
            return response.error();
        }

        if (!response.value().is_success()) {
            if (response.value().status_code == 404) {
                return error_info(404, "Job not found: " + job_id, "ai_service_connector");
            }
            return error_info(
                response.value().status_code,
                "Failed to get job status",
                "ai_service_connector");
        }

        // Parse status
        auto status = json_util::parse_status_json(response.value().body);
        if (!status) {
            return error_info(-1, "Failed to parse status response", "ai_service_connector");
        }

        // Update local cache
        job_tracker_->update_status(job_id, *status);

        return *status;
    }

    auto cancel(const std::string& job_id) -> Result<std::monostate> {
        if (!initialized_) {
            return error_info(-1, "AI service connector not initialized", "ai_service_connector");
        }

        auto response = http_client_->del("/inference/" + job_id);
        if (response.is_err()) {
            return response.error();
        }

        if (!response.value().is_success() && response.value().status_code != 404) {
            return error_info(
                response.value().status_code,
                "Failed to cancel job",
                "ai_service_connector");
        }

        // Update local status
        inference_status status;
        status.job_id = job_id;
        status.status = inference_status_code::cancelled;
        status.message = "Job cancelled by user";
        status.completed_at = std::chrono::system_clock::now();
        job_tracker_->update_status(job_id, status);

        pacs::integration::logger_adapter::info("Inference job cancelled: {}", job_id);

        return std::monostate{};
    }

    auto wait_for_completion(const std::string& job_id,
                             std::chrono::milliseconds timeout,
                             status_callback callback) -> Result<inference_status> {
        if (!initialized_) {
            return error_info(-1, "AI service connector not initialized", "ai_service_connector");
        }

        auto start_time = std::chrono::steady_clock::now();
        auto deadline = start_time + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            auto result = check_status(job_id);
            if (result.is_err()) {
                return result;
            }

            auto& status = result.value();

            // Call callback if provided
            if (callback) {
                callback(status);
            }

            // Check if job is complete
            if (status.status == inference_status_code::completed ||
                status.status == inference_status_code::failed ||
                status.status == inference_status_code::cancelled ||
                status.status == inference_status_code::timeout) {
                return status;
            }

            // Wait before polling again
            std::this_thread::sleep_for(config_.polling_interval);
        }

        // Timeout
        inference_status timeout_status;
        timeout_status.job_id = job_id;
        timeout_status.status = inference_status_code::timeout;
        timeout_status.message = "Timed out waiting for job completion";
        return timeout_status;
    }

    auto list_active_jobs() -> Result<std::vector<inference_status>> {
        if (!initialized_) {
            return error_info(-1, "AI service connector not initialized", "ai_service_connector");
        }

        return job_tracker_->get_active_jobs();
    }

    auto list_models() -> Result<std::vector<model_info>> {
        if (!initialized_) {
            return error_info(-1, "AI service connector not initialized", "ai_service_connector");
        }

        auto response = http_client_->get("/models");
        if (response.is_err()) {
            return response.error();
        }

        if (!response.value().is_success()) {
            return error_info(
                response.value().status_code,
                "Failed to list models",
                "ai_service_connector");
        }

        // TODO: Parse models array from response
        // For now, return empty list
        return std::vector<model_info>{};
    }

    auto get_model_info(const std::string& model_id) -> Result<model_info> {
        if (!initialized_) {
            return error_info(-1, "AI service connector not initialized", "ai_service_connector");
        }

        auto response = http_client_->get("/models/" + model_id);
        if (response.is_err()) {
            return response.error();
        }

        if (!response.value().is_success()) {
            return error_info(
                response.value().status_code,
                "Failed to get model info",
                "ai_service_connector");
        }

        auto info = json_util::parse_model_json(response.value().body);
        if (!info) {
            return error_info(-1, "Failed to parse model info", "ai_service_connector");
        }

        return *info;
    }

    [[nodiscard]] bool check_health() {
        if (!initialized_ || !http_client_) {
            return false;
        }
        return http_client_->check_connectivity();
    }

    [[nodiscard]] auto get_latency() -> std::optional<std::chrono::milliseconds> {
        if (!initialized_ || !http_client_) {
            return std::nullopt;
        }
        return http_client_->measure_latency();
    }

    [[nodiscard]] const ai_service_config& get_config() const {
        return config_;
    }

    auto update_credentials(authentication_type auth_type,
                            const std::string& credentials) -> Result<std::monostate> {
        std::lock_guard lock(mutex_);

        config_.auth_type = auth_type;

        switch (auth_type) {
            case authentication_type::api_key:
                config_.api_key = credentials;
                break;
            case authentication_type::bearer_token:
                config_.bearer_token = credentials;
                break;
            case authentication_type::basic: {
                auto colon_pos = credentials.find(':');
                if (colon_pos != std::string::npos) {
                    config_.username = credentials.substr(0, colon_pos);
                    config_.password = credentials.substr(colon_pos + 1);
                } else {
                    return error_info(-1, "Basic auth credentials must be in format 'username:password'",
                                      "ai_service_connector");
                }
                break;
            }
            case authentication_type::none:
                break;
        }

        // Recreate HTTP client with new credentials
        http_client_ = std::make_unique<http_client>(config_);

        pacs::integration::logger_adapter::info(
            "AI service credentials updated: auth={}",
            to_string(auth_type));

        return std::monostate{};
    }

private:
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_{false};
    ai_service_config config_;
    std::unique_ptr<http_client> http_client_;
    std::unique_ptr<job_tracker> job_tracker_;
};

// =============================================================================
// Static Member Initialization
// =============================================================================

std::unique_ptr<ai_service_connector::impl> ai_service_connector::pimpl_ =
    std::make_unique<ai_service_connector::impl>();

// =============================================================================
// Public API Implementation
// =============================================================================

auto ai_service_connector::initialize(const ai_service_config& config)
    -> Result<std::monostate> {
    return pimpl_->initialize(config);
}

void ai_service_connector::shutdown() {
    pimpl_->shutdown();
}

auto ai_service_connector::is_initialized() noexcept -> bool {
    return pimpl_->is_initialized();
}

auto ai_service_connector::request_inference(const inference_request& request)
    -> Result<std::string> {
    return pimpl_->request_inference(request);
}

auto ai_service_connector::check_status(const std::string& job_id)
    -> Result<inference_status> {
    return pimpl_->check_status(job_id);
}

auto ai_service_connector::cancel(const std::string& job_id)
    -> Result<std::monostate> {
    return pimpl_->cancel(job_id);
}

auto ai_service_connector::wait_for_completion(
    const std::string& job_id,
    std::chrono::milliseconds timeout,
    status_callback callback)
    -> Result<inference_status> {
    return pimpl_->wait_for_completion(job_id, timeout, std::move(callback));
}

auto ai_service_connector::list_active_jobs()
    -> Result<std::vector<inference_status>> {
    return pimpl_->list_active_jobs();
}

auto ai_service_connector::list_models()
    -> Result<std::vector<model_info>> {
    return pimpl_->list_models();
}

auto ai_service_connector::get_model_info(const std::string& model_id)
    -> Result<model_info> {
    return pimpl_->get_model_info(model_id);
}

auto ai_service_connector::check_health() -> bool {
    return pimpl_->check_health();
}

auto ai_service_connector::get_latency()
    -> std::optional<std::chrono::milliseconds> {
    return pimpl_->get_latency();
}

auto ai_service_connector::get_config() -> const ai_service_config& {
    return pimpl_->get_config();
}

auto ai_service_connector::update_credentials(
    authentication_type auth_type,
    const std::string& credentials)
    -> Result<std::monostate> {
    return pimpl_->update_credentials(auth_type, credentials);
}

}  // namespace pacs::ai
