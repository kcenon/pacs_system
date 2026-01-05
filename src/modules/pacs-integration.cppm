// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-integration.cppm
 * @brief C++20 module partition for external system adapters.
 *
 * This module partition exports integration-related types:
 * - logger_adapter: Logging framework integration
 * - thread_adapter: Thread utilities
 * - thread_pool_adapter: Thread pool wrapper
 * - executor_adapter: Executor pattern
 * - network_adapter: Network integration
 * - container_adapter: DI container
 * - dicom_session: DICOM-aware session wrapper
 * - itk_adapter: ITK integration (optional, requires PACS_WITH_ITK)
 * - monitoring_adapter: Monitoring system bridge
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// PACS integration headers
#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/thread_adapter.hpp>
#include <pacs/integration/thread_pool_adapter.hpp>
#include <pacs/integration/executor_adapter.hpp>
#include <pacs/integration/network_adapter.hpp>
#include <pacs/integration/container_adapter.hpp>
#include <pacs/integration/dicom_session.hpp>
#include <pacs/integration/monitoring_adapter.hpp>

// ITK adapter (optional)
#ifdef PACS_WITH_ITK
#include <pacs/integration/itk_adapter.hpp>
#endif

export module kcenon.pacs:integration;

// ============================================================================
// Re-export pacs::integration namespace
// ============================================================================

export namespace pacs::integration {

// Adapters
using pacs::integration::logger_adapter;
using pacs::integration::thread_adapter;
using pacs::integration::thread_pool_adapter;
using pacs::integration::thread_pool_interface;
using pacs::integration::executor_adapter;
using pacs::integration::network_adapter;
using pacs::integration::container_adapter;
using pacs::integration::monitoring_adapter;

// DICOM session
using pacs::integration::dicom_session;

} // namespace pacs::integration

// ============================================================================
// Re-export pacs::integration::itk namespace (optional)
// ============================================================================

#ifdef PACS_WITH_ITK

export namespace pacs::integration::itk {

// Type aliases
using pacs::integration::itk::ct_image_type;
using pacs::integration::itk::mr_image_type;
using pacs::integration::itk::grayscale_2d_type;
using pacs::integration::itk::grayscale_3d_type;
using pacs::integration::itk::signed_grayscale_3d_type;
using pacs::integration::itk::rgb_pixel_type;
using pacs::integration::itk::rgb_2d_type;

// Metadata types
using pacs::integration::itk::image_metadata;
using pacs::integration::itk::slice_info;

// Metadata extraction
using pacs::integration::itk::extract_metadata;
using pacs::integration::itk::sort_slices;

// Dataset conversion
using pacs::integration::itk::dataset_to_image;
using pacs::integration::itk::series_to_image;

// Pixel data utilities
using pacs::integration::itk::get_pixel_data;
using pacs::integration::itk::is_signed_pixel_data;
using pacs::integration::itk::is_multi_frame;
using pacs::integration::itk::get_frame_count;

// Hounsfield Unit conversion
using pacs::integration::itk::apply_hounsfield_conversion;

// Convenience functions
using pacs::integration::itk::load_ct_series;
using pacs::integration::itk::load_mr_series;
using pacs::integration::itk::scan_dicom_directory;
using pacs::integration::itk::group_by_series;

} // namespace pacs::integration::itk

#endif // PACS_WITH_ITK
