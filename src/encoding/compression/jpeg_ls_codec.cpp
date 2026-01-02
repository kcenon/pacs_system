#include "pacs/encoding/compression/jpeg_ls_codec.hpp"
#include <pacs/core/result.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>

#ifdef PACS_WITH_JPEGLS_CODEC
#include <charls/charls.h>
#endif

namespace pacs::encoding::compression {

namespace {

#ifdef PACS_WITH_JPEGLS_CODEC

// CharLS 2.x uses jpegls_error::what() for error messages

/**
 * @brief Determines CharLS interleave mode from image parameters.
 */
[[nodiscard]] charls::interleave_mode get_interleave_mode(const image_params& params) {
    if (params.samples_per_pixel == 1) {
        return charls::interleave_mode::none;
    }
    // For color images, use sample interleaved mode (RGBRGB...)
    return (params.planar_configuration == 0)
               ? charls::interleave_mode::sample
               : charls::interleave_mode::line;
}

/**
 * @brief Determines CharLS color transformation from photometric interpretation.
 */
[[nodiscard]] charls::color_transformation get_color_transform(const image_params& params) {
    if (params.samples_per_pixel == 1) {
        return charls::color_transformation::none;
    }
    // For RGB images, no color transformation is applied
    // (JPEG-LS can optionally use HP color transforms, but DICOM typically doesn't)
    return charls::color_transformation::none;
}

#endif  // PACS_WITH_JPEGLS_CODEC

}  // namespace

/**
 * @brief PIMPL implementation for jpeg_ls_codec.
 */
class jpeg_ls_codec::impl {
public:
    explicit impl(bool lossless, int near_value)
        : lossless_(lossless)
        , near_value_(0) {
        // Handle sentinel value: auto-determine NEAR based on mode
        if (near_value == kAutoNearValue) {
            near_value_ = lossless_ ? kLosslessNearValue : kDefaultNearLosslessValue;
        }
        // Lossless mode always forces NEAR=0 (DICOM compliance)
        else if (lossless_) {
            near_value_ = kLosslessNearValue;
        }
        // Explicit NEAR=0 forces lossless mode
        else if (near_value <= 0) {
            lossless_ = true;
            near_value_ = kLosslessNearValue;
        }
        // Near-lossless with explicit NEAR value
        else {
            near_value_ = std::clamp(near_value, 1, kMaxNearValue);
        }
    }

    [[nodiscard]] bool is_lossless_mode() const noexcept {
        return lossless_;
    }

    [[nodiscard]] int near_value() const noexcept {
        return near_value_;
    }

    [[nodiscard]] codec_result encode(std::span<const uint8_t> pixel_data,
                                       const image_params& params,
                                       const compression_options& options) const {
#ifndef PACS_WITH_JPEGLS_CODEC
        (void)pixel_data;
        (void)params;
        (void)options;
        return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
            "JPEG-LS codec not available: CharLS library not found at build time");
#else
        // Validate input
        if (pixel_data.empty()) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Empty pixel data");
        }

        if (params.width == 0 || params.height == 0) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Invalid image dimensions");
        }

        // Determine effective NEAR value
        int effective_near = near_value_;
        bool use_lossless = lossless_ || options.lossless;

        if (use_lossless) {
            effective_near = 0;
        } else if (!lossless_ && options.quality > 0 && options.quality <= 100) {
            // Map quality (1-100) to NEAR parameter
            // quality 100 -> NEAR 0 (lossless)
            // quality 1 -> NEAR ~10 (more compression)
            effective_near = static_cast<int>((100 - options.quality) * 10 / 100);
            effective_near = std::clamp(effective_near, 0, kMaxNearValue);
        }

        try {
            // Create encoder
            charls::jpegls_encoder encoder;

            // Set frame info
            charls::frame_info frame_info{};
            frame_info.width = params.width;
            frame_info.height = params.height;
            frame_info.bits_per_sample = params.bits_stored;
            frame_info.component_count = params.samples_per_pixel;

            encoder.frame_info(frame_info);

            // Set interleave mode
            encoder.interleave_mode(get_interleave_mode(params));

            // Set NEAR parameter (0 = lossless)
            encoder.near_lossless(effective_near);

            // Set color transformation
            encoder.color_transformation(get_color_transform(params));

            // Calculate destination buffer size
            // For high-entropy data, JPEG-LS output may exceed input size.
            // Use the maximum of: CharLS estimate, input size + 20%, or input + 1KB overhead.
            size_t estimated_size = encoder.estimated_destination_size();
            size_t safe_size = (std::max)({
                estimated_size,
                pixel_data.size() + pixel_data.size() / 5,  // input + 20%
                pixel_data.size() + 1024                     // input + 1KB header overhead
            });
            std::vector<uint8_t> destination(safe_size);

            // Encode
            encoder.destination(destination);
            size_t bytes_written = encoder.encode(pixel_data);

            // Resize to actual size
            destination.resize(bytes_written);

            return pacs::ok<compression_result>(compression_result{std::move(destination), params});

        } catch (const charls::jpegls_error& e) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                std::string("JPEG-LS encoding failed: ") + e.what());
        } catch (const std::exception& e) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                std::string("JPEG-LS encoding failed: ") + e.what());
        }
