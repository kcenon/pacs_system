/**
 * @file main.cpp
 * @brief Level 5 Sample: Production PACS with Enterprise Features
 *
 * This sample demonstrates a production-grade PACS implementation with:
 * - YAML configuration file support
 * - TLS security for DICOM connections
 * - Role-based access control (RBAC)
 * - Automatic anonymization profiles
 * - REST API for web access
 * - Health monitoring and metrics
 * - Event-driven architecture
 *
 * After completing this sample, you will understand:
 * 1. Configuration Management - YAML-based server configuration
 * 2. TLS Security - Secure DICOM communication
 * 3. Access Control - Role-based permissions (RBAC)
 * 4. Anonymization - De-identification profiles (HIPAA, GDPR)
 * 5. REST API - Web-based PACS access
 * 6. Health Monitoring - Service health checks and metrics
 * 7. Event Architecture - Decoupled event handling
 *
 * @see DICOM PS3.15 - Security and System Management Profiles
 * @see DICOM PS3.4 - Service Class Specifications
 */

#include "production_pacs.hpp"
#include "config_loader.hpp"
#include "console_utils.hpp"
#include "signal_handler.hpp"

#include <iostream>
#include <string>

using namespace pacs::samples;

namespace {

/**
 * @brief Print usage information
 */
void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [config_file]\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  config_file    Path to YAML configuration file\n";
    std::cout << "                 (default: config/pacs_config.yaml)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program << "\n";
    std::cout << "  " << program << " /etc/pacs/production.yaml\n";
    std::cout << "  " << program << " config/development.yaml\n";
}

/**
 * @brief Format anonymization profile as string
 */
