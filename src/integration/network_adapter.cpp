// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file network_adapter.cpp
 * @brief Implementation of network_adapter for network_system integration
 */

#include <kcenon/pacs/integration/network_adapter.h>
#include <kcenon/pacs/integration/dicom_session.h>
#include <kcenon/pacs/network/dicom_server.h>

#include <kcenon/network/facade/tcp_facade.h>

#include <filesystem>
#include <future>
#include <stdexcept>

namespace kcenon::pacs::integration {

// =============================================================================
// Server Creation
// =============================================================================

std::unique_ptr<network::dicom_server>
network_adapter::create_server(const network::server_config& config,
                               const tls_config& tls_cfg) {
    // Validate configuration
    if (config.ae_title.empty()) {
        return nullptr;
    }

    if (config.port == 0) {
        return nullptr;
    }

    // Validate TLS configuration if enabled
    if (tls_cfg.enabled && !tls_cfg.is_valid()) {
        return nullptr;
    }

    // Create DICOM server with the provided configuration
    // The dicom_server internally uses network_system for TCP operations
    return std::make_unique<network::dicom_server>(config);
}

// =============================================================================
// Client Connection
// =============================================================================

Result<network_adapter::session_ptr>
network_adapter::connect(const connection_config& config) {
    // Validate configuration
    if (config.host.empty()) {
        return Result<session_ptr>(error_info("Connection failed: empty host"));
    }

    if (config.port == 0) {
        return Result<session_ptr>(error_info("Connection failed: invalid port"));
    }

    // Validate TLS configuration if enabled
    if (config.tls.enabled && !config.tls.is_valid()) {
        return Result<session_ptr>(error_info("Connection failed: invalid TLS configuration"));
    }

    try {
        // Create TCP client via tcp_facade
        kcenon::network::facade::tcp_facade facade;
        kcenon::network::facade::tcp_facade::client_config client_cfg;
        client_cfg.host = config.host;
        client_cfg.port = config.port;
        client_cfg.client_id = "pacs_client";
        client_cfg.timeout = config.timeout;
        client_cfg.use_ssl = config.tls.enabled;
        if (config.tls.enabled && !config.tls.ca_path.empty()) {
            client_cfg.ca_cert_path = config.tls.ca_path.string();
        }
        client_cfg.verify_certificate = config.tls.verify_peer;

        auto client_result = facade.create_client(client_cfg);
        if (client_result.is_err()) {
            return Result<session_ptr>(error_info("Connection failed: unable to create client"));
        }
        auto client = client_result.value();

        // Set up promise/future for synchronous connection
        std::promise<std::error_code> connect_promise;
        auto connect_future = connect_promise.get_future();

        // Use deprecated callbacks for synchronous connect pattern
        // (observer pattern would be preferred for long-lived clients)
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        client->set_connected_callback([&connect_promise]() {
            connect_promise.set_value(std::error_code{});
        });

        client->set_error_callback([&connect_promise](std::error_code ec) {
            try {
                connect_promise.set_value(ec);
            } catch (const std::future_error&) {
                // Promise already satisfied
            }
        });
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

        // Start connection
        auto start_result = client->start(config.host, config.port);
        if (start_result.is_err()) {
            return Result<session_ptr>(error_info("Connection failed: unable to start client"));
        }

        // Wait for connection with timeout
        auto status = connect_future.wait_for(config.timeout);
        if (status == std::future_status::timeout) {
            (void)client->stop();
            return Result<session_ptr>(error_info("Connection failed: timeout"));
        }

        // Check connection result
        auto ec = connect_future.get();
        if (ec) {
            (void)client->stop();
            return Result<session_ptr>(error_info("Connection failed: " + ec.message()));
        }

        // Connection successful but client-based session wrapping
        // is not yet fully implemented for the public API
        return Result<session_ptr>(error_info("Connection not yet fully implemented"));

    } catch (const std::exception& e) {
        return Result<session_ptr>(
            error_info(std::string("Connection failed: ") + e.what()));
    }
}

Result<network_adapter::session_ptr>
network_adapter::connect(const std::string& host,
                         uint16_t port,
                         std::chrono::milliseconds timeout) {
    connection_config config{host, port};
    config.timeout = timeout;
    return connect(config);
}

// =============================================================================
// TLS Configuration
// =============================================================================

Result<std::monostate>
network_adapter::configure_tls(const tls_config& config) {
    if (!config.enabled) {
        return Result<std::monostate>(std::monostate{});
    }

    // Validate certificate path
    if (!std::filesystem::exists(config.cert_path)) {
        return Result<std::monostate>(error_info(
            "TLS configuration error: certificate file not found: " +
            config.cert_path.string()));
    }

    // Validate key path
    if (!std::filesystem::exists(config.key_path)) {
        return Result<std::monostate>(error_info(
            "TLS configuration error: key file not found: " +
            config.key_path.string()));
    }

    // Validate CA path if specified
    if (!config.ca_path.empty() && !std::filesystem::exists(config.ca_path)) {
        return Result<std::monostate>(error_info(
            "TLS configuration error: CA certificate file not found: " +
            config.ca_path.string()));
    }

    // TLS version validation
    if (config.min_version != tls_config::tls_version::v1_2 &&
        config.min_version != tls_config::tls_version::v1_3) {
        return Result<std::monostate>(error_info(
            "TLS configuration error: unsupported TLS version"));
    }

    return Result<std::monostate>(std::monostate{});
}

// =============================================================================
// Session Wrapping
// =============================================================================

network_adapter::session_ptr
network_adapter::wrap_session(
    std::shared_ptr<network_system::session::messaging_session> session) {
    if (!session) {
        return nullptr;
    }
    return std::make_shared<dicom_session>(std::move(session));
}

network_adapter::session_ptr
network_adapter::wrap_session(
    std::shared_ptr<network_system::session::secure_session> session) {
    if (!session) {
        return nullptr;
    }
    return std::make_shared<dicom_session>(std::move(session));
}

network_adapter::session_ptr
network_adapter::wrap_session(
    std::shared_ptr<kcenon::network::interfaces::i_session> session) {
    if (!session) {
        return nullptr;
    }
    return std::make_shared<dicom_session>(std::move(session));
}

}  // namespace kcenon::pacs::integration
