/**
 * @file dicom_file.cpp
 * @brief Implementation of DICOM Part 10 file handling
 */

#include "pacs/core/dicom_file.hpp"

#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/encoding/compression/codec_factory.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace pacs::core {

namespace {

/// Undefined length marker (0xFFFFFFFF)
constexpr uint32_t kUndefinedLength = 0xFFFFFFFF;

/// Item tag (FFFE,E000)
constexpr uint16_t kItemTagGroup = 0xFFFE;
constexpr uint16_t kItemTagElement = 0xE000;

/// Item delimitation tag (FFFE,E00D)
constexpr uint16_t kItemDelimitationElement = 0xE00D;

/// Sequence delimitation tag (FFFE,E0DD)
constexpr uint16_t kSequenceDelimitationElement = 0xE0DD;

/**
 * @brief Read a 16-bit unsigned integer from little-endian bytes
 */
[[nodiscard]] auto read_uint16_le(std::span<const uint8_t> data) -> uint16_t {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

/**
 * @brief Read a 32-bit unsigned integer from little-endian bytes
 */
[[nodiscard]] auto read_uint32_le(std::span<const uint8_t> data) -> uint32_t {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

/**
 * @brief Read a 16-bit unsigned integer from big-endian bytes
 */
[[nodiscard]] auto read_uint16_be(std::span<const uint8_t> data) -> uint16_t {
    return static_cast<uint16_t>(data[1]) |
           (static_cast<uint16_t>(data[0]) << 8);
}

/**
 * @brief Read a 32-bit unsigned integer from big-endian bytes
 */
[[nodiscard]] auto read_uint32_be(std::span<const uint8_t> data) -> uint32_t {
    return static_cast<uint32_t>(data[3]) |
           (static_cast<uint32_t>(data[2]) << 8) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[0]) << 24);
}

/**
 * @brief Write a 16-bit unsigned integer as little-endian bytes
 */
void write_uint16_le(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

/**
 * @brief Write a 32-bit unsigned integer as little-endian bytes
 */
void write_uint32_le(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

/**
 * @brief Write a 16-bit unsigned integer as big-endian bytes
 */
void write_uint16_be(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

/**
 * @brief Write a 32-bit unsigned integer as big-endian bytes
 */
void write_uint32_be(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

/**
 * @brief Read file contents into a vector
 */
[[nodiscard]] auto read_file_contents(const std::filesystem::path& path)
    -> pacs::Result<std::vector<uint8_t>> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return pacs::pacs_error<std::vector<uint8_t>>(
            pacs::error_codes::file_not_found,
            "File not found: " + path.string());
    }

    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(size))) {
        return pacs::pacs_error<std::vector<uint8_t>>(
            pacs::error_codes::file_read_error,
            "Failed to read file: " + path.string());
    }

    return pacs::Result<std::vector<uint8_t>>::ok(std::move(buffer));
}

}  // namespace

// ============================================================================
// Construction
// ============================================================================

dicom_file::dicom_file(dicom_dataset meta_info, dicom_dataset main_dataset)
    : meta_info_(std::move(meta_info)), dataset_(std::move(main_dataset)) {}

// ============================================================================
// Static Factory Methods (Reading)
// ============================================================================

auto dicom_file::open(const std::filesystem::path& path)
    -> pacs::Result<dicom_file> {
    auto contents = read_file_contents(path);
    if (contents.is_err()) {
        return pacs::Result<dicom_file>::err(contents.error());
    }

    return from_bytes(contents.value());
}

