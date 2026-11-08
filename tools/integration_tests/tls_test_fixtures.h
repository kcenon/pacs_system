// BSD 3-Clause License
// See the LICENSE file in the project root for full license information.
#pragma once
#include "test_fixtures.h"
#include "kcenon/pacs/network/tls_config.h"
#include "kcenon/pacs/services/verification_scp.h"
#ifdef PACS_WITH_NETWORK_SYSTEM
#include "kcenon/pacs/network/v2/dicom_server_v2.h"
#endif

namespace kcenon::pacs::integration_test {
/**
 * @brief Test certificate bundle paths
 */
struct test_certificate_bundle {
    std::filesystem::path ca_cert;
    std::filesystem::path ca_key;
    std::filesystem::path server_cert;
    std::filesystem::path server_key;
    std::filesystem::path client_cert;
    std::filesystem::path client_key;
    std::filesystem::path other_ca_cert;
    /// Signed by the test CA, identity 127.0.0.2 only (not checked by all_exist())
    std::filesystem::path wrong_ip_server_cert;
    std::filesystem::path wrong_ip_server_key;

    /**
     * @brief Check if all required certificate files exist
     */
    [[nodiscard]] bool all_exist() const {
        return std::filesystem::exists(ca_cert) &&
               std::filesystem::exists(ca_key) &&
               std::filesystem::exists(server_cert) &&
               std::filesystem::exists(server_key) &&
               std::filesystem::exists(client_cert) &&
               std::filesystem::exists(client_key) &&
               std::filesystem::exists(other_ca_cert);
    }
};

/**
 * @brief Get test certificate bundle from test_data/certs directory
 * @return Certificate bundle with paths to test certificates
 */
inline test_certificate_bundle get_test_certificates() {
    // Look for certificates in build directory first, then source directory
    std::vector<std::filesystem::path> search_paths = {
        std::filesystem::current_path() / "test_data" / "certs",
        std::filesystem::current_path() / "bin" / "test_data" / "certs",
        std::filesystem::path(__FILE__).parent_path() / "test_data" / "certs"
    };

    // Also check environment variable
    if (const char* cert_dir = std::getenv("PACS_TEST_CERT_DIR")) {
        search_paths = {cert_dir};
    }

    for (const auto& cert_dir : search_paths) {
        if (std::filesystem::exists(cert_dir / "ca.crt")) {
            return {
                cert_dir / "ca.crt",
                cert_dir / "ca.key",
                cert_dir / "server.crt",
                cert_dir / "server.key",
                cert_dir / "client.crt",
                cert_dir / "client.key",
                cert_dir / "other_ca.crt",
                cert_dir / "wrong_ip_server.crt",
                cert_dir / "wrong_ip_server.key"
            };
        }
    }

    // Return the requested paths; required v2 cases fail if material is missing.
    auto default_dir = search_paths.front();
    return {
        default_dir / "ca.crt",
        default_dir / "ca.key",
        default_dir / "server.crt",
        default_dir / "server.key",
        default_dir / "client.crt",
        default_dir / "client.key",
        default_dir / "other_ca.crt",
        default_dir / "wrong_ip_server.crt",
        default_dir / "wrong_ip_server.key"
    };
}


inline network::tls_config server_tls_config(const test_certificate_bundle& certs, bool mutual = false) {
    network::tls_config config;
    config.enabled = true;
    config.cert_path = certs.server_cert;
    config.key_path = certs.server_key;
    config.ca_path = certs.ca_cert;
    config.verify_peer = mutual;
    return config;
}

inline network::tls_config client_tls_config(const test_certificate_bundle& certs, bool mutual = false) {
    network::tls_config config;
    config.enabled = true;
    config.ca_path = certs.ca_cert;
    if (mutual) { config.cert_path = certs.client_cert; config.key_path = certs.client_key; }
    return config;
}

inline network::Result<network::association> connect_tls(
    uint16_t port, const std::string& ae, const network::tls_config& tls,
    std::vector<std::string> classes = {std::string(services::verification_sop_class_uid)},
    const std::string& calling = "TLS_V2_SCU", const std::string& host = "127.0.0.1") {
    network::association_config config;
    config.calling_ae_title = calling;
    config.called_ae_title = ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.200";
    config.tls = tls;
    uint8_t id = 1;
    for (const auto& sop : classes) {
        config.proposed_contexts.emplace_back(id, sop, std::vector<std::string>{"1.2.840.10008.1.2.1"});
        id += 2;
    }
    return network::association::connect(host, port, config, std::chrono::seconds{5});
}

#ifdef PACS_WITH_NETWORK_SYSTEM
class tls_test_server_v2 {
public:
    tls_test_server_v2(const network::tls_config& tls, const std::string& ae = "TLS_V2_SCP")
        : config_(ae, find_available_port()) {
        config_.tls = tls;
        config_.association_timeout = std::chrono::seconds{5};
        server_ = std::make_unique<network::v2::dicom_server_v2>(config_);
    }
    ~tls_test_server_v2() { stop(); }
    void register_service(services::scp_service_ptr service) { server_->register_service(std::move(service)); }
    auto start() { return server_->start(); }
    void stop() { server_->stop(); }
    uint16_t port() const { return config_.port; }
    const std::string& ae_title() const { return config_.ae_title; }
    network::v2::dicom_server_v2& server() { return *server_; }
private:
    network::server_config config_;
    std::unique_ptr<network::v2::dicom_server_v2> server_;
};
#endif
} // namespace kcenon::pacs::integration_test
