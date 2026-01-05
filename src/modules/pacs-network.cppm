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
 * - pdu_buffer_pool: Buffer pooling for PDU I/O
 * - dicom_server: DICOM server implementation
 * - v2 implementation: network_system integration
 * - DIMSE message types (basic exports, see :network-dimse for full)
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
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// PACS network headers - Core
#include <pacs/network/association.hpp>
#include <pacs/network/pdu_encoder.hpp>
#include <pacs/network/pdu_decoder.hpp>
#include <pacs/network/pdu_types.hpp>
#include <pacs/network/pdu_buffer_pool.hpp>
#include <pacs/network/server_config.hpp>
#include <pacs/network/dicom_server.hpp>

// PACS network headers - V2 implementation
#include <pacs/network/v2/dicom_association_handler.hpp>
#include <pacs/network/v2/dicom_server_v2.hpp>

// PACS network headers - DIMSE (basic)
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

// PDU types and enums
using pacs::network::pdu_type;
using pacs::network::item_type;
using pacs::network::rejection_result;
using pacs::network::rejection_source;
using pacs::network::rejection_reason;
using pacs::network::abort_source;
using pacs::network::abort_reason;

// PDU encoder/decoder
using pacs::network::pdu_encoder;
using pacs::network::pdu_decoder;

// Buffer pool
using pacs::network::pdu_buffer_pool;
using pacs::network::pooled_buffer;

// Server configuration
using pacs::network::server_config;
using pacs::network::tls_config;

// Server
using pacs::network::dicom_server;

} // namespace pacs::network

// ============================================================================
// Re-export pacs::network::v2 namespace
// ============================================================================

export namespace pacs::network::v2 {

// Handler state
using pacs::network::v2::handler_state;
using pacs::network::v2::to_string;

// Association handler
using pacs::network::v2::dicom_association_handler;

// Server v2
using pacs::network::v2::dicom_server_v2;

} // namespace pacs::network::v2

// ============================================================================
// Re-export pacs::network::dimse namespace (basic types)
// ============================================================================

export namespace pacs::network::dimse {

// Message types
using pacs::network::dimse::dimse_message;
using pacs::network::dimse::command_field;

// Status codes
using pacs::network::dimse::status_code;
using pacs::network::dimse::status_success;
using pacs::network::dimse::status_pending;
using pacs::network::dimse::status_pending_warning;
using pacs::network::dimse::status_cancel;

// Status category checks
using pacs::network::dimse::is_success;
using pacs::network::dimse::is_pending;
using pacs::network::dimse::is_warning;
using pacs::network::dimse::is_failure;

} // namespace pacs::network::dimse
