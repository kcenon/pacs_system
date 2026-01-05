// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-ai.cppm
 * @brief C++20 module partition for AI/ML integration.
 *
 * This module partition exports AI-related types:
 * - ai_service_connector: External AI service connection
 * - ai_result_handler: AI inference result processing
 *
 * Part of the kcenon.pacs module.
 *
 * @note This module is optional and requires KCENON_WITH_AI to be defined.
 */

module;

// Standard library imports
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// PACS AI headers
#include <pacs/ai/ai_service_connector.hpp>
#include <pacs/ai/ai_result_handler.hpp>

export module kcenon.pacs:ai;

// ============================================================================
// Re-export pacs::ai namespace
// ============================================================================

export namespace pacs::ai {

// AI service
using pacs::ai::ai_service_connector;

// Result handling
using pacs::ai::ai_result_handler;

} // namespace pacs::ai