auto dicom_file::from_bytes(std::span<const uint8_t> data)
    -> pacs::Result<dicom_file> {
    // Minimum size: 128 (preamble) + 4 (DICM) + minimal meta info
    if (data.size() < kPreambleSize + 4) {
        return pacs::pacs_error<dicom_file>(
            pacs::error_codes::invalid_dicom_file,
            "File too small to be valid DICOM Part 10 file");
    }

    // Check for DICM prefix at offset 128
    const auto prefix = data.subspan(kPreambleSize, 4);
    if (std::memcmp(prefix.data(), kDicmPrefix, 4) != 0) {
        return pacs::pacs_error<dicom_file>(
            pacs::error_codes::missing_dicm_prefix,
            "Missing DICM prefix at offset 128");
    }

    // Parse File Meta Information (starts after preamble + DICM)
    const auto meta_start = data.subspan(kPreambleSize + 4);
    size_t meta_bytes_read = 0;

    auto meta_result = parse_meta_information(meta_start, meta_bytes_read);
    if (meta_result.is_err()) {
        return pacs::Result<dicom_file>::err(meta_result.error());
    }

    // Extract Transfer Syntax from meta information
    const auto* ts_elem = meta_result.value().get(tags::transfer_syntax_uid);
    if (ts_elem == nullptr) {
        return pacs::pacs_error<dicom_file>(
            pacs::error_codes::missing_transfer_syntax,
            "Transfer Syntax UID not found in meta information");
    }

    auto ts_uid_result = ts_elem->as_string();
    if (ts_uid_result.is_err()) {
        return pacs::pacs_error<dicom_file>(
            pacs::error_codes::value_conversion_error,
            "Failed to read Transfer Syntax UID");
    }
    const auto ts_uid = ts_uid_result.value();
    encoding::transfer_syntax ts{ts_uid};

    if (!ts.is_valid()) {
        return pacs::pacs_error<dicom_file>(
            pacs::error_codes::unsupported_transfer_syntax,
            "Unsupported Transfer Syntax: " + ts_uid);
    }

    // Parse main dataset using appropriate decoder based on Transfer Syntax
    const auto dataset_start = meta_start.subspan(meta_bytes_read);
    size_t dataset_bytes_read = 0;

    auto dataset_result = decode_dataset(dataset_start, ts, dataset_bytes_read);
    if (dataset_result.is_err()) {
        return pacs::Result<dicom_file>::err(dataset_result.error());
    }

    return pacs::Result<dicom_file>::ok(
        dicom_file{std::move(meta_result.value()), std::move(dataset_result.value())});
}

// ============================================================================
// Static Factory Methods (Creation)
// ============================================================================

auto dicom_file::create(dicom_dataset dataset,
                        const encoding::transfer_syntax& ts)
    -> dicom_file {
    auto meta_info = generate_meta_information(dataset, ts);
    return dicom_file{std::move(meta_info), std::move(dataset)};
}

// ============================================================================
// Writing
// ============================================================================

auto dicom_file::save(const std::filesystem::path& path) const
    -> pacs::VoidResult {
    auto bytes = to_bytes();

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return pacs::pacs_void_error(
            pacs::error_codes::file_write_error,
            "Failed to open file for writing: " + path.string());
    }

    if (!file.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()))) {
        return pacs::pacs_void_error(
            pacs::error_codes::file_write_error,
            "Failed to write to file: " + path.string());
    }

    return pacs::ok();
}

auto dicom_file::to_bytes() const -> std::vector<uint8_t> {
    std::vector<uint8_t> result;

    // Reserve estimated size
    result.reserve(kPreambleSize + 4 + 512 + 65536);

    // Write 128-byte preamble (zeros)
    result.resize(kPreambleSize, 0);

    // Write DICM prefix
    result.insert(result.end(), std::begin(kDicmPrefix), std::end(kDicmPrefix));

    // Encode and write File Meta Information (always Explicit VR LE)
    auto meta_bytes = encode_explicit_vr_le(meta_info_);
    result.insert(result.end(), meta_bytes.begin(), meta_bytes.end());

    // Encode and write main dataset using appropriate Transfer Syntax
    auto ts = transfer_syntax();
    auto dataset_bytes = encode_dataset(dataset_, ts);
    result.insert(result.end(), dataset_bytes.begin(), dataset_bytes.end());

    return result;
}

// ============================================================================
// Accessors
// ============================================================================

auto dicom_file::meta_information() const noexcept -> const dicom_dataset& {
    return meta_info_;
}

