#include "pacs/encoding/compression/rle_codec.hpp"
#include <pacs/core/result.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace pacs::encoding::compression {

namespace {

/**
 * @brief Reads a 32-bit little-endian value from buffer.
 */
inline uint32_t read_le32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

/**
 * @brief Writes a 32-bit little-endian value to buffer.
 */
inline void write_le32(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

/**
 * @brief Encodes a single RLE segment using PackBits algorithm.
 *
 * DICOM RLE uses a variant of PackBits encoding:
 * - Control byte N:
 *   - 0 <= N <= 127: Copy next N+1 bytes literally
 *   - -128 <= N <= -1: Repeat next byte (1-N) times (i.e., 2 to 129 copies)
 *   - N = -128: No operation (reserved)
 *
 * @param input Input byte array
 * @param output Output vector to append encoded data
 */
void encode_rle_segment(std::span<const uint8_t> input, std::vector<uint8_t>& output) {
    if (input.empty()) {
        return;
    }

    size_t pos = 0;
    const size_t size = input.size();

    while (pos < size) {
        // Look for runs of identical bytes
        uint8_t current = input[pos];
        size_t run_length = 1;

        while (pos + run_length < size &&
               input[pos + run_length] == current &&
               run_length < 128) {
            ++run_length;
        }

        if (run_length >= 3) {
            // Encode as a run (replicate)
            // Control byte = 1 - run_length (range: -127 to -2 for lengths 3-128)
            output.push_back(static_cast<uint8_t>(1 - static_cast<int>(run_length)));
            output.push_back(current);
            pos += run_length;
        } else {
            // Collect literal bytes (non-repeating or short runs)
            std::vector<uint8_t> literal;
            literal.reserve(128);

            while (pos < size && literal.size() < 128) {
                // Check if a run of 3+ starts here
                size_t ahead_run = 1;
                while (pos + ahead_run < size &&
                       input[pos + ahead_run] == input[pos] &&
                       ahead_run < 3) {
                    ++ahead_run;
                }

                if (ahead_run >= 3) {
                    // Don't include this in literal, it's the start of a run
                    break;
                }

                literal.push_back(input[pos]);
                ++pos;
            }

            if (!literal.empty()) {
                // Control byte = literal.size() - 1 (range: 0 to 127)
                output.push_back(static_cast<uint8_t>(literal.size() - 1));
                output.insert(output.end(), literal.begin(), literal.end());
            }
        }
    }
}

/**
 * @brief Decodes a single RLE segment.
 *
 * @param input RLE encoded data for this segment
 * @param expected_size Expected output size in bytes
 * @return Decoded byte array
 * @throws std::runtime_error if decoding fails
 */
std::vector<uint8_t> decode_rle_segment(std::span<const uint8_t> input, size_t expected_size) {
    std::vector<uint8_t> output;
    output.reserve(expected_size);

    size_t pos = 0;
    const size_t size = input.size();

    while (pos < size && output.size() < expected_size) {
        auto control = static_cast<int8_t>(input[pos]);
        ++pos;

        if (control >= 0) {
            // Literal: copy next (control + 1) bytes
            size_t count = static_cast<size_t>(control) + 1;
            if (pos + count > size) {
                throw std::runtime_error("RLE decode: insufficient literal data");
            }
            output.insert(output.end(), input.begin() + pos, input.begin() + pos + count);
            pos += count;
        } else if (control != -128) {
            // Run: repeat next byte (1 - control) times
            if (pos >= size) {
                throw std::runtime_error("RLE decode: missing replicate byte");
            }
            size_t count = static_cast<size_t>(1 - control);
            uint8_t value = input[pos];
            ++pos;
            for (size_t i = 0; i < count && output.size() < expected_size; ++i) {
                output.push_back(value);
            }
        }
        // control == -128 is a no-op
    }

    return output;
}

/**
 * @brief Calculates the number of segments for given image parameters.
 *
 * DICOM RLE segments:
 * - 8-bit grayscale: 1 segment
 * - 16-bit grayscale: 2 segments (high byte, low byte)
 * - 8-bit RGB: 3 segments
 * - 16-bit RGB: 6 segments
 */
int calculate_segment_count(const image_params& params) {
    int bytes_per_sample = (params.bits_allocated + 7) / 8;
    return params.samples_per_pixel * bytes_per_sample;
}

}  // namespace

/**
 * @brief PIMPL implementation for rle_codec.
 */
class rle_codec::impl {
public:
    impl() = default;

    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        [[maybe_unused]] const compression_options& options) const {

        if (pixel_data.empty()) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Empty pixel data");
        }

