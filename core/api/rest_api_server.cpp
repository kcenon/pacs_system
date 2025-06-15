/**
 * @file rest_api_server.cpp
 * @brief RESTful API server implementation
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#include "rest_api_server.h"
#include "common/logger/logger.h"
#include "common/security/security_manager.h"
#include "common/audit/audit_logger.h"
// PACS server functionality is integrated

#include <sstream>
#include <chrono>
#include <iomanip>

namespace pacs {

class RestApiServer::Impl {
public:
    std::unordered_map<std::string, std::vector<ApiEndpoint>> endpoints_;
    std::vector<std::pair<std::string, Middleware>> middlewares_;
    std::unique_ptr<std::thread> serverThread_;
    
    HttpResponse routeRequest(const HttpRequest& request) {
        // Find matching endpoint
        auto it = endpoints_.find(request.path);
        if (it == endpoints_.end()) {
            return ApiResponses::notFound("Endpoint not found");
        }
        
        // Find handler for method
        for (const auto& endpoint : it->second) {
            if (endpoint.method == request.method) {
                // Check API version
                auto versionHeader = request.headers.find("X-API-Version");
                if (versionHeader != request.headers.end()) {
                    try {
                        int clientVersion = std::stoi(versionHeader->second);
                        if (clientVersion < endpoint.minVersion) {
                            return ApiResponses::badRequest(
                                "API version " + std::to_string(endpoint.minVersion) + " required");
                        }
                    } catch (...) {
                        // Invalid version header
                    }
                }
                
                // Execute handler
                return endpoint.handler(request);
            }
        }
        
        return ApiResponses::badRequest("Method not allowed");
    }
};

RestApiServer::RestApiServer(const RestApiConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {
    registerDefaultEndpoints();
}

RestApiServer::~RestApiServer() {
    stop();
}

core::Result<void> RestApiServer::start() {
    if (running_) {
        return core::Result<void>::error("Server already running");
    }
    
    try {
        // Start HTTP server in background thread
        impl_->serverThread_ = std::make_unique<std::thread>([this]() {
            common::logger::logInfo("REST API server starting on {}:{}", 
                            config_.bindAddress, config_.port);
            
            // Simulated server loop (in real implementation, use proper HTTP server library)
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        
        running_ = true;
        common::logger::logInfo("REST API server started successfully");
        
        return core::Result<void>::ok();
    } catch (const std::exception& e) {
        return core::Result<void>::error(std::string("Failed to start server: ") + e.what());
    }
}

void RestApiServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (impl_->serverThread_ && impl_->serverThread_->joinable()) {
        impl_->serverThread_->join();
    }
    
    common::logger::logInfo("REST API server stopped");
}

void RestApiServer::registerEndpoint(const ApiEndpoint& endpoint) {
    impl_->endpoints_[endpoint.path].push_back(endpoint);
    common::logger::logDebug("Registered endpoint: {} {}", 
                     endpoint.method == HttpMethod::GET ? "GET" :
                     endpoint.method == HttpMethod::POST ? "POST" :
                     endpoint.method == HttpMethod::PUT ? "PUT" :
                     endpoint.method == HttpMethod::DELETE ? "DELETE" : "OTHER",
                     endpoint.path);
}

void RestApiServer::addMiddleware(const std::string& name, Middleware middleware) {
    impl_->middlewares_.emplace_back(name, middleware);
}

void RestApiServer::registerDefaultEndpoints() {
    // API version endpoint
    registerEndpoint({
        "/api/version",
        HttpMethod::GET,
        1,
        [this](const HttpRequest& req) { return handleVersionRequest(req); },
        "Get API version information",
        false
    });
    
    // Health check endpoint
    registerEndpoint({
        "/api/health",
        HttpMethod::GET,
        1,
        [this](const HttpRequest& req) { return handleHealthCheck(req); },
        "Health check endpoint",
        false
    });
    
    // API documentation
    registerEndpoint({
        "/api/docs",
        HttpMethod::GET,
        1,
        [this](const HttpRequest& req) { return handleApiDocs(req); },
        "Get API documentation",
        false
    });
    
    // Study endpoints
    registerEndpoint({
        "/api/v1/studies",
        HttpMethod::GET,
        1,
        [](const HttpRequest& req) {
            JsonBuilder response;
            response.add("status", "success")
                    .add("count", 0);
            
            HttpResponse resp;
            resp.body = response.build();
            return resp;
        },
        "List studies",
        true
    });
    
    registerEndpoint({
        "/api/v1/studies/{studyId}",
        HttpMethod::GET,
        1,
        [](const HttpRequest& req) {
            // Extract study ID from path
            HttpResponse resp;
            resp.statusCode = 404;
            resp.body = "{\"error\": \"Study not found\"}";
            return resp;
        },
        "Get study details",
        true
    });
    
    // Patient endpoints
    registerEndpoint({
        "/api/v1/patients",
        HttpMethod::GET,
        1,
        [](const HttpRequest& req) {
            JsonBuilder response;
            response.add("status", "success")
                    .add("count", 0);
            
            HttpResponse resp;
            resp.body = response.build();
            return resp;
        },
        "List patients",
        true
    });
    
    // Configuration endpoints
    registerEndpoint({
        "/api/v1/config",
        HttpMethod::GET,
        1,
        [](const HttpRequest& req) {
            JsonBuilder response;
            response.add("status", "success")
                    .add("version", ApiVersion::VERSION_STRING);
            
            HttpResponse resp;
            resp.body = response.build();
            return resp;
        },
        "Get configuration",
        true
    });
}

HttpResponse RestApiServer::handleVersionRequest(const HttpRequest& request) {
    JsonBuilder response;
    response.add("version", ApiVersion::VERSION_STRING)
            .add("major", ApiVersion::MAJOR)
            .add("minor", ApiVersion::MINOR)
            .add("patch", ApiVersion::PATCH)
            .add("build_date", ApiVersion::BUILD_DATE)
            .add("build_time", ApiVersion::BUILD_TIME)
            .add("capabilities", static_cast<int>(ApiVersion::getCapabilities()));
    
    HttpResponse resp;
    resp.body = response.build();
    return resp;
}

HttpResponse RestApiServer::handleHealthCheck(const HttpRequest& request) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    JsonBuilder response;
    response.add("status", "healthy")
            .add("timestamp", std::to_string(time_t))
            .add("version", ApiVersion::VERSION_STRING)
            .add("uptime", 0);  // TODO: Calculate actual uptime
    
    HttpResponse resp;
    resp.body = response.build();
    return resp;
}

HttpResponse RestApiServer::handleApiDocs(const HttpRequest& request) const {
    JsonBuilder docs;
    docs.add("version", ApiVersion::VERSION_STRING)
        .add("title", "PACS System REST API")
        .add("description", "RESTful API for PACS System");
    
    std::vector<JsonBuilder> endpoints;
    for (const auto& [path, handlers] : impl_->endpoints_) {
        for (const auto& endpoint : handlers) {
            JsonBuilder ep;
            ep.add("path", path)
              .add("method", endpoint.method == HttpMethod::GET ? "GET" :
                            endpoint.method == HttpMethod::POST ? "POST" :
                            endpoint.method == HttpMethod::PUT ? "PUT" :
                            endpoint.method == HttpMethod::DELETE ? "DELETE" : "OTHER")
              .add("description", endpoint.description)
              .add("requiresAuth", endpoint.requiresAuth)
              .add("minVersion", endpoint.minVersion);
            endpoints.push_back(ep);
        }
    }
    
    docs.addArray("endpoints", endpoints);
    
    HttpResponse resp;
    resp.body = docs.build();
    return resp;
}

std::string RestApiServer::getApiDocumentation() const {
    return handleApiDocs(HttpRequest{}).body;
}

// JsonBuilder implementation
JsonBuilder& JsonBuilder::add(const std::string& key, const std::string& value) {
    if (!json_.empty()) json_ += ",";
    json_ += "\"" + key + "\":\"" + value + "\"";
    return *this;
}

JsonBuilder& JsonBuilder::add(const std::string& key, int value) {
    if (!json_.empty()) json_ += ",";
    json_ += "\"" + key + "\":" + std::to_string(value);
    return *this;
}

JsonBuilder& JsonBuilder::add(const std::string& key, bool value) {
    if (!json_.empty()) json_ += ",";
    json_ += "\"" + key + "\":" + (value ? "true" : "false");
    return *this;
}

JsonBuilder& JsonBuilder::addObject(const std::string& key, const JsonBuilder& obj) {
    if (!json_.empty()) json_ += ",";
    json_ += "\"" + key + "\":" + obj.build();
    return *this;
}

JsonBuilder& JsonBuilder::addArray(const std::string& key, const std::vector<JsonBuilder>& array) {
    if (!json_.empty()) json_ += ",";
    json_ += "\"" + key + "\":[";
    for (size_t i = 0; i < array.size(); ++i) {
        if (i > 0) json_ += ",";
        json_ += array[i].build();
    }
    json_ += "]";
    return *this;
}

std::string JsonBuilder::build() const {
    return "{" + json_ + "}";
}

// ApiResponses implementation
HttpResponse ApiResponses::success(const std::string& message) {
    HttpResponse resp;
    resp.statusCode = 200;
    resp.body = "{\"status\":\"success\",\"message\":\"" + message + "\"}";
    return resp;
}

HttpResponse ApiResponses::created(const std::string& location) {
    HttpResponse resp;
    resp.statusCode = 201;
    resp.statusMessage = "Created";
    resp.headers["Location"] = location;
    resp.body = "{\"status\":\"created\",\"location\":\"" + location + "\"}";
    return resp;
}

HttpResponse ApiResponses::noContent() {
    HttpResponse resp;
    resp.statusCode = 204;
    resp.statusMessage = "No Content";
    resp.body = "";
    return resp;
}

HttpResponse ApiResponses::badRequest(const std::string& error) {
    HttpResponse resp;
    resp.statusCode = 400;
    resp.statusMessage = "Bad Request";
    resp.body = "{\"status\":\"error\",\"error\":\"" + error + "\"}";
    return resp;
}

HttpResponse ApiResponses::unauthorized(const std::string& error) {
    HttpResponse resp;
    resp.statusCode = 401;
    resp.statusMessage = "Unauthorized";
    resp.body = "{\"status\":\"error\",\"error\":\"" + error + "\"}";
    return resp;
}

HttpResponse ApiResponses::forbidden(const std::string& error) {
    HttpResponse resp;
    resp.statusCode = 403;
    resp.statusMessage = "Forbidden";
    resp.body = "{\"status\":\"error\",\"error\":\"" + error + "\"}";
    return resp;
}

HttpResponse ApiResponses::notFound(const std::string& error) {
    HttpResponse resp;
    resp.statusCode = 404;
    resp.statusMessage = "Not Found";
    resp.body = "{\"status\":\"error\",\"error\":\"" + error + "\"}";
    return resp;
}

HttpResponse ApiResponses::conflict(const std::string& error) {
    HttpResponse resp;
    resp.statusCode = 409;
    resp.statusMessage = "Conflict";
    resp.body = "{\"status\":\"error\",\"error\":\"" + error + "\"}";
    return resp;
}

HttpResponse ApiResponses::internalError(const std::string& error) {
    HttpResponse resp;
    resp.statusCode = 500;
    resp.statusMessage = "Internal Server Error";
    resp.body = "{\"status\":\"error\",\"error\":\"" + error + "\"}";
    return resp;
}

HttpResponse ApiResponses::notImplemented(const std::string& feature) {
    HttpResponse resp;
    resp.statusCode = 501;
    resp.statusMessage = "Not Implemented";
    resp.body = "{\"status\":\"error\",\"error\":\"Feature not implemented: " + feature + "\"}";
    return resp;
}

} // namespace pacs