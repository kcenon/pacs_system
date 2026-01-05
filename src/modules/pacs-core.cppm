// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-core.cppm
 * @brief C++20 module partition for DICOM core types.
 *
 * This module partition exports the fundamental DICOM data types:
 * - dicom_tag: Tag representation (Group, Element pairs)
 * - dicom_element: Data Element (Tag, VR, Value)
 * - dicom_dataset: Ordered collection of Data Elements
 * - dicom_file: DICOM file parsing and serialization
 * - dicom_dictionary: Tag definitions and lookup
 * - tag_info: Tag metadata
 * - pool_manager: Resource pooling utilities
 * - events: Event definitions
 * - Result<T>: Error handling pattern
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports needed before module declaration
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// PACS core headers
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/tag_info.hpp>
#include <pacs/core/dicom_dictionary.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_file.hpp>
#include <pacs/core/pool_manager.hpp>
#include <pacs/core/events.hpp>
#include <pacs/core/result.hpp>

export module kcenon.pacs:core;

// ============================================================================
// Re-export pacs::core namespace
// ============================================================================

export namespace pacs::core {

// Tag types
using pacs::core::dicom_tag;

// Tag constants namespace
namespace tags = ::pacs::core::tags;

// Tag info
using pacs::core::tag_info;

// Dictionary
using pacs::core::dicom_dictionary;

// Element types
using pacs::core::dicom_element;

// Dataset types
using pacs::core::dicom_dataset;

// File types
using pacs::core::dicom_file;

// Pool manager
using pacs::core::pool_manager;

} // namespace pacs::core

// ============================================================================
// Re-export pacs namespace (Result pattern)
// ============================================================================

export namespace pacs {

// Result types
using pacs::Result;
using pacs::VoidResult;
using pacs::error_info;

// Error code namespace
namespace error_codes = ::pacs::error_codes;

// Utility functions
using pacs::ok;
using pacs::make_error;
using pacs::is_ok;
using pacs::is_error;
using pacs::get_value;
using pacs::get_error;
using pacs::try_catch;
using pacs::try_catch_void;
using pacs::pacs_error;
using pacs::pacs_void_error;

} // namespace pacs

// ============================================================================
// Re-export pacs::core::events namespace
// ============================================================================

export namespace pacs::core::events {

using namespace ::pacs::core::events;

} // namespace pacs::core::events
