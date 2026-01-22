/**
 * @file production_pacs.cpp
 * @brief Implementation of production-grade PACS
 */

#include "production_pacs.hpp"
#include "console_utils.hpp"

#include <pacs/web/rest_config.hpp>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace pacs::samples {

// =============================================================================
// Construction
// =============================================================================

production_pacs::production_pacs(const production_config& config)
    : config_(config) {
    stats_.start_time = std::chrono::system_clock::now();
}

production_pacs::~production_pacs() {
    if (running_) {
        stop();
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

bool production_pacs::start() {
    if (running_) {
        return true;
    }

    try {
        // Initialize components in order
        setup_mini_pacs();
        setup_security();
        setup_anonymization();
        setup_monitoring();
        setup_rest_api();
        setup_event_handlers();

        // Start Mini PACS
        if (!pacs_->start()) {
            std::cerr << "Failed to start Mini PACS\n";
            return false;
        }

        // Start REST API if enabled
        if (config_.rest_api.enabled && rest_server_) {
            rest_server_->start_async();
        }

        running_ = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to start Production PACS: " << e.what() << "\n";
        return false;
    }
}

void production_pacs::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Stop REST API
    if (rest_server_) {
        rest_server_->stop();
    }

    // Stop Mini PACS
    if (pacs_) {
        pacs_->stop();
    }

    // Notify waiting threads
    shutdown_cv_.notify_all();
}

void production_pacs::wait() {
    if (!running_) {
        return;
    }

    std::unique_lock<std::mutex> lock(shutdown_mutex_);
    shutdown_cv_.wait(lock, [this] { return !running_.load(); });
}

bool production_pacs::is_running() const noexcept {
    return running_;
}

// =============================================================================
// Event Handlers
// =============================================================================

void production_pacs::on_image_received(
    std::function<void(const events::image_received_event&)> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    image_handlers_.push_back(std::move(handler));
}

void production_pacs::on_query_executed(
    std::function<void(const events::query_executed_event&)> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    query_handlers_.push_back(std::move(handler));
}

void production_pacs::on_association_event(
    std::function<void(const events::association_event&)> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    association_handlers_.push_back(std::move(handler));
}

void production_pacs::on_access_denied(
    std::function<void(const events::access_denied_event&)> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    access_denied_handlers_.push_back(std::move(handler));
}

// =============================================================================
// Status and Statistics
// =============================================================================

void production_pacs::print_status() const {
    auto uptime = stats_.uptime();
    auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime) % 60;
    auto seconds = uptime.count() % 60;

    std::ostringstream uptime_str;
    uptime_str << hours.count() << "h " << minutes.count() << "m " << seconds << "s";

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                  Production PACS Server Status                   ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Server:                                                         ║\n";
    std::cout << "║    AE Title:    " << std::setw(48) << std::left
              << config_.server.ae_title << "║\n";
    std::cout << "║    DICOM Port:  " << std::setw(48) << std::left
              << config_.server.port << "║\n";
    std::cout << "║    TLS:         " << std::setw(48) << std::left
              << (config_.server.tls.enabled ? "Enabled" : "Disabled") << "║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  REST API:                                                       ║\n";
    std::cout << "║    Status:      " << std::setw(48) << std::left
              << (config_.rest_api.enabled ? "Enabled" : "Disabled") << "║\n";
    if (config_.rest_api.enabled) {
        std::cout << "║    Port:        " << std::setw(48) << std::left
                  << config_.rest_api.port << "║\n";
    }
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Security:                                                       ║\n";
    std::cout << "║    RBAC:        " << std::setw(48) << std::left
              << (config_.security.access_control.enabled ? "Enabled" : "Disabled") << "║\n";
    std::cout << "║    Auto-Anon:   " << std::setw(48) << std::left
              << (config_.security.anonymization.auto_anonymize ? "Enabled" : "Disabled")
              << "║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Statistics:                                                     ║\n";
    std::cout << "║    Uptime:      " << std::setw(48) << std::left
              << uptime_str.str() << "║\n";
    std::cout << "║    Images:      " << std::setw(48) << std::left
              << stats_.images_stored.load() << "║\n";
    std::cout << "║    Queries:     " << std::setw(48) << std::left
              << stats_.queries_executed.load() << "║\n";
    std::cout << "║    Associations:" << std::setw(48) << std::left
              << stats_.active_associations.load() << "║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
}

const production_statistics& production_pacs::statistics() const noexcept {
    return stats_;
}

monitoring::health_status production_pacs::get_health() const {
    if (health_checker_) {
        return health_checker_->get_cached_status();
    }
    return monitoring::health_status{};
}

