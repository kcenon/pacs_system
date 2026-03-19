// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file ai_service_connector.cpp
 * @brief Implementation of AI service connector
 */

#include <pacs/ai/ai_service_connector.hpp>

#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/monitoring_adapter.hpp>

// Platform-specific socket headers for HTTP client
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

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

namespace kcenon::pacs::ai {

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

/**
 * @brief Encode data to Base64
 */
[[nodiscard]] inline std::string base64_encode(std::string_view input) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((input.size() + 2) / 3) * 4);

    auto ptr = reinterpret_cast<const unsigned char*>(input.data());
    auto len = input.size();

    for (std::size_t i = 0; i < len; i += 3) {
        uint32_t octet_a = ptr[i];
        uint32_t octet_b = (i + 1 < len) ? ptr[i + 1] : 0;
        uint32_t octet_c = (i + 2 < len) ? ptr[i + 2] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        result += table[(triple >> 18) & 0x3F];
        result += table[(triple >> 12) & 0x3F];
        result += (i + 1 < len) ? table[(triple >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? table[triple & 0x3F] : '=';
    }

    return result;
}

/**
 * @brief Extract JSON objects from a JSON array string
 */
[[nodiscard]] inline std::vector<std::string> extract_json_array_objects(
    const std::string& json) {
    std::vector<std::string> objects;

    // Find array start
    auto arr_start = json.find('[');
    if (arr_start == std::string::npos) {
        return objects;
    }

    int depth = 0;
    std::size_t obj_start = std::string::npos;

    for (std::size_t i = arr_start + 1; i < json.size(); ++i) {
        char c = json[i];

        if (c == '"') {
            // Skip string content
            ++i;
            while (i < json.size() && json[i] != '"') {
                if (json[i] == '\\') ++i;  // Skip escaped character
                ++i;
            }
            continue;
        }

        if (c == '{') {
            if (depth == 0) {
                obj_start = i;
            }
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && obj_start != std::string::npos) {
                objects.push_back(json.substr(obj_start, i - obj_start + 1));
                obj_start = std::string::npos;
            }
        } else if (c == ']' && depth == 0) {
            break;
        }
    }

    return objects;
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
 * @brief RAII wrapper for platform socket handle
 */
class socket_handle {
public:
#ifdef _WIN32
    using native_type = SOCKET;
    static constexpr native_type invalid_value = INVALID_SOCKET;
#else
    using native_type = int;
    static constexpr native_type invalid_value = -1;
#endif

    socket_handle() = default;
    explicit socket_handle(native_type fd) : fd_(fd) {}
    ~socket_handle() { close(); }

    socket_handle(const socket_handle&) = delete;
    socket_handle& operator=(const socket_handle&) = delete;

    socket_handle(socket_handle&& other) noexcept : fd_(other.fd_) {
        other.fd_ = invalid_value;
    }

    socket_handle& operator=(socket_handle&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = invalid_value;
        }
        return *this;
    }

    [[nodiscard]] bool valid() const noexcept { return fd_ != invalid_value; }
    [[nodiscard]] native_type get() const noexcept { return fd_; }

    void close() noexcept {
        if (fd_ != invalid_value) {
#ifdef _WIN32
            closesocket(fd_);
#else
            ::close(fd_);
#endif
            fd_ = invalid_value;
        }
    }

private:
    native_type fd_{invalid_value};
};

/**
 * @brief Parse URL into host, port, and path components
 */
struct parsed_url {
    std::string host;
    std::string port;
    std::string path;

    static std::optional<parsed_url> parse(const std::string& url) {
        parsed_url result;

        // Find scheme end
        auto scheme_end = url.find("://");
        if (scheme_end == std::string::npos) return std::nullopt;

        auto host_start = scheme_end + 3;
        auto path_start = url.find('/', host_start);

        std::string host_port;
        if (path_start == std::string::npos) {
            host_port = url.substr(host_start);
            result.path = "/";
        } else {
            host_port = url.substr(host_start, path_start - host_start);
            result.path = url.substr(path_start);
        }

        auto colon = host_port.find(':');
        if (colon != std::string::npos) {
            result.host = host_port.substr(0, colon);
            result.port = host_port.substr(colon + 1);
        } else {
            result.host = host_port;
            result.port = "80";
        }

        return result;
    }
};