auto dicom_file::meta_information() noexcept -> dicom_dataset& {
    return meta_info_;
}

auto dicom_file::dataset() const noexcept -> const dicom_dataset& {
    return dataset_;
}

auto dicom_file::dataset() noexcept -> dicom_dataset& {
    return dataset_;
}

// ============================================================================
// Convenience Accessors
// ============================================================================

auto dicom_file::transfer_syntax() const -> encoding::transfer_syntax {
    const auto ts_uid = meta_info_.get_string(tags::transfer_syntax_uid);
    return encoding::transfer_syntax{ts_uid};
}

auto dicom_file::sop_class_uid() const -> std::string {
    return dataset_.get_string(tags::sop_class_uid);
}

auto dicom_file::sop_instance_uid() const -> std::string {
    return dataset_.get_string(tags::sop_instance_uid);
}

// ============================================================================
// Private Helper Methods
// ============================================================================

auto dicom_file::parse_meta_information(std::span<const uint8_t> data,
                                        size_t& bytes_read)
    -> pacs::Result<dicom_dataset> {
    // File Meta Information is always encoded as Explicit VR Little Endian
    // First, we need to find the group length to know how much to read

    dicom_dataset meta_info;
    size_t offset = 0;

    // Parse elements until we leave group 0x0002 or run out of data
    while (offset + 8 <= data.size()) {  // Minimum element size: 8 bytes
        // Read tag (4 bytes)
        const uint16_t group = read_uint16_le(data.subspan(offset, 2));
        const uint16_t element = read_uint16_le(data.subspan(offset + 2, 2));
        const dicom_tag tag{group, element};

        // Stop if we've left the meta information group
        if (group != 0x0002) {
            break;
        }

        // Read VR (2 bytes)
        if (offset + 6 > data.size()) {
            return pacs::pacs_error<dicom_dataset>(
                pacs::error_codes::decode_error,
                "Unexpected end of data while reading VR");
        }

        const char vr_chars[3] = {
            static_cast<char>(data[offset + 4]),
            static_cast<char>(data[offset + 5]),
            '\0'
        };
        const auto vr_opt = encoding::from_string(std::string_view(vr_chars, 2));
        if (!vr_opt) {
            return pacs::pacs_error<dicom_dataset>(
                pacs::error_codes::decode_error,
                "Invalid VR: " + std::string(vr_chars, 2));
        }
        const auto vr = *vr_opt;

        // Determine length field size based on VR
        uint32_t length = 0;
        size_t header_size = 0;

        if (encoding::has_explicit_32bit_length(vr)) {
            // VR + 2 reserved bytes + 4 byte length
            if (offset + 12 > data.size()) {
                return pacs::pacs_error<dicom_dataset>(
                    pacs::error_codes::decode_error,
                    "Unexpected end of data while reading length field");
            }
            length = read_uint32_le(data.subspan(offset + 8, 4));
            header_size = 12;
        } else {
            // VR + 2 byte length
            if (offset + 8 > data.size()) {
                return pacs::pacs_error<dicom_dataset>(
                    pacs::error_codes::decode_error,
                    "Unexpected end of data while reading length field");
            }
            length = read_uint16_le(data.subspan(offset + 6, 2));
            header_size = 8;
        }

        // Validate and read value
        if (offset + header_size + length > data.size()) {
            return pacs::pacs_error<dicom_dataset>(
                pacs::error_codes::decode_error,
                "Value length exceeds available data");
        }

        const auto value_data = data.subspan(offset + header_size, length);
        dicom_element elem{tag, vr, value_data};
        meta_info.insert(std::move(elem));

        offset += header_size + length;
    }

    bytes_read = offset;
    return pacs::Result<dicom_dataset>::ok(std::move(meta_info));
}

