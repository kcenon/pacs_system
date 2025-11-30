#ifndef PACS_NETWORK_PDU_ENCODER_HPP
#define PACS_NETWORK_PDU_ENCODER_HPP

#include "pdu_types.hpp"

#include <cstdint>
#include <vector>

namespace pacs::network {

/**
 * @brief Encoder for DICOM PDU (Protocol Data Unit) messages.
 *
 * This class provides static methods to encode various PDU types
 * according to DICOM PS3.8 Upper Layer Protocol.
 *
 * PDU Structure:
 * @code
 * ┌─────────────────────────────────────┐
 * │ PDU Header                          │
 * ├───────────┬───────────┬─────────────┤
 * │ Type      │ Reserved  │ Length      │
 * │ (1 byte)  │ (1 byte)  │ (4 bytes)   │
 * └───────────┴───────────┴─────────────┘
 * │ PDU Data (variable)                 │
 * └─────────────────────────────────────┘
 * @endcode
 *
 * @see DICOM PS3.8 Section 9 - Upper Layer Protocol
 */
class pdu_encoder {
public:
    /// @name Association PDUs
    /// @{

    /**
     * @brief Encodes an A-ASSOCIATE-RQ PDU.
     * @param rq The association request data
     * @return Encoded PDU bytes
     *
     * @see DICOM PS3.8 Section 9.3.2
     */
    [[nodiscard]] static std::vector<uint8_t> encode_associate_rq(
        const associate_rq& rq);

    /**
     * @brief Encodes an A-ASSOCIATE-AC PDU.
     * @param ac The association accept data
     * @return Encoded PDU bytes
     *
     * @see DICOM PS3.8 Section 9.3.3
     */
    [[nodiscard]] static std::vector<uint8_t> encode_associate_ac(
        const associate_ac& ac);

    /**
     * @brief Encodes an A-ASSOCIATE-RJ PDU.
     * @param rj The association reject data
     * @return Encoded PDU bytes
     *
     * @see DICOM PS3.8 Section 9.3.4
     */
    [[nodiscard]] static std::vector<uint8_t> encode_associate_rj(
        const associate_rj& rj);

    /// @}

    /// @name Release PDUs
    /// @{

    /**
     * @brief Encodes an A-RELEASE-RQ PDU.
     * @return Encoded PDU bytes (always 10 bytes)
     *
     * @see DICOM PS3.8 Section 9.3.6
     */
    [[nodiscard]] static std::vector<uint8_t> encode_release_rq();

    /**
     * @brief Encodes an A-RELEASE-RP PDU.
     * @return Encoded PDU bytes (always 10 bytes)
     *
     * @see DICOM PS3.8 Section 9.3.7
     */
    [[nodiscard]] static std::vector<uint8_t> encode_release_rp();

    /// @}

    /// @name Abort PDU
    /// @{

    /**
     * @brief Encodes an A-ABORT PDU.
     * @param source Abort source (0=UL service-user, 2=UL service-provider)
     * @param reason Abort reason (only applicable when source=2)
     * @return Encoded PDU bytes (always 10 bytes)
     *
     * @see DICOM PS3.8 Section 9.3.8
     */
    [[nodiscard]] static std::vector<uint8_t> encode_abort(
        uint8_t source, uint8_t reason);

    /**
     * @brief Encodes an A-ABORT PDU using typed enums.
     * @param source Abort source
     * @param reason Abort reason
     * @return Encoded PDU bytes
     */
    [[nodiscard]] static std::vector<uint8_t> encode_abort(
        abort_source source, abort_reason reason);

    /// @}

    /// @name Data PDU
    /// @{

    /**
     * @brief Encodes a P-DATA-TF PDU.
     * @param pdvs List of Presentation Data Values to send
     * @return Encoded PDU bytes
     *
     * P-DATA-TF can contain multiple PDV items. Each PDV has:
     * - 4-byte length
     * - 1-byte Presentation Context ID
     * - 1-byte Message Control Header
     * - Variable data
     *
     * @see DICOM PS3.8 Section 9.3.5
     */
    [[nodiscard]] static std::vector<uint8_t> encode_p_data_tf(
        const std::vector<presentation_data_value>& pdvs);

    /**
     * @brief Encodes a single PDV into a P-DATA-TF PDU.
     * @param pdv The Presentation Data Value
     * @return Encoded PDU bytes
     */
    [[nodiscard]] static std::vector<uint8_t> encode_p_data_tf(
        const presentation_data_value& pdv);

    /// @}

private:
    /// @name Helper Functions
    /// @{

    /**
     * @brief Writes a 16-bit unsigned integer in big-endian format.
     */
    static void write_uint16_be(std::vector<uint8_t>& buffer, uint16_t value);

    /**
     * @brief Writes a 32-bit unsigned integer in big-endian format.
     */
    static void write_uint32_be(std::vector<uint8_t>& buffer, uint32_t value);

    /**
     * @brief Writes an AE Title (16 bytes, space-padded).
     */
    static void write_ae_title(std::vector<uint8_t>& buffer,
                               const std::string& ae_title);

    /**
     * @brief Writes a UID string.
     */
    static void write_uid(std::vector<uint8_t>& buffer,
                          const std::string& uid);

    /**
     * @brief Updates the PDU length field at position 2-5.
     */
    static void update_pdu_length(std::vector<uint8_t>& buffer);

    /**
     * @brief Encodes an Application Context item.
     */
    static void encode_application_context(std::vector<uint8_t>& buffer,
                                           const std::string& context_name);

    /**
     * @brief Encodes a Presentation Context item for A-ASSOCIATE-RQ.
     */
    static void encode_presentation_context_rq(std::vector<uint8_t>& buffer,
                                               const presentation_context_rq& pc);

    /**
     * @brief Encodes a Presentation Context item for A-ASSOCIATE-AC.
     */
    static void encode_presentation_context_ac(std::vector<uint8_t>& buffer,
                                               const presentation_context_ac& pc);

    /**
     * @brief Encodes a User Information item.
     */
    static void encode_user_information(std::vector<uint8_t>& buffer,
                                        const user_information& user_info);

    /**
     * @brief Encodes the common header portion for ASSOCIATE-RQ/AC PDUs.
     */
    static void encode_associate_header(std::vector<uint8_t>& buffer,
                                        pdu_type type,
                                        const std::string& called_ae,
                                        const std::string& calling_ae);

    /// @}
};

}  // namespace pacs::network

#endif  // PACS_NETWORK_PDU_ENCODER_HPP
