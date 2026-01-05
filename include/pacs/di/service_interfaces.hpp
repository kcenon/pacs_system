/**
 * @file service_interfaces.hpp
 * @brief Service interface aliases for dependency injection
 *
 * This file provides type aliases for DICOM service interfaces used with
 * ServiceContainer-based dependency injection. It unifies existing interfaces
 * under a consistent naming convention for DI registration and resolution.
 *
 * @see Issue #312 - ServiceContainer based DI Integration
 */

#pragma once

#include <pacs/storage/storage_interface.hpp>
#include <pacs/encoding/compression/compression_codec.hpp>
#include <pacs/integration/network_adapter.hpp>
#include <pacs/network/dicom_server.hpp>
#include <pacs/di/ilogger.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace pacs::di {

// =============================================================================
// Storage Interface
// =============================================================================

/**
 * @brief Storage interface for DICOM data persistence
 *
 * Alias for pacs::storage::storage_interface providing unified access
 * through the DI container.
 *
 * @see pacs::storage::storage_interface
 */
using IDicomStorage = storage::storage_interface;

// =============================================================================
// Codec Interface
// =============================================================================

/**
 * @brief Codec interface for image compression/decompression
 *
 * Alias for pacs::encoding::compression::compression_codec providing
 * unified access through the DI container.
 *
 * @see pacs::encoding::compression::compression_codec
 */
using IDicomCodec = encoding::compression::compression_codec;

// =============================================================================
// Network Interface
// =============================================================================

/**
 * @brief Network service interface for DICOM communication
 *
 * Abstract interface for DICOM network operations including server creation
 * and client connections. This interface provides dependency injection
 * support for network functionality.
 *
 * Thread Safety:
 * - All methods must be thread-safe in concrete implementations
 * - Server/client instances should be managed independently
 */
class IDicomNetwork {
public:
    virtual ~IDicomNetwork() = default;

    // =========================================================================
    // Server Operations
    // =========================================================================

    /**
     * @brief Create a DICOM server
     *
     * @param config Server configuration
     * @param tls_cfg Optional TLS configuration
     * @return Unique pointer to dicom_server, or nullptr on failure
     */
    [[nodiscard]] virtual std::unique_ptr<network::dicom_server> create_server(
        const network::server_config& config,
        const integration::tls_config& tls_cfg = {}) = 0;

    // =========================================================================
    // Client Operations
    // =========================================================================

    /**
     * @brief Connect to a remote DICOM peer
     *
     * @param config Connection configuration
     * @return Result containing session on success, or error message
     */
    [[nodiscard]] virtual integration::Result<integration::network_adapter::session_ptr>
        connect(const integration::connection_config& config) = 0;

    /**
     * @brief Connect to a remote DICOM peer (simplified)
     *
     * @param host Remote host address
     * @param port Remote port
     * @param timeout Connection timeout
     * @return Result containing session on success, or error message
     */
    [[nodiscard]] virtual integration::Result<integration::network_adapter::session_ptr>
        connect(const std::string& host, uint16_t port,
                std::chrono::milliseconds timeout = std::chrono::seconds{30}) = 0;

protected:
    IDicomNetwork() = default;
    IDicomNetwork(const IDicomNetwork&) = default;
    IDicomNetwork& operator=(const IDicomNetwork&) = default;
    IDicomNetwork(IDicomNetwork&&) = default;
    IDicomNetwork& operator=(IDicomNetwork&&) = default;
};

// =============================================================================
// Default Network Implementation
// =============================================================================

/**
 * @brief Default implementation of IDicomNetwork using network_adapter
 *
 * Delegates all operations to the static network_adapter methods.
 */
class DicomNetworkService final : public IDicomNetwork {
public:
    DicomNetworkService() = default;
    ~DicomNetworkService() override = default;

    [[nodiscard]] std::unique_ptr<network::dicom_server> create_server(
        const network::server_config& config,
        const integration::tls_config& tls_cfg = {}) override {
        return integration::network_adapter::create_server(config, tls_cfg);
    }

    [[nodiscard]] integration::Result<integration::network_adapter::session_ptr>
        connect(const integration::connection_config& config) override {
        return integration::network_adapter::connect(config);
    }

    [[nodiscard]] integration::Result<integration::network_adapter::session_ptr>
        connect(const std::string& host, uint16_t port,
                std::chrono::milliseconds timeout = std::chrono::seconds{30}) override {
        return integration::network_adapter::connect(host, port, timeout);
    }
};

}  // namespace pacs::di