auto dicom_file::generate_meta_information(const dicom_dataset& dataset,
                                           const encoding::transfer_syntax& ts)
    -> dicom_dataset {
    dicom_dataset meta_info;

    // (0002,0001) File Meta Information Version
    const uint8_t version_bytes[] = {0x00, 0x01};
    meta_info.insert(dicom_element{
        tags::file_meta_information_version,
        encoding::vr_type::OB,
        std::span<const uint8_t>(version_bytes, 2)
    });

    // (0002,0002) Media Storage SOP Class UID
    const auto sop_class = dataset.get_string(tags::sop_class_uid);
    meta_info.set_string(
        tags::media_storage_sop_class_uid,
        encoding::vr_type::UI,
        sop_class
    );

    // (0002,0003) Media Storage SOP Instance UID
    const auto sop_instance = dataset.get_string(tags::sop_instance_uid);
    meta_info.set_string(
        tags::media_storage_sop_instance_uid,
        encoding::vr_type::UI,
        sop_instance
    );

    // (0002,0010) Transfer Syntax UID
    meta_info.set_string(
        tags::transfer_syntax_uid,
        encoding::vr_type::UI,
        std::string(ts.uid())
    );

    // (0002,0012) Implementation Class UID
    meta_info.set_string(
        tags::implementation_class_uid,
        encoding::vr_type::UI,
        std::string(kImplementationClassUid)
    );

    // (0002,0013) Implementation Version Name
    meta_info.set_string(
        tags::implementation_version_name,
        encoding::vr_type::SH,
        std::string(kImplementationVersionName)
    );

    return meta_info;
}

auto dicom_file::encode_explicit_vr_le(const dicom_dataset& dataset)
    -> std::vector<uint8_t> {
    std::vector<uint8_t> result;
    result.reserve(65536);  // Initial estimate

    for (const auto& [tag, element] : dataset) {
        // Write tag (4 bytes, little-endian)
        write_uint16_le(result, tag.group());
        write_uint16_le(result, tag.element());

        // Write VR (2 bytes)
        const auto vr_str = encoding::to_string(element.vr());
        result.push_back(static_cast<uint8_t>(vr_str[0]));
        result.push_back(static_cast<uint8_t>(vr_str[1]));

        const auto& raw_data = element.raw_data();
        const auto length = static_cast<uint32_t>(raw_data.size());

        if (encoding::has_explicit_32bit_length(element.vr())) {
            // 2 reserved bytes + 4 byte length
            result.push_back(0x00);
            result.push_back(0x00);
            write_uint32_le(result, length);
        } else {
            // 2 byte length
            write_uint16_le(result, static_cast<uint16_t>(length));
        }

        // Write value
        result.insert(result.end(), raw_data.begin(), raw_data.end());
    }

    return result;
}

auto dicom_file::decode_explicit_vr_le(std::span<const uint8_t> data,
                                       size_t& bytes_read)
    -> pacs::Result<dicom_dataset> {
    dicom_dataset dataset;
    size_t offset = 0;

    while (offset + 8 <= data.size()) {  // Minimum element size
        // Read tag (4 bytes)
        const uint16_t group = read_uint16_le(data.subspan(offset, 2));
        const uint16_t element = read_uint16_le(data.subspan(offset + 2, 2));
        const dicom_tag tag{group, element};

        // Skip item delimiter tags
        if (group == 0xFFFE) {
            break;
        }

        // Read VR (2 bytes)
        if (offset + 6 > data.size()) {
            break;
        }

        const char vr_chars[3] = {
            static_cast<char>(data[offset + 4]),
            static_cast<char>(data[offset + 5]),
            '\0'
        };
        const auto vr_opt = encoding::from_string(std::string_view(vr_chars, 2));
        if (!vr_opt) {
            // Unknown VR, treat as UN
            break;
        }
        const auto vr = *vr_opt;

        // Determine length field size
        uint32_t length = 0;
        size_t header_size = 0;

        if (encoding::has_explicit_32bit_length(vr)) {
            if (offset + 12 > data.size()) {
                break;
            }
            length = read_uint32_le(data.subspan(offset + 8, 4));
            header_size = 12;
        } else {
            if (offset + 8 > data.size()) {
                break;
            }
            length = read_uint16_le(data.subspan(offset + 6, 2));
            header_size = 8;
        }

        // Handle undefined length (0xFFFFFFFF) - skip for now
        if (length == 0xFFFFFFFF) {
            // TODO: Handle sequences with undefined length in Phase 2
            break;
        }

        // Validate value bounds
        if (offset + header_size + length > data.size()) {
            break;
        }

        // Read value and create element
        const auto value_data = data.subspan(offset + header_size, length);
        dicom_element elem{tag, vr, value_data};
        dataset.insert(std::move(elem));

        offset += header_size + length;
    }

    bytes_read = offset;
    return pacs::Result<dicom_dataset>::ok(std::move(dataset));
}

