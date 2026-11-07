// BSD 3-Clause License. See LICENSE.
// External validation harness using production transports, services and storage.
// No fabricated service responses or direct writes to receiver evidence.
#include "kcenon/pacs/network/v2/dicom_server_v2.h"
#include "kcenon/pacs/security/atna_audit_logger.h"
#include "kcenon/pacs/security/atna_service_auditor.h"
#include "kcenon/pacs/security/atna_syslog_transport.h"
#include "kcenon/pacs/services/storage_scp.h"
#include "kcenon/pacs/services/verification_scp.h"
#include "kcenon/pacs/storage/file_storage.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {
volatile std::sig_atomic_t stopped = 0;
void stop_signal(int) { stopped = 1; }
std::string optional(const char* value) { return std::string(value) == "-" ? "" : value; }

kcenon::pacs::security::syslog_transport_config audit_config(char** args) {
    using namespace kcenon::pacs::security;
    syslog_transport_config config;
    const std::string protocol = args[0];
    if (protocol != "tls" && protocol != "udp") { throw std::runtime_error("Expected tls or udp"); }
    config.protocol = protocol == "tls" ? syslog_transport_protocol::tls : syslog_transport_protocol::udp;
    config.host = args[1];
    const auto port = std::stoi(args[2]);
    if (port <= 0 || port > 65535) { throw std::runtime_error("Invalid audit port"); }
    config.port = static_cast<uint16_t>(port);
    config.ca_cert_path = optional(args[3]);
    config.client_cert_path = optional(args[4]);
    config.client_key_path = optional(args[5]);
    config.hostname = "pacs-security-validation";
    config.app_name = "pacs-security-probe";
    return config;
}
} // namespace

int main(int argc, char** argv) {
    using namespace kcenon::pacs;
    try {
        if (argc == 9 && std::string(argv[1]) == "send") {
            security::atna_syslog_transport transport(audit_config(argv + 2));
            const auto count = std::stoi(argv[8]);
            if (count < 1 || count > 64) { throw std::runtime_error("Invalid send count"); }
            // One transport is shared just as it is by concurrently executing SCP services.
            std::atomic<unsigned> failed{0};
            std::vector<std::thread> workers;
            for (int index = 0; index < count; ++index) {
                workers.emplace_back([&, index] {
                    const auto message = security::atna_audit_logger::build_user_authentication(
                        "SECURITY_PROBE", "SYNTHETIC_AUDIT_SUBJECT_" + std::to_string(index),
                        "127.0.0.1", true, security::atna_event_outcome::success);
                    auto result = transport.send(security::atna_audit_logger::to_xml(message));
                    if (result.is_err()) {
                        ++failed;
                        std::cerr << result.error().message << '\n';
                    }
                });
            }
            for (auto& worker : workers) { worker.join(); }
            std::cout << "sent=" << transport.messages_sent() << " failed=" << transport.send_errors() << '\n';
            return failed == 0 && transport.messages_sent() == static_cast<size_t>(count) ? 0 : 1;
        }
        if (argc == 13 && std::string(argv[1]) == "serve") {
            network::server_config config;
            const auto port = std::stoi(argv[2]);
            if (port <= 0 || port > 65535) { throw std::runtime_error("Invalid DICOM port"); }
            config.port = static_cast<uint16_t>(port);
            config.ae_title = "SECURITY_SCP";
            config.tls.enabled = true;
            config.tls.cert_path = argv[3];
            config.tls.key_path = argv[4];
            config.tls.ca_path = argv[5];
            config.tls.verify_peer = true;
            auto auditor = std::make_shared<security::atna_service_auditor>(audit_config(argv + 6), "SECURITY_PROBE");
            storage::file_storage_config storage_config;
            storage_config.root_path = argv[12];
            storage::file_storage files(storage_config);
            auto storage_scp = std::make_shared<services::storage_scp>();
            storage_scp->set_context_handler([&](const services::storage_context& request) {
                auto result = files.store(request.dataset, request.transfer_syntax);
                return result.is_ok() ? services::storage_status::success : services::storage_status::out_of_resources;
            });
            storage_scp->set_audit_handler(auditor);
            network::v2::dicom_server_v2 server(config);
            server.register_service(std::make_shared<services::verification_scp>());
            server.register_service(storage_scp);
            server.on_error([](const std::string& error) { std::cerr << error << '\n'; });
            std::signal(SIGINT, stop_signal);
            std::signal(SIGTERM, stop_signal);
            auto started = server.start();
            if (started.is_err()) { std::cerr << started.error().message << '\n'; return 1; }
            while (!stopped) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
            server.stop();
            std::cout << "audit_sent=" << auditor->events_sent() << " audit_failed=" << auditor->events_failed() << '\n';
            return auditor->events_failed() == 0 ? 0 : 1;
        }
        std::cerr << "Usage: pacs_security_probe send <tls|udp> <host> <port> <ca|-> <cert|-> <key|-> <count>\n"
                  << "       pacs_security_probe serve <port> <cert> <key> <ca> <tls|udp> <audit-host> <audit-port> <audit-ca|-> <audit-cert|-> <audit-key|-> <storage-dir>\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
