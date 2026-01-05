// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-encoding.cppm
 * @brief C++20 module partition for DICOM encoding types.
 *
 * This module partition exports encoding-related types:
 * - vr_type: Value Representation enumeration
 * - vr_info: VR metadata and validation
 * - byte_order: Endianness enumeration
 * - byte_swap: Byte swapping utilities
 * - transfer_syntax: DICOM Transfer Syntax
 * - Codecs: Explicit VR, Implicit VR, Big Endian
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
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// PACS encoding headers
#include <pacs/encoding/byte_order.hpp>
#include <pacs/encoding/byte_swap.hpp>
#include <pacs/encoding/vr_type.hpp>
#include <pacs/encoding/vr_info.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/explicit_vr_codec.hpp>
#include <pacs/encoding/explicit_vr_big_endian_codec.hpp>
#include <pacs/encoding/implicit_vr_codec.hpp>

export module kcenon.pacs:encoding;

// ============================================================================
// Re-export pacs::encoding namespace
// ============================================================================

export namespace pacs::encoding {

// Byte order types
using pacs::encoding::byte_order;

// Byte swap utilities
using pacs::encoding::byte_swap;

// VR types
using pacs::encoding::vr_type;
using pacs::encoding::to_string;
using pacs::encoding::from_string;

// VR info
using pacs::encoding::vr_info;
using pacs::encoding::get_vr_info;

// Transfer syntax
using pacs::encoding::transfer_syntax;
using pacs::encoding::vr_encoding;
using pacs::encoding::compression_type;

// Codecs
using pacs::encoding::explicit_vr_codec;
using pacs::encoding::explicit_vr_big_endian_codec;
using pacs::encoding::implicit_vr_codec;

} // namespace pacs::encoding