auto dicom_file::decode_implicit_vr_le(std::span<const uint8_t> data,
                                       size_t& bytes_read)
    -> pacs::Result<dicom_dataset> {
    dicom_dataset dataset;
    size_t offset = 0;
    const auto& dict = dicom_dictionary::instance();

    while (offset + 8 <= data.size()) {
        // Read tag (4 bytes, little-endian)
        const uint16_t group = read_uint16_le(data.subspan(offset, 2));
        const uint16_t element = read_uint16_le(data.subspan(offset + 2, 2));
        const dicom_tag tag{group, element};

        // Check for item/sequence delimiters
        if (group == kItemTagGroup) {
            if (element == kSequenceDelimitationElement ||
                element == kItemDelimitationElement) {
                break;
            }
        }

        // Read length (4 bytes in Implicit VR)
        if (offset + 8 > data.size()) {
            break;
        }
        uint32_t length = read_uint32_le(data.subspan(offset + 4, 4));

        // Look up VR from dictionary
        auto tag_info = dict.find(tag);
        encoding::vr_type vr = encoding::vr_type::UN;
        if (tag_info) {
            auto vr_opt = encoding::from_string(
                encoding::to_string(static_cast<encoding::vr_type>(tag_info->vr)));
            if (vr_opt) {
                vr = *vr_opt;
            }
        }

        constexpr size_t kImplicitHeaderSize = 8;  // 4 bytes tag + 4 bytes length

        // Handle undefined length
        if (length == kUndefinedLength) {
            // For sequences or pixel data with undefined length
            if (vr == encoding::vr_type::SQ) {
                size_t seq_bytes_read = 0;
                auto seq_result = parse_undefined_length_sequence(
                    data.subspan(offset + kImplicitHeaderSize),
                    seq_bytes_read, false, false);
                // Skip sequence for now (TODO: proper sequence support)
                offset += kImplicitHeaderSize + seq_bytes_read;
                continue;
            }
            // Skip to next item for undefined length pixel data
            break;
        }

        // Validate value bounds
        if (offset + kImplicitHeaderSize + length > data.size()) {
            break;
        }

        // Read value and create element
        const auto value_data = data.subspan(offset + kImplicitHeaderSize, length);
        dicom_element elem{tag, vr, value_data};
        dataset.insert(std::move(elem));

        offset += kImplicitHeaderSize + length;
    }

    bytes_read = offset;
    return pacs::Result<dicom_dataset>::ok(std::move(dataset));
}

