/**
 * @file byte_swap.hpp
 * @brief Byte swapping utilities for endianness conversion
 *
 * This file provides constexpr functions for converting between
 * little-endian and big-endian byte ordering. Bulk operations use
 * SIMD optimization when available (SSE/AVX on x86, NEON on ARM).
 *
 * @see DICOM PS3.5 Section 7 - Data Set Encoding
 */

#ifndef PACS_ENCODING_BYTE_SWAP_HPP
#define PACS_ENCODING_BYTE_SWAP_HPP

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "simd/simd_utils.hpp"

namespace pacs::encoding {

/// @name Single Value Byte Swapping
/// @{

/**
 * @brief Swaps bytes in a 16-bit value.
 * @param value The value to swap
 * @return The byte-swapped value
 *
 * Example: 0x1234 -> 0x3412
 */
[[nodiscard]] constexpr uint16_t byte_swap16(uint16_t value) noexcept {
    return static_cast<uint16_t>((value >> 8) | (value << 8));
}

/**
 * @brief Swaps bytes in a 32-bit value.
 * @param value The value to swap
 * @return The byte-swapped value
 *
 * Example: 0x12345678 -> 0x78563412
 */
[[nodiscard]] constexpr uint32_t byte_swap32(uint32_t value) noexcept {
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8)  & 0x0000FF00) |
           ((value << 8)  & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

/**
 * @brief Swaps bytes in a 64-bit value.
 * @param value The value to swap
 * @return The byte-swapped value
 *
 * Example: 0x123456789ABCDEF0 -> 0xF0DEBC9A78563412
 */
[[nodiscard]] constexpr uint64_t byte_swap64(uint64_t value) noexcept {
    return ((value >> 56) & 0x00000000000000FF) |
           ((value >> 40) & 0x000000000000FF00) |
           ((value >> 24) & 0x0000000000FF0000) |
           ((value >> 8)  & 0x00000000FF000000) |
           ((value << 8)  & 0x000000FF00000000) |
           ((value << 24) & 0x0000FF0000000000) |
           ((value << 40) & 0x00FF000000000000) |
           ((value << 56) & 0xFF00000000000000);
}

/// @}

/// @name Big Endian Read/Write Functions
/// @{

/**
 * @brief Reads a 16-bit value from big-endian bytes.
 * @param data Pointer to at least 2 bytes
 * @return The value in native byte order
 */
[[nodiscard]] constexpr uint16_t read_be16(const uint8_t* data) noexcept {
    return (static_cast<uint16_t>(data[0]) << 8) |
           static_cast<uint16_t>(data[1]);
}

/**
 * @brief Reads a 32-bit value from big-endian bytes.
 * @param data Pointer to at least 4 bytes
 * @return The value in native byte order
 */
[[nodiscard]] constexpr uint32_t read_be32(const uint8_t* data) noexcept {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

/**
 * @brief Reads a 64-bit value from big-endian bytes.
 * @param data Pointer to at least 8 bytes
 * @return The value in native byte order
 */
[[nodiscard]] constexpr uint64_t read_be64(const uint8_t* data) noexcept {
    return (static_cast<uint64_t>(data[0]) << 56) |
           (static_cast<uint64_t>(data[1]) << 48) |
           (static_cast<uint64_t>(data[2]) << 40) |
           (static_cast<uint64_t>(data[3]) << 32) |
           (static_cast<uint64_t>(data[4]) << 24) |
           (static_cast<uint64_t>(data[5]) << 16) |
           (static_cast<uint64_t>(data[6]) << 8) |
           static_cast<uint64_t>(data[7]);
}

/**
 * @brief Writes a 16-bit value in big-endian byte order.
 * @param buffer Buffer to append to
 * @param value The value to write
 */
inline void write_be16(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

/**
 * @brief Writes a 32-bit value in big-endian byte order.
 * @param buffer Buffer to append to
 * @param value The value to write
 */
inline void write_be32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

/**
 * @brief Writes a 64-bit value in big-endian byte order.
 * @param buffer Buffer to append to
 * @param value The value to write
 */
inline void write_be64(std::vector<uint8_t>& buffer, uint64_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

/// @}

/// @name Bulk Byte Swapping for VR Types
/// @{

/**
 * @brief Swaps bytes in-place for OW (Other Word) data.
 * @param data Span of bytes (must be even length)
 * @return Byte-swapped copy of the data
 *
 * OW data consists of 16-bit words that need individual byte swapping.
 * Uses SIMD optimization (SSE/AVX/NEON) when available.
 */
[[nodiscard]] inline std::vector<uint8_t> swap_ow_bytes(
    std::span<const uint8_t> data) {
    std::vector<uint8_t> result(data.size());
    simd::swap_bytes_16_simd(data.data(), result.data(), data.size());
    return result;
}

/**
 * @brief Swaps bytes in-place for OL (Other Long) data.
 * @param data Span of bytes (must be multiple of 4)
 * @return Byte-swapped copy of the data
 *
 * OL data consists of 32-bit values that need individual byte swapping.
 * Uses SIMD optimization (SSE/AVX/NEON) when available.
 */
[[nodiscard]] inline std::vector<uint8_t> swap_ol_bytes(
    std::span<const uint8_t> data) {
    std::vector<uint8_t> result(data.size());
    simd::swap_bytes_32_simd(data.data(), result.data(), data.size());
    return result;
}

/**
 * @brief Swaps bytes in-place for OF (Other Float) data.
 * @param data Span of bytes (must be multiple of 4)
 * @return Byte-swapped copy of the data
 *
 * OF data consists of 32-bit floats that need individual byte swapping.
 */
[[nodiscard]] inline std::vector<uint8_t> swap_of_bytes(
    std::span<const uint8_t> data) {
    return swap_ol_bytes(data);  // Same as OL (32-bit values)
}

/**
 * @brief Swaps bytes in-place for OD (Other Double) data.
 * @param data Span of bytes (must be multiple of 8)
 * @return Byte-swapped copy of the data
 *
 * OD data consists of 64-bit doubles that need individual byte swapping.
 * Uses SIMD optimization (SSE/AVX/NEON) when available.
 */
[[nodiscard]] inline std::vector<uint8_t> swap_od_bytes(
    std::span<const uint8_t> data) {
    std::vector<uint8_t> result(data.size());
    simd::swap_bytes_64_simd(data.data(), result.data(), data.size());
    return result;
}

/**
 * @brief Swaps bytes for AT (Attribute Tag) data.
 * @param data Span of bytes (must be multiple of 4)
 * @return Byte-swapped copy of the data
 *
 * AT consists of two 16-bit values (group, element), each needing byte swap.
 */
[[nodiscard]] inline std::vector<uint8_t> swap_at_bytes(
    std::span<const uint8_t> data) {
    return swap_ow_bytes(data);  // Each 16-bit part swapped individually
}

/// @}

/// @name Numeric Value Byte Swapping
/// @{

/**
 * @brief Swaps bytes for US (Unsigned Short) value in raw data.
 * @param data Span of 2 bytes
 * @return Byte-swapped copy
 */
[[nodiscard]] inline std::vector<uint8_t> swap_us_bytes(
    std::span<const uint8_t> data) {
    return swap_ow_bytes(data);
}

/**
 * @brief Swaps bytes for SS (Signed Short) value in raw data.
 * @param data Span of 2 bytes
 * @return Byte-swapped copy
 */
[[nodiscard]] inline std::vector<uint8_t> swap_ss_bytes(
    std::span<const uint8_t> data) {
    return swap_ow_bytes(data);
}

/**
 * @brief Swaps bytes for UL (Unsigned Long) value in raw data.
 * @param data Span of 4 bytes
 * @return Byte-swapped copy
 */
[[nodiscard]] inline std::vector<uint8_t> swap_ul_bytes(
    std::span<const uint8_t> data) {
    return swap_ol_bytes(data);
}

/**
 * @brief Swaps bytes for SL (Signed Long) value in raw data.
 * @param data Span of 4 bytes
 * @return Byte-swapped copy
 */
[[nodiscard]] inline std::vector<uint8_t> swap_sl_bytes(
    std::span<const uint8_t> data) {
    return swap_ol_bytes(data);
}

/**
 * @brief Swaps bytes for FL (Float) value in raw data.
 * @param data Span of 4 bytes
 * @return Byte-swapped copy
 */
[[nodiscard]] inline std::vector<uint8_t> swap_fl_bytes(
    std::span<const uint8_t> data) {
    return swap_ol_bytes(data);
}

/**
 * @brief Swaps bytes for FD (Double) value in raw data.
 * @param data Span of 8 bytes
 * @return Byte-swapped copy
 */
[[nodiscard]] inline std::vector<uint8_t> swap_fd_bytes(
    std::span<const uint8_t> data) {
    return swap_od_bytes(data);
}

/// @}

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_BYTE_SWAP_HPP
