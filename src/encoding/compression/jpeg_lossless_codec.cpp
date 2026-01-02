#include "pacs/encoding/compression/jpeg_lossless_codec.hpp"
#include <pacs/core/result.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace pacs::encoding::compression {

namespace {

// JPEG marker definitions
constexpr uint8_t kMarkerPrefix = 0xFF;
constexpr uint8_t kSOI = 0xD8;   // Start of Image
constexpr uint8_t kEOI = 0xD9;   // End of Image
constexpr uint8_t kSOF3 = 0xC3;  // Start of Frame (Lossless)
constexpr uint8_t kDHT = 0xC4;   // Define Huffman Table
constexpr uint8_t kSOS = 0xDA;   // Start of Scan

// DICOM JPEG Lossless default precision values
constexpr int kMaxPrecision = 16;
constexpr int kMinPrecision = 2;

/**
 * @brief Write a 16-bit big-endian value to buffer.
 */
inline void write_be16(std::vector<uint8_t>& buf, uint16_t value) {
    buf.push_back(static_cast<uint8_t>(value >> 8));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

/**
 * @brief Read a 16-bit big-endian value from buffer.
 */
inline uint16_t read_be16(const uint8_t* data) {
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

/**
 * @brief Bit writer for Huffman encoding.
 */
class bit_writer {
public:
    explicit bit_writer(std::vector<uint8_t>& output) : output_(output) {}

    void write_bits(uint32_t value, int num_bits) {
        while (num_bits > 0) {
            int bits_to_write = (std::min)(num_bits, 8 - bit_pos_);
            int shift = num_bits - bits_to_write;
            uint8_t mask = static_cast<uint8_t>((1 << bits_to_write) - 1);
            current_byte_ |= static_cast<uint8_t>(((value >> shift) & mask) << (8 - bit_pos_ - bits_to_write));
            bit_pos_ += bits_to_write;
            num_bits -= bits_to_write;

            if (bit_pos_ == 8) {
                flush_byte();
            }
        }
    }

    void flush() {
        if (bit_pos_ > 0) {
            flush_byte();
        }
    }

private:
    void flush_byte() {
        output_.push_back(current_byte_);
        // Byte stuffing: insert 0x00 after 0xFF
        if (current_byte_ == 0xFF) {
            output_.push_back(0x00);
        }
        current_byte_ = 0;
        bit_pos_ = 0;
    }

    std::vector<uint8_t>& output_;
    uint8_t current_byte_ = 0;
    int bit_pos_ = 0;
};

/**
 * @brief Bit reader for Huffman decoding.
 */
class bit_reader {
public:
    bit_reader(const uint8_t* data, size_t size)
        : data_(data), size_(size), pos_(0), bit_pos_(0), current_byte_(0) {
        if (size_ > 0) {
            current_byte_ = data_[0];
        }
    }

    int read_bits(int num_bits) {
        int value = 0;
        while (num_bits > 0) {
            if (bit_pos_ == 8) {
                advance_byte();
            }
            int bits_available = 8 - bit_pos_;
            int bits_to_read = (std::min)(num_bits, bits_available);
            int shift = bits_available - bits_to_read;
            uint8_t mask = static_cast<uint8_t>((1 << bits_to_read) - 1);
            value = (value << bits_to_read) | ((current_byte_ >> shift) & mask);
            bit_pos_ += bits_to_read;
            num_bits -= bits_to_read;
        }
        return value;
    }

    bool has_more() const {
        return pos_ < size_ || bit_pos_ < 8;
    }

private:
    void advance_byte() {
        ++pos_;
        if (pos_ < size_) {
            current_byte_ = data_[pos_];
            // Handle byte stuffing: skip 0x00 after 0xFF
            if (pos_ > 0 && data_[pos_ - 1] == 0xFF && current_byte_ == 0x00) {
                ++pos_;
                if (pos_ < size_) {
                    current_byte_ = data_[pos_];
                }
            }
        }
        bit_pos_ = 0;
    }

    const uint8_t* data_;
    size_t size_;
    size_t pos_;
    int bit_pos_;
    uint8_t current_byte_;
};

/**
 * @brief Default Huffman table for JPEG Lossless encoding.
 *
 * This is a simplified table optimized for medical imaging where
 * differences tend to be small.
 */
struct huffman_table {
    // Code lengths for each category (0-16)
    std::array<int, 17> code_lengths{};
    // Huffman codes for each category
    std::array<uint32_t, 17> codes{};

    huffman_table() {
        // Initialize with default JPEG lossless Huffman table
        // Category 0-16 with appropriate code lengths
        // This is a simplified version - actual implementation would
        // optimize based on data statistics
        generate_default_table();
    }

    void generate_default_table() {
        // Default code lengths for lossless JPEG
        // Shorter codes for smaller differences (more common)
        static constexpr std::array<int, 17> default_lengths = {
            2, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16
        };

        code_lengths = default_lengths;

        // Generate codes from lengths
        uint32_t code = 0;
        int last_length = 0;
        for (int i = 0; i <= 16; ++i) {
            if (code_lengths[i] > last_length) {
                code <<= (code_lengths[i] - last_length);
            }
            codes[i] = code;
            ++code;
            last_length = code_lengths[i];
        }
    }
};

/**
 * @brief Calculates the category (number of bits needed) for a difference value.
 */
inline int category(int diff) {
    if (diff == 0) return 0;
    int abs_diff = diff < 0 ? -diff : diff;
    int cat = 0;
    while (abs_diff > 0) {
        abs_diff >>= 1;
        ++cat;
    }
    return cat;
}

/**
 * @brief Encodes a difference value for Huffman coding.
 *
 * @param diff The difference value
 * @param cat The category (number of bits)
 * @return The encoded value to follow the Huffman code
 */
inline int encode_diff(int diff, int cat) {
    if (diff < 0) {
        // For negative numbers, use one's complement
        return diff + (1 << cat) - 1;
    }
    return diff;
}

/**
 * @brief Decodes a difference value from Huffman decoding.
 */
inline int decode_diff(int value, int cat) {
    if (cat == 0) return 0;
    int half = 1 << (cat - 1);
    if (value < half) {
        // Negative value (one's complement)
        return value - (1 << cat) + 1;
    }
    return value;
}

/**
 * @brief Apply prediction based on predictor selection.
 *
 * @param ra Left neighbor (same row, previous column)
 * @param rb Above neighbor (previous row, same column)
 * @param rc Diagonal neighbor (previous row, previous column)
 * @param predictor Predictor selection value (1-7)
 * @return Predicted value
 */
inline int predict(int ra, int rb, int rc, int predictor) {
    switch (predictor) {
        case 1: return ra;                      // Px = Ra
        case 2: return rb;                      // Px = Rb
        case 3: return rc;                      // Px = Rc
        case 4: return ra + rb - rc;            // Px = Ra + Rb - Rc
        case 5: return ra + ((rb - rc) >> 1);   // Px = Ra + (Rb - Rc) / 2
        case 6: return rb + ((ra - rc) >> 1);   // Px = Rb + (Ra - Rc) / 2
        case 7: return (ra + rb) >> 1;          // Px = (Ra + Rb) / 2
        default: return ra;                     // Default to predictor 1
    }
}

}  // namespace

/**
 * @brief PIMPL implementation for jpeg_lossless_codec.
 */
class jpeg_lossless_codec::impl {
public:
    impl(int predictor, int point_transform)
        : predictor_(std::clamp(predictor, 1, 7)),
          point_transform_(std::clamp(point_transform, 0, 15)) {}

    [[nodiscard]] int predictor() const noexcept { return predictor_; }
    [[nodiscard]] int point_transform() const noexcept { return point_transform_; }

    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        [[maybe_unused]] const compression_options& options) const {

        if (pixel_data.empty()) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Empty pixel data");
        }

        if (!valid_for_jpeg_lossless(params)) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, 
                "Invalid parameters for JPEG Lossless: requires 8/12/16-bit grayscale");
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
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, std::string("JPEG Lossless encoding failed: ") + e.what());
        }
    }

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const {

        if (compressed_data.empty()) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Empty compressed data");
        }

        try {
            return decode_frame(compressed_data, params);
        } catch (const std::exception& e) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, std::string("JPEG Lossless decoding failed: ") + e.what());
        }
    }