auto dicom_file::decode_explicit_vr_be(std::span<const uint8_t> data,
                                       size_t& bytes_read)
    -> pacs::Result<dicom_dataset> {
    dicom_dataset dataset;
    size_t offset = 0;

    while (offset + 8 <= data.size()) {
        // Read tag (4 bytes, big-endian)
        const uint16_t group = read_uint16_be(data.subspan(offset, 2));
        const uint16_t element = read_uint16_be(data.subspan(offset + 2, 2));
        const dicom_tag tag{group, element};

        // Check for item/sequence delimiters
        if (group == kItemTagGroup) {
            break;
        }

        // Read VR (2 bytes ASCII)
        if (offset + 6 > data.size()) {
            break;
        }

        const char vr_chars[3] = {
            static_cast<char>(data[offset + 4]),
            static_cast<char>(data[offset + 5]),
            '\0'
        };
        const auto vr_opt = encoding::from_string(std::string_view(vr_chars, 2));
        if (!vr_opt) {
            break;
        }
        const auto vr = *vr_opt;

        // Determine length field size
        uint32_t length = 0;
        size_t header_size = 0;

        if (encoding::has_explicit_32bit_length(vr)) {
            if (offset + 12 > data.size()) {
                break;
            }
            // Skip 2 reserved bytes, then read 4-byte length (big-endian)
            length = read_uint32_be(data.subspan(offset + 8, 4));
            header_size = 12;
        } else {
            if (offset + 8 > data.size()) {
                break;
            }
            length = read_uint16_be(data.subspan(offset + 6, 2));
            header_size = 8;
        }

        // Handle undefined length
        if (length == kUndefinedLength) {
            if (vr == encoding::vr_type::SQ) {
                size_t seq_bytes_read = 0;
                auto seq_result = parse_undefined_length_sequence(
                    data.subspan(offset + header_size),
                    seq_bytes_read, true, true);
                offset += header_size + seq_bytes_read;
                continue;
            }
            break;
        }

        // Validate value bounds
        if (offset + header_size + length > data.size()) {
            break;
        }

        // Read value - need to swap bytes for numeric types
        const auto value_data = data.subspan(offset + header_size, length);
        std::vector<uint8_t> swapped_data(value_data.begin(), value_data.end());

        // Swap bytes for numeric VRs
        if (encoding::is_numeric_vr(vr)) {
            const size_t element_size = encoding::fixed_length(vr);
            if (element_size == 2) {
                for (size_t i = 0; i + 1 < swapped_data.size(); i += 2) {
                    std::swap(swapped_data[i], swapped_data[i + 1]);
                }
            } else if (element_size == 4) {
                for (size_t i = 0; i + 3 < swapped_data.size(); i += 4) {
                    std::swap(swapped_data[i], swapped_data[i + 3]);
                    std::swap(swapped_data[i + 1], swapped_data[i + 2]);
                }
            } else if (element_size == 8) {
                for (size_t i = 0; i + 7 < swapped_data.size(); i += 8) {
                    std::swap(swapped_data[i], swapped_data[i + 7]);
                    std::swap(swapped_data[i + 1], swapped_data[i + 6]);
                    std::swap(swapped_data[i + 2], swapped_data[i + 5]);
                    std::swap(swapped_data[i + 3], swapped_data[i + 4]);
                }
            }
        }

        dicom_element elem{tag, vr, std::span<const uint8_t>(swapped_data)};
        dataset.insert(std::move(elem));

        offset += header_size + length;
    }

    bytes_read = offset;
    return pacs::Result<dicom_dataset>::ok(std::move(dataset));
}

auto dicom_file::encode_implicit_vr_le(const dicom_dataset& dataset)
    -> std::vector<uint8_t> {
    std::vector<uint8_t> result;
    result.reserve(65536);

    for (const auto& [tag, element] : dataset) {
        // Write tag (4 bytes, little-endian)
        write_uint16_le(result, tag.group());
        write_uint16_le(result, tag.element());

        // Write length (4 bytes in Implicit VR)
        const auto& raw_data = element.raw_data();
        write_uint32_le(result, static_cast<uint32_t>(raw_data.size()));

        // Write value
        result.insert(result.end(), raw_data.begin(), raw_data.end());
    }

    return result;
}

