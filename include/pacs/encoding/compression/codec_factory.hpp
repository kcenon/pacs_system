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

#ifndef PACS_ENCODING_COMPRESSION_CODEC_FACTORY_HPP
#define PACS_ENCODING_COMPRESSION_CODEC_FACTORY_HPP

#include "pacs/encoding/compression/compression_codec.hpp"
#include "pacs/encoding/transfer_syntax.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace pacs::encoding::compression {

/**
 * @brief Factory class for creating compression codec instances.
 *
 * Provides a centralized registry for codec creation based on Transfer Syntax UID.
 * Thread-safe: All factory methods can be called from multiple threads.
 *
 * Usage:
 * @code
 * auto codec = codec_factory::create("1.2.840.10008.1.2.4.50");
 * if (codec) {
 *     auto result = codec->encode(pixel_data, params);
 * }
 * @endcode
 */
class codec_factory {
public:
    /**
     * @brief Creates a codec instance for the given Transfer Syntax UID.
     *
     * @param transfer_syntax_uid The DICOM Transfer Syntax UID
     * @return A codec instance if supported, nullptr otherwise
     *
     * Supported UIDs:
     * - 1.2.840.10008.1.2.4.50 - JPEG Baseline (Process 1)
     */
    [[nodiscard]] static std::unique_ptr<compression_codec> create(
        std::string_view transfer_syntax_uid);

    /**
     * @brief Creates a codec instance for the given Transfer Syntax.
     *
     * @param ts The Transfer Syntax object
     * @return A codec instance if supported, nullptr otherwise
     */
    [[nodiscard]] static std::unique_ptr<compression_codec> create(
        const transfer_syntax& ts);

    /**
     * @brief Returns a list of all supported Transfer Syntax UIDs.
     *
     * @return Vector of supported UIDs for compression codecs
     */
    [[nodiscard]] static std::vector<std::string_view> supported_transfer_syntaxes();

    /**
     * @brief Checks if a Transfer Syntax is supported for compression.
     *
     * @param transfer_syntax_uid The DICOM Transfer Syntax UID
     * @return true if compression/decompression is supported
     */
    [[nodiscard]] static bool is_supported(std::string_view transfer_syntax_uid);

    /**
     * @brief Checks if a Transfer Syntax is supported for compression.
     *
     * @param ts The Transfer Syntax object
     * @return true if compression/decompression is supported
     */
    [[nodiscard]] static bool is_supported(const transfer_syntax& ts);

private:
    codec_factory() = delete;  // Static-only class
};

}  // namespace pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_CODEC_FACTORY_HPP
