// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-services-cache.cppm
 * @brief C++20 module partition for query caching.
 *
 * This module partition exports query caching components:
 * - simple_lru_cache: Generic LRU cache implementation
 * - query_cache: DICOM query result cache with monitoring
 * - database_cursor: Database cursor abstraction (requires PACS_WITH_DATABASE_SYSTEM)
 * - parallel_query_executor: Multi-threaded query execution
 * - query_result_stream: Streaming query results (requires PACS_WITH_DATABASE_SYSTEM)
 * - streaming_query_handler: Handler for streaming queries
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// PACS cache headers
#include <pacs/services/cache/simple_lru_cache.hpp>
#include <pacs/services/cache/query_cache.hpp>
#include <pacs/services/cache/parallel_query_executor.hpp>
#include <pacs/services/cache/streaming_query_handler.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <pacs/services/cache/database_cursor.hpp>
#include <pacs/services/cache/query_result_stream.hpp>
#endif

export module kcenon.pacs:services_cache;

// ============================================================================
// Re-export pacs::services::cache namespace
// ============================================================================

export namespace pacs::services::cache {

// ==========================================================================
// LRU Cache
// ==========================================================================

// Generic LRU cache template
using pacs::services::cache::simple_lru_cache;
using pacs::services::cache::cache_entry;
using pacs::services::cache::cache_stats;

// ==========================================================================
// Query Cache
// ==========================================================================

// Query cache with monitoring integration
using pacs::services::cache::query_cache;
using pacs::services::cache::query_cache_config;
using pacs::services::cache::cached_query_result;

// ==========================================================================
// Parallel Query Executor
// ==========================================================================

// Multi-threaded query execution
using pacs::services::cache::parallel_query_executor;
using pacs::services::cache::query_task;
using pacs::services::cache::query_result_aggregator;

// ==========================================================================
// Streaming Query Handler
// ==========================================================================

// Streaming query handler
using pacs::services::cache::streaming_query_handler;

} // namespace pacs::services::cache

#ifdef PACS_WITH_DATABASE_SYSTEM
export namespace pacs::services {

// ==========================================================================
// Database Cursor (requires database_system)
// ==========================================================================

// Cursor for iterating over large result sets
using pacs::services::database_cursor;
using pacs::services::query_record;

// ==========================================================================
// Query Result Streaming (requires database_system)
// ==========================================================================

// Streaming query results
using pacs::services::query_result_stream;
using pacs::services::stream_config;

} // namespace pacs::services
#endif