std::string profile_to_string(pacs::security::anonymization_profile profile) {
    switch (profile) {
        case pacs::security::anonymization_profile::basic:
            return "Basic";
        case pacs::security::anonymization_profile::hipaa_safe_harbor:
            return "HIPAA Safe Harbor";
        case pacs::security::anonymization_profile::retain_longitudinal:
            return "Retain Longitudinal";
        case pacs::security::anonymization_profile::retain_patient_characteristics:
            return "Retain Patient Characteristics";
        case pacs::security::anonymization_profile::gdpr_compliant:
            return "GDPR Compliant";
        case pacs::security::anonymization_profile::clean_pixel:
            return "Clean Pixel";
        case pacs::security::anonymization_profile::clean_descriptions:
            return "Clean Descriptions";
        default:
            return "Unknown";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    print_header("Production PACS - Level 5 Sample");

    // Check for help flag
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }

    // ==========================================================================
    // Part 1: Load Configuration
    // ==========================================================================
    // Production PACS uses YAML configuration files for flexible deployment.
    // This allows separation of configuration from code and supports:
    // - Different environments (dev, staging, production)
    // - Container deployments with mounted config files
    // - Easy configuration updates without recompilation

    print_section("Part 1: Load Configuration");

    std::filesystem::path config_path = argc > 1
        ? argv[1]
        : "config/pacs_config.yaml";

    std::cout << "Configuration file: " << config_path << "\n\n";

    production_config config;

    if (std::filesystem::exists(config_path)) {
        std::cout << "Loading configuration from file...\n";

        auto result = config_loader::load(config_path);
        if (!result.is_ok()) {
            print_error("Failed to load configuration: " + result.error().message);
            return 1;
        }

        config = result.value();
        print_success("Configuration loaded successfully!");
    } else {
        std::cout << "Configuration file not found, using defaults...\n";
        config = config_loader::create_default();
        print_info("Using default configuration");
    }

    // Display configuration summary
    print_table("Server Configuration", {
        {"AE Title", config.server.ae_title},
        {"DICOM Port", std::to_string(config.server.port)},
        {"Max Associations", std::to_string(config.server.max_associations)},
        {"Idle Timeout", std::to_string(config.server.idle_timeout.count()) + "s"},
        {"TLS Enabled", config.server.tls.enabled ? "Yes" : "No"}
    });

    print_table("Storage Configuration", {
        {"Root Path", config.storage.root_path.string()},
        {"Naming Scheme", config.storage.naming_scheme},
        {"Duplicate Policy", config.storage.duplicate_policy},
        {"Database Path", config.storage.database.path.string()}
    });

    print_table("Security Configuration", {
        {"Access Control", config.security.access_control.enabled ? "Enabled" : "Disabled"},
        {"Default Role", config.security.access_control.default_role},
        {"Auto-Anonymize", config.security.anonymization.auto_anonymize ? "Yes" : "No"},
        {"Anon Profile", profile_to_string(config.security.anonymization.profile)}
    });

    print_table("REST API Configuration", {
        {"Enabled", config.rest_api.enabled ? "Yes" : "No"},
        {"Port", std::to_string(config.rest_api.port)},
        {"CORS", config.rest_api.cors_enabled ? "Enabled" : "Disabled"}
    });

    print_table("Monitoring Configuration", {
        {"Health Check Interval", std::to_string(config.monitoring.health_check_interval.count()) + "s"},
        {"Metrics Enabled", config.monitoring.metrics_enabled ? "Yes" : "No"}
    });

    print_success("Part 1 complete - Configuration ready!");

    // ==========================================================================
    // Part 2: Initialize Production PACS
    // ==========================================================================
    // The production_pacs class integrates all enterprise features:
    // - Mini PACS for core DICOM services
    // - Security manager for access control
    // - Anonymizer for de-identification
    // - REST server for web access
    // - Health checker for monitoring

    print_section("Part 2: Initialize Production PACS");

    std::cout << "Creating Production PACS instance...\n";

    production_pacs pacs(config);

    // Setup event handlers for monitoring
    pacs.on_image_received([](const events::image_received_event& evt) {
        std::cout << colors::green << "[EVENT] "
                  << colors::reset << "Image received: "
                  << evt.sop_instance_uid
                  << " from " << evt.calling_ae << "\n";
    });

    pacs.on_query_executed([](const events::query_executed_event& evt) {
        std::cout << colors::blue << "[EVENT] "
                  << colors::reset << "Query at " << evt.query_level
                  << " level returned " << evt.result_count
                  << " results (" << evt.duration.count() << "ms)\n";
    });

    pacs.on_association_event([](const events::association_event& evt) {
        const char* type_str = "";
        switch (evt.event_type) {
            case events::association_event::type::opened:
                type_str = "opened";
                break;
            case events::association_event::type::closed:
                type_str = "closed";
                break;
            case events::association_event::type::rejected:
                type_str = "rejected";
                break;
        }
        std::cout << colors::cyan << "[EVENT] "
                  << colors::reset << "Association " << type_str
                  << ": " << evt.calling_ae << " -> " << evt.called_ae << "\n";
    });

    pacs.on_access_denied([](const events::access_denied_event& evt) {
        std::cout << colors::red << "[SECURITY] "
                  << colors::reset << "Access denied: "
                  << evt.calling_ae << " attempted " << evt.operation
                  << " - " << evt.reason << "\n";
    });

    print_success("Part 2 complete - Production PACS initialized!");

    // ==========================================================================
    // Part 3: Start Services
    // ==========================================================================

    print_section("Part 3: Start Services");

    if (!pacs.start()) {
        print_error("Failed to start Production PACS");
        return 1;
    }

    print_success("Part 3 complete - All services started!");

    // ==========================================================================
    // Part 4: Display Status and Wait
    // ==========================================================================

    print_section("Part 4: Running Server");

    pacs.print_status();

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    Test Commands                                  ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  DICOM Connectivity:                                              ║\n";
    std::cout << "║    echoscu -aec " << config.server.ae_title
              << " localhost " << config.server.port;
    for (size_t i = config.server.ae_title.length() + std::to_string(config.server.port).length();
         i < 27; ++i) {
        std::cout << " ";
    }
    std::cout << "║\n";
    std::cout << "║                                                                   ║\n";
    std::cout << "║  Store Image:                                                     ║\n";
    std::cout << "║    storescu -aec " << config.server.ae_title
              << " localhost " << config.server.port << " image.dcm";
    for (size_t i = config.server.ae_title.length() + std::to_string(config.server.port).length();
         i < 17; ++i) {
        std::cout << " ";
    }
    std::cout << "║\n";

    if (config.rest_api.enabled) {
        std::cout << "║                                                                   ║\n";
        std::cout << "║  REST API:                                                        ║\n";
        std::cout << "║    Health:  curl http://localhost:"
                  << config.rest_api.port << "/api/v1/system/status";
        for (size_t i = std::to_string(config.rest_api.port).length(); i < 9; ++i) {
            std::cout << " ";
        }
        std::cout << "║\n";
        std::cout << "║    Metrics: curl http://localhost:"
                  << config.rest_api.port << "/api/v1/system/metrics";
        for (size_t i = std::to_string(config.rest_api.port).length(); i < 8; ++i) {
            std::cout << " ";
        }
        std::cout << "║\n";
    }

    if (config.server.tls.enabled) {
        std::cout << "║                                                                   ║\n";
        std::cout << "║  TLS Connection:                                                  ║\n";
        std::cout << "║    echoscu --tls --add-cert-file ca.pem -aec " << config.server.ae_title;
        for (size_t i = config.server.ae_title.length(); i < 12; ++i) {
            std::cout << " ";
        }
        std::cout << "║\n";
        std::cout << "║      localhost " << config.server.port;
        for (size_t i = std::to_string(config.server.port).length(); i < 47; ++i) {
            std::cout << " ";
        }
        std::cout << "║\n";
    }

    std::cout << "║                                                                   ║\n";
    std::cout << "║  Press Ctrl+C to stop                                             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    // Install signal handler for graceful shutdown
    scoped_signal_handler sig_handler([&pacs]() {
        std::cout << "\n" << colors::yellow
                  << "Graceful shutdown initiated..."
                  << colors::reset << "\n";
        pacs.stop();
    });

    // Wait for shutdown signal
    sig_handler.wait();

    // ==========================================================================
    // Part 5: Export Statistics and Cleanup
    // ==========================================================================

    print_section("Final Statistics");

    const auto& stats = pacs.statistics();
    auto uptime = stats.uptime();
    auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime) % 60;
    auto seconds = uptime.count() % 60;

    std::ostringstream uptime_str;
    uptime_str << hours.count() << "h " << minutes.count() << "m " << seconds << "s";

    print_table("Session Statistics", {
        {"Uptime", uptime_str.str()},
        {"Images Stored", std::to_string(stats.images_stored.load())},
        {"Images Anonymized", std::to_string(stats.images_anonymized.load())},
        {"Queries Executed", std::to_string(stats.queries_executed.load())},
        {"Access Denied", std::to_string(stats.access_denied_count.load())},
        {"REST Requests", std::to_string(stats.rest_requests.load())}
    });

    // Export statistics to file
    pacs.export_statistics("statistics.json");
    print_info("Statistics exported to statistics.json");

    // Summary
    print_box({
        "Congratulations! You have learned:",
        "",
        "1. Configuration Management  - YAML-based configuration",
        "2. TLS Security             - Secure DICOM connections",
        "3. Access Control           - Role-based permissions (RBAC)",
        "4. Anonymization            - De-identification profiles",
        "5. REST API                 - Web-based PACS access",
        "6. Health Monitoring        - Service health checks",
        "7. Event Architecture       - Decoupled event handling",
        "",
        "This Production PACS demonstrates enterprise-ready patterns",
        "for building secure, scalable DICOM infrastructure.",
        "",
        "Further reading:",
        "  - DICOM PS3.15: Security and System Management Profiles",
        "  - HIPAA Technical Safeguards",
        "  - GDPR Requirements for Medical Imaging"
    });

    print_success("Production PACS terminated successfully.");

    return 0;
}
