#include "pacs/encoding/compression/jpeg2000_codec.hpp"
#include <pacs/core/result.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>

#ifdef PACS_WITH_JPEG2000_CODEC
#include <openjpeg.h>
#endif

namespace pacs::encoding::compression {

namespace {

#ifdef PACS_WITH_JPEG2000_CODEC

/**
 * @brief Custom error callback for OpenJPEG operations.
 */
void opj_error_callback(const char* msg, void* client_data) {
    auto* error_msg = static_cast<std::string*>(client_data);
    if (error_msg && msg) {
        if (!error_msg->empty()) {
            error_msg->append("; ");
        }
        error_msg->append(msg);
        // Remove trailing newline if present
        while (!error_msg->empty() && error_msg->back() == '\n') {
            error_msg->pop_back();
        }
    }
}

/**
 * @brief Custom warning callback for OpenJPEG operations.
 * Warnings are logged but don't cause failures.
 */
void opj_warning_callback([[maybe_unused]] const char* msg,
                          [[maybe_unused]] void* client_data) {
    // Warnings are silently ignored in production
    // Could be logged via logger_adapter if needed
}

/**
 * @brief Custom info callback for OpenJPEG operations.
 * Info messages are typically suppressed.
 */
void opj_info_callback([[maybe_unused]] const char* msg,
                       [[maybe_unused]] void* client_data) {
    // Info messages are suppressed
}

/**
 * @brief Memory stream structure for OpenJPEG read operations.
 */
struct opj_memory_stream {
    const uint8_t* data;
    size_t size;
    size_t offset;
};

/**
 * @brief Read callback for memory stream.
 */
OPJ_SIZE_T opj_memory_stream_read(void* buffer, OPJ_SIZE_T nb_bytes, void* user_data) {
    auto* stream = static_cast<opj_memory_stream*>(user_data);
    if (!stream || stream->offset >= stream->size) {
        return static_cast<OPJ_SIZE_T>(-1);
    }

    OPJ_SIZE_T bytes_to_read = (std::min)(nb_bytes,
                                         static_cast<OPJ_SIZE_T>(stream->size - stream->offset));
    std::memcpy(buffer, stream->data + stream->offset, bytes_to_read);
    stream->offset += bytes_to_read;
    return bytes_to_read;
}

/**
 * @brief Skip callback for memory stream.
 */
OPJ_OFF_T opj_memory_stream_skip(OPJ_OFF_T nb_bytes, void* user_data) {
    auto* stream = static_cast<opj_memory_stream*>(user_data);
    if (!stream) {
        return -1;
    }

    if (nb_bytes < 0) {
        // Handle negative skip (backward)
        if (stream->offset < static_cast<size_t>(-nb_bytes)) {
            nb_bytes = -static_cast<OPJ_OFF_T>(stream->offset);
        }
    } else {
        // Handle positive skip (forward)
        if (stream->offset + nb_bytes > stream->size) {
            nb_bytes = static_cast<OPJ_OFF_T>(stream->size - stream->offset);
        }
    }

    stream->offset = static_cast<size_t>(static_cast<OPJ_OFF_T>(stream->offset) + nb_bytes);
    return nb_bytes;
}

/**
 * @brief Seek callback for memory stream.
 */
OPJ_BOOL opj_memory_stream_seek(OPJ_OFF_T nb_bytes, void* user_data) {
    auto* stream = static_cast<opj_memory_stream*>(user_data);
    if (!stream || nb_bytes < 0 || static_cast<size_t>(nb_bytes) > stream->size) {
        return OPJ_FALSE;
    }
    stream->offset = static_cast<size_t>(nb_bytes);
    return OPJ_TRUE;
}

/**
 * @brief Output buffer structure for OpenJPEG write operations.
 */
struct opj_output_buffer {
    std::vector<uint8_t> data;
    size_t offset;
};

/**
 * @brief Write callback for output buffer.
 */
OPJ_SIZE_T opj_output_buffer_write(void* buffer, OPJ_SIZE_T nb_bytes, void* user_data) {
    auto* output = static_cast<opj_output_buffer*>(user_data);
    if (!output || !buffer) {
        return static_cast<OPJ_SIZE_T>(-1);
    }

    size_t new_size = output->offset + nb_bytes;
    if (new_size > output->data.size()) {
        output->data.resize(new_size);
    }

    std::memcpy(output->data.data() + output->offset, buffer, nb_bytes);
    output->offset += nb_bytes;
    return nb_bytes;
}

/**
 * @brief Skip callback for output buffer.
 */
OPJ_OFF_T opj_output_buffer_skip(OPJ_OFF_T nb_bytes, void* user_data) {
    auto* output = static_cast<opj_output_buffer*>(user_data);
    if (!output || nb_bytes < 0) {
        return -1;
    }

    size_t new_offset = output->offset + static_cast<size_t>(nb_bytes);
    if (new_offset > output->data.size()) {
        output->data.resize(new_offset);
    }
    output->offset = new_offset;
    return nb_bytes;
}

/**
 * @brief Seek callback for output buffer.
 */
OPJ_BOOL opj_output_buffer_seek(OPJ_OFF_T nb_bytes, void* user_data) {
    auto* output = static_cast<opj_output_buffer*>(user_data);
    if (!output || nb_bytes < 0) {
        return OPJ_FALSE;
    }

    size_t new_offset = static_cast<size_t>(nb_bytes);
    if (new_offset > output->data.size()) {
        output->data.resize(new_offset);
    }
    output->offset = new_offset;
    return OPJ_TRUE;
}

/**
 * @brief Detects JPEG 2000 format from magic bytes.
 * @return OPJ_CODEC_J2K for raw codestream, OPJ_CODEC_JP2 for JP2 format
 */
OPJ_CODEC_FORMAT detect_j2k_format(std::span<const uint8_t> data) {
    if (data.size() < 12) {
        return OPJ_CODEC_J2K;  // Default to raw codestream
    }

    // JP2 signature: 00 00 00 0C 6A 50 20 20 0D 0A 87 0A
    static constexpr uint8_t jp2_signature[] = {
        0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A
    };

    if (std::memcmp(data.data(), jp2_signature, sizeof(jp2_signature)) == 0) {
        return OPJ_CODEC_JP2;
    }

    // J2K codestream signature: FF 4F FF 51 (SOC + SIZ markers)
    if (data[0] == 0xFF && data[1] == 0x4F) {
        return OPJ_CODEC_J2K;
    }

    return OPJ_CODEC_J2K;  // Default
}

#endif  // PACS_WITH_JPEG2000_CODEC

}  // namespace

/**
 * @brief PIMPL implementation for jpeg2000_codec.
 */
class jpeg2000_codec::impl {
public:
    explicit impl(bool lossless, float compression_ratio, int resolution_levels)
        : lossless_(lossless)
        , compression_ratio_(compression_ratio)
        , resolution_levels_(std::clamp(resolution_levels, 1, 32)) {}