auto dicom_file::encode_explicit_vr_be(const dicom_dataset& dataset)
    -> std::vector<uint8_t> {
    std::vector<uint8_t> result;
    result.reserve(65536);

    for (const auto& [tag, element] : dataset) {
        // Write tag (4 bytes, big-endian)
        write_uint16_be(result, tag.group());
        write_uint16_be(result, tag.element());

        // Write VR (2 bytes ASCII)
        const auto vr_str = encoding::to_string(element.vr());
        result.push_back(static_cast<uint8_t>(vr_str[0]));
        result.push_back(static_cast<uint8_t>(vr_str[1]));

        const auto& raw_data = element.raw_data();
        const auto length = static_cast<uint32_t>(raw_data.size());

        if (encoding::has_explicit_32bit_length(element.vr())) {
            // 2 reserved bytes + 4 byte length (big-endian)
            result.push_back(0x00);
            result.push_back(0x00);
            write_uint32_be(result, length);
        } else {
            // 2 byte length (big-endian)
            write_uint16_be(result, static_cast<uint16_t>(length));
        }

        // Write value - swap bytes for numeric types
        if (encoding::is_numeric_vr(element.vr())) {
            const size_t element_size = encoding::fixed_length(element.vr());
            std::vector<uint8_t> swapped_data(raw_data.begin(), raw_data.end());

            if (element_size == 2) {
                for (size_t i = 0; i + 1 < swapped_data.size(); i += 2) {
                    std::swap(swapped_data[i], swapped_data[i + 1]);
                }
            } else if (element_size == 4) {
                for (size_t i = 0; i + 3 < swapped_data.size(); i += 4) {
                    std::swap(swapped_data[i], swapped_data[i + 3]);
                    std::swap(swapped_data[i + 1], swapped_data[i + 2]);
                }
            } else if (element_size == 8) {
                for (size_t i = 0; i + 7 < swapped_data.size(); i += 8) {
                    std::swap(swapped_data[i], swapped_data[i + 7]);
                    std::swap(swapped_data[i + 1], swapped_data[i + 6]);
                    std::swap(swapped_data[i + 2], swapped_data[i + 5]);
                    std::swap(swapped_data[i + 3], swapped_data[i + 4]);
                }
            }
            result.insert(result.end(), swapped_data.begin(), swapped_data.end());
        } else {
            result.insert(result.end(), raw_data.begin(), raw_data.end());
        }
    }

    return result;
}

auto dicom_file::decode_dataset(std::span<const uint8_t> data,
                                const encoding::transfer_syntax& ts,
                                size_t& bytes_read)
    -> pacs::Result<dicom_dataset> {

    // Route to appropriate decoder based on Transfer Syntax properties
    if (ts.vr_type() == encoding::vr_encoding::implicit) {
        // Implicit VR Little Endian (only implicit is always LE)
        return decode_implicit_vr_le(data, bytes_read);
    }

    if (ts.endianness() == encoding::byte_order::big_endian) {
        // Explicit VR Big Endian
        return decode_explicit_vr_be(data, bytes_read);
    }

    // Explicit VR Little Endian (default and compressed transfer syntaxes)
    // Note: For compressed TS, pixel data is handled specially but
    // other elements are still Explicit VR LE
    return decode_explicit_vr_le(data, bytes_read);
}

auto dicom_file::encode_dataset(const dicom_dataset& dataset,
                                const encoding::transfer_syntax& ts)
    -> std::vector<uint8_t> {

    // Route to appropriate encoder based on Transfer Syntax properties
    if (ts.vr_type() == encoding::vr_encoding::implicit) {
        return encode_implicit_vr_le(dataset);
    }

    if (ts.endianness() == encoding::byte_order::big_endian) {
        return encode_explicit_vr_be(dataset);
    }

    // Explicit VR Little Endian (default)
    return encode_explicit_vr_le(dataset);
}

