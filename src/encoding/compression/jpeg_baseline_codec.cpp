#include "pacs/encoding/compression/jpeg_baseline_codec.hpp"

#include <pacs/core/result.hpp>

#include <algorithm>
#include <csetjmp>
#include <cstdio>
#include <stdexcept>

// libjpeg-turbo headers
#include <jpeglib.h>
#include <jerror.h>

namespace pacs::encoding::compression {

namespace {

/**
 * @brief Custom error handler for libjpeg that uses exceptions.
 *
 * libjpeg uses setjmp/longjmp for error handling by default.
 * This structure extends jpeg_error_mgr to allow C++ exception handling.
 */
struct jpeg_error_handler {
    jpeg_error_mgr pub;          // Public fields (must be first)
    jmp_buf setjmp_buffer;       // For return to caller
    std::string error_message;   // Captured error message
};

/**
 * @brief Error exit handler that stores the message and longjmps.
 */
void jpeg_error_exit(j_common_ptr cinfo) {
    auto* err = reinterpret_cast<jpeg_error_handler*>(cinfo->err);

    // Format error message
    char buffer[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, buffer);
    err->error_message = buffer;

    // Return control to the setjmp point
    std::longjmp(err->setjmp_buffer, 1);
}

/**
 * @brief Suppress output messages (we capture via error_exit).
 */
void jpeg_output_message([[maybe_unused]] j_common_ptr cinfo) {
    // Silently ignore warning messages
}

/**
 * @brief RAII wrapper for jpeg_compress_struct.
 */
class jpeg_compressor {
public:
    jpeg_compressor() {
        cinfo_.err = jpeg_std_error(&jerr_.pub);
        jerr_.pub.error_exit = jpeg_error_exit;
        jerr_.pub.output_message = jpeg_output_message;

        jpeg_create_compress(&cinfo_);
    }

    ~jpeg_compressor() {
        jpeg_destroy_compress(&cinfo_);
    }

    jpeg_compressor(const jpeg_compressor&) = delete;
    jpeg_compressor& operator=(const jpeg_compressor&) = delete;

    jpeg_compress_struct* operator->() { return &cinfo_; }
    jpeg_compress_struct& get() { return cinfo_; }
    jpeg_error_handler& error() { return jerr_; }

private:
    jpeg_compress_struct cinfo_{};
    jpeg_error_handler jerr_{};
};

/**
 * @brief RAII wrapper for jpeg_decompress_struct.
 */
class jpeg_decompressor {
public:
    jpeg_decompressor() {
        cinfo_.err = jpeg_std_error(&jerr_.pub);
        jerr_.pub.error_exit = jpeg_error_exit;
        jerr_.pub.output_message = jpeg_output_message;

        jpeg_create_decompress(&cinfo_);
    }

    ~jpeg_decompressor() {
        jpeg_destroy_decompress(&cinfo_);
    }

    jpeg_decompressor(const jpeg_decompressor&) = delete;
    jpeg_decompressor& operator=(const jpeg_decompressor&) = delete;