#endif  // PACS_WITH_JPEGLS_CODEC
    }

    [[nodiscard]] codec_result decode(std::span<const uint8_t> compressed_data,
                                       const image_params& params) const {
#ifndef PACS_WITH_JPEGLS_CODEC
        (void)compressed_data;
        (void)params;
        return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
            "JPEG-LS codec not available: CharLS library not found at build time");
#else
        if (compressed_data.empty()) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Empty compressed data");
        }

        try {
            // Create decoder
            charls::jpegls_decoder decoder;
            decoder.source(compressed_data);

            // Read header to get frame info
            decoder.read_header();

            // Get frame info
            const charls::frame_info& frame_info = decoder.frame_info();

            // Build output parameters
            image_params output_params = params;
            output_params.width = static_cast<uint16_t>(frame_info.width);
            output_params.height = static_cast<uint16_t>(frame_info.height);
            output_params.bits_stored = static_cast<uint16_t>(frame_info.bits_per_sample);
            output_params.bits_allocated = (frame_info.bits_per_sample <= 8) ? 8 : 16;
            output_params.high_bit = output_params.bits_stored - 1;
            output_params.samples_per_pixel = static_cast<uint16_t>(frame_info.component_count);

            // Determine photometric interpretation
            if (frame_info.component_count == 1) {
                output_params.photometric = photometric_interpretation::monochrome2;
            } else if (frame_info.component_count == 3) {
                output_params.photometric = photometric_interpretation::rgb;
            }

            // Validate dimensions if provided
            if (params.width > 0 && params.width != output_params.width) {
                return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Image width mismatch: expected " +
                                            std::to_string(params.width) + ", got " +
                                            std::to_string(output_params.width));
            }
            if (params.height > 0 && params.height != output_params.height) {
                return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Image height mismatch: expected " +
                                            std::to_string(params.height) + ", got " +
                                            std::to_string(output_params.height));
            }

            // Allocate destination buffer
            size_t destination_size = decoder.destination_size();
            std::vector<uint8_t> destination(destination_size);

            // Decode
            decoder.decode(destination);

            // Set output as interleaved
            output_params.planar_configuration = 0;

            return pacs::ok<compression_result>(compression_result{std::move(destination), output_params});

        } catch (const charls::jpegls_error& e) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                std::string("JPEG-LS decoding failed: ") + e.what());
        } catch (const std::exception& e) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                std::string("JPEG-LS decoding failed: ") + e.what());
        }
#endif  // PACS_WITH_JPEGLS_CODEC
    }

private:
    bool lossless_;
    int near_value_;
};

// jpeg_ls_codec implementation

jpeg_ls_codec::jpeg_ls_codec(bool lossless, int near_value)
    : impl_(std::make_unique<impl>(lossless, near_value)) {}

jpeg_ls_codec::~jpeg_ls_codec() = default;

jpeg_ls_codec::jpeg_ls_codec(jpeg_ls_codec&&) noexcept = default;
jpeg_ls_codec& jpeg_ls_codec::operator=(jpeg_ls_codec&&) noexcept = default;

std::string_view jpeg_ls_codec::transfer_syntax_uid() const noexcept {
    return impl_->is_lossless_mode() ? kTransferSyntaxUIDLossless : kTransferSyntaxUIDNearLossless;
}

std::string_view jpeg_ls_codec::name() const noexcept {
    return impl_->is_lossless_mode() ? "JPEG-LS Lossless" : "JPEG-LS Near-Lossless";
}

bool jpeg_ls_codec::is_lossy() const noexcept {
    return !impl_->is_lossless_mode();
}

bool jpeg_ls_codec::can_encode(const image_params& params) const noexcept {
    // JPEG-LS supports 2-16 bit precision per component
    if (params.bits_stored < 2 || params.bits_stored > 16) {
        return false;
    }

    // bits_allocated must be 8 or 16
    if (params.bits_allocated != 8 && params.bits_allocated != 16) {
        return false;
    }

    // Support grayscale (1) and color (3) images
    if (params.samples_per_pixel != 1 && params.samples_per_pixel != 3) {
        return false;
    }

    // Require valid dimensions
    if (params.width == 0 || params.height == 0) {
        return false;
    }

    // Maximum dimension check (JPEG-LS limit)
    if (params.width > 65535 || params.height > 65535) {
        return false;
    }

    return true;
}

bool jpeg_ls_codec::can_decode(const image_params& params) const noexcept {
    // For decoding, we're more lenient as actual parameters come from the bitstream
    // Just validate samples_per_pixel if specified
    if (params.samples_per_pixel != 0 &&
        params.samples_per_pixel != 1 &&
        params.samples_per_pixel != 3) {
        return false;
    }
    return true;
}

bool jpeg_ls_codec::is_lossless_mode() const noexcept {
    return impl_->is_lossless_mode();
}

int jpeg_ls_codec::near_value() const noexcept {
    return impl_->near_value();
}

codec_result jpeg_ls_codec::encode(std::span<const uint8_t> pixel_data,
                                    const image_params& params,
                                    const compression_options& options) const {
    return impl_->encode(pixel_data, params, options);
}

codec_result jpeg_ls_codec::decode(std::span<const uint8_t> compressed_data,
                                    const image_params& params) const {
    return impl_->decode(compressed_data, params);
}

}  // namespace pacs::encoding::compression