    [[nodiscard]] bool is_lossless_mode() const noexcept {
        return lossless_;
    }

    [[nodiscard]] float compression_ratio() const noexcept {
        return compression_ratio_;
    }

    [[nodiscard]] int resolution_levels() const noexcept {
        return resolution_levels_;
    }

    [[nodiscard]] codec_result encode(std::span<const uint8_t> pixel_data,
                                       const image_params& params,
                                       const compression_options& options) const {
#ifndef PACS_WITH_JPEG2000_CODEC
        (void)pixel_data;
        (void)params;
        (void)options;
        return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
            "JPEG 2000 codec not available: OpenJPEG library not found at build time");
#else
        // Determine if we should use lossless mode
        bool use_lossless = lossless_ || options.lossless;

        // Create image component parameters
        std::vector<opj_image_cmptparm_t> cmptparm(params.samples_per_pixel);
        for (uint16_t i = 0; i < params.samples_per_pixel; ++i) {
            std::memset(&cmptparm[i], 0, sizeof(opj_image_cmptparm_t));
            cmptparm[i].prec = params.bits_stored;
            cmptparm[i].bpp = params.bits_stored;
            cmptparm[i].sgnd = params.is_signed() ? 1 : 0;
            cmptparm[i].dx = 1;
            cmptparm[i].dy = 1;
            cmptparm[i].w = params.width;
            cmptparm[i].h = params.height;
        }

        // Determine color space
        OPJ_COLOR_SPACE color_space = OPJ_CLRSPC_GRAY;
        if (params.samples_per_pixel == 3) {
            color_space = (params.photometric == photometric_interpretation::rgb)
                              ? OPJ_CLRSPC_SRGB
                              : OPJ_CLRSPC_SYCC;
        }