    jpeg_decompress_struct* operator->() { return &cinfo_; }
    jpeg_decompress_struct& get() { return cinfo_; }
    jpeg_error_handler& error() { return jerr_; }

private:
    jpeg_decompress_struct cinfo_{};
    jpeg_error_handler jerr_{};
};

// Helper function for creating codec errors
codec_result make_compression_error(const std::string& message) {
    return pacs::pacs_error<compression_result>(
        pacs::error_codes::compression_error, message);
}

codec_result make_decompression_error(const std::string& message) {
    return pacs::pacs_error<compression_result>(
        pacs::error_codes::decompression_error, message);
}

codec_result make_compression_ok(std::vector<uint8_t> data, const image_params& params) {
    return pacs::ok<compression_result>(compression_result{std::move(data), params});
}

}  // namespace

/**
 * @brief PIMPL implementation for jpeg_baseline_codec.
 *
 * Encapsulates libjpeg-turbo usage details.
 */
class jpeg_baseline_codec::impl {
public:
    impl() = default;
    ~impl() = default;

    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options) const {

        if (pixel_data.empty()) {
            return make_compression_error("Empty pixel data");
        }

        if (!params.valid_for_jpeg_baseline()) {
            return make_compression_error(
                "Invalid parameters for JPEG Baseline: requires 8-bit depth");
        }

        size_t expected_size = params.frame_size_bytes();
        if (pixel_data.size() != expected_size) {
            return make_compression_error(
                "Pixel data size mismatch: expected " + std::to_string(expected_size) +
                ", got " + std::to_string(pixel_data.size()));
        }

        jpeg_compressor compressor;

        // Setup error handling with setjmp
        if (setjmp(compressor.error().setjmp_buffer)) {
            return make_compression_error(
                "JPEG compression failed: " + compressor.error().error_message);
        }

        // Configure memory destination
        uint8_t* out_buffer = nullptr;
        unsigned long out_size = 0;
        jpeg_mem_dest(&compressor.get(), &out_buffer, &out_size);

        // Set image parameters
        compressor->image_width = params.width;
        compressor->image_height = params.height;
        compressor->input_components = static_cast<int>(params.samples_per_pixel);

        if (params.is_grayscale()) {
            compressor->in_color_space = JCS_GRAYSCALE;
        } else {
            // Assume input is RGB, libjpeg will convert to YCbCr
            compressor->in_color_space = JCS_RGB;
        }

        jpeg_set_defaults(&compressor.get());

        // Apply quality setting (1-100)
        int quality = std::clamp(options.quality, 1, 100);
        jpeg_set_quality(&compressor.get(), quality, TRUE);

        // Configure chroma subsampling for color images
        if (!params.is_grayscale()) {
            switch (options.chroma_subsampling) {
                case 0:  // 4:4:4
                    compressor->comp_info[0].h_samp_factor = 1;
                    compressor->comp_info[0].v_samp_factor = 1;
                    compressor->comp_info[1].h_samp_factor = 1;
                    compressor->comp_info[1].v_samp_factor = 1;
                    compressor->comp_info[2].h_samp_factor = 1;
                    compressor->comp_info[2].v_samp_factor = 1;
                    break;
                case 1:  // 4:2:2
                    compressor->comp_info[0].h_samp_factor = 2;
                    compressor->comp_info[0].v_samp_factor = 1;
                    compressor->comp_info[1].h_samp_factor = 1;
                    compressor->comp_info[1].v_samp_factor = 1;
                    compressor->comp_info[2].h_samp_factor = 1;
                    compressor->comp_info[2].v_samp_factor = 1;
                    break;
                case 2:  // 4:2:0 (default)
                default:
                    compressor->comp_info[0].h_samp_factor = 2;
                    compressor->comp_info[0].v_samp_factor = 2;
                    compressor->comp_info[1].h_samp_factor = 1;
                    compressor->comp_info[1].v_samp_factor = 1;
                    compressor->comp_info[2].h_samp_factor = 1;
                    compressor->comp_info[2].v_samp_factor = 1;
                    break;
            }
        }

        // Start compression
        jpeg_start_compress(&compressor.get(), TRUE);

        // Process scanlines
        JDIMENSION row_stride = params.width * params.samples_per_pixel;
        std::vector<JSAMPROW> row_pointers(params.height);

        for (JDIMENSION row = 0; row < params.height; ++row) {
            row_pointers[row] = const_cast<JSAMPROW>(
                pixel_data.data() + row * row_stride);
        }

        // Write all scanlines at once
        jpeg_write_scanlines(&compressor.get(), row_pointers.data(),
                            static_cast<JDIMENSION>(params.height));

        // Finish compression
        jpeg_finish_compress(&compressor.get());

        // Copy output to result
        std::vector<uint8_t> result(out_buffer, out_buffer + out_size);

        // Free libjpeg-allocated buffer
        std::free(out_buffer);

        // Create output params (same as input for compression)
        image_params output_params = params;

        return make_compression_ok(std::move(result), output_params);
    }

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const {

        if (compressed_data.empty()) {
            return make_decompression_error("Empty compressed data");
        }

        jpeg_decompressor decompressor;

        // Setup error handling with setjmp
        if (setjmp(decompressor.error().setjmp_buffer)) {
            return make_decompression_error(
                "JPEG decompression failed: " + decompressor.error().error_message);
        }

        // Setup memory source
        // Note: Some libjpeg versions (e.g., Mono.framework on macOS) declare
        // jpeg_mem_src with non-const buffer parameter. Use const_cast for compatibility.
        // The buffer is only read, not modified, so this is safe.
        jpeg_mem_src(&decompressor.get(),
                     const_cast<unsigned char*>(compressed_data.data()),
                     static_cast<unsigned long>(compressed_data.size()));

        // Read JPEG header
        int header_result = jpeg_read_header(&decompressor.get(), TRUE);
        if (header_result != JPEG_HEADER_OK) {
            return make_decompression_error("Invalid JPEG header");
        }

        // Validate dimensions if provided
        if (params.width > 0 && decompressor->image_width != params.width) {
            return make_decompression_error(
                "Image width mismatch: expected " + std::to_string(params.width) +
                ", got " + std::to_string(decompressor->image_width));
        }
        if (params.height > 0 && decompressor->image_height != params.height) {
            return make_decompression_error(
                "Image height mismatch: expected " + std::to_string(params.height) +
                ", got " + std::to_string(decompressor->image_height));
        }

        // Request RGB output for color images
        if (decompressor->num_components == 3) {
            decompressor->out_color_space = JCS_RGB;
        }

        // Start decompression
        jpeg_start_decompress(&decompressor.get());

        // Allocate output buffer
        JDIMENSION row_stride = decompressor->output_width *
                                static_cast<JDIMENSION>(decompressor->output_components);
        size_t output_size = static_cast<size_t>(row_stride) * decompressor->output_height;
        std::vector<uint8_t> output(output_size);

        // Read scanlines
        while (decompressor->output_scanline < decompressor->output_height) {
            JSAMPROW row = output.data() +
                          decompressor->output_scanline * row_stride;
            jpeg_read_scanlines(&decompressor.get(), &row, 1);
        }

        // Finish decompression
        jpeg_finish_decompress(&decompressor.get());

        // Build output parameters from JPEG header info
        image_params output_params;
        output_params.width = static_cast<uint16_t>(decompressor->output_width);
        output_params.height = static_cast<uint16_t>(decompressor->output_height);
        output_params.bits_allocated = 8;
        output_params.bits_stored = 8;
        output_params.high_bit = 7;
        output_params.samples_per_pixel = static_cast<uint16_t>(decompressor->output_components);
        output_params.planar_configuration = 0;  // Interleaved
        output_params.pixel_representation = 0;  // Unsigned

        if (output_params.samples_per_pixel == 1) {
            output_params.photometric = photometric_interpretation::monochrome2;
        } else {
            output_params.photometric = photometric_interpretation::rgb;
        }

        return make_compression_ok(std::move(output), output_params);
    }
};

