// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-network.cppm
 * @brief C++20 module partition for DICOM network protocol.
 *
 * This module partition exports network-related types:
 * - association: DICOM Association management
 * - pdu_encoder/decoder: PDU serialization
 * - pdu_types: PDU data structures
 * - dicom_server: DICOM server implementation
 * - DIMSE message types
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

// PACS network headers
#include <pacs/network/association.hpp>
#include <pacs/network/pdu_encoder.hpp>
#include <pacs/network/pdu_decoder.hpp>
#include <pacs/network/pdu_types.hpp>
#include <pacs/network/pdu_buffer_pool.hpp>
#include <pacs/network/server_config.hpp>
#include <pacs/network/dicom_server.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/status_codes.hpp>

export module kcenon.pacs:network;

// ============================================================================
// Re-export pacs::network namespace
// ============================================================================

export namespace pacs::network {

// Association
using pacs::network::association;
using pacs::network::association_state;
using pacs::network::presentation_context;

// PDU types
using pacs::network::pdu_encoder;
using pacs::network::pdu_decoder;
using pacs::network::pdu_buffer_pool;

// Server
using pacs::network::server_config;
using pacs::network::dicom_server;

} // namespace pacs::network

// ============================================================================
// Re-export pacs::network::dimse namespace
// ============================================================================

export namespace pacs::network::dimse {

using pacs::network::dimse::dimse_message;
using pacs::network::dimse::command_field;

} // namespace pacs::network::dimse