        // Create image
        opj_image_t* image = opj_image_create(
            params.samples_per_pixel,
            cmptparm.data(),
            color_space);

        if (!image) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to create OpenJPEG image structure");
        }

        // Set image offset and reference grid
        image->x0 = 0;
        image->y0 = 0;
        image->x1 = params.width;
        image->y1 = params.height;

        // Copy pixel data to image components
        size_t bytes_per_sample = (params.bits_allocated + 7) / 8;
        size_t pixel_count = static_cast<size_t>(params.width) * params.height;

        for (uint16_t c = 0; c < params.samples_per_pixel; ++c) {
            OPJ_INT32* comp_data = image->comps[c].data;

            for (size_t i = 0; i < pixel_count; ++i) {
                size_t src_idx;
                if (params.planar_configuration == 0) {
                    // Interleaved: R0G0B0R1G1B1...
                    src_idx = (i * params.samples_per_pixel + c) * bytes_per_sample;
                } else {
                    // Planar: RRR...GGG...BBB...
                    src_idx = (c * pixel_count + i) * bytes_per_sample;
                }

                if (src_idx + bytes_per_sample > pixel_data.size()) {
                    opj_image_destroy(image);
                    return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Pixel data buffer too small");
                }

                OPJ_INT32 value = 0;
                if (bytes_per_sample == 1) {
                    value = params.is_signed()
                                ? static_cast<OPJ_INT32>(static_cast<int8_t>(pixel_data[src_idx]))
                                : static_cast<OPJ_INT32>(pixel_data[src_idx]);
                } else {
                    // 16-bit (little-endian)
                    uint16_t raw = static_cast<uint16_t>(pixel_data[src_idx]) |
                                   (static_cast<uint16_t>(pixel_data[src_idx + 1]) << 8);
                    value = params.is_signed()
                                ? static_cast<OPJ_INT32>(static_cast<int16_t>(raw))
                                : static_cast<OPJ_INT32>(raw);
                }
                comp_data[i] = value;
            }
        }

        // Create encoder
        opj_codec_t* codec = opj_create_compress(OPJ_CODEC_J2K);
        if (!codec) {
            opj_image_destroy(image);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to create OpenJPEG encoder");
        }

        // Set up error handling
        std::string error_msg;
        opj_set_error_handler(codec, opj_error_callback, &error_msg);
        opj_set_warning_handler(codec, opj_warning_callback, nullptr);
        opj_set_info_handler(codec, opj_info_callback, nullptr);

        // Set encoding parameters
        opj_cparameters_t parameters;
        opj_set_default_encoder_parameters(&parameters);

        parameters.tcp_numlayers = 1;
        parameters.cp_disto_alloc = 1;
        parameters.numresolution = resolution_levels_;

        if (use_lossless) {
            // Lossless: use reversible 5/3 wavelet
            parameters.irreversible = 0;
            parameters.tcp_rates[0] = 0;  // 0 = lossless
        } else {
            // Lossy: use irreversible 9/7 wavelet
            parameters.irreversible = 1;

            // Convert quality (1-100) to compression ratio
            float ratio = compression_ratio_;
            if (options.quality > 0 && options.quality <= 100) {
                // Map quality 100->2 (near lossless), quality 1->100 (high compression)
                ratio = 2.0f + (100.0f - static_cast<float>(options.quality)) * 0.98f;
            }
            parameters.tcp_rates[0] = ratio;
        }

        // Setup encoder with parameters
        if (!opj_setup_encoder(codec, &parameters, image)) {
            opj_destroy_codec(codec);
            opj_image_destroy(image);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to setup OpenJPEG encoder: " + error_msg);
        }

        // Create output stream
        opj_output_buffer output_buf;
        output_buf.offset = 0;
        output_buf.data.reserve(pixel_data.size() / 2);  // Estimate compressed size

        opj_stream_t* stream = opj_stream_default_create(OPJ_FALSE);
        if (!stream) {
            opj_destroy_codec(codec);
            opj_image_destroy(image);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to create OpenJPEG output stream");
        }

        opj_stream_set_user_data(stream, &output_buf, nullptr);
        opj_stream_set_user_data_length(stream, 0);
        opj_stream_set_write_function(stream, opj_output_buffer_write);
        opj_stream_set_skip_function(stream, opj_output_buffer_skip);
        opj_stream_set_seek_function(stream, opj_output_buffer_seek);