// jpeg_baseline_codec implementation

jpeg_baseline_codec::jpeg_baseline_codec()
    : impl_(std::make_unique<impl>()) {}

jpeg_baseline_codec::~jpeg_baseline_codec() = default;

jpeg_baseline_codec::jpeg_baseline_codec(jpeg_baseline_codec&&) noexcept = default;

jpeg_baseline_codec& jpeg_baseline_codec::operator=(jpeg_baseline_codec&&) noexcept = default;

std::string_view jpeg_baseline_codec::transfer_syntax_uid() const noexcept {
    return kTransferSyntaxUID;
}

std::string_view jpeg_baseline_codec::name() const noexcept {
    return "JPEG Baseline (Process 1)";
}

bool jpeg_baseline_codec::is_lossy() const noexcept {
    return true;
}

bool jpeg_baseline_codec::can_encode(const image_params& params) const noexcept {
    return params.valid_for_jpeg_baseline();
}

bool jpeg_baseline_codec::can_decode(const image_params& params) const noexcept {
    // Can decode any 8-bit grayscale or color JPEG
    // Width/height can be 0 (unknown) for decode - will read from JPEG header
    if (params.bits_allocated != 0 && params.bits_allocated != 8) return false;
    if (params.samples_per_pixel != 0 &&
        params.samples_per_pixel != 1 &&
        params.samples_per_pixel != 3) {
        return false;
    }
    return true;
}

codec_result jpeg_baseline_codec::encode(
    std::span<const uint8_t> pixel_data,
    const image_params& params,
    const compression_options& options) const {
    return impl_->encode(pixel_data, params, options);
}

codec_result jpeg_baseline_codec::decode(
    std::span<const uint8_t> compressed_data,
    const image_params& params) const {
    return impl_->decode(compressed_data, params);
}

}  // namespace pacs::encoding::compression