auto dicom_file::parse_undefined_length_sequence(
    std::span<const uint8_t> data, size_t& bytes_read,
    bool explicit_vr, bool big_endian)
    -> pacs::Result<std::vector<dicom_dataset>> {

    std::vector<dicom_dataset> items;
    size_t offset = 0;

    // Helper function pointers for reading based on endianness
    auto read_u16 = big_endian ? read_uint16_be : read_uint16_le;
    auto read_u32 = big_endian ? read_uint32_be : read_uint32_le;

    while (offset + 8 <= data.size()) {
        // Read item tag
        const uint16_t group = read_u16(data.subspan(offset, 2));
        const uint16_t element = read_u16(data.subspan(offset + 2, 2));

        // Check for sequence delimitation item
        if (group == kItemTagGroup && element == kSequenceDelimitationElement) {
            // Read length (should be 0)
            offset += 8;
            break;
        }

        // Must be an item tag (FFFE,E000)
        if (group != kItemTagGroup || element != kItemTagElement) {
            break;  // Unexpected tag
        }

        // Read item length
        uint32_t item_length = read_u32(data.subspan(offset + 4, 4));
        offset += 8;

        if (item_length == kUndefinedLength) {
            // Item with undefined length - find item delimitation tag
            size_t item_end = offset;
            while (item_end + 8 <= data.size()) {
                uint16_t g = read_u16(data.subspan(item_end, 2));
                uint16_t e = read_u16(data.subspan(item_end + 2, 2));
                if (g == kItemTagGroup && e == kItemDelimitationElement) {
                    break;
                }
                // Simple skip - move forward by element
                // This is a simplified approach; full implementation would parse elements
                item_end += 4;
                if (item_end + 4 > data.size()) break;
                uint32_t len = read_u32(data.subspan(item_end, 4));
                item_end += 4;
                if (len != kUndefinedLength) {
                    item_end += len;
                }
            }
            // Parse item content
            size_t item_bytes_read = 0;
            auto item_data = data.subspan(offset, item_end - offset);
            pacs::Result<dicom_dataset> item_result = explicit_vr
                ? (big_endian ? decode_explicit_vr_be(item_data, item_bytes_read)
                              : decode_explicit_vr_le(item_data, item_bytes_read))
                : decode_implicit_vr_le(item_data, item_bytes_read);

            if (item_result.is_ok()) {
                items.push_back(std::move(item_result.value()));
            }
            offset = item_end + 8;  // Skip delimitation item
        } else {
            // Item with defined length
            if (offset + item_length > data.size()) {
                break;
            }
            size_t item_bytes_read = 0;
            auto item_data = data.subspan(offset, item_length);
            pacs::Result<dicom_dataset> item_result = explicit_vr
                ? (big_endian ? decode_explicit_vr_be(item_data, item_bytes_read)
                              : decode_explicit_vr_le(item_data, item_bytes_read))
                : decode_implicit_vr_le(item_data, item_bytes_read);

            if (item_result.is_ok()) {
                items.push_back(std::move(item_result.value()));
            }
            offset += item_length;
        }
    }

    bytes_read = offset;
    return pacs::Result<std::vector<dicom_dataset>>::ok(std::move(items));
}

auto dicom_file::parse_encapsulated_frames(std::span<const uint8_t> data)
    -> std::vector<std::vector<uint8_t>> {

    std::vector<std::vector<uint8_t>> frames;
    size_t offset = 0;

    // First item is Basic Offset Table (may be empty)
    if (offset + 8 <= data.size()) {
        const uint16_t group = read_uint16_le(data.subspan(offset, 2));
        const uint16_t element = read_uint16_le(data.subspan(offset + 2, 2));

        if (group == kItemTagGroup && element == kItemTagElement) {
            uint32_t bot_length = read_uint32_le(data.subspan(offset + 4, 4));
            offset += 8 + bot_length;  // Skip BOT
        }
    }

    // Parse fragment items
    while (offset + 8 <= data.size()) {
        const uint16_t group = read_uint16_le(data.subspan(offset, 2));
        const uint16_t element = read_uint16_le(data.subspan(offset + 2, 2));

        // Check for sequence delimitation
        if (group == kItemTagGroup && element == kSequenceDelimitationElement) {
            break;
        }

        // Must be an item tag
        if (group != kItemTagGroup || element != kItemTagElement) {
            break;
        }

        uint32_t fragment_length = read_uint32_le(data.subspan(offset + 4, 4));
        offset += 8;

        if (offset + fragment_length > data.size()) {
            break;
        }

        // Copy fragment data
        std::vector<uint8_t> fragment(
            data.begin() + static_cast<std::ptrdiff_t>(offset),
            data.begin() + static_cast<std::ptrdiff_t>(offset + fragment_length));
        frames.push_back(std::move(fragment));

        offset += fragment_length;
    }

    return frames;
}

}  // namespace pacs::core