        // Encode
        if (!opj_start_compress(codec, image, stream)) {
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            opj_image_destroy(image);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to start JPEG 2000 encoding: " + error_msg);
        }

        if (!opj_encode(codec, stream)) {
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            opj_image_destroy(image);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to encode JPEG 2000 data: " + error_msg);
        }

        if (!opj_end_compress(codec, stream)) {
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            opj_image_destroy(image);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to finalize JPEG 2000 encoding: " + error_msg);
        }

        // Cleanup
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        opj_image_destroy(image);

        // Trim output buffer to actual size
        output_buf.data.resize(output_buf.offset);

        return pacs::ok<compression_result>(compression_result{std::move(output_buf.data), params});
#endif  // PACS_WITH_JPEG2000_CODEC
    }

    [[nodiscard]] codec_result decode(std::span<const uint8_t> compressed_data,
                                       const image_params& params) const {
#ifndef PACS_WITH_JPEG2000_CODEC
        (void)compressed_data;
        (void)params;
        return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
            "JPEG 2000 codec not available: OpenJPEG library not found at build time");
#else
        if (compressed_data.empty()) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Empty compressed data");
        }

        // Detect format
        OPJ_CODEC_FORMAT format = detect_j2k_format(compressed_data);

        // Create decoder
        opj_codec_t* codec = opj_create_decompress(format);
        if (!codec) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to create OpenJPEG decoder");
        }

        // Set up error handling
        std::string error_msg;
        opj_set_error_handler(codec, opj_error_callback, &error_msg);
        opj_set_warning_handler(codec, opj_warning_callback, nullptr);
        opj_set_info_handler(codec, opj_info_callback, nullptr);

        // Set decoding parameters
        opj_dparameters_t parameters;
        opj_set_default_decoder_parameters(&parameters);

        if (!opj_setup_decoder(codec, &parameters)) {
            opj_destroy_codec(codec);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to setup OpenJPEG decoder: " + error_msg);
        }

        // Create input stream
        opj_memory_stream input_stream;
        input_stream.data = compressed_data.data();
        input_stream.size = compressed_data.size();
        input_stream.offset = 0;

        opj_stream_t* stream = opj_stream_default_create(OPJ_TRUE);
        if (!stream) {
            opj_destroy_codec(codec);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to create OpenJPEG input stream");
        }

        opj_stream_set_user_data(stream, &input_stream, nullptr);
        opj_stream_set_user_data_length(stream, compressed_data.size());
        opj_stream_set_read_function(stream, opj_memory_stream_read);
        opj_stream_set_skip_function(stream, opj_memory_stream_skip);
        opj_stream_set_seek_function(stream, opj_memory_stream_seek);

        // Read header
        opj_image_t* image = nullptr;
        if (!opj_read_header(stream, codec, &image)) {
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to read JPEG 2000 header: " + error_msg);
        }

        // Decode
        if (!opj_decode(codec, stream, image)) {
            opj_image_destroy(image);
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to decode JPEG 2000 data: " + error_msg);
        }

        if (!opj_end_decompress(codec, stream)) {
            opj_image_destroy(image);
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Failed to finalize JPEG 2000 decoding: " + error_msg);
        }

        // Extract image parameters from decoded data
        image_params output_params = params;
        output_params.width = static_cast<uint16_t>(image->x1 - image->x0);
        output_params.height = static_cast<uint16_t>(image->y1 - image->y0);
        output_params.samples_per_pixel = static_cast<uint16_t>(image->numcomps);

        if (image->numcomps > 0) {
            output_params.bits_stored = static_cast<uint16_t>(image->comps[0].prec);
            output_params.bits_allocated = (output_params.bits_stored <= 8) ? 8 : 16;
            output_params.high_bit = output_params.bits_stored - 1;
            output_params.pixel_representation = image->comps[0].sgnd ? 1 : 0;
        }

        // Determine photometric interpretation
        if (image->numcomps == 1) {
            output_params.photometric = photometric_interpretation::monochrome2;
        } else if (image->numcomps == 3) {
            output_params.photometric = (image->color_space == OPJ_CLRSPC_SRGB)
                                            ? photometric_interpretation::rgb
                                            : photometric_interpretation::ycbcr_full;
        }

        // Validate dimensions if provided
        if (params.width > 0 && params.width != output_params.width) {
            opj_image_destroy(image);
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Image width mismatch: expected " +
                                        std::to_string(params.width) + ", got " +
                                        std::to_string(output_params.width));
        }
        if (params.height > 0 && params.height != output_params.height) {
            opj_image_destroy(image);
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Image height mismatch: expected " +
                                        std::to_string(params.height) + ", got " +
                                        std::to_string(output_params.height));
        }

        // Allocate output buffer
        size_t pixel_count = static_cast<size_t>(output_params.width) * output_params.height;
        size_t bytes_per_sample = (output_params.bits_allocated + 7) / 8;
        size_t total_size = pixel_count * output_params.samples_per_pixel * bytes_per_sample;

        std::vector<uint8_t> output_data(total_size);

        // Copy decoded data to output buffer (interleaved format)
        for (OPJ_UINT32 c = 0; c < image->numcomps; ++c) {
            const OPJ_INT32* comp_data = image->comps[c].data;

            for (size_t i = 0; i < pixel_count; ++i) {
                size_t dst_idx = (i * output_params.samples_per_pixel + c) * bytes_per_sample;
                OPJ_INT32 value = comp_data[i];

                if (bytes_per_sample == 1) {
                    output_data[dst_idx] = static_cast<uint8_t>(value & 0xFF);
                } else {
                    // 16-bit little-endian
                    output_data[dst_idx] = static_cast<uint8_t>(value & 0xFF);
                    output_data[dst_idx + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
                }
            }
        }

        // Cleanup
        opj_image_destroy(image);
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);

        output_params.planar_configuration = 0;  // Output is always interleaved
        return pacs::ok<compression_result>(compression_result{std::move(output_data), output_params});
