#ifndef PACS_ENCODING_BYTE_ORDER_HPP
#define PACS_ENCODING_BYTE_ORDER_HPP

namespace pacs::encoding {

/**
 * @brief Byte ordering for DICOM data encoding.
 *
 * DICOM supports both little-endian and big-endian byte ordering.
 * Little-endian is the most common and is used by default in DICOM.
 */
enum class byte_order {
    little_endian,  ///< Least significant byte first (most common)
    big_endian      ///< Most significant byte first (legacy, rarely used)
};

/**
 * @brief Value Representation encoding mode.
 *
 * Determines whether VR (Value Representation) is explicitly
 * encoded in the data stream or implicitly determined from the tag.
 */
enum class vr_encoding {
    implicit,    ///< VR determined from data dictionary lookup
    explicit_vr  ///< VR explicitly encoded in the data stream
};

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_BYTE_ORDER_HPP
