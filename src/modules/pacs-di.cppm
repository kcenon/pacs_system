// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-di.cppm
 * @brief C++20 module partition for dependency injection.
 *
 * This module partition exports DI-related types:
 * - ILogger: Logger interface and implementations
 * - Service interfaces: IDicomStorage, IDicomNetwork, IDicomCodec
 * - service_registration: Registration helpers for common_system::di
 * - test_support: Testing utilities (mocks, TestContainerBuilder)
 *
 * Part of the kcenon.pacs module.
 *
 * Integration with common_system::di:
 * This module works with kcenon::common::di::service_container for
 * dependency injection throughout pacs_system.
 */

module;

// Standard library imports
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// PACS DI headers
#include <kcenon/pacs/di/ilogger.h>
#include <kcenon/pacs/di/service_interfaces.h>
#include <kcenon/pacs/di/service_registration.h>
#include <kcenon/pacs/di/test_support.h>

export module kcenon.pacs:di;

// ============================================================================
// Re-export kcenon::pacs::di namespace
// ============================================================================

export namespace kcenon::pacs::di {

// Logger interface and implementations
using kcenon::pacs::di::ILogger;
using kcenon::pacs::di::NullLogger;
using kcenon::pacs::di::LoggerService;
using kcenon::pacs::di::null_logger;

// Service interfaces
using kcenon::pacs::di::IDicomStorage;
using kcenon::pacs::di::IDicomCodec;
using kcenon::pacs::di::IDicomNetwork;
using kcenon::pacs::di::DicomNetworkService;

// Registration configuration
using kcenon::pacs::di::registration_config;

// Registration functions
using kcenon::pacs::di::register_services;
using kcenon::pacs::di::register_storage;
using kcenon::pacs::di::register_storage_instance;
using kcenon::pacs::di::register_network;
using kcenon::pacs::di::register_network_instance;
using kcenon::pacs::di::register_logger;
using kcenon::pacs::di::register_logger_instance;
using kcenon::pacs::di::create_container;

} // namespace kcenon::pacs::di

// ============================================================================
// Re-export kcenon::pacs::di::test namespace
// ============================================================================

export namespace kcenon::pacs::di::test {

// Mock implementations
using kcenon::pacs::di::test::MockStorage;
using kcenon::pacs::di::test::MockNetwork;

// Test container builder
using kcenon::pacs::di::test::TestContainerBuilder;

// Convenience functions
using kcenon::pacs::di::test::create_test_container;
using kcenon::pacs::di::test::register_mock_storage;
using kcenon::pacs::di::test::register_mock_network;

} // namespace kcenon::pacs::di::test
