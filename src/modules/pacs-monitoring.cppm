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
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// PACS monitoring headers
#include <pacs/monitoring/pacs_monitor.hpp>
#include <pacs/monitoring/health_checker.hpp>
#include <pacs/monitoring/health_status.hpp>
#include <pacs/monitoring/health_json.hpp>
#include <pacs/monitoring/pacs_metrics.hpp>

// PACS metric collectors (CRTP-based)
#include <pacs/monitoring/collectors/dicom_collector_base.hpp>
#include <pacs/monitoring/collectors/dicom_metrics_collector.hpp>
#include <pacs/monitoring/collectors/dicom_association_collector.hpp>
#include <pacs/monitoring/collectors/dicom_service_collector.hpp>
#include <pacs/monitoring/collectors/dicom_storage_collector.hpp>

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

// Metrics core
using pacs::monitoring::pacs_metrics;
using pacs::monitoring::association_counters;
using pacs::monitoring::transfer_counters;
using pacs::monitoring::pool_counters;
using pacs::monitoring::operation_counter;
using pacs::monitoring::dimse_operation;
using pacs::monitoring::to_string;

// CRTP collector base
using pacs::monitoring::dicom_collector_base;
using pacs::monitoring::dicom_metric;
using pacs::monitoring::config_map;
using pacs::monitoring::stats_map;

// Unified DICOM metrics collector
using pacs::monitoring::dicom_metrics_collector;
using pacs::monitoring::dicom_metrics_snapshot;

// Specialized collectors
using pacs::monitoring::dicom_association_collector;
using pacs::monitoring::association_metric;

using pacs::monitoring::dicom_service_collector;
using pacs::monitoring::service_metric;

using pacs::monitoring::dicom_storage_collector;
using pacs::monitoring::storage_metric;

} // namespace pacs::monitoring
