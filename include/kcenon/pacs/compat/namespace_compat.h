// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file namespace_compat.h
 * @brief Backward-compatible namespace alias for migration from pacs:: to kcenon::pacs::
 *
 * Include this header in downstream code that still uses the old pacs:: namespace
 * to transparently redirect to kcenon::pacs::.
 *
 * IMPORTANT: Do NOT include this header from any pacs internal header, as it
 * would prevent reopening the kcenon::pacs namespace via the alias.
 *
 * @code
 * #include <kcenon/pacs/compat/namespace_compat.h>
 * // Now pacs::core::dicom_tag resolves to kcenon::pacs::core::dicom_tag
 * @endcode
 *
 * @author kcenon
 * @since 2.0.0
 */

#pragma once

// Establish kcenon::pacs namespace (must exist before aliasing)
namespace kcenon::pacs {}

/// Backward-compatible alias: allows existing code using pacs:: to compile
/// without modification after the namespace migration to kcenon::pacs::.
namespace pacs = kcenon::pacs;