#endif  // PACS_WITH_JPEG2000_CODEC
    }

private:
    bool lossless_;
    float compression_ratio_;
    int resolution_levels_;
};

// jpeg2000_codec implementation

jpeg2000_codec::jpeg2000_codec(bool lossless,
                               float compression_ratio,
                               int resolution_levels)
    : impl_(std::make_unique<impl>(lossless, compression_ratio, resolution_levels)) {}

jpeg2000_codec::~jpeg2000_codec() = default;

jpeg2000_codec::jpeg2000_codec(jpeg2000_codec&&) noexcept = default;
jpeg2000_codec& jpeg2000_codec::operator=(jpeg2000_codec&&) noexcept = default;

std::string_view jpeg2000_codec::transfer_syntax_uid() const noexcept {
    return impl_->is_lossless_mode() ? kTransferSyntaxUIDLossless : kTransferSyntaxUIDLossy;
}

std::string_view jpeg2000_codec::name() const noexcept {
    return impl_->is_lossless_mode() ? "JPEG 2000 Lossless" : "JPEG 2000";
}

bool jpeg2000_codec::is_lossy() const noexcept {
    return !impl_->is_lossless_mode();
}

bool jpeg2000_codec::can_encode(const image_params& params) const noexcept {
    // JPEG 2000 supports wide range of bit depths
    if (params.bits_stored < 1 || params.bits_stored > 16) {
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

    return true;
}

bool jpeg2000_codec::can_decode(const image_params& params) const noexcept {
    // For decoding, we're more lenient as actual parameters come from the codestream
    // Just validate samples_per_pixel if specified
    if (params.samples_per_pixel != 0 &&
        params.samples_per_pixel != 1 &&
        params.samples_per_pixel != 3) {
        return false;
    }
    return true;
}

bool jpeg2000_codec::is_lossless_mode() const noexcept {
    return impl_->is_lossless_mode();
}

float jpeg2000_codec::compression_ratio() const noexcept {
    return impl_->compression_ratio();
}

int jpeg2000_codec::resolution_levels() const noexcept {
    return impl_->resolution_levels();
}

codec_result jpeg2000_codec::encode(std::span<const uint8_t> pixel_data,
                                     const image_params& params,
                                     const compression_options& options) const {
    return impl_->encode(pixel_data, params, options);
}

codec_result jpeg2000_codec::decode(std::span<const uint8_t> compressed_data,
                                     const image_params& params) const {
    return impl_->decode(compressed_data, params);
}

}  // namespace pacs::encoding::compression
