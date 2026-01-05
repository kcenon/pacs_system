// BSD 3-Clause License
// Copyright (c) 2025, kcenon
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
#include <pacs/di/ilogger.hpp>
#include <pacs/di/service_interfaces.hpp>
#include <pacs/di/service_registration.hpp>
#include <pacs/di/test_support.hpp>

export module kcenon.pacs:di;

// ============================================================================
// Re-export pacs::di namespace
// ============================================================================

export namespace pacs::di {

// Logger interface and implementations
using pacs::di::ILogger;
using pacs::di::NullLogger;
using pacs::di::LoggerService;
using pacs::di::null_logger;

// Service interfaces
using pacs::di::IDicomStorage;
using pacs::di::IDicomCodec;
using pacs::di::IDicomNetwork;
using pacs::di::DicomNetworkService;

// Registration configuration
using pacs::di::registration_config;

// Registration functions
using pacs::di::register_services;
using pacs::di::register_storage;
using pacs::di::register_storage_instance;
using pacs::di::register_network;
using pacs::di::register_network_instance;
using pacs::di::register_logger;
using pacs::di::register_logger_instance;
using pacs::di::create_container;

} // namespace pacs::di

// ============================================================================
// Re-export pacs::di::test namespace
// ============================================================================

export namespace pacs::di::test {

// Mock implementations
using pacs::di::test::MockStorage;
using pacs::di::test::MockNetwork;

// Test container builder
using pacs::di::test::TestContainerBuilder;

// Convenience functions
using pacs::di::test::create_test_container;
using pacs::di::test::register_mock_storage;
using pacs::di::test::register_mock_network;

} // namespace pacs::di::test
