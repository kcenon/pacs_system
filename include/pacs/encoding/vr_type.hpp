#ifndef PACS_ENCODING_VR_TYPE_HPP
#define PACS_ENCODING_VR_TYPE_HPP

#include <cstdint>
#include <optional>
#include <string_view>

namespace pacs::encoding {

/**
 * @brief DICOM Value Representation (VR) types.
 *
 * Value Representation specifies the data type and format of the data element
 * value. Each VR is encoded as two ASCII characters (e.g., "PN" for Person Name).
 * The enum values are the uint16_t representation of these two ASCII characters.
 *
 * @see DICOM PS3.5 Section 6.2 - Value Representation (VR)
 */
enum class vr_type : uint16_t {
    // String VRs
    AE = 0x4145,  ///< Application Entity (16 chars max)
    AS = 0x4153,  ///< Age String (4 chars, format: nnnD/W/M/Y)
    CS = 0x4353,  ///< Code String (16 chars max, uppercase + digits + space + underscore)
    DA = 0x4441,  ///< Date (8 chars, format: YYYYMMDD)
    DS = 0x4453,  ///< Decimal String (16 chars max)
    DT = 0x4454,  ///< Date Time (26 chars max)
    IS = 0x4953,  ///< Integer String (12 chars max)
    LO = 0x4C4F,  ///< Long String (64 chars max)
    LT = 0x4C54,  ///< Long Text (10240 chars max)
    PN = 0x504E,  ///< Person Name (64 chars max per component group)
    SH = 0x5348,  ///< Short String (16 chars max)
    ST = 0x5354,  ///< Short Text (1024 chars max)
    TM = 0x544D,  ///< Time (14 chars max, format: HHMMSS.FFFFFF)
    UC = 0x5543,  ///< Unlimited Characters (2^32-2 max)
    UI = 0x5549,  ///< Unique Identifier (64 chars max)
    UR = 0x5552,  ///< Universal Resource Identifier (2^32-2 max)
    UT = 0x5554,  ///< Unlimited Text (2^32-2 max)

    // Numeric VRs (binary encoded)
    FL = 0x464C,  ///< Floating Point Single (4 bytes)
    FD = 0x4644,  ///< Floating Point Double (8 bytes)
    SL = 0x534C,  ///< Signed Long (4 bytes)
    SS = 0x5353,  ///< Signed Short (2 bytes)
    UL = 0x554C,  ///< Unsigned Long (4 bytes)
    US = 0x5553,  ///< Unsigned Short (2 bytes)

    // Binary VRs (raw bytes)
    OB = 0x4F42,  ///< Other Byte (variable length)
    OD = 0x4F44,  ///< Other Double (variable length)
    OF = 0x4F46,  ///< Other Float (variable length)
    OL = 0x4F4C,  ///< Other Long (variable length)
    OV = 0x4F56,  ///< Other 64-bit Very Long (variable length)
    OW = 0x4F57,  ///< Other Word (variable length)
    UN = 0x554E,  ///< Unknown (variable length)