private:
    [[nodiscard]] bool valid_for_jpeg_lossless(const image_params& params) const noexcept {
        // JPEG Lossless supports 2-16 bit precision
        if (params.bits_stored < kMinPrecision || params.bits_stored > kMaxPrecision) {
            return false;
        }
        // bits_allocated must be 8 or 16
        if (params.bits_allocated != 8 && params.bits_allocated != 16) {
            return false;
        }
        // Grayscale only for DICOM JPEG Lossless
        if (params.samples_per_pixel != 1) {
            return false;
        }
        return true;
    }

    [[nodiscard]] codec_result encode_frame(
        std::span<const uint8_t> pixel_data,
        const image_params& params) const {

        std::vector<uint8_t> output;
        output.reserve(pixel_data.size());  // Reserve worst-case

        int precision = params.bits_stored;
        bool is_16bit = params.bits_allocated == 16;

        // Write SOI marker
        output.push_back(kMarkerPrefix);
        output.push_back(kSOI);

        // Write SOF3 (Start of Frame - Lossless)
        write_sof3(output, params);

        // Write DHT (Huffman Table)
        write_dht(output, precision);

        // Write SOS (Start of Scan)
        write_sos(output);

        // Encode image data
        huffman_table ht;
        bit_writer writer(output);

        int width = params.width;
        int height = params.height;

        auto get_pixel = [&](int x, int y) -> int {
            if (x < 0 || y < 0) return 0;
            size_t idx = static_cast<size_t>(y) * width + x;
            if (is_16bit) {
                idx *= 2;
                // Little-endian pixel data
                return pixel_data[idx] | (pixel_data[idx + 1] << 8);
            }
            return pixel_data[idx];
        };

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int pixel = get_pixel(x, y);
                int ra = get_pixel(x - 1, y);
                int rb = get_pixel(x, y - 1);
                int rc = get_pixel(x - 1, y - 1);

                // Apply point transform
                int shifted_pixel = pixel >> point_transform_;

                // Compute prediction
                int pred;
                if (x == 0 && y == 0) {
                    // First pixel: use 2^(P-Pt-1) as prediction
                    pred = 1 << (precision - point_transform_ - 1);
                } else if (y == 0) {
                    // First row: use Ra
                    pred = ra >> point_transform_;
                } else if (x == 0) {
                    // First column: use Rb
                    pred = rb >> point_transform_;
                } else {
                    // Use selected predictor
                    pred = predict(ra >> point_transform_,
                                   rb >> point_transform_,
                                   rc >> point_transform_,
                                   predictor_);
                }

                // Compute difference
                int diff = shifted_pixel - pred;

                // Modulo operation for precision
                int mod_range = 1 << (precision - point_transform_);
                if (diff < -mod_range / 2) {
                    diff += mod_range;
                } else if (diff >= mod_range / 2) {
                    diff -= mod_range;
                }

                // Encode difference
                int cat = category(diff);
                int encoded = encode_diff(diff, cat);

                // Write Huffman code for category
                writer.write_bits(ht.codes[cat], ht.code_lengths[cat]);

                // Write additional bits for actual value
                if (cat > 0) {
                    writer.write_bits(static_cast<uint32_t>(encoded), cat);
                }
            }
        }

        writer.flush();

        // Write EOI marker
        output.push_back(kMarkerPrefix);
        output.push_back(kEOI);

        image_params output_params = params;
        return pacs::ok<compression_result>(compression_result{std::move(output), output_params});
    }

    void write_sof3(std::vector<uint8_t>& output, const image_params& params) const {
        output.push_back(kMarkerPrefix);
        output.push_back(kSOF3);

        // Length = 8 + 3 * number_of_components
        uint16_t length = 8 + 3 * 1;  // 1 component (grayscale)
        write_be16(output, length);

        // Precision
        output.push_back(static_cast<uint8_t>(params.bits_stored));

        // Height
        write_be16(output, params.height);

        // Width
        write_be16(output, params.width);

        // Number of components
        output.push_back(1);

        // Component specification
        output.push_back(1);  // Component ID
        output.push_back(0x11);  // Sampling factors (1x1)
        output.push_back(0);  // Quantization table (not used for lossless)
    }

    void write_dht(std::vector<uint8_t>& output, [[maybe_unused]] int precision) const {
        huffman_table ht;

        output.push_back(kMarkerPrefix);
        output.push_back(kDHT);

        // Calculate table data
        std::vector<uint8_t> table_data;
        table_data.push_back(0x00);  // DC table, ID 0

        // Count codes of each length
        std::array<int, 16> length_counts{};
        for (int i = 0; i <= 16; ++i) {
            int len = ht.code_lengths[i];
            if (len >= 1 && len <= 16) {
                ++length_counts[len - 1];
            }
        }

        // Write length counts
        for (int i = 0; i < 16; ++i) {
            table_data.push_back(static_cast<uint8_t>(length_counts[i]));
        }

        // Write symbols in order of code length
        for (int len = 1; len <= 16; ++len) {
            for (int i = 0; i <= 16; ++i) {
                if (ht.code_lengths[i] == len) {
                    table_data.push_back(static_cast<uint8_t>(i));
                }
            }
        }

        // Write length
        write_be16(output, static_cast<uint16_t>(2 + table_data.size()));

        // Write table data
        output.insert(output.end(), table_data.begin(), table_data.end());
    }

    void write_sos(std::vector<uint8_t>& output) const {
        output.push_back(kMarkerPrefix);
        output.push_back(kSOS);

        // Length
        write_be16(output, 8);  // Fixed length for 1 component

        // Number of components
        output.push_back(1);

        // Component selector and Huffman table
        output.push_back(1);  // Component ID
        output.push_back(0x00);  // DC=0, AC=0

        // Spectral selection (predictor for lossless)
        output.push_back(static_cast<uint8_t>(predictor_));

        // Spectral selection end (not used)
        output.push_back(0);

        // Successive approximation (point transform)
        output.push_back(static_cast<uint8_t>(point_transform_));
    }

    [[nodiscard]] codec_result decode_frame(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const {

        const uint8_t* data = compressed_data.data();
        size_t size = compressed_data.size();
        size_t pos = 0;

        // Parse SOI
        if (size < 2 || data[0] != kMarkerPrefix || data[1] != kSOI) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Invalid JPEG: missing SOI marker");
        }
        pos = 2;

        int precision = 0;
        int width = 0;
        int height = 0;
        int predictor = predictor_;
        int point_transform = point_transform_;
        huffman_table ht;
        bool found_sof = false;
        bool found_sos = false;
        size_t scan_start = 0;

        // Parse markers
        while (pos < size - 1) {
            if (data[pos] != kMarkerPrefix) {
                return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Invalid JPEG: expected marker");
            }

            uint8_t marker = data[pos + 1];
            pos += 2;

            if (marker == kEOI) {
                break;
            }

            // Skip fill bytes (0xFF)
            if (marker == kMarkerPrefix) {
                --pos;
                continue;
            }

            // Markers with no length
            if (marker >= 0xD0 && marker <= 0xD7) {  // RST markers
                continue;
            }

            // Read marker length
            if (pos + 2 > size) {
                return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Invalid JPEG: truncated marker");
            }
            uint16_t length = read_be16(&data[pos]);
            pos += 2;

            if (pos + length - 2 > size) {
                return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Invalid JPEG: truncated marker data");
            }

            if (marker == kSOF3) {
                // Parse SOF3
                if (length < 8) {
                    return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Invalid SOF3 marker");
                }
                precision = data[pos];
                height = read_be16(&data[pos + 1]);
                width = read_be16(&data[pos + 3]);
                found_sof = true;
            } else if (marker == kDHT) {
                // Parse DHT - use the table from the file
                // For simplicity, we use our default table
                // A full implementation would parse and use the embedded table
            } else if (marker == kSOS) {
                // Parse SOS
                if (length >= 6) {
                    predictor = data[pos + 3];
                    point_transform = data[pos + 5] & 0x0F;
                }
                found_sos = true;
                pos += length - 2;
                scan_start = pos;
                break;
            }

            pos += length - 2;
        }

        if (!found_sof || !found_sos) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Invalid JPEG: missing required markers");
        }

        // Validate against params if provided
        if (params.width > 0 && params.width != width) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Width mismatch");
        }
        if (params.height > 0 && params.height != height) {
            return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Height mismatch");
        }

        // Find scan data end (EOI marker)
        size_t scan_end = size;
        for (size_t i = scan_start; i < size - 1; ++i) {
            if (data[i] == kMarkerPrefix && data[i + 1] == kEOI) {
                scan_end = i;
                break;
            }
        }

        // Decode image data
        bool is_16bit = precision > 8;
        size_t output_size = static_cast<size_t>(width) * height * (is_16bit ? 2 : 1);
        std::vector<uint8_t> output(output_size);

        bit_reader reader(&data[scan_start], scan_end - scan_start);

        auto set_pixel = [&](int x, int y, int value) {
            size_t idx = static_cast<size_t>(y) * width + x;
            if (is_16bit) {
                idx *= 2;
                output[idx] = static_cast<uint8_t>(value & 0xFF);
                output[idx + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
            } else {
                output[idx] = static_cast<uint8_t>(value);
            }
        };

        auto get_decoded_pixel = [&](int x, int y) -> int {
            if (x < 0 || y < 0) return 0;
            size_t idx = static_cast<size_t>(y) * width + x;
            if (is_16bit) {
                idx *= 2;
                return output[idx] | (output[idx + 1] << 8);
            }
            return output[idx];
        };

        int mod_range = 1 << (precision - point_transform);
        int max_value = (1 << precision) - 1;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Decode Huffman category
                int cat = decode_huffman_category(reader, ht);
                if (cat < 0 || cat > 16) {
                    return pacs::pacs_error<compression_result>(pacs::error_codes::decompression_error, "Invalid Huffman code");
                }

                // Read additional bits
                int encoded = 0;
                if (cat > 0) {
                    encoded = reader.read_bits(cat);
                }

                // Decode difference
                int diff = decode_diff(encoded, cat);

                // Compute prediction
                int ra = get_decoded_pixel(x - 1, y);
                int rb = get_decoded_pixel(x, y - 1);
                int rc = get_decoded_pixel(x - 1, y - 1);

                int pred;
                if (x == 0 && y == 0) {
                    pred = 1 << (precision - point_transform - 1);
                } else if (y == 0) {
                    pred = ra >> point_transform;
                } else if (x == 0) {
                    pred = rb >> point_transform;
                } else {
                    pred = predict(ra >> point_transform,
                                   rb >> point_transform,
                                   rc >> point_transform,
                                   predictor);
                }

                // Reconstruct pixel
                int shifted_pixel = (pred + diff) & (mod_range - 1);
                int pixel = shifted_pixel << point_transform;
                pixel = std::clamp(pixel, 0, max_value);

                set_pixel(x, y, pixel);
            }
        }

        // Build output parameters
        image_params output_params;
        output_params.width = static_cast<uint16_t>(width);
        output_params.height = static_cast<uint16_t>(height);
        output_params.bits_allocated = is_16bit ? 16 : 8;
        output_params.bits_stored = static_cast<uint16_t>(precision);
        output_params.high_bit = static_cast<uint16_t>(precision - 1);
        output_params.samples_per_pixel = 1;
        output_params.planar_configuration = 0;
        output_params.pixel_representation = 0;
        output_params.photometric = photometric_interpretation::monochrome2;

        return pacs::ok<compression_result>(compression_result{std::move(output), output_params});
    }

    int decode_huffman_category(bit_reader& reader, const huffman_table& ht) const {
        // Simple Huffman decoding - match against known codes
        uint32_t code = 0;
        for (int bits = 1; bits <= 16; ++bits) {
            code = (code << 1) | reader.read_bits(1);
            for (int cat = 0; cat <= 16; ++cat) {
                if (ht.code_lengths[cat] == bits && ht.codes[cat] == code) {
                    return cat;
                }
            }
        }
        return -1;  // Error
    }

    int predictor_;
    int point_transform_;
};

