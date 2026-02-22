// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file byte_order.hpp
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_BYTE_ORDER_HPP
#define PACS_ENCODING_BYTE_ORDER_HPP

namespace pacs::encoding {

/**
 * @brief Byte ordering for DICOM data encoding.
 *
 * DICOM supports both little-endian and big-endian byte ordering.
 * Little-endian is the most common and is used by default in DICOM.
 */
enum class byte_order {
    little_endian,  ///< Least significant byte first (most common)
    big_endian      ///< Most significant byte first (legacy, rarely used)
};

/**
 * @brief Value Representation encoding mode.
 *
 * Determines whether VR (Value Representation) is explicitly
 * encoded in the data stream or implicitly determined from the tag.
 */
enum class vr_encoding {
    implicit,    ///< VR determined from data dictionary lookup
    explicit_vr  ///< VR explicitly encoded in the data stream
};

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_BYTE_ORDER_HPP