    // Special VRs
    AT = 0x4154,  ///< Attribute Tag (4 bytes)
    SQ = 0x5351,  ///< Sequence of Items (undefined length)
    SV = 0x5356,  ///< Signed 64-bit Very Long (8 bytes)
    UV = 0x5556,  ///< Unsigned 64-bit Very Long (8 bytes)
};

/// @name String Conversion Functions
/// @{

/**
 * @brief Converts a vr_type to its two-character string representation.
 * @param vr The VR type to convert
 * @return A string_view containing the VR code (e.g., "PN", "US")
 *
 * Returns "??" for unknown or invalid VR types.
 */
[[nodiscard]] constexpr std::string_view to_string(vr_type vr) noexcept {
    switch (vr) {
        // String VRs
        case vr_type::AE: return "AE";
        case vr_type::AS: return "AS";
        case vr_type::CS: return "CS";
        case vr_type::DA: return "DA";
        case vr_type::DS: return "DS";
        case vr_type::DT: return "DT";
        case vr_type::IS: return "IS";
        case vr_type::LO: return "LO";
        case vr_type::LT: return "LT";
        case vr_type::PN: return "PN";
        case vr_type::SH: return "SH";
        case vr_type::ST: return "ST";
        case vr_type::TM: return "TM";
        case vr_type::UC: return "UC";
        case vr_type::UI: return "UI";
        case vr_type::UR: return "UR";
        case vr_type::UT: return "UT";
        // Numeric VRs
        case vr_type::FL: return "FL";
        case vr_type::FD: return "FD";
        case vr_type::SL: return "SL";
        case vr_type::SS: return "SS";
        case vr_type::UL: return "UL";
        case vr_type::US: return "US";
        // Binary VRs
        case vr_type::OB: return "OB";
        case vr_type::OD: return "OD";
        case vr_type::OF: return "OF";
        case vr_type::OL: return "OL";
        case vr_type::OV: return "OV";
        case vr_type::OW: return "OW";
        case vr_type::UN: return "UN";
        // Special VRs
        case vr_type::AT: return "AT";
        case vr_type::SQ: return "SQ";
        case vr_type::SV: return "SV";
        case vr_type::UV: return "UV";
        default: return "??";
    }
}

/**
 * @brief Parses a two-character string to a vr_type.
 * @param str The string to parse (must be exactly 2 characters)
 * @return The corresponding vr_type, or std::nullopt if not recognized
 */
[[nodiscard]] constexpr std::optional<vr_type> from_string(std::string_view str) noexcept {
    if (str.size() != 2) {
        return std::nullopt;
    }

    // Convert two ASCII chars to uint16_t (big-endian: first char is high byte)
    const auto code = static_cast<uint16_t>(
        (static_cast<uint16_t>(static_cast<unsigned char>(str[0])) << 8) |
        static_cast<uint16_t>(static_cast<unsigned char>(str[1]))
    );

    // Validate by checking if the code corresponds to a known VR
    switch (static_cast<vr_type>(code)) {
        case vr_type::AE: case vr_type::AS: case vr_type::AT:
        case vr_type::CS: case vr_type::DA: case vr_type::DS:
        case vr_type::DT: case vr_type::FD: case vr_type::FL:
        case vr_type::IS: case vr_type::LO: case vr_type::LT:
        case vr_type::OB: case vr_type::OD: case vr_type::OF:
        case vr_type::OL: case vr_type::OV: case vr_type::OW:
        case vr_type::PN: case vr_type::SH: case vr_type::SL:
        case vr_type::SQ: case vr_type::SS: case vr_type::ST:
        case vr_type::SV: case vr_type::TM: case vr_type::UC:
        case vr_type::UI: case vr_type::UL: case vr_type::UN:
        case vr_type::UR: case vr_type::US: case vr_type::UT:
        case vr_type::UV:
            return static_cast<vr_type>(code);
        default:
            return std::nullopt;
    }
}

/// @}

/// @name VR Category Classification Functions
/// @{

/**
 * @brief Checks if a VR is a string type.
 * @param vr The VR type to check
 * @return true if the VR represents string/text data
 *
 * String VRs contain character data that may need padding with spaces.
 */
[[nodiscard]] constexpr bool is_string_vr(vr_type vr) noexcept {
    switch (vr) {
        case vr_type::AE: case vr_type::AS: case vr_type::CS:
        case vr_type::DA: case vr_type::DS: case vr_type::DT:
        case vr_type::IS: case vr_type::LO: case vr_type::LT:
        case vr_type::PN: case vr_type::SH: case vr_type::ST:
        case vr_type::TM: case vr_type::UC: case vr_type::UI:
        case vr_type::UR: case vr_type::UT:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Checks if a VR is a binary/raw byte type.
 * @param vr The VR type to check
 * @return true if the VR represents raw binary data
 *
 * Binary VRs include OB, OD, OF, OL, OV, OW, and UN.
 */
[[nodiscard]] constexpr bool is_binary_vr(vr_type vr) noexcept {
    switch (vr) {
        case vr_type::OB: case vr_type::OD: case vr_type::OF:
        case vr_type::OL: case vr_type::OV: case vr_type::OW:
        case vr_type::UN:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Checks if a VR is a numeric type.
 * @param vr The VR type to check
 * @return true if the VR represents numeric data (binary encoded)
 *
 * Numeric VRs include FL, FD, SL, SS, SV, UL, US, UV.
 */
[[nodiscard]] constexpr bool is_numeric_vr(vr_type vr) noexcept {
    switch (vr) {
        case vr_type::FL: case vr_type::FD:
        case vr_type::SL: case vr_type::SS: case vr_type::SV:
        case vr_type::UL: case vr_type::US: case vr_type::UV:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Checks if a VR requires 32-bit length field in Explicit VR encoding.
 * @param vr The VR type to check
 * @return true if the VR requires extended 32-bit length encoding
 *
 * In Explicit VR encoding, these VRs have a 2-byte reserved field followed
 * by a 4-byte length field, instead of a 2-byte length field.
 *
 * @see DICOM PS3.5 Section 7.1.2 - Data Element Structure with Explicit VR
 */
[[nodiscard]] constexpr bool has_explicit_32bit_length(vr_type vr) noexcept {
    switch (vr) {
        case vr_type::OB: case vr_type::OD: case vr_type::OF:
        case vr_type::OL: case vr_type::OV: case vr_type::OW:
        case vr_type::SQ: case vr_type::SV: case vr_type::UC:
        case vr_type::UN: case vr_type::UR: case vr_type::UT:
        case vr_type::UV:
            return true;
        default:
            return false;
    }
}

/// @}

/// @name Additional VR Utility Functions
/// @{

/**
 * @brief Gets the fixed size of a VR if applicable.
 * @param vr The VR type to check
 * @return The fixed size in bytes, or 0 if the VR has variable length
 *
 * Fixed-length VRs have a predetermined size regardless of value.
 */
[[nodiscard]] constexpr std::size_t fixed_length(vr_type vr) noexcept {
    switch (vr) {
        case vr_type::AT: return 4;  // Attribute Tag
        case vr_type::FL: return 4;  // Float
        case vr_type::FD: return 8;  // Double
        case vr_type::SL: return 4;  // Signed Long
        case vr_type::SS: return 2;  // Signed Short
        case vr_type::SV: return 8;  // Signed 64-bit
        case vr_type::UL: return 4;  // Unsigned Long
        case vr_type::US: return 2;  // Unsigned Short
        case vr_type::UV: return 8;  // Unsigned 64-bit
        default: return 0;           // Variable length
    }
}

/**
 * @brief Checks if a VR has a fixed length.
 * @param vr The VR type to check
 * @return true if the VR has a fixed length
 */
[[nodiscard]] constexpr bool is_fixed_length(vr_type vr) noexcept {
    return fixed_length(vr) > 0;
}

/**
 * @brief Gets the padding character for a VR.
 * @param vr The VR type to check
 * @return The padding character (' ' for most strings, '\0' for UI, 0 for binary)
 *
 * DICOM requires data elements to have even length. String VRs are padded
 * with space (' ') except for UI which is padded with null ('\0').
 */
[[nodiscard]] constexpr char padding_char(vr_type vr) noexcept {
    if (vr == vr_type::UI) {
        return '\0';  // UI uses null padding
    }
    if (is_string_vr(vr)) {
        return ' ';   // Other string VRs use space padding
    }
    return '\0';      // Binary VRs use null padding if needed
}

/// @}

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_VR_TYPE_HPP
