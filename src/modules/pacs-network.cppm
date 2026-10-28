// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
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
 *
 * DIMSE message types are exported by the :network_dimse partition.
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
#include <kcenon/pacs/network/association.h>
#include <kcenon/pacs/network/pdu_encoder.h>
#include <kcenon/pacs/network/pdu_decoder.h>
#include <kcenon/pacs/network/pdu_types.h>
#include <kcenon/pacs/network/pdu_buffer_pool.h>
#include <kcenon/pacs/network/server_config.h>
#include <kcenon/pacs/network/dicom_server.h>

// PACS network headers - V2 implementation
#include <kcenon/pacs/network/v2/dicom_association_handler.h>
#include <kcenon/pacs/network/v2/dicom_server_v2.h>

export module kcenon.pacs:network;

// ============================================================================
// Re-export kcenon::pacs::network namespace
// ============================================================================
// The DICOM_PROTOCOL_VERSION, AE_TITLE_LENGTH, and *_MAX_PDU_LENGTH constants
// in pdu_types.h have internal linkage, which a module cannot export.

export namespace kcenon::pacs::network {

// Association
using kcenon::pacs::network::association;
using kcenon::pacs::network::association_state;
using kcenon::pacs::network::association_error;
using kcenon::pacs::network::proposed_presentation_context;
using kcenon::pacs::network::accepted_presentation_context;
using kcenon::pacs::network::association_config;
using kcenon::pacs::network::scp_config;
using kcenon::pacs::network::rejection_info;

// PDU types and enums
using kcenon::pacs::network::pdu_type;
using kcenon::pacs::network::item_type;
using kcenon::pacs::network::presentation_context_result;
using kcenon::pacs::network::abort_source;
using kcenon::pacs::network::abort_reason;
using kcenon::pacs::network::reject_result;
using kcenon::pacs::network::reject_source;
using kcenon::pacs::network::reject_reason_user;
using kcenon::pacs::network::reject_reason_provider_acse;
using kcenon::pacs::network::reject_reason_provider_presentation;
using kcenon::pacs::network::presentation_data_value;
using kcenon::pacs::network::presentation_context_rq;
using kcenon::pacs::network::presentation_context_ac;
using kcenon::pacs::network::scp_scu_role_selection;
using kcenon::pacs::network::user_information;
using kcenon::pacs::network::associate_rq;
using kcenon::pacs::network::associate_ac;
using kcenon::pacs::network::associate_rj;
using kcenon::pacs::network::release_rq_pdu;
using kcenon::pacs::network::release_rp_pdu;
using kcenon::pacs::network::abort_pdu;
using kcenon::pacs::network::p_data_tf_pdu;
using kcenon::pacs::network::pdu;

// PDU encoder/decoder
using kcenon::pacs::network::pdu_encoder;
using kcenon::pacs::network::pdu_decoder;
using kcenon::pacs::network::pdu_decode_error;

// Buffer pool
using kcenon::pacs::network::pdu_pool_statistics;
using kcenon::pacs::network::pooled_buffer;
using kcenon::pacs::network::tracked_pdu_pool;
using kcenon::pacs::network::pdu_buffer_pool;
using kcenon::pacs::network::make_pooled_pdu_buffer;
using kcenon::pacs::network::make_pooled_pdv;
using kcenon::pacs::network::make_pooled_p_data_tf;

// Server configuration
using kcenon::pacs::network::server_config;
using kcenon::pacs::network::server_statistics;

// Server
using kcenon::pacs::network::dicom_server;

// to_string overloads (association and PDU enums)
using kcenon::pacs::network::to_string;

} // namespace kcenon::pacs::network

// ============================================================================
// Re-export kcenon::pacs::network::v2 namespace
// ============================================================================

export namespace kcenon::pacs::network::v2 {

// Handler state
using kcenon::pacs::network::v2::handler_state;
using kcenon::pacs::network::v2::to_string;

// Association handler
using kcenon::pacs::network::v2::association_established_callback;
using kcenon::pacs::network::v2::association_closed_callback;
using kcenon::pacs::network::v2::handler_error_callback;
using kcenon::pacs::network::v2::dicom_association_handler;

// Server v2
using kcenon::pacs::network::v2::dicom_server_v2;

} // namespace kcenon::pacs::network::v2