void production_pacs::export_statistics(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open statistics file: " << path << "\n";
        return;
    }

    auto uptime = stats_.uptime();

    file << "{\n";
    file << "  \"uptime_seconds\": " << uptime.count() << ",\n";
    file << "  \"images_stored\": " << stats_.images_stored.load() << ",\n";
    file << "  \"images_anonymized\": " << stats_.images_anonymized.load() << ",\n";
    file << "  \"queries_executed\": " << stats_.queries_executed.load() << ",\n";
    file << "  \"access_denied_count\": " << stats_.access_denied_count.load() << ",\n";
    file << "  \"rest_requests\": " << stats_.rest_requests.load() << ",\n";
    file << "  \"active_associations\": " << stats_.active_associations.load() << ",\n";
    file << "  \"server\": {\n";
    file << "    \"ae_title\": \"" << config_.server.ae_title << "\",\n";
    file << "    \"port\": " << config_.server.port << ",\n";
    file << "    \"tls_enabled\": " << (config_.server.tls.enabled ? "true" : "false") << "\n";
    file << "  },\n";
    file << "  \"rest_api\": {\n";
    file << "    \"enabled\": " << (config_.rest_api.enabled ? "true" : "false") << ",\n";
    file << "    \"port\": " << config_.rest_api.port << "\n";
    file << "  }\n";
    file << "}\n";
}

const production_config& production_pacs::config() const noexcept {
    return config_;
}

// =============================================================================
// Initialization
// =============================================================================

void production_pacs::setup_mini_pacs() {
    mini_pacs_config pacs_config;
    pacs_config.ae_title = config_.server.ae_title;
    pacs_config.port = config_.server.port;
    pacs_config.storage_path = config_.storage.root_path;
    pacs_config.max_associations = config_.server.max_associations;
    pacs_config.enable_worklist = true;
    pacs_config.enable_mpps = true;
    pacs_config.verbose_logging = (config_.logging.level == "debug");

    pacs_ = std::make_unique<mini_pacs>(pacs_config);
}

void production_pacs::setup_security() {
    if (!config_.security.access_control.enabled) {
        return;
    }

    access_control_ = std::make_shared<security::access_control_manager>();

    // Register allowed AE titles
    for (const auto& ae_title : config_.security.allowed_ae_titles) {
        // For simplicity, map AE titles to users with viewer role
        access_control_->register_ae_title(ae_title, ae_title);
    }
}

void production_pacs::setup_anonymization() {
    if (!config_.security.anonymization.auto_anonymize) {
        return;
    }

    anonymizer_ = std::make_unique<security::anonymizer>(
        config_.security.anonymization.profile);
}

void production_pacs::setup_rest_api() {
    if (!config_.rest_api.enabled) {
        return;
    }

    web::rest_server_config rest_config;
    rest_config.port = config_.rest_api.port;
    rest_config.enable_cors = config_.rest_api.cors_enabled;
    rest_config.concurrency = 4;

    rest_server_ = std::make_unique<web::rest_server>(rest_config);

    // Integrate with monitoring
    if (health_checker_) {
        rest_server_->set_health_checker(health_checker_);
    }
    if (metrics_) {
        rest_server_->set_metrics_provider(metrics_);
    }
    if (access_control_) {
        rest_server_->set_access_control_manager(access_control_);
    }
}

void production_pacs::setup_monitoring() {
    // Setup health checker
    monitoring::health_checker_config health_config;
    health_config.check_interval = config_.monitoring.health_check_interval;
    health_config.cache_duration = std::chrono::seconds{5};
    health_config.storage_warning_threshold = 80.0;
    health_config.storage_critical_threshold = 95.0;

    health_checker_ = std::make_shared<monitoring::health_checker>(health_config);
    health_checker_->set_version(0, 1, 0, "production_pacs");

    // Setup metrics
    if (config_.monitoring.metrics_enabled) {
        metrics_ = std::make_shared<monitoring::pacs_metrics>();
    }
}

void production_pacs::setup_event_handlers() {
    // Note: In a real implementation, we would hook into the Mini PACS
    // event system. For this sample, we demonstrate the event architecture
    // without full integration to keep the sample focused.
}

// =============================================================================
// Event Dispatch
// =============================================================================

void production_pacs::dispatch_image_received(const events::image_received_event& event) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    for (const auto& handler : image_handlers_) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            std::cerr << "Error in image received handler: " << e.what() << "\n";
        }
    }
}

void production_pacs::dispatch_query_executed(const events::query_executed_event& event) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    for (const auto& handler : query_handlers_) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            std::cerr << "Error in query executed handler: " << e.what() << "\n";
        }
    }
}

void production_pacs::dispatch_association_event(const events::association_event& event) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    for (const auto& handler : association_handlers_) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            std::cerr << "Error in association event handler: " << e.what() << "\n";
        }
    }
}

void production_pacs::dispatch_access_denied(const events::access_denied_event& event) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    stats_.access_denied_count++;
    for (const auto& handler : access_denied_handlers_) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            std::cerr << "Error in access denied handler: " << e.what() << "\n";
        }
    }
}

}  // namespace pacs::samples