// jpeg_lossless_codec implementation

jpeg_lossless_codec::jpeg_lossless_codec(int predictor, int point_transform)
    : impl_(std::make_unique<impl>(predictor, point_transform)) {}

jpeg_lossless_codec::~jpeg_lossless_codec() = default;

jpeg_lossless_codec::jpeg_lossless_codec(jpeg_lossless_codec&&) noexcept = default;

jpeg_lossless_codec& jpeg_lossless_codec::operator=(jpeg_lossless_codec&&) noexcept = default;

std::string_view jpeg_lossless_codec::transfer_syntax_uid() const noexcept {
    return kTransferSyntaxUID;
}

std::string_view jpeg_lossless_codec::name() const noexcept {
    return "JPEG Lossless (Process 14, SV1)";
}

bool jpeg_lossless_codec::is_lossy() const noexcept {
    return false;
}

bool jpeg_lossless_codec::can_encode(const image_params& params) const noexcept {
    // JPEG Lossless supports 2-16 bit precision, grayscale only
    if (params.bits_stored < 2 || params.bits_stored > 16) return false;
    if (params.bits_allocated != 8 && params.bits_allocated != 16) return false;
    if (params.samples_per_pixel != 1) return false;
    return true;
}

bool jpeg_lossless_codec::can_decode(const image_params& params) const noexcept {
    // Can decode any grayscale JPEG Lossless
    // Width/height can be 0 (unknown) - will read from JPEG header
    if (params.bits_allocated != 0 &&
        params.bits_allocated != 8 &&
        params.bits_allocated != 16) {
        return false;
    }
    if (params.samples_per_pixel != 0 && params.samples_per_pixel != 1) {
        return false;
    }
    return true;
}

int jpeg_lossless_codec::predictor() const noexcept {
    return impl_->predictor();
}

int jpeg_lossless_codec::point_transform() const noexcept {
    return impl_->point_transform();
}

codec_result jpeg_lossless_codec::encode(
    std::span<const uint8_t> pixel_data,
    const image_params& params,
    const compression_options& options) const {
    return impl_->encode(pixel_data, params, options);
}

codec_result jpeg_lossless_codec::decode(
    std::span<const uint8_t> compressed_data,
    const image_params& params) const {
    return impl_->decode(compressed_data, params);
}

}  // namespace pacs::encoding::compression
