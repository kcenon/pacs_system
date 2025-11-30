/**
 * @file dicom_file.hpp
 * @brief DICOM Part 10 file handling for reading/writing DICOM files
 *
 * This file provides the dicom_file class which represents a DICOM Part 10
 * file structure as specified in DICOM PS3.10.
 *
 * @see DICOM PS3.10 - Media Storage and File Format
 */

#pragma once

#include "dicom_dataset.hpp"
#include "dicom_tag_constants.hpp"

#include <pacs/encoding/transfer_syntax.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pacs::core {

/**
 * @brief Error codes for DICOM file operations
 */
enum class dicom_file_error {
    file_not_found,          ///< File does not exist
    file_read_error,         ///< Failed to read file
    file_write_error,        ///< Failed to write file
    invalid_dicom_file,      ///< Not a valid DICOM Part 10 file
    missing_dicm_prefix,     ///< Missing "DICM" magic bytes at offset 128
    invalid_meta_info,       ///< Invalid File Meta Information
    missing_transfer_syntax, ///< Transfer Syntax UID not found in meta info
    unsupported_transfer_syntax, ///< Transfer Syntax is not supported
    decode_error,            ///< Failed to decode data
    encode_error,            ///< Failed to encode data
};

/**
 * @brief Convert error code to human-readable string
 * @param error The error code
 * @return String description of the error
 */
[[nodiscard]] auto to_string(dicom_file_error error) -> std::string;

/**
 * @brief Simple result type for operations that can fail
 *
 * This is a C++20 compatible alternative to std::expected (C++23).
 *
 * @tparam T The success value type
 * @tparam E The error type
 */
template <typename T, typename E>
class result {
public:
    /// Construct a success result
    result(T value) : data_(std::move(value)) {}  // NOLINT: implicit

    /// Construct an error result
    result(E error) : data_(std::move(error)) {}  // NOLINT: implicit

    /// Check if the result contains a value
    [[nodiscard]] auto has_value() const noexcept -> bool {
        return std::holds_alternative<T>(data_);
    }

    /// Implicit conversion to bool
    explicit operator bool() const noexcept {
        return has_value();
    }

    /// Get the value (undefined if has_value() is false)
    [[nodiscard]] auto value() & -> T& {
        return std::get<T>(data_);
    }

    [[nodiscard]] auto value() const& -> const T& {
        return std::get<T>(data_);
    }

    [[nodiscard]] auto value() && -> T&& {
        return std::get<T>(std::move(data_));
    }

    /// Get the error (undefined if has_value() is true)
    [[nodiscard]] auto error() const -> const E& {
        return std::get<E>(data_);
    }

    /// Dereference operator
    [[nodiscard]] auto operator*() & -> T& { return value(); }
    [[nodiscard]] auto operator*() const& -> const T& { return value(); }
    [[nodiscard]] auto operator*() && -> T&& { return std::move(*this).value(); }

    /// Arrow operator
    [[nodiscard]] auto operator->() -> T* { return &value(); }
    [[nodiscard]] auto operator->() const -> const T* { return &value(); }

private:
    std::variant<T, E> data_;
};

/**
 * @brief Specialization of result for void value type
 */
template <typename E>
class result<void, E> {
public:
    /// Construct a success result
    result() : error_(std::nullopt) {}

    /// Construct an error result
    result(E error) : error_(std::move(error)) {}  // NOLINT: implicit

    /// Check if the result is successful
    [[nodiscard]] auto has_value() const noexcept -> bool {
        return !error_.has_value();
    }

    /// Implicit conversion to bool
    explicit operator bool() const noexcept {
        return has_value();
    }

    /// Get the error (undefined if has_value() is true)
    [[nodiscard]] auto error() const -> const E& {
        return *error_;
    }

private:
    std::optional<E> error_;
};

/**
 * @brief Represents a DICOM Part 10 file
 *
 * A DICOM Part 10 file consists of:
 * - 128-byte preamble (optional, may be zeros or vendor-specific)
 * - 4-byte "DICM" prefix
 * - File Meta Information (Group 0002, always Explicit VR LE)
 * - Main dataset (encoded per Transfer Syntax from meta info)
 *
 * This class supports reading and writing DICOM files with automatic
 * handling of the file structure and Transfer Syntax negotiation.
 *
 * @example
 * @code
 * // Reading a DICOM file
 * auto result = dicom_file::open("image.dcm");
 * if (result) {
 *     auto& file = *result;
 *     std::cout << "Patient: " << file.dataset().get_string(tags::patient_name) << "\n";
 *     std::cout << "Transfer Syntax: " << file.transfer_syntax().name() << "\n";
 * }
 *
 * // Creating a new DICOM file
 * dicom_dataset ds;
 * ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
 * ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
 * ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
 *
 * auto file = dicom_file::create(std::move(ds),
 *     encoding::transfer_syntax::explicit_vr_little_endian);
 * file.save("new_image.dcm");
 * @endcode
 */
class dicom_file {
public:
    /// Result type alias for convenience
    template <typename T>
    using file_result = result<T, dicom_file_error>;

    // ========================================================================
    // Static Factory Methods (Reading)
    // ========================================================================

    /**
     * @brief Open and read a DICOM file from disk
     * @param path Path to the DICOM file
     * @return Result containing the parsed file or an error
     */
    [[nodiscard]] static auto open(const std::filesystem::path& path)
        -> file_result<dicom_file>;

    /**
     * @brief Parse a DICOM file from raw bytes
     * @param data Raw byte data of the DICOM file
     * @return Result containing the parsed file or an error
     */
    [[nodiscard]] static auto from_bytes(std::span<const uint8_t> data)
        -> file_result<dicom_file>;

