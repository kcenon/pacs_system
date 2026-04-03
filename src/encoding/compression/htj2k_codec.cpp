// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "pacs/encoding/compression/htj2k_codec.hpp"
#include <pacs/core/result.hpp>

#include <algorithm>
#include <cstring>

#ifdef PACS_WITH_HTJ2K_CODEC
#include <openjph/ojph_codestream.h>
#include <openjph/ojph_file.h>
#include <openjph/ojph_mem.h>
#include <openjph/ojph_params.h>
#endif

namespace kcenon::pacs::encoding::compression {

htj2k_codec::htj2k_codec(bool lossless,
                           bool use_rpcl,
                           float compression_ratio,
                           int resolution_levels)
    : lossless_(lossless),
      use_rpcl_(use_rpcl),
      compression_ratio_(compression_ratio),
      resolution_levels_(resolution_levels) {}

htj2k_codec::~htj2k_codec() = default;

htj2k_codec::htj2k_codec(htj2k_codec&&) noexcept = default;
htj2k_codec& htj2k_codec::operator=(htj2k_codec&&) noexcept = default;

std::string_view htj2k_codec::transfer_syntax_uid() const noexcept {
    if (lossless_) {
        return use_rpcl_ ? kTransferSyntaxUIDRPCL : kTransferSyntaxUIDLossless;
    }
    return kTransferSyntaxUIDLossy;
}

std::string_view htj2k_codec::name() const noexcept {
    if (lossless_) {
        return use_rpcl_ ? "HTJ2K with RPCL (Lossless)" : "HTJ2K (Lossless)";
    }
    return "HTJ2K (Lossy)";
}

bool htj2k_codec::is_lossy() const noexcept {
    return !lossless_;
}

bool htj2k_codec::can_encode(const image_params& params) const noexcept {
    if (params.width == 0 || params.height == 0) {
        return false;
    }

    if (params.samples_per_pixel != 1 && params.samples_per_pixel != 3) {
        return false;
    }

    if (params.bits_stored < 1 || params.bits_stored > 16) {
        return false;
    }

    return true;
}

bool htj2k_codec::can_decode(const image_params& params) const noexcept {
    return can_encode(params);
}

bool htj2k_codec::is_lossless_mode() const noexcept {
    return lossless_;
}

bool htj2k_codec::is_rpcl_mode() const noexcept {
    return use_rpcl_;
}

float htj2k_codec::compression_ratio() const noexcept {
    return compression_ratio_;
}

int htj2k_codec::resolution_levels() const noexcept {
    return resolution_levels_;
}

#ifdef PACS_WITH_HTJ2K_CODEC

namespace {

// Clamp resolution levels to valid range for image dimensions
int compute_resolution_levels(int requested, uint16_t width, uint16_t height) {
    int max_levels = 1;
    auto min_dim = std::min(width, height);
    while (min_dim > 1 && max_levels < requested) {
        min_dim >>= 1;
        ++max_levels;
    }
    return std::clamp(max_levels, 1, 32);
}

}  // namespace

codec_result htj2k_codec::encode(
    std::span<const uint8_t> pixel_data,
    const image_params& params,
    [[maybe_unused]] const compression_options& options) const {

    if (pixel_data.empty()) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::compression_error, "Empty pixel data");
    }

    if (params.width == 0 || params.height == 0) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::compression_error, "Invalid image dimensions");
    }

    const int bytes_per_sample = (params.bits_stored <= 8) ? 1 : 2;
    const size_t expected_size = static_cast<size_t>(params.width)
        * params.height * params.samples_per_pixel * bytes_per_sample;
    if (pixel_data.size() < expected_size) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::compression_error,
            "Pixel data too small: expected " + std::to_string(expected_size)
            + " bytes, got " + std::to_string(pixel_data.size()));
    }

    try {
        ojph::codestream codestream;

        // Configure SIZ marker (image dimensions and components)
        ojph::param_siz siz = codestream.access_siz();
        siz.set_image_extent(ojph::point{
            static_cast<ojph::ui32>(params.width),
            static_cast<ojph::ui32>(params.height)});
        siz.set_image_offset(ojph::point{0, 0});
        siz.set_tile_size(ojph::size{0, 0});
        siz.set_tile_offset(ojph::point{0, 0});
        siz.set_num_components(params.samples_per_pixel);

        bool is_signed = (params.pixel_representation != 0);
        for (ojph::ui32 c = 0; c < params.samples_per_pixel; ++c) {
            siz.set_component(c, ojph::point{1, 1}, params.bits_stored, is_signed);
        }

        // Configure COD marker (coding parameters)
        ojph::param_cod cod = codestream.access_cod();
        int num_levels = compute_resolution_levels(
            resolution_levels_, params.width, params.height);
        cod.set_num_decomposition(static_cast<ojph::ui32>(num_levels));

        // Block dimensions (64x64 is standard for HTJ2K)
        cod.set_block_dims(64, 64);

        // Reversible for lossless, irreversible for lossy
        cod.set_reversible(lossless_);

        // Color transform for multi-component images
        cod.set_color_transform(params.samples_per_pixel == 3);

        // Progression order
        if (use_rpcl_) {
            cod.set_progression_order("RPCL");
        } else {
            cod.set_progression_order("CPRL");
        }

        // For lossy mode, set quantization step
        if (!lossless_) {
            ojph::param_qcd qcd = codestream.access_qcd();
            // Use a quality-preserving delta scaled by compression ratio.
            // JPEG 2000 quantization cascades across subbands, so the
            // base delta must be small enough for acceptable reconstruction.
            float delta = compression_ratio_ / 10000.0f;
            qcd.set_irrev_quant(delta);
        }

        // Set planar mode based on component count
        // Planar = process one component fully before the next
        // Non-planar needed when color transform is used
        codestream.set_planar(params.samples_per_pixel == 1);

        // Write to memory buffer
        ojph::mem_outfile output;
        output.open();

        codestream.write_headers(&output);

        // Push pixel data row by row
        const auto width = static_cast<ojph::ui32>(params.width);
        const auto height = static_cast<ojph::ui32>(params.height);
        const auto num_comps = static_cast<ojph::ui32>(params.samples_per_pixel);

        ojph::ui32 next_comp = 0;
        ojph::line_buf* line = codestream.exchange(nullptr, next_comp);

        for (ojph::ui32 y = 0; y < height; ++y) {
            for (ojph::ui32 c = 0; c < num_comps; ++c) {
                // Fill line buffer with pixel data for this row/component.
                // OpenJPH uses i32 at the API boundary for both lossless and
                // lossy modes; the library handles float conversion internally
                // during the irreversible wavelet transform.
                auto* dst = line->i32;
                if (bytes_per_sample == 1) {
                    if (num_comps == 1) {
                        const uint8_t* src = pixel_data.data()
                            + static_cast<size_t>(y) * width;
                        for (ojph::ui32 x = 0; x < width; ++x) {
                            dst[x] = is_signed
                                ? static_cast<ojph::si32>(static_cast<int8_t>(src[x]))
                                : static_cast<ojph::si32>(src[x]);
                        }
                    } else {
                        const uint8_t* src = pixel_data.data()
                            + static_cast<size_t>(y) * width * num_comps;
                        for (ojph::ui32 x = 0; x < width; ++x) {
                            dst[x] = is_signed
                                ? static_cast<ojph::si32>(
                                    static_cast<int8_t>(src[x * num_comps + next_comp]))
                                : static_cast<ojph::si32>(src[x * num_comps + next_comp]);
                        }
                    }
                } else {
                    if (num_comps == 1) {
                        const auto* src = reinterpret_cast<const uint16_t*>(
                            pixel_data.data() + static_cast<size_t>(y) * width * 2);
                        for (ojph::ui32 x = 0; x < width; ++x) {
                            dst[x] = is_signed
                                ? static_cast<ojph::si32>(static_cast<int16_t>(src[x]))
                                : static_cast<ojph::si32>(src[x]);
                        }
                    } else {
                        const auto* src = reinterpret_cast<const uint16_t*>(
                            pixel_data.data()
                            + static_cast<size_t>(y) * width * num_comps * 2);
                        for (ojph::ui32 x = 0; x < width; ++x) {
                            dst[x] = is_signed
                                ? static_cast<ojph::si32>(
                                    static_cast<int16_t>(src[x * num_comps + next_comp]))
                                : static_cast<ojph::si32>(src[x * num_comps + next_comp]);
                        }
                    }
                }

                line = codestream.exchange(line, next_comp);
            }
        }

        codestream.flush();

        // Copy compressed data BEFORE close (close deallocates the buffer)
        auto compressed_size = static_cast<size_t>(output.tell());
        std::vector<uint8_t> result_data(compressed_size);
        if (compressed_size > 0) {
            std::memcpy(result_data.data(), output.get_data(), compressed_size);
        }

        codestream.close();

        return kcenon::pacs::ok<compression_result>(
            compression_result{std::move(result_data), params});

    } catch (const std::exception& e) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::compression_error,
            std::string("HTJ2K encoding failed: ") + e.what());
    }
}

