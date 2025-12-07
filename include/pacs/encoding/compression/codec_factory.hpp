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