        if (!valid_for_rle(params)) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                "Invalid parameters for RLE: requires 8/16-bit, 1-3 samples per pixel");
        }

        size_t expected_size = params.frame_size_bytes();
        if (pixel_data.size() != expected_size) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                "Pixel data size mismatch: expected " + std::to_string(expected_size) +
                ", got " + std::to_string(pixel_data.size()));
        }

        try {
            return encode_frame(pixel_data, params);
        } catch (const std::exception& e) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, std::string("RLE encoding failed: ") + e.what());
        }
    }

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const {

        if (compressed_data.empty()) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Empty compressed data");
        }

        if (compressed_data.size() < kRLEHeaderSize) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Compressed data too small for RLE header");
        }

        try {
            return decode_frame(compressed_data, params);
        } catch (const std::exception& e) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, std::string("RLE decoding failed: ") + e.what());
        }
    }

private:
    [[nodiscard]] bool valid_for_rle(const image_params& params) const noexcept {
        // RLE supports 8-bit and 16-bit samples
        if (params.bits_allocated != 8 && params.bits_allocated != 16) {
            return false;
        }
        // 1-3 samples per pixel (grayscale, RGB)
        if (params.samples_per_pixel < 1 || params.samples_per_pixel > 3) {
            return false;
        }
        // Check segment count limit
        int num_segments = calculate_segment_count(params);
        if (num_segments > kMaxSegments) {
            return false;
        }
        // Valid dimensions
        if (params.width == 0 || params.height == 0) {
            return false;
        }
        return true;
    }

    [[nodiscard]] codec_result encode_frame(
        std::span<const uint8_t> pixel_data,
        const image_params& params) const {

        int num_segments = calculate_segment_count(params);
        size_t pixels_per_frame = static_cast<size_t>(params.width) * params.height;
        int bytes_per_sample = (params.bits_allocated + 7) / 8;

        // Prepare segment data (one vector per segment)
        std::vector<std::vector<uint8_t>> segments(num_segments);
        for (auto& seg : segments) {
            seg.reserve(pixels_per_frame);
        }

        // Extract segments from pixel data
        // DICOM RLE stores each byte plane separately
        // For 16-bit: high byte first, then low byte
        // For color: each color component separately

        if (bytes_per_sample == 1) {
            // 8-bit samples
            if (params.samples_per_pixel == 1) {
                // Grayscale: single segment
                for (size_t i = 0; i < pixel_data.size(); ++i) {
                    segments[0].push_back(pixel_data[i]);
                }
            } else {
                // Color (RGB): 3 segments
                for (size_t i = 0; i < pixels_per_frame; ++i) {
                    for (int s = 0; s < params.samples_per_pixel; ++s) {
                        segments[s].push_back(pixel_data[i * params.samples_per_pixel + s]);
                    }
                }
            }
        } else {
            // 16-bit samples (little-endian input)
            // DICOM RLE stores: all high bytes, then all low bytes
            if (params.samples_per_pixel == 1) {
                // Grayscale: 2 segments (high byte, low byte)
                for (size_t i = 0; i < pixels_per_frame; ++i) {
                    size_t idx = i * 2;
                    segments[0].push_back(pixel_data[idx + 1]);  // High byte
                    segments[1].push_back(pixel_data[idx]);      // Low byte
                }
            } else {
                // Color: 2 segments per color component
                for (size_t i = 0; i < pixels_per_frame; ++i) {
                    for (int s = 0; s < params.samples_per_pixel; ++s) {
                        size_t idx = (i * params.samples_per_pixel + s) * 2;
                        segments[s * 2].push_back(pixel_data[idx + 1]);      // High byte
                        segments[s * 2 + 1].push_back(pixel_data[idx]);      // Low byte
                    }
                }
            }
        }

        // Encode each segment
        std::vector<std::vector<uint8_t>> encoded_segments(num_segments);
        for (int i = 0; i < num_segments; ++i) {
            encode_rle_segment(segments[i], encoded_segments[i]);
        }

        // Build output with RLE header
        std::vector<uint8_t> output;

        // Calculate total size (header + all segments, each padded to even length)
        size_t total_size = kRLEHeaderSize;
        for (const auto& seg : encoded_segments) {
            total_size += seg.size();
            if (seg.size() % 2 != 0) {
                ++total_size;  // Padding byte
            }
        }
        output.reserve(total_size);

        // Write header (16 x 4-byte offsets)
        output.resize(kRLEHeaderSize, 0);

        // First 4 bytes: number of segments
        write_le32(output.data(), static_cast<uint32_t>(num_segments));

        // Calculate and write segment offsets
        uint32_t current_offset = kRLEHeaderSize;
        for (int i = 0; i < num_segments; ++i) {
            write_le32(output.data() + 4 + i * 4, current_offset);
            current_offset += static_cast<uint32_t>(encoded_segments[i].size());
            if (encoded_segments[i].size() % 2 != 0) {
                ++current_offset;  // Account for padding
            }
        }

        // Write encoded segments
        for (const auto& seg : encoded_segments) {
            output.insert(output.end(), seg.begin(), seg.end());
            if (seg.size() % 2 != 0) {
                output.push_back(0);  // Padding byte
            }
        }

        image_params output_params = params;
        return pacs::ok<compression_result>(compression_result{std::move(output), output_params});
    }

    [[nodiscard]] codec_result decode_frame(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const {

        const uint8_t* header = compressed_data.data();

        // Read number of segments from header
        uint32_t num_segments = read_le32(header);
        if (num_segments == 0 || num_segments > kMaxSegments) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                "Invalid RLE segment count: " + std::to_string(num_segments));
        }

        int expected_segments = calculate_segment_count(params);
        if (static_cast<int>(num_segments) != expected_segments) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                "Segment count mismatch: expected " + std::to_string(expected_segments) +
                ", got " + std::to_string(num_segments));
        }

        // Read segment offsets
        std::vector<uint32_t> offsets(num_segments);
        for (uint32_t i = 0; i < num_segments; ++i) {
            offsets[i] = read_le32(header + 4 + i * 4);
            if (offsets[i] >= compressed_data.size()) {
                return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                    "Invalid segment offset: " + std::to_string(offsets[i]));
            }
        }

        // Calculate segment sizes
        std::vector<size_t> sizes(num_segments);
        for (uint32_t i = 0; i < num_segments; ++i) {
            if (i + 1 < num_segments) {
                sizes[i] = offsets[i + 1] - offsets[i];
            } else {
                sizes[i] = compressed_data.size() - offsets[i];
            }
        }

        size_t pixels_per_frame = static_cast<size_t>(params.width) * params.height;
        int bytes_per_sample = (params.bits_allocated + 7) / 8;

        // Decode each segment
        std::vector<std::vector<uint8_t>> decoded_segments(num_segments);
        for (uint32_t i = 0; i < num_segments; ++i) {
            std::span<const uint8_t> segment_data(
                compressed_data.data() + offsets[i], sizes[i]);
            decoded_segments[i] = decode_rle_segment(segment_data, pixels_per_frame);

            if (decoded_segments[i].size() != pixels_per_frame) {
                return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                    "Segment " + std::to_string(i) + " decoded size mismatch: expected " +
                    std::to_string(pixels_per_frame) + ", got " +
                    std::to_string(decoded_segments[i].size()));
            }
        }

        // Reconstruct pixel data from segments
        size_t output_size = pixels_per_frame * params.samples_per_pixel * bytes_per_sample;
        std::vector<uint8_t> output(output_size);

        if (bytes_per_sample == 1) {
            // 8-bit samples
            if (params.samples_per_pixel == 1) {
                output = std::move(decoded_segments[0]);
            } else {
                // Color: interleave samples
                for (size_t i = 0; i < pixels_per_frame; ++i) {
                    for (int s = 0; s < params.samples_per_pixel; ++s) {
                        output[i * params.samples_per_pixel + s] = decoded_segments[s][i];
                    }
                }
            }
        } else {
            // 16-bit samples
            if (params.samples_per_pixel == 1) {
                // Grayscale: combine high and low byte segments
                for (size_t i = 0; i < pixels_per_frame; ++i) {
                    size_t idx = i * 2;
                    output[idx] = decoded_segments[1][i];      // Low byte
                    output[idx + 1] = decoded_segments[0][i];  // High byte
                }
            } else {
                // Color: combine and interleave
                for (size_t i = 0; i < pixels_per_frame; ++i) {
                    for (int s = 0; s < params.samples_per_pixel; ++s) {
                        size_t idx = (i * params.samples_per_pixel + s) * 2;
                        output[idx] = decoded_segments[s * 2 + 1][i];      // Low byte
                        output[idx + 1] = decoded_segments[s * 2][i];      // High byte
                    }
                }
            }
        }

        // Build output parameters
        image_params output_params;
        output_params.width = params.width;
        output_params.height = params.height;
        output_params.bits_allocated = params.bits_allocated;
        output_params.bits_stored = params.bits_stored > 0 ? params.bits_stored : params.bits_allocated;
        output_params.high_bit = output_params.bits_stored - 1;
        output_params.samples_per_pixel = params.samples_per_pixel;
        output_params.planar_configuration = 0;  // Always interleaved output
        output_params.pixel_representation = params.pixel_representation;
        output_params.photometric = params.photometric;

        return pacs::ok<compression_result>(compression_result{std::move(output), output_params});
    }
};