codec_result htj2k_codec::decode(
    std::span<const uint8_t> compressed_data,
    const image_params& params) const {

    if (compressed_data.empty()) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::decompression_error, "Empty compressed data");
    }

    try {
        ojph::codestream codestream;

        // Read from memory
        ojph::mem_infile input;
        input.open(compressed_data.data(), compressed_data.size());

        codestream.read_headers(&input);

        // Get image information from the codestream headers
        ojph::param_siz siz = codestream.access_siz();
        ojph::point extent = siz.get_image_extent();
        auto decoded_width = static_cast<uint16_t>(extent.x);
        auto decoded_height = static_cast<uint16_t>(extent.y);
        auto num_comps = static_cast<uint16_t>(siz.get_num_components());
        auto bit_depth = static_cast<uint16_t>(siz.get_bit_depth(0));
        bool is_signed = siz.is_signed(0);

        // Build output parameters
        image_params output_params = params;
        output_params.width = decoded_width;
        output_params.height = decoded_height;
        output_params.samples_per_pixel = num_comps;
        output_params.bits_stored = bit_depth;
        output_params.bits_allocated = (bit_depth <= 8) ? 8 : 16;
        output_params.high_bit = bit_depth - 1;
        output_params.pixel_representation = is_signed ? 1 : 0;
        output_params.planar_configuration = 0;

        if (num_comps == 1) {
            output_params.photometric = photometric_interpretation::monochrome2;
        } else if (num_comps == 3) {
            output_params.photometric = photometric_interpretation::rgb;
        }

        // Validate dimensions if provided
        if (params.width > 0 && params.width != decoded_width) {
            return kcenon::pacs::pacs_error<compression_result>(
                kcenon::pacs::error_codes::decompression_error,
                "Image width mismatch: expected " + std::to_string(params.width)
                + ", got " + std::to_string(decoded_width));
        }
        if (params.height > 0 && params.height != decoded_height) {
            return kcenon::pacs::pacs_error<compression_result>(
                kcenon::pacs::error_codes::decompression_error,
                "Image height mismatch: expected " + std::to_string(params.height)
                + ", got " + std::to_string(decoded_height));
        }

        // Set planar mode matching the encoder
        codestream.set_planar(num_comps == 1);

        codestream.create();

        // Allocate output buffer
        const int bytes_per_sample = (bit_depth <= 8) ? 1 : 2;
        const size_t output_size = static_cast<size_t>(decoded_width)
            * decoded_height * num_comps * bytes_per_sample;
        std::vector<uint8_t> output_data(output_size);

        // Pull decoded data row by row.
        // OpenJPH returns i32 from pull() for both reversible and
        // irreversible modes (internal float-to-int conversion is done).
        ojph::ui32 comp_num = 0;
        for (ojph::ui32 y = 0; y < decoded_height; ++y) {
            for (ojph::ui32 c = 0; c < num_comps; ++c) {
                ojph::line_buf* line = codestream.pull(comp_num);

                if (bytes_per_sample == 1) {
                    if (num_comps == 1) {
                        auto* dst = output_data.data()
                            + static_cast<size_t>(y) * decoded_width;
                        for (ojph::ui32 x = 0; x < decoded_width; ++x) {
                            auto val = line->i32[x];
                            dst[x] = static_cast<uint8_t>(std::clamp(val, 0, 255));
                        }
                    } else {
                        auto* dst = output_data.data()
                            + static_cast<size_t>(y) * decoded_width * num_comps;
                        for (ojph::ui32 x = 0; x < decoded_width; ++x) {
                            auto val = line->i32[x];
                            dst[x * num_comps + comp_num] =
                                static_cast<uint8_t>(std::clamp(val, 0, 255));
                        }
                    }
                } else {
                    if (num_comps == 1) {
                        auto* dst = reinterpret_cast<uint16_t*>(
                            output_data.data()
                            + static_cast<size_t>(y) * decoded_width * 2);
                        for (ojph::ui32 x = 0; x < decoded_width; ++x) {
                            auto val = line->i32[x];
                            if (is_signed) {
                                dst[x] = static_cast<uint16_t>(
                                    static_cast<int16_t>(
                                        std::clamp(val, -32768, 32767)));
                            } else {
                                dst[x] = static_cast<uint16_t>(
                                    std::clamp(val, 0, 65535));
                            }
                        }
                    } else {
                        auto* dst = reinterpret_cast<uint16_t*>(
                            output_data.data()
                            + static_cast<size_t>(y) * decoded_width * num_comps * 2);
                        for (ojph::ui32 x = 0; x < decoded_width; ++x) {
                            auto val = line->i32[x];
                            if (is_signed) {
                                dst[x * num_comps + comp_num] =
                                    static_cast<uint16_t>(
                                        static_cast<int16_t>(
                                            std::clamp(val, -32768, 32767)));
                            } else {
                                dst[x * num_comps + comp_num] =
                                    static_cast<uint16_t>(
                                        std::clamp(val, 0, 65535));
                            }
                        }
                    }
                }
            }
        }

        codestream.close();

        return kcenon::pacs::ok<compression_result>(
            compression_result{std::move(output_data), output_params});

    } catch (const std::exception& e) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::decompression_error,
            std::string("HTJ2K decoding failed: ") + e.what());
    }
}

#else  // !PACS_WITH_HTJ2K_CODEC

codec_result htj2k_codec::encode(
    [[maybe_unused]] std::span<const uint8_t> pixel_data,
    [[maybe_unused]] const image_params& params,
    [[maybe_unused]] const compression_options& options) const {
    return kcenon::pacs::pacs_error<compression_result>(
        kcenon::pacs::error_codes::compression_error,
        "HTJ2K codec not available: OpenJPH library not found at build time");
}

codec_result htj2k_codec::decode(
    [[maybe_unused]] std::span<const uint8_t> compressed_data,
    [[maybe_unused]] const image_params& params) const {
    return kcenon::pacs::pacs_error<compression_result>(
        kcenon::pacs::error_codes::decompression_error,
        "HTJ2K codec not available: OpenJPH library not found at build time");
}

#endif  // PACS_WITH_HTJ2K_CODEC

}  // namespace kcenon::pacs::encoding::compression
