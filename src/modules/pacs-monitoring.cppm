// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-monitoring.cppm
 * @brief C++20 module partition for system monitoring.
 *
 * This module partition exports monitoring-related types:
 * - pacs_monitor: Unified monitoring system
 * - health_checker: Health status verification
 * - health_status: Health status data
 * - pacs_metrics: DICOM-specific metrics
 * - Metric collectors for associations, services, storage
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
#include <string>
#include <string_view>
#include <vector>

// PACS monitoring headers
#include <pacs/monitoring/pacs_monitor.hpp>
#include <pacs/monitoring/health_checker.hpp>
#include <pacs/monitoring/health_status.hpp>
#include <pacs/monitoring/health_json.hpp>
#include <pacs/monitoring/pacs_metrics.hpp>

export module kcenon.pacs:monitoring;

// ============================================================================
// Re-export pacs::monitoring namespace
// ============================================================================

export namespace pacs::monitoring {

// Monitor
using pacs::monitoring::pacs_monitor;
using pacs::monitoring::metric_type;

// Health
using pacs::monitoring::health_checker;
using pacs::monitoring::health_status;

// Metrics
using pacs::monitoring::pacs_metrics;

} // namespace pacs::monitoring