    // ========================================================================
    // Static Factory Methods (Creation)
    // ========================================================================

    /**
     * @brief Create a new DICOM file from a dataset
     * @param dataset The main dataset (SOP Class UID and Instance UID required)
     * @param ts The transfer syntax to use for encoding
     * @return A new dicom_file with auto-generated File Meta Information
     *
     * The File Meta Information will be automatically generated with:
     * - (0002,0001) File Meta Information Version = 0x00, 0x01
     * - (0002,0002) Media Storage SOP Class UID from dataset
     * - (0002,0003) Media Storage SOP Instance UID from dataset
     * - (0002,0010) Transfer Syntax UID
     * - (0002,0012) Implementation Class UID
     * - (0002,0013) Implementation Version Name
     */
    [[nodiscard]] static auto create(dicom_dataset dataset,
                                     const encoding::transfer_syntax& ts)
        -> dicom_file;

    // ========================================================================
    // Writing
    // ========================================================================

    /**
     * @brief Save the DICOM file to disk
     * @param path Destination file path
     * @return Result indicating success or failure
     */
    [[nodiscard]] auto save(const std::filesystem::path& path) const
        -> file_result<void>;

    /**
     * @brief Encode the DICOM file to raw bytes
     * @return Vector containing the encoded file data
     */
    [[nodiscard]] auto to_bytes() const -> std::vector<uint8_t>;

    // ========================================================================
    // Accessors
    // ========================================================================

    /**
     * @brief Get read-only access to the File Meta Information
     * @return Const reference to the meta information dataset
     */
    [[nodiscard]] auto meta_information() const noexcept -> const dicom_dataset&;

    /**
     * @brief Get mutable access to the File Meta Information
     * @return Reference to the meta information dataset
     */
    [[nodiscard]] auto meta_information() noexcept -> dicom_dataset&;

    /**
     * @brief Get read-only access to the main dataset
     * @return Const reference to the main dataset
     */
    [[nodiscard]] auto dataset() const noexcept -> const dicom_dataset&;

    /**
     * @brief Get mutable access to the main dataset
     * @return Reference to the main dataset
     */
    [[nodiscard]] auto dataset() noexcept -> dicom_dataset&;

    // ========================================================================
    // Convenience Accessors
    // ========================================================================

    /**
     * @brief Get the Transfer Syntax of this file
     * @return The transfer syntax (from File Meta Information)
     */
    [[nodiscard]] auto transfer_syntax() const -> encoding::transfer_syntax;

    /**
     * @brief Get the SOP Class UID
     * @return The SOP Class UID from the main dataset
     */
    [[nodiscard]] auto sop_class_uid() const -> std::string;

    /**
     * @brief Get the SOP Instance UID
     * @return The SOP Instance UID from the main dataset
     */
    [[nodiscard]] auto sop_instance_uid() const -> std::string;

    // ========================================================================
    // Construction
    // ========================================================================

    /**
     * @brief Default constructor - creates an empty file
     */
    dicom_file() = default;

    /**
     * @brief Copy constructor
     */
    dicom_file(const dicom_file&) = default;

    /**
     * @brief Move constructor
     */
    dicom_file(dicom_file&&) noexcept = default;

    /**
     * @brief Copy assignment
     */
    auto operator=(const dicom_file&) -> dicom_file& = default;

    /**
     * @brief Move assignment
     */
    auto operator=(dicom_file&&) noexcept -> dicom_file& = default;

    /**
     * @brief Destructor
     */
    ~dicom_file() = default;

private:
    /**
     * @brief Private constructor for internal use
     */
    dicom_file(dicom_dataset meta_info, dicom_dataset main_dataset);

    /**
     * @brief Parse file meta information from raw data
     * @param data Raw byte data starting at the meta information
     * @param bytes_read Output parameter for bytes consumed
     * @return Result containing the parsed meta info or error
     */
    [[nodiscard]] static auto parse_meta_information(
        std::span<const uint8_t> data, size_t& bytes_read)
        -> file_result<dicom_dataset>;

    /**
     * @brief Generate File Meta Information for a dataset
     * @param dataset The main dataset
     * @param ts The transfer syntax
     * @return The generated meta information dataset
     */
    [[nodiscard]] static auto generate_meta_information(
        const dicom_dataset& dataset, const encoding::transfer_syntax& ts)
        -> dicom_dataset;

    /**
     * @brief Encode a dataset using Explicit VR Little Endian
     * @param dataset The dataset to encode
     * @return Encoded byte data
     */
    [[nodiscard]] static auto encode_explicit_vr_le(const dicom_dataset& dataset)
        -> std::vector<uint8_t>;

    /**
     * @brief Decode a dataset from Explicit VR Little Endian format
     * @param data The raw byte data
     * @param bytes_read Output parameter for bytes consumed
     * @return Result containing the decoded dataset or error
     */
    [[nodiscard]] static auto decode_explicit_vr_le(
        std::span<const uint8_t> data, size_t& bytes_read)
        -> file_result<dicom_dataset>;

    /// File Meta Information (Group 0002)
    dicom_dataset meta_info_;

    /// Main dataset (encoded per Transfer Syntax)
    dicom_dataset dataset_;

    /// Implementation Class UID for this library
    static constexpr std::string_view kImplementationClassUid =
        "1.2.826.0.1.3680043.8.1055.1";

    /// Implementation Version Name
    static constexpr std::string_view kImplementationVersionName = "PACS_SYS_001";

    /// DICOM magic bytes
    static constexpr uint8_t kDicmPrefix[4] = {'D', 'I', 'C', 'M'};

    /// Preamble size
    static constexpr size_t kPreambleSize = 128;
};

}  // namespace pacs::core
