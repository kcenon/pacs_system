#include "pacs/network/pdu_encoder.hpp"

#include <algorithm>
#include <stdexcept>

namespace pacs::network {

// ============================================================================
// Helper Functions
// ============================================================================

void pdu_encoder::write_uint16_be(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

void pdu_encoder::write_uint32_be(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

void pdu_encoder::write_ae_title(std::vector<uint8_t>& buffer,
                                  const std::string& ae_title) {
    // AE Title is exactly 16 bytes, space-padded
    std::string padded = ae_title;
    if (padded.size() > AE_TITLE_LENGTH) {
        padded = padded.substr(0, AE_TITLE_LENGTH);
    }
    padded.resize(AE_TITLE_LENGTH, ' ');

    buffer.insert(buffer.end(), padded.begin(), padded.end());
}

void pdu_encoder::write_uid(std::vector<uint8_t>& buffer, const std::string& uid) {
    buffer.insert(buffer.end(), uid.begin(), uid.end());
    // Pad to even length if necessary
    if (uid.size() % 2 != 0) {
        buffer.push_back(0x00);
    }
}

void pdu_encoder::update_pdu_length(std::vector<uint8_t>& buffer) {
    if (buffer.size() < 6) {
        return;
    }
    // PDU length is bytes 2-5 (0-indexed), and covers data after the 6-byte header
    const uint32_t length = static_cast<uint32_t>(buffer.size() - 6);
    buffer[2] = static_cast<uint8_t>((length >> 24) & 0xFF);
    buffer[3] = static_cast<uint8_t>((length >> 16) & 0xFF);
    buffer[4] = static_cast<uint8_t>((length >> 8) & 0xFF);
    buffer[5] = static_cast<uint8_t>(length & 0xFF);
}

// ============================================================================
// Item Encoding
// ============================================================================

void pdu_encoder::encode_application_context(std::vector<uint8_t>& buffer,
                                              const std::string& context_name) {
    // Application Context Item structure:
    // - Item type (1 byte) = 0x10
    // - Reserved (1 byte)
    // - Item length (2 bytes)
    // - Application Context Name (variable)

    buffer.push_back(static_cast<uint8_t>(item_type::application_context));
    buffer.push_back(0x00);  // Reserved

    // Calculate padded length
    size_t uid_length = context_name.size();
    if (uid_length % 2 != 0) {
        uid_length++;
    }

    write_uint16_be(buffer, static_cast<uint16_t>(uid_length));
    write_uid(buffer, context_name);
}

void pdu_encoder::encode_presentation_context_rq(std::vector<uint8_t>& buffer,
                                                  const presentation_context_rq& pc) {
    // Presentation Context Item (RQ) structure:
    // - Item type (1 byte) = 0x20
    // - Reserved (1 byte)
    // - Item length (2 bytes)
    // - Presentation Context ID (1 byte)
    // - Reserved (1 byte)
    // - Reserved (1 byte)
    // - Reserved (1 byte)
    // - Abstract Syntax sub-item
    // - Transfer Syntax sub-items

    const size_t start_pos = buffer.size();

    buffer.push_back(static_cast<uint8_t>(item_type::presentation_context_rq));
    buffer.push_back(0x00);  // Reserved

    // Placeholder for item length
    const size_t length_pos = buffer.size();
    write_uint16_be(buffer, 0x0000);

    // Presentation Context ID
    buffer.push_back(pc.id);
    buffer.push_back(0x00);  // Reserved
    buffer.push_back(0x00);  // Reserved
    buffer.push_back(0x00);  // Reserved

    // Abstract Syntax sub-item
    buffer.push_back(static_cast<uint8_t>(item_type::abstract_syntax));
    buffer.push_back(0x00);  // Reserved

    size_t abs_length = pc.abstract_syntax.size();
    if (abs_length % 2 != 0) {
        abs_length++;
    }
    write_uint16_be(buffer, static_cast<uint16_t>(abs_length));
    write_uid(buffer, pc.abstract_syntax);

    // Transfer Syntax sub-items
    for (const auto& ts : pc.transfer_syntaxes) {
        buffer.push_back(static_cast<uint8_t>(item_type::transfer_syntax));
        buffer.push_back(0x00);  // Reserved

        size_t ts_length = ts.size();
        if (ts_length % 2 != 0) {
            ts_length++;
        }
        write_uint16_be(buffer, static_cast<uint16_t>(ts_length));
        write_uid(buffer, ts);
    }

    // Update item length
    const uint16_t item_length = static_cast<uint16_t>(buffer.size() - start_pos - 4);
    buffer[length_pos] = static_cast<uint8_t>((item_length >> 8) & 0xFF);
    buffer[length_pos + 1] = static_cast<uint8_t>(item_length & 0xFF);
}

void pdu_encoder::encode_presentation_context_ac(std::vector<uint8_t>& buffer,
                                                  const presentation_context_ac& pc) {
    // Presentation Context Item (AC) structure:
    // - Item type (1 byte) = 0x21
    // - Reserved (1 byte)
    // - Item length (2 bytes)
    // - Presentation Context ID (1 byte)
    // - Reserved (1 byte)
    // - Result/Reason (1 byte)
    // - Reserved (1 byte)
    // - Transfer Syntax sub-item

    const size_t start_pos = buffer.size();

    buffer.push_back(static_cast<uint8_t>(item_type::presentation_context_ac));
    buffer.push_back(0x00);  // Reserved

    // Placeholder for item length
    const size_t length_pos = buffer.size();
    write_uint16_be(buffer, 0x0000);

    buffer.push_back(pc.id);
    buffer.push_back(0x00);  // Reserved
    buffer.push_back(static_cast<uint8_t>(pc.result));
    buffer.push_back(0x00);  // Reserved

    // Transfer Syntax sub-item (only if accepted)
    if (pc.result == presentation_context_result::acceptance &&
        !pc.transfer_syntax.empty()) {
        buffer.push_back(static_cast<uint8_t>(item_type::transfer_syntax));
        buffer.push_back(0x00);  // Reserved

        size_t ts_length = pc.transfer_syntax.size();
        if (ts_length % 2 != 0) {
            ts_length++;
        }
        write_uint16_be(buffer, static_cast<uint16_t>(ts_length));
        write_uid(buffer, pc.transfer_syntax);
    }

    // Update item length
    const uint16_t item_length = static_cast<uint16_t>(buffer.size() - start_pos - 4);
    buffer[length_pos] = static_cast<uint8_t>((item_length >> 8) & 0xFF);
    buffer[length_pos + 1] = static_cast<uint8_t>(item_length & 0xFF);
}

void pdu_encoder::encode_user_information(std::vector<uint8_t>& buffer,
                                           const user_information& user_info) {
    // User Information Item structure:
    // - Item type (1 byte) = 0x50
    // - Reserved (1 byte)
    // - Item length (2 bytes)
    // - User data sub-items

    const size_t start_pos = buffer.size();

    buffer.push_back(static_cast<uint8_t>(item_type::user_information));
    buffer.push_back(0x00);  // Reserved

    // Placeholder for item length
    const size_t length_pos = buffer.size();
    write_uint16_be(buffer, 0x0000);

    // Maximum Length sub-item (required)
    buffer.push_back(static_cast<uint8_t>(item_type::maximum_length));
    buffer.push_back(0x00);  // Reserved
    write_uint16_be(buffer, 0x0004);  // Length is always 4
    write_uint32_be(buffer, user_info.max_pdu_length);

    // Implementation Class UID sub-item (required)
    if (!user_info.implementation_class_uid.empty()) {
        buffer.push_back(static_cast<uint8_t>(item_type::implementation_class_uid));
        buffer.push_back(0x00);  // Reserved

        size_t uid_length = user_info.implementation_class_uid.size();
        if (uid_length % 2 != 0) {
            uid_length++;
        }
        write_uint16_be(buffer, static_cast<uint16_t>(uid_length));
        write_uid(buffer, user_info.implementation_class_uid);
    }

    // Implementation Version Name sub-item (optional)
    if (!user_info.implementation_version_name.empty()) {
        buffer.push_back(static_cast<uint8_t>(item_type::implementation_version_name));
        buffer.push_back(0x00);  // Reserved

        size_t name_length = user_info.implementation_version_name.size();
        if (name_length % 2 != 0) {
            name_length++;
        }
        write_uint16_be(buffer, static_cast<uint16_t>(name_length));

        buffer.insert(buffer.end(),
                      user_info.implementation_version_name.begin(),
                      user_info.implementation_version_name.end());
        if (user_info.implementation_version_name.size() % 2 != 0) {
            buffer.push_back(' ');  // Pad with space
        }
    }

    // SCP/SCU Role Selection sub-items (optional)
    for (const auto& role : user_info.role_selections) {
        buffer.push_back(static_cast<uint8_t>(item_type::scp_scu_role_selection));
        buffer.push_back(0x00);  // Reserved

        size_t uid_len = role.sop_class_uid.size();
        if (uid_len % 2 != 0) {
            uid_len++;
        }
        // Item length = UID length field (2) + UID + SCU role (1) + SCP role (1)
        write_uint16_be(buffer, static_cast<uint16_t>(2 + uid_len + 2));

        // UID length
        write_uint16_be(buffer, static_cast<uint16_t>(role.sop_class_uid.size()));
        write_uid(buffer, role.sop_class_uid);

        buffer.push_back(role.scu_role ? 0x01 : 0x00);
        buffer.push_back(role.scp_role ? 0x01 : 0x00);
    }

    // Update item length
    const uint16_t item_length = static_cast<uint16_t>(buffer.size() - start_pos - 4);
    buffer[length_pos] = static_cast<uint8_t>((item_length >> 8) & 0xFF);
    buffer[length_pos + 1] = static_cast<uint8_t>(item_length & 0xFF);
}

void pdu_encoder::encode_associate_header(std::vector<uint8_t>& buffer,
                                           pdu_type type,
                                           const std::string& called_ae,
                                           const std::string& calling_ae) {
    // PDU Header
    buffer.push_back(static_cast<uint8_t>(type));
    buffer.push_back(0x00);  // Reserved

    // PDU Length placeholder (4 bytes)
    write_uint32_be(buffer, 0x00000000);

    // Protocol Version (2 bytes)
    write_uint16_be(buffer, DICOM_PROTOCOL_VERSION);

    // Reserved (2 bytes)
    write_uint16_be(buffer, 0x0000);

    // Called AE Title (16 bytes)
    write_ae_title(buffer, called_ae);

    // Calling AE Title (16 bytes)
    write_ae_title(buffer, calling_ae);

    // Reserved (32 bytes)
    buffer.insert(buffer.end(), 32, 0x00);
}

// ============================================================================
// PDU Encoding Functions
// ============================================================================

std::vector<uint8_t> pdu_encoder::encode_associate_rq(const associate_rq& rq) {
    std::vector<uint8_t> buffer;
    buffer.reserve(512);  // Pre-allocate reasonable size

    encode_associate_header(buffer, pdu_type::associate_rq,
                            rq.called_ae_title, rq.calling_ae_title);

    // Variable Items
    encode_application_context(buffer, rq.application_context.empty()
                                           ? DICOM_APPLICATION_CONTEXT
                                           : rq.application_context);

    for (const auto& pc : rq.presentation_contexts) {
        encode_presentation_context_rq(buffer, pc);
    }

    encode_user_information(buffer, rq.user_info);

    update_pdu_length(buffer);
    return buffer;
}

std::vector<uint8_t> pdu_encoder::encode_associate_ac(const associate_ac& ac) {
    std::vector<uint8_t> buffer;
    buffer.reserve(512);

    encode_associate_header(buffer, pdu_type::associate_ac,
                            ac.called_ae_title, ac.calling_ae_title);

    // Variable Items
    encode_application_context(buffer, ac.application_context.empty()
                                           ? DICOM_APPLICATION_CONTEXT
                                           : ac.application_context);

    for (const auto& pc : ac.presentation_contexts) {
        encode_presentation_context_ac(buffer, pc);
    }

    encode_user_information(buffer, ac.user_info);

    update_pdu_length(buffer);
    return buffer;
}

std::vector<uint8_t> pdu_encoder::encode_associate_rj(const associate_rj& rj) {
    // A-ASSOCIATE-RJ PDU structure (10 bytes total):
    // - PDU Type (1 byte) = 0x03
    // - Reserved (1 byte)
    // - PDU Length (4 bytes) = 0x00000004
    // - Reserved (1 byte)
    // - Result (1 byte)
    // - Source (1 byte)
    // - Reason/Diag (1 byte)

    std::vector<uint8_t> buffer;
    buffer.reserve(10);

    buffer.push_back(static_cast<uint8_t>(pdu_type::associate_rj));
    buffer.push_back(0x00);  // Reserved
    write_uint32_be(buffer, 0x00000004);  // PDU Length

    buffer.push_back(0x00);  // Reserved
    buffer.push_back(static_cast<uint8_t>(rj.result));
    buffer.push_back(rj.source);
    buffer.push_back(rj.reason);

    return buffer;
}

std::vector<uint8_t> pdu_encoder::encode_release_rq() {
    // A-RELEASE-RQ PDU structure (10 bytes total):
    // - PDU Type (1 byte) = 0x05
    // - Reserved (1 byte)
    // - PDU Length (4 bytes) = 0x00000004
    // - Reserved (4 bytes)

    std::vector<uint8_t> buffer;
    buffer.reserve(10);

    buffer.push_back(static_cast<uint8_t>(pdu_type::release_rq));
    buffer.push_back(0x00);  // Reserved
    write_uint32_be(buffer, 0x00000004);  // PDU Length
    write_uint32_be(buffer, 0x00000000);  // Reserved

    return buffer;
}

std::vector<uint8_t> pdu_encoder::encode_release_rp() {
    // A-RELEASE-RP PDU structure (10 bytes total):
    // - PDU Type (1 byte) = 0x06
    // - Reserved (1 byte)
    // - PDU Length (4 bytes) = 0x00000004
    // - Reserved (4 bytes)

    std::vector<uint8_t> buffer;
    buffer.reserve(10);

    buffer.push_back(static_cast<uint8_t>(pdu_type::release_rp));
    buffer.push_back(0x00);  // Reserved
    write_uint32_be(buffer, 0x00000004);  // PDU Length
    write_uint32_be(buffer, 0x00000000);  // Reserved

    return buffer;
}

std::vector<uint8_t> pdu_encoder::encode_abort(uint8_t source, uint8_t reason) {
    // A-ABORT PDU structure (10 bytes total):
    // - PDU Type (1 byte) = 0x07
    // - Reserved (1 byte)
    // - PDU Length (4 bytes) = 0x00000004
    // - Reserved (1 byte)
    // - Reserved (1 byte)
    // - Source (1 byte)
    // - Reason/Diag (1 byte)

    std::vector<uint8_t> buffer;
    buffer.reserve(10);

    buffer.push_back(static_cast<uint8_t>(pdu_type::abort));
    buffer.push_back(0x00);  // Reserved
    write_uint32_be(buffer, 0x00000004);  // PDU Length
    buffer.push_back(0x00);  // Reserved
    buffer.push_back(0x00);  // Reserved
    buffer.push_back(source);
    buffer.push_back(reason);

    return buffer;
}

std::vector<uint8_t> pdu_encoder::encode_abort(abort_source source,
                                                abort_reason reason) {
    return encode_abort(static_cast<uint8_t>(source),
                        static_cast<uint8_t>(reason));
}

std::vector<uint8_t> pdu_encoder::encode_p_data_tf(
    const std::vector<presentation_data_value>& pdvs) {

    std::vector<uint8_t> buffer;
    buffer.reserve(256);

    // PDU Header
    buffer.push_back(static_cast<uint8_t>(pdu_type::p_data_tf));
    buffer.push_back(0x00);  // Reserved

    // PDU Length placeholder
    write_uint32_be(buffer, 0x00000000);

    // PDV Items
    for (const auto& pdv : pdvs) {
        // PDV Item structure:
        // - Item length (4 bytes) = context_id (1) + control (1) + data
        // - Presentation Context ID (1 byte)
        // - Message Control Header (1 byte)
        // - Presentation Data Value (variable)

        const uint32_t pdv_length = static_cast<uint32_t>(2 + pdv.data.size());
        write_uint32_be(buffer, pdv_length);

        buffer.push_back(pdv.context_id);

        // Message Control Header:
        // - Bit 0: 0 = Data, 1 = Command
        // - Bit 1: 0 = Not last, 1 = Last
        uint8_t control = 0;
        if (pdv.is_command) {
            control |= 0x01;
        }
        if (pdv.is_last) {
            control |= 0x02;
        }
        buffer.push_back(control);

        buffer.insert(buffer.end(), pdv.data.begin(), pdv.data.end());
    }

    update_pdu_length(buffer);
    return buffer;
}

std::vector<uint8_t> pdu_encoder::encode_p_data_tf(
    const presentation_data_value& pdv) {
    return encode_p_data_tf(std::vector<presentation_data_value>{pdv});
}

}  // namespace pacs::network
