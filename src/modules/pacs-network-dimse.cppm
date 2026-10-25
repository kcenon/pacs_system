// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-network-dimse.cppm
 * @brief C++20 module partition for DIMSE message types.
 *
 * This module partition exports DIMSE types:
 * - dimse_message: Base message class
 * - command_field: DIMSE command field enumeration and helpers
 * - status_code: Status code type and category checks
 * - Factory functions: C-ECHO, C-STORE, C-FIND, and the N-* services
 *   (N-ACTION, N-CREATE, N-DELETE, N-EVENT-REPORT, N-GET, N-SET)
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// PACS DIMSE headers
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/network/dimse/command_field.h>
#include <kcenon/pacs/network/dimse/status_codes.h>
#include <kcenon/pacs/network/dimse/n_action.h>
#include <kcenon/pacs/network/dimse/n_create.h>
#include <kcenon/pacs/network/dimse/n_delete.h>
#include <kcenon/pacs/network/dimse/n_event_report.h>
#include <kcenon/pacs/network/dimse/n_get.h>
#include <kcenon/pacs/network/dimse/n_set.h>

export module kcenon.pacs:network_dimse;

// ============================================================================
// Re-export kcenon::pacs::network::dimse namespace
// ============================================================================
// The status_* codes (status_codes.h) and the tag_*, priority_*, and
// command_data_set_type_* constants (dimse_message.h) are namespace-scope
// constexpr variables with internal linkage, which a module cannot export.
// Use the is_success() / is_pending() / ... helpers, or include the header.

export namespace kcenon::pacs::network::dimse {

// ==========================================================================
// Core Message Types
// ==========================================================================

// DIMSE message class
using kcenon::pacs::network::dimse::dimse_message;
using kcenon::pacs::network::dimse::dimse_error;
using kcenon::pacs::network::dimse::dimse_result;

// Command field enumeration and helpers
using kcenon::pacs::network::dimse::command_field;
using kcenon::pacs::network::dimse::is_request;
using kcenon::pacs::network::dimse::is_response;
using kcenon::pacs::network::dimse::is_dimse_c;
using kcenon::pacs::network::dimse::is_dimse_n;
using kcenon::pacs::network::dimse::get_response_command;
using kcenon::pacs::network::dimse::get_request_command;

// to_string overloads (command_field, dimse_error)
using kcenon::pacs::network::dimse::to_string;

// ==========================================================================
// Status Codes
// ==========================================================================

// Status code type
using kcenon::pacs::network::dimse::status_code;

// Status category checks
using kcenon::pacs::network::dimse::is_success;
using kcenon::pacs::network::dimse::is_pending;
using kcenon::pacs::network::dimse::is_cancel;
using kcenon::pacs::network::dimse::is_warning;
using kcenon::pacs::network::dimse::is_failure;
using kcenon::pacs::network::dimse::is_final;
using kcenon::pacs::network::dimse::status_description;
using kcenon::pacs::network::dimse::status_category;

// ==========================================================================
// C-* Message Factory Functions
// ==========================================================================

// C-ECHO
using kcenon::pacs::network::dimse::make_c_echo_rq;
using kcenon::pacs::network::dimse::make_c_echo_rsp;

// C-STORE
using kcenon::pacs::network::dimse::make_c_store_rq;
using kcenon::pacs::network::dimse::make_c_store_rsp;

// C-FIND
using kcenon::pacs::network::dimse::make_c_find_rq;
using kcenon::pacs::network::dimse::make_c_find_rsp;

// ==========================================================================
// N-* Message Factory Functions
// ==========================================================================

// N-ACTION
using kcenon::pacs::network::dimse::make_n_action_rq;
using kcenon::pacs::network::dimse::make_n_action_rsp;

// N-CREATE
using kcenon::pacs::network::dimse::make_n_create_rq;
using kcenon::pacs::network::dimse::make_n_create_rsp;

// N-DELETE
using kcenon::pacs::network::dimse::make_n_delete_rq;
using kcenon::pacs::network::dimse::make_n_delete_rsp;

// N-EVENT-REPORT
using kcenon::pacs::network::dimse::make_n_event_report_rq;
using kcenon::pacs::network::dimse::make_n_event_report_rsp;

// N-GET
using kcenon::pacs::network::dimse::make_n_get_rq;
using kcenon::pacs::network::dimse::make_n_get_rsp;

// N-SET
using kcenon::pacs::network::dimse::make_n_set_rq;
using kcenon::pacs::network::dimse::make_n_set_rsp;

} // namespace kcenon::pacs::network::dimse
