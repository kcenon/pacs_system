// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-encoding.cppm
 * @brief C++20 module partition for DICOM encoding types.
 *
 * This module partition exports encoding-related types:
 * - vr_type: Value Representation enumeration
 * - vr_info: VR metadata and validation
 * - byte_order: Endianness enumeration
 * - byte_swap16/32/64, read_be*, write_be*: Byte swapping utilities
 * - transfer_syntax: DICOM Transfer Syntax and registry lookups
 * - Codecs: Explicit VR, Implicit VR, Big Endian
 * - Compression codecs: JPEG, JPEG-LS, JPEG 2000, HTJ2K, JPEG XL, RLE,
 *   Frame Deflate, HEVC
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
#include <kcenon/pacs/encoding/byte_order.h>
#include <kcenon/pacs/encoding/byte_swap.h>
#include <kcenon/pacs/encoding/vr_type.h>
#include <kcenon/pacs/encoding/vr_info.h>
#include <kcenon/pacs/encoding/transfer_syntax.h>
#include <kcenon/pacs/encoding/explicit_vr_codec.h>
#include <kcenon/pacs/encoding/explicit_vr_big_endian_codec.h>
#include <kcenon/pacs/encoding/implicit_vr_codec.h>

// Compression codec headers
#include <kcenon/pacs/encoding/compression/image_params.h>
#include <kcenon/pacs/encoding/compression/compression_codec.h>
#include <kcenon/pacs/encoding/compression/codec_factory.h>
#include <kcenon/pacs/encoding/compression/jpeg_baseline_codec.h>
#include <kcenon/pacs/encoding/compression/jpeg_lossless_codec.h>
#include <kcenon/pacs/encoding/compression/jpeg_ls_codec.h>
#include <kcenon/pacs/encoding/compression/jpeg2000_codec.h>
#include <kcenon/pacs/encoding/compression/htj2k_codec.h>
#include <kcenon/pacs/encoding/compression/jpegxl_codec.h>
#include <kcenon/pacs/encoding/compression/rle_codec.h>
#include <kcenon/pacs/encoding/compression/frame_deflate_codec.h>
#include <kcenon/pacs/encoding/compression/hevc_codec.h>

// SIMD utility headers
#include <kcenon/pacs/encoding/simd/simd_config.h>
#include <kcenon/pacs/encoding/simd/simd_types.h>
#include <kcenon/pacs/encoding/simd/simd_utils.h>
#include <kcenon/pacs/encoding/simd/simd_photometric.h>
#include <kcenon/pacs/encoding/simd/simd_rle.h>
#include <kcenon/pacs/encoding/simd/simd_windowing.h>

export module kcenon.pacs:encoding;

// ============================================================================
// Re-export kcenon::pacs::encoding namespace
// ============================================================================

export namespace kcenon::pacs::encoding {

// Byte order types
using kcenon::pacs::encoding::byte_order;
using kcenon::pacs::encoding::vr_encoding;

// Byte swap utilities
using kcenon::pacs::encoding::byte_swap16;
using kcenon::pacs::encoding::byte_swap32;
using kcenon::pacs::encoding::byte_swap64;
using kcenon::pacs::encoding::read_be16;
using kcenon::pacs::encoding::read_be32;
using kcenon::pacs::encoding::read_be64;
using kcenon::pacs::encoding::write_be16;
using kcenon::pacs::encoding::write_be32;
using kcenon::pacs::encoding::write_be64;

// VR types
using kcenon::pacs::encoding::vr_type;
using kcenon::pacs::encoding::to_string;
using kcenon::pacs::encoding::from_string;
using kcenon::pacs::encoding::is_string_vr;
using kcenon::pacs::encoding::is_binary_vr;
using kcenon::pacs::encoding::is_numeric_vr;
using kcenon::pacs::encoding::has_explicit_32bit_length;
using kcenon::pacs::encoding::fixed_length;
using kcenon::pacs::encoding::is_fixed_length;
using kcenon::pacs::encoding::padding_char;

// VR info
using kcenon::pacs::encoding::vr_info;
using kcenon::pacs::encoding::get_vr_info;
using kcenon::pacs::encoding::validate_value;
using kcenon::pacs::encoding::validate_string;
using kcenon::pacs::encoding::pad_to_even;
using kcenon::pacs::encoding::trim_padding;
using kcenon::pacs::encoding::is_valid_charset;

// Transfer syntax
using kcenon::pacs::encoding::transfer_syntax;
using kcenon::pacs::encoding::find_transfer_syntax;
using kcenon::pacs::encoding::supported_transfer_syntaxes;
using kcenon::pacs::encoding::all_transfer_syntaxes;

// Codecs
using kcenon::pacs::encoding::explicit_vr_codec;
using kcenon::pacs::encoding::explicit_vr_big_endian_codec;
using kcenon::pacs::encoding::implicit_vr_codec;

} // namespace kcenon::pacs::encoding

// ============================================================================
// Re-export kcenon::pacs::encoding::compression namespace
// ============================================================================

export namespace kcenon::pacs::encoding::compression {

// Image parameters
using kcenon::pacs::encoding::compression::photometric_interpretation;
using kcenon::pacs::encoding::compression::to_string;
using kcenon::pacs::encoding::compression::parse_photometric_interpretation;
using kcenon::pacs::encoding::compression::image_params;

// Compression types
using kcenon::pacs::encoding::compression::compression_options;
using kcenon::pacs::encoding::compression::compression_result;
using kcenon::pacs::encoding::compression::codec_result;

// Codec base class
using kcenon::pacs::encoding::compression::compression_codec;

// Codec factory
using kcenon::pacs::encoding::compression::codec_factory;

// JPEG codecs
using kcenon::pacs::encoding::compression::jpeg_baseline_codec;
using kcenon::pacs::encoding::compression::jpeg_lossless_codec;
using kcenon::pacs::encoding::compression::jpeg_ls_codec;
using kcenon::pacs::encoding::compression::jpeg2000_codec;
using kcenon::pacs::encoding::compression::htj2k_codec;
using kcenon::pacs::encoding::compression::jpegxl_codec;

// RLE, Deflate, and HEVC codecs
using kcenon::pacs::encoding::compression::rle_codec;
using kcenon::pacs::encoding::compression::frame_deflate_codec;
using kcenon::pacs::encoding::compression::hevc_codec;

} // namespace kcenon::pacs::encoding::compression

// ============================================================================
// Re-export kcenon::pacs::encoding::simd namespace
// ============================================================================

export namespace kcenon::pacs::encoding::simd {

// SIMD feature detection
using kcenon::pacs::encoding::simd::simd_feature;
using kcenon::pacs::encoding::simd::has_feature;
using kcenon::pacs::encoding::simd::detect_features;
using kcenon::pacs::encoding::simd::get_features;
using kcenon::pacs::encoding::simd::optimal_vector_width;

// Individual feature checks
using kcenon::pacs::encoding::simd::has_sse2;
using kcenon::pacs::encoding::simd::has_ssse3;
using kcenon::pacs::encoding::simd::has_sse41;
using kcenon::pacs::encoding::simd::has_avx;
using kcenon::pacs::encoding::simd::has_avx2;
using kcenon::pacs::encoding::simd::has_avx512f;
using kcenon::pacs::encoding::simd::has_neon;

// SIMD byte swap operations
using kcenon::pacs::encoding::simd::swap_bytes_16_simd;
using kcenon::pacs::encoding::simd::swap_bytes_32_simd;
using kcenon::pacs::encoding::simd::swap_bytes_64_simd;

} // namespace kcenon::pacs::encoding::simd
