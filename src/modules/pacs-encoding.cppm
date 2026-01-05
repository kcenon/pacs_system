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
 * - Compression codecs: JPEG, JPEG-LS, JPEG 2000, RLE
 * - SIMD utilities: CPU feature detection, vectorized operations
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

// Compression codec headers
#include <pacs/encoding/compression/image_params.hpp>
#include <pacs/encoding/compression/compression_codec.hpp>
#include <pacs/encoding/compression/codec_factory.hpp>
#include <pacs/encoding/compression/jpeg_baseline_codec.hpp>
#include <pacs/encoding/compression/jpeg_lossless_codec.hpp>
#include <pacs/encoding/compression/jpeg_ls_codec.hpp>
#include <pacs/encoding/compression/jpeg2000_codec.hpp>
#include <pacs/encoding/compression/rle_codec.hpp>

// SIMD utility headers
#include <pacs/encoding/simd/simd_config.hpp>
#include <pacs/encoding/simd/simd_types.hpp>
#include <pacs/encoding/simd/simd_utils.hpp>
#include <pacs/encoding/simd/simd_photometric.hpp>
#include <pacs/encoding/simd/simd_rle.hpp>
#include <pacs/encoding/simd/simd_windowing.hpp>

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

// ============================================================================
// Re-export pacs::encoding::compression namespace
// ============================================================================

export namespace pacs::encoding::compression {

// Image parameters
using pacs::encoding::compression::photometric_interpretation;
using pacs::encoding::compression::to_string;
using pacs::encoding::compression::parse_photometric_interpretation;
using pacs::encoding::compression::image_params;

// Compression types
using pacs::encoding::compression::compression_options;
using pacs::encoding::compression::compression_result;
using pacs::encoding::compression::codec_result;

// Codec base class
using pacs::encoding::compression::compression_codec;

// Codec factory
using pacs::encoding::compression::codec_factory;

// JPEG codecs
using pacs::encoding::compression::jpeg_baseline_codec;
using pacs::encoding::compression::jpeg_lossless_codec;
using pacs::encoding::compression::jpeg_ls_codec;
using pacs::encoding::compression::jpeg2000_codec;

// RLE codec
using pacs::encoding::compression::rle_codec;

} // namespace pacs::encoding::compression

// ============================================================================
// Re-export pacs::encoding::simd namespace
// ============================================================================

export namespace pacs::encoding::simd {

// SIMD feature detection
using pacs::encoding::simd::simd_feature;
using pacs::encoding::simd::has_feature;
using pacs::encoding::simd::detect_features;
using pacs::encoding::simd::get_features;
using pacs::encoding::simd::optimal_vector_width;

// Individual feature checks
using pacs::encoding::simd::has_sse2;
using pacs::encoding::simd::has_ssse3;
using pacs::encoding::simd::has_sse41;
using pacs::encoding::simd::has_avx;
using pacs::encoding::simd::has_avx2;
using pacs::encoding::simd::has_avx512f;
using pacs::encoding::simd::has_neon;

// SIMD byte swap operations
using pacs::encoding::simd::swap_bytes_16_simd;
using pacs::encoding::simd::swap_bytes_32_simd;
using pacs::encoding::simd::swap_bytes_64_simd;

} // namespace pacs::encoding::simd