/**
 * @brief HTTP client for AI service communication
 *
 * Uses synchronous POSIX/Winsock sockets for HTTP/1.1 operations.
 * No dependency on network_system internals (messaging_client, thread_manager).
 */
class http_client {
public:
    explicit http_client(const ai_service_config& config)
        : config_(config) {}

    [[nodiscard]] auto post(const std::string& path,
                            const std::string& body,
                            const std::string& content_type = "application/json")
        -> Result<http_response> {
        kcenon::pacs::integration::logger_adapter::debug(
            "AI service POST request: {} (body size: {} bytes)",
            config_.base_url + path, body.size());

        auto headers = build_headers();
        headers["Content-Type"] = content_type;
        return send_request("POST", path, headers, body);
    }

    [[nodiscard]] auto get(const std::string& path)
        -> Result<http_response> {
        kcenon::pacs::integration::logger_adapter::debug(
            "AI service GET request: {}", config_.base_url + path);

        auto headers = build_headers();
        return send_request("GET", path, headers, "");
    }

    [[nodiscard]] auto del(const std::string& path)
        -> Result<http_response> {
        kcenon::pacs::integration::logger_adapter::debug(
            "AI service DELETE request: {}", config_.base_url + path);

        auto headers = build_headers();
        return send_request("DELETE", path, headers, "");
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

    [[nodiscard]] auto send_request(
        const std::string& method,
        const std::string& path,
        const std::map<std::string, std::string>& headers,
        const std::string& body) -> Result<http_response> {

        std::string full_url = config_.base_url + path;
        auto url = parsed_url::parse(full_url);
        if (!url) {
            return error_info(-1, "Invalid URL: " + full_url, "ai_service_connector");
        }

        // Resolve hostname
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        struct addrinfo* addr_result = nullptr;
        int gai_err = getaddrinfo(url->host.c_str(), url->port.c_str(),
                                  &hints, &addr_result);
        if (gai_err != 0) {
            return error_info(-1,
                "DNS resolution failed for " + url->host + ": " +
                    gai_strerror(gai_err),
                "ai_service_connector");
        }

        // RAII cleanup for addrinfo
        auto addr_cleanup = [](struct addrinfo* p) { freeaddrinfo(p); };
        std::unique_ptr<struct addrinfo, decltype(addr_cleanup)>
            addr_guard(addr_result, addr_cleanup);

        // Create and connect socket
        socket_handle sock(
            ::socket(addr_result->ai_family, addr_result->ai_socktype,
                     addr_result->ai_protocol));
        if (!sock.valid()) {
            return error_info(-1, "Failed to create socket", "ai_service_connector");
        }

        // Set socket timeout
        auto timeout_sec = std::chrono::duration_cast<std::chrono::seconds>(
            config_.connection_timeout).count();
        if (timeout_sec <= 0) timeout_sec = 30;

#ifdef _WIN32
        DWORD tv = static_cast<DWORD>(timeout_sec * 1000);
        setsockopt(sock.get(), SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(sock.get(), SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
        struct timeval tv{};
        tv.tv_sec = timeout_sec;
        setsockopt(sock.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock.get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

        // Connect
        if (::connect(sock.get(), addr_result->ai_addr,
                      static_cast<int>(addr_result->ai_addrlen)) != 0) {
            return error_info(-1,
                "Connection failed to " + url->host + ":" + url->port,
                "ai_service_connector");
        }

        // Build HTTP request
        std::ostringstream request_stream;
        request_stream << method << " " << url->path << " HTTP/1.1\r\n";
        request_stream << "Host: " << url->host << "\r\n";
        request_stream << "Connection: close\r\n";

        for (const auto& [name, value] : headers) {
            request_stream << name << ": " << value << "\r\n";
        }

        if (!body.empty()) {
            request_stream << "Content-Length: " << body.size() << "\r\n";
        }
        request_stream << "\r\n";
        request_stream << body;

        std::string request_data = request_stream.str();

        // Send request
        auto total_sent = std::size_t{0};
        while (total_sent < request_data.size()) {
            auto sent = ::send(sock.get(),
                               request_data.data() + total_sent,
                               static_cast<int>(request_data.size() - total_sent),
                               0);
            if (sent <= 0) {
                return error_info(-1, "Failed to send HTTP request",
                                  "ai_service_connector");
            }
            total_sent += static_cast<std::size_t>(sent);
        }

        // Receive response
        std::string response_data;
        char buffer[4096];
        while (true) {
            auto received = ::recv(sock.get(), buffer, sizeof(buffer), 0);
            if (received < 0) {
                return error_info(-1, "Failed to receive HTTP response",
                                  "ai_service_connector");
            }
            if (received == 0) break;  // Connection closed
            response_data.append(buffer, static_cast<std::size_t>(received));
        }

        // Parse HTTP response
        return parse_http_response(response_data);
    }

    [[nodiscard]] static auto parse_http_response(const std::string& raw)
        -> Result<http_response> {
        http_response response;

        auto header_end = raw.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            return error_info(-1, "Malformed HTTP response", "ai_service_connector");
        }

        // Parse status line
        auto first_line_end = raw.find("\r\n");
        std::string status_line = raw.substr(0, first_line_end);

        // Parse "HTTP/1.x STATUS_CODE REASON"
        auto first_space = status_line.find(' ');
        if (first_space == std::string::npos) {
            return error_info(-1, "Invalid HTTP status line", "ai_service_connector");
        }
        auto code_start = first_space + 1;
        try {
            response.status_code = std::stoi(status_line.substr(code_start));
        } catch (...) {
            return error_info(-1, "Invalid HTTP status code", "ai_service_connector");
        }

        // Parse headers
        auto headers_str = raw.substr(first_line_end + 2,
                                      header_end - first_line_end - 2);
        std::istringstream header_stream(headers_str);
        std::string line;
        while (std::getline(header_stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto name = line.substr(0, colon);
                auto value = line.substr(colon + 1);
                // Trim leading whitespace from value
                auto val_start = value.find_first_not_of(' ');
                if (val_start != std::string::npos) {
                    value = value.substr(val_start);
                }
                response.headers[name] = value;
            }
        }

        // Extract body
        response.body = raw.substr(header_end + 4);

        return response;
    }

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
                std::string credentials = config_.username + ":" + config_.password;
                headers["Authorization"] = "Basic " + json_util::base64_encode(credentials);
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

        kcenon::pacs::integration::monitoring_adapter::set_gauge(
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

                kcenon::pacs::integration::monitoring_adapter::record_timing(
                    metrics::inference_duration,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(duration));

                if (status.status == inference_status_code::completed) {
                    kcenon::pacs::integration::monitoring_adapter::increment_counter(
                        metrics::inference_requests_success);
                } else {
                    kcenon::pacs::integration::monitoring_adapter::increment_counter(
                        metrics::inference_requests_failed);
                }
            }
        }
    }

    void remove_job(const std::string& job_id) {
        std::lock_guard lock(mutex_);
        jobs_.erase(job_id);
        request_map_.erase(job_id);

        kcenon::pacs::integration::monitoring_adapter::set_gauge(
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
            kcenon::pacs::integration::logger_adapter::warn(
                "AI service at {} is not responding, continuing anyway",
                config.base_url);
        }

        // Initialize job tracker
        job_tracker_ = std::make_unique<job_tracker>();

        // Initialize metrics
        kcenon::pacs::integration::monitoring_adapter::set_gauge(metrics::active_jobs, 0);

        kcenon::pacs::integration::logger_adapter::info(
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

        kcenon::pacs::integration::logger_adapter::info("AI service connector shutting down");

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
            kcenon::pacs::integration::logger_adapter::error(
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

        kcenon::pacs::integration::logger_adapter::info(
            "Inference request submitted: job_id={}, study={}, model={}",
            *job_id, request.study_instance_uid, request.model_id);

        kcenon::pacs::integration::monitoring_adapter::increment_counter(
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

        kcenon::pacs::integration::logger_adapter::info("Inference job cancelled: {}", job_id);

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

        // Parse models array from JSON response
        std::vector<model_info> models;
        auto model_objects = json_util::extract_json_array_objects(
            response.value().body);

        for (const auto& obj : model_objects) {
            auto info = json_util::parse_model_json(obj);
            if (info) {
                models.push_back(std::move(*info));
            }
        }

        return models;
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

        kcenon::pacs::integration::logger_adapter::info(
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

}  // namespace kcenon::pacs::ai
