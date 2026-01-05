// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-di.cppm
 * @brief C++20 module partition for dependency injection.
 *
 * This module partition exports DI-related types:
 * - ILogger: Logger interface
 * - service_interfaces: Service interface registry
 * - service_registration: Registration helpers
 * - test_support: Testing utilities
 *
 * Part of the kcenon.pacs module.
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

// PACS DI headers
#include <pacs/di/ilogger.hpp>
#include <pacs/di/service_interfaces.hpp>
#include <pacs/di/service_registration.hpp>
#include <pacs/di/test_support.hpp>

export module kcenon.pacs:di;

// ============================================================================
// Re-export pacs::di namespace
// ============================================================================

export namespace pacs::di {

// Logger interface
using pacs::di::ILogger;
using pacs::di::null_logger;

// Service interfaces
using pacs::di::service_interfaces;

// Registration
using pacs::di::service_registration;

// Testing
using pacs::di::test_support;

} // namespace pacs::di
