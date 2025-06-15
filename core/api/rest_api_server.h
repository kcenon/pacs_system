/**
 * @file rest_api_server.h
 * @brief RESTful API server for PACS System
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#pragma once

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <thread>

#include "core/result/result.h"
#include "common/version/api_version.h"

namespace pacs {

/**
 * @brief HTTP method types
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS
};

/**
 * @brief HTTP request
 */
struct HttpRequest {
    HttpMethod method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> queryParams;
    std::string body;
    std::string clientIp;
};

/**
 * @brief HTTP response
 */
struct HttpResponse {
    int statusCode = 200;
    std::string statusMessage = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    
    HttpResponse() {
        headers["Content-Type"] = "application/json";
        headers["X-API-Version"] = ApiVersion::VERSION_STRING;
    }
};

/**
 * @brief API endpoint handler
 */
using ApiHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * @brief API endpoint with versioning
 */
struct ApiEndpoint {
    std::string path;
    HttpMethod method;
    int minVersion;  // Minimum API version required
    ApiHandler handler;
    std::string description;
    bool requiresAuth;
};

/**
 * @brief REST API server configuration
 */
struct RestApiConfig {
    std::string bindAddress = "0.0.0.0";
    uint16_t port = 8080;
    size_t threadPoolSize = 4;
    bool enableSSL = false;
    std::string sslCertPath;
    std::string sslKeyPath;
    bool enableCORS = true;
    std::vector<std::string> allowedOrigins = {"*"};
    size_t maxRequestSize = 10 * 1024 * 1024;  // 10MB
    std::chrono::seconds requestTimeout{30};
};

/**
 * @brief RESTful API server
 */
class RestApiServer {
public:
    explicit RestApiServer(const RestApiConfig& config);
    ~RestApiServer();
    
    /**
     * @brief Start the API server
     */
    core::Result<void> start();
    
    /**
     * @brief Stop the API server
     */
    void stop();
    
    /**
     * @brief Register API endpoint
     */
    void registerEndpoint(const ApiEndpoint& endpoint);
    
    /**
     * @brief Register middleware
     */
    using Middleware = std::function<bool(HttpRequest&, HttpResponse&)>;
    void addMiddleware(const std::string& name, Middleware middleware);
    
    /**
     * @brief Get server status
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Get API documentation
     */
    std::string getApiDocumentation() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    RestApiConfig config_;
    bool running_ = false;
    
    /**
     * @brief Register default endpoints
     */
    void registerDefaultEndpoints();
    
    /**
     * @brief Version negotiation handler
     */
    HttpResponse handleVersionRequest(const HttpRequest& request);
    
    /**
     * @brief Health check handler
     */
    HttpResponse handleHealthCheck(const HttpRequest& request);
    
    /**
     * @brief API documentation handler
     */
    HttpResponse handleApiDocs(const HttpRequest& request) const;
};

/**
 * @brief JSON helper utilities
 */
class JsonBuilder {
public:
    JsonBuilder& add(const std::string& key, const std::string& value);
    JsonBuilder& add(const std::string& key, int value);
    JsonBuilder& add(const std::string& key, bool value);
    JsonBuilder& addObject(const std::string& key, const JsonBuilder& obj);
    JsonBuilder& addArray(const std::string& key, const std::vector<JsonBuilder>& array);
    
    std::string build() const;
    
private:
    std::string json_;
};

/**
 * @brief Standard API response builders
 */
class ApiResponses {
public:
    static HttpResponse success(const std::string& message = "Success");
    static HttpResponse created(const std::string& location);
    static HttpResponse noContent();
    static HttpResponse badRequest(const std::string& error);
    static HttpResponse unauthorized(const std::string& error = "Unauthorized");
    static HttpResponse forbidden(const std::string& error = "Forbidden");
    static HttpResponse notFound(const std::string& error = "Not Found");
    static HttpResponse conflict(const std::string& error);
    static HttpResponse internalError(const std::string& error = "Internal Server Error");
    static HttpResponse notImplemented(const std::string& feature);
};

} // namespace pacs