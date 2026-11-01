// BSD 3-Clause License
// See the LICENSE file in the project root for full license information.
#pragma once

#include <filesystem>

namespace kcenon::pacs::network {

/**
 * @struct tls_config
 * @brief Configuration for TLS/SSL secure transport
 *
 * DICOM supports TLS 1.2/1.3 for secure communication as defined in
 * DICOM PS3.15 (Security and System Management Profiles).
 */
struct tls_config {
    /// Enable TLS for connections
    bool enabled = false;

    /// Path to certificate file (PEM format)
    std::filesystem::path cert_path;

    /// Path to private key file (PEM format)
    std::filesystem::path key_path;

    /// Path to CA certificate file for verification (optional)
    std::filesystem::path ca_path;

    /// Verify the server identity on clients; require a client certificate on servers.
    /// Server credentials are mandatory. Client credentials are optional unless mTLS is required.
    /// Verify peer certificate
    bool verify_peer = true;

    /// Minimum TLS version (1.2 recommended for DICOM)
    enum class tls_version { v1_2, v1_3 } min_version = tls_version::v1_2;

    /**
     * @brief Check if TLS configuration is valid
     * @return true if all required paths are set when enabled
     */
    [[nodiscard]] bool is_valid() const noexcept {
        if (!enabled) return true;
        return !cert_path.empty() && !key_path.empty();
    }
};

} // namespace kcenon::pacs::network