// rle_codec implementation

rle_codec::rle_codec() : impl_(std::make_unique<impl>()) {}

rle_codec::~rle_codec() = default;

rle_codec::rle_codec(rle_codec&&) noexcept = default;

rle_codec& rle_codec::operator=(rle_codec&&) noexcept = default;

std::string_view rle_codec::transfer_syntax_uid() const noexcept {
    return kTransferSyntaxUID;
}

std::string_view rle_codec::name() const noexcept {
    return "RLE Lossless";
}

bool rle_codec::is_lossy() const noexcept {
    return false;
}

bool rle_codec::can_encode(const image_params& params) const noexcept {
    // RLE supports 8-bit and 16-bit samples
    if (params.bits_allocated != 8 && params.bits_allocated != 16) {
        return false;
    }
    // 1-3 samples per pixel
    if (params.samples_per_pixel < 1 || params.samples_per_pixel > 3) {
        return false;
    }
    // Check segment count limit
    int bytes_per_sample = (params.bits_allocated + 7) / 8;
    int num_segments = params.samples_per_pixel * bytes_per_sample;
    if (num_segments > kMaxSegments) {
        return false;
    }
    // Valid dimensions
    if (params.width == 0 || params.height == 0) {
        return false;
    }
    return true;
}

bool rle_codec::can_decode(const image_params& params) const noexcept {
    // Can decode if parameters are valid for RLE
    // Width/height can be 0 (unknown) for partial validation
    if (params.bits_allocated != 0 &&
        params.bits_allocated != 8 &&
        params.bits_allocated != 16) {
        return false;
    }
    if (params.samples_per_pixel != 0 &&
        (params.samples_per_pixel < 1 || params.samples_per_pixel > 3)) {
        return false;
    }
    return true;
}

codec_result rle_codec::encode(
    std::span<const uint8_t> pixel_data,
    const image_params& params,
    const compression_options& options) const {
    return impl_->encode(pixel_data, params, options);
}

codec_result rle_codec::decode(
    std::span<const uint8_t> compressed_data,
    const image_params& params) const {
    return impl_->decode(compressed_data, params);
}

}  // namespace pacs::encoding::compression
