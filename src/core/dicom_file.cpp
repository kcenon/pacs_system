/**
 * @file dicom_file.cpp
 * @brief Implementation of DICOM Part 10 file handling
 */

#include "pacs/core/dicom_file.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace pacs::core {

namespace {

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

    const auto ts_uid = ts_elem->as_string();
    encoding::transfer_syntax ts{ts_uid};

    if (!ts.is_valid()) {
        return pacs::pacs_error<dicom_file>(
            pacs::error_codes::unsupported_transfer_syntax,
            "Unsupported Transfer Syntax: " + ts_uid);
    }

    // Parse main dataset
    const auto dataset_start = meta_start.subspan(meta_bytes_read);
    size_t dataset_bytes_read = 0;

    // For Phase 1, only support Explicit VR Little Endian for simplicity
    // TODO: Add support for other transfer syntaxes in Phase 2
    auto dataset_result = decode_explicit_vr_le(dataset_start, dataset_bytes_read);
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

    // Encode and write File Meta Information
    auto meta_bytes = encode_explicit_vr_le(meta_info_);
    result.insert(result.end(), meta_bytes.begin(), meta_bytes.end());

    // Encode and write main dataset
    // TODO: Use appropriate transfer syntax encoding in Phase 2
    auto dataset_bytes = encode_explicit_vr_le(dataset_);
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

}  // namespace pacs::core
