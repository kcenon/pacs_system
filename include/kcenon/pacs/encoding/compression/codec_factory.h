// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file codec_factory.h
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_COMPRESSION_CODEC_FACTORY_HPP
#define PACS_ENCODING_COMPRESSION_CODEC_FACTORY_HPP

#include "kcenon/pacs/encoding/compression/compression_codec.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace kcenon::pacs::encoding::compression {

/**
 * @brief Factory class for creating compression codec instances.
 *
 * Provides a centralized registry for codec creation based on Transfer Syntax UID.
 * Thread-safe: All factory methods can be called from multiple threads.
 *
 * Two questions are answered separately:
 * - is_supported(): pacs_system implements a codec for the syntax (built in
 *   or backed by an optional library). This does not depend on the build and
 *   matches transfer_syntax::is_supported() for encapsulated syntaxes.
 * - is_available(): this build can encode and decode with that codec, that
 *   is, the codec is built in or CMake found its library.
 *
 * Usage:
 * @code
 * if (codec_factory::is_available("1.2.840.10008.1.2.4.50")) {
 *     auto codec = codec_factory::create("1.2.840.10008.1.2.4.50");
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
     * @return A codec instance if one exists for the UID, nullptr otherwise
     *
     * Returns a codec for every UID in supported_transfer_syntaxes() and for
     * the stub codecs (HEVC, JPEG XL, Frame Deflate), whose encode() and
     * decode() always fail. A codec whose optional library was not found at
     * build time fails the same way; check is_available() first.
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
     * @brief Returns the Transfer Syntax UIDs that pacs_system has a codec for.
     *
     * @return UIDs of the implemented codecs, whether or not this build found
     *         their optional libraries; stub codecs are not listed
     */
    [[nodiscard]] static std::vector<std::string_view> supported_transfer_syntaxes();

    /**
     * @brief Checks if pacs_system has a codec for a Transfer Syntax.
     *
     * @param transfer_syntax_uid The DICOM Transfer Syntax UID
     * @return true if the codec is implemented (built in or backed by an
     *         optional library), false for stub codecs and unknown UIDs.
     *         Does not depend on the build; see is_available().
     */
    [[nodiscard]] static bool is_supported(std::string_view transfer_syntax_uid);

    /**
     * @brief Checks if pacs_system has a codec for a Transfer Syntax.
     *
     * @param ts The Transfer Syntax object
     * @return Same as is_supported(ts.uid())
     */
    [[nodiscard]] static bool is_supported(const transfer_syntax& ts);

    /**
     * @brief Returns the Transfer Syntax UIDs this build can encode and decode.
     *
     * @return The subset of supported_transfer_syntaxes() whose codec is built
     *         in or whose optional library CMake found
     */
    [[nodiscard]] static std::vector<std::string_view> available_transfer_syntaxes();

    /**
     * @brief Checks if this build can encode and decode a Transfer Syntax.
     *
     * @param transfer_syntax_uid The DICOM Transfer Syntax UID
     * @return true if is_supported() and the codec is built in or was built
     *         with its optional library (PACS_WITH_JPEG_CODEC,
     *         PACS_WITH_JPEG2000_CODEC, PACS_WITH_JPEGLS_CODEC, or
     *         PACS_WITH_HTJ2K_CODEC)
     */
    [[nodiscard]] static bool is_available(std::string_view transfer_syntax_uid);

    /**
     * @brief Checks if this build can encode and decode a Transfer Syntax.
     *
     * @param ts The Transfer Syntax object
     * @return Same as is_available(ts.uid())
     */
    [[nodiscard]] static bool is_available(const transfer_syntax& ts);

private:
    codec_factory() = delete;  // Static-only class
};

}  // namespace kcenon::pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_CODEC_FACTORY_HPP
