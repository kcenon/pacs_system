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
 * @file service_interfaces.hpp
 * @brief Service interface aliases for dependency injection
 *
 * This file provides type aliases for DICOM service interfaces used with
 * ServiceContainer-based dependency injection. It unifies existing interfaces
 * under a consistent naming convention for DI registration and resolution.
 *
 * @see Issue #312 - ServiceContainer based DI Integration
 * @author kcenon
 * @since 1.0.0
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

namespace kcenon::pacs::di {

// =============================================================================
// Storage Interface
// =============================================================================

/**
 * @brief Storage interface for DICOM data persistence
 *
 * Alias for kcenon::pacs::storage::storage_interface providing unified access
 * through the DI container.
 *
 * @see kcenon::pacs::storage::storage_interface
 */
using IDicomStorage = storage::storage_interface;

// =============================================================================
// Codec Interface
// =============================================================================

/**
 * @brief Codec interface for image compression/decompression
 *
 * Alias for kcenon::pacs::encoding::compression::compression_codec providing
 * unified access through the DI container.
 *
 * @see kcenon::pacs::encoding::compression::compression_codec
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

}  // namespace kcenon::pacs::di
