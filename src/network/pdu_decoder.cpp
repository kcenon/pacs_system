#include "pacs/network/pdu_decoder.hpp"

#include <algorithm>
#include <cstring>
#include <tuple>

namespace pacs::network {

namespace {

/// Minimum PDU header size (type + reserved + length)
constexpr size_t PDU_HEADER_SIZE = 6;

/// Fixed size PDUs (A-RELEASE-RQ, A-RELEASE-RP, A-ABORT, A-ASSOCIATE-RJ)
constexpr size_t FIXED_PDU_SIZE = 10;

/// ASSOCIATE PDU header size after 6-byte PDU header
/// (version + reserved + called AE + calling AE + reserved)
constexpr size_t ASSOCIATE_HEADER_SIZE = 68;  // 2 + 2 + 16 + 16 + 32

#ifdef PACS_WITH_COMMON_SYSTEM
template<typename T>
DecodeResult<T> make_error(pdu_decode_error err, const std::string& msg = "") {
    std::string full_msg = to_string(err);
    if (!msg.empty()) {
        full_msg += ": " + msg;
    }
    return DecodeResult<T>::err(kcenon::common::error_info(
        static_cast<int>(err), full_msg, "pacs::network::pdu_decoder"));
}

template<typename T>
DecodeResult<T> make_ok(T value) {
    return DecodeResult<T>::ok(std::move(value));
}
#else
template<typename T>
DecodeResult<T> make_error(pdu_decode_error err, const std::string& msg = "") {
    std::string full_msg = to_string(err);
    if (!msg.empty()) {
        full_msg += ": " + msg;
    }
    return DecodeResult<T>::error(err, full_msg);
}

template<typename T>
DecodeResult<T> make_ok(T value) {
    return DecodeResult<T>::ok(std::move(value));
}
#endif

}  // namespace

// ============================================================================
// Helper Functions
// ============================================================================

uint16_t pdu_decoder::read_uint16_be(std::span<const uint8_t> data, size_t offset) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[offset]) << 8) |
        static_cast<uint16_t>(data[offset + 1]));
}

uint32_t pdu_decoder::read_uint32_be(std::span<const uint8_t> data, size_t offset) {
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

std::string pdu_decoder::read_ae_title(std::span<const uint8_t> data, size_t offset) {
    std::string ae_title(reinterpret_cast<const char*>(data.data() + offset),
                         AE_TITLE_LENGTH);
    // Trim trailing spaces
    auto end = ae_title.find_last_not_of(' ');
    if (end != std::string::npos) {
        ae_title.resize(end + 1);
    } else if (ae_title.find_first_not_of(' ') == std::string::npos) {
        ae_title.clear();  // All spaces
    }
    return ae_title;
}

std::string pdu_decoder::read_uid(std::span<const uint8_t> data, size_t offset,
                                   size_t length) {
    std::string uid(reinterpret_cast<const char*>(data.data() + offset), length);
    // Trim trailing nulls and spaces
    while (!uid.empty() && (uid.back() == '\0' || uid.back() == ' ')) {
        uid.pop_back();
    }
    return uid;
}

DecodeResult<uint32_t> pdu_decoder::validate_pdu_header(
    std::span<const uint8_t> data, uint8_t expected_type) {

    if (data.size() < PDU_HEADER_SIZE) {
        return make_error<uint32_t>(pdu_decode_error::incomplete_header);
    }

    if (expected_type != 0 && data[0] != expected_type) {
        return make_error<uint32_t>(pdu_decode_error::invalid_pdu_type,
            "Expected type " + std::to_string(expected_type) +
            ", got " + std::to_string(data[0]));
    }

    const uint32_t pdu_length = read_uint32_be(data, 2);
    const size_t total_length = PDU_HEADER_SIZE + pdu_length;

    if (data.size() < total_length) {
        return make_error<uint32_t>(pdu_decode_error::incomplete_pdu,
            "Need " + std::to_string(total_length) +
            " bytes, have " + std::to_string(data.size()));
    }

    return make_ok(pdu_length);
}

// ============================================================================
// General Decoding
// ============================================================================

std::optional<size_t> pdu_decoder::pdu_length(std::span<const uint8_t> data) {
    if (data.size() < PDU_HEADER_SIZE) {
        return std::nullopt;
    }

    const uint32_t length = read_uint32_be(data, 2);
    const size_t total = PDU_HEADER_SIZE + length;

    // Return the total length even if we don't have all the data yet.
    // This allows callers to know how much data is needed for a complete PDU.
    return total;
}

std::optional<pdu_type> pdu_decoder::peek_pdu_type(std::span<const uint8_t> data) {
    if (data.empty()) {
        return std::nullopt;
    }

    switch (data[0]) {
        case 0x01: return pdu_type::associate_rq;
        case 0x02: return pdu_type::associate_ac;
        case 0x03: return pdu_type::associate_rj;
        case 0x04: return pdu_type::p_data_tf;
        case 0x05: return pdu_type::release_rq;
        case 0x06: return pdu_type::release_rp;
        case 0x07: return pdu_type::abort;
        default: return std::nullopt;
    }
}

DecodeResult<pdu> pdu_decoder::decode(std::span<const uint8_t> data) {
    if (data.size() < PDU_HEADER_SIZE) {
        return make_error<pdu>(pdu_decode_error::incomplete_header);
    }

    const uint8_t type = data[0];

    switch (type) {
        case 0x01: {
            auto result = decode_associate_rq(data);
            if (result.is_ok()) {
                return make_ok<pdu>(std::move(result.value()));
            }
#ifdef PACS_WITH_COMMON_SYSTEM
            return make_error<pdu>(pdu_decode_error::malformed_pdu,
                result.error().message);
#else
            return make_error<pdu>(result.error_code(), result.error_message());
#endif
        }
        case 0x02: {
            auto result = decode_associate_ac(data);
            if (result.is_ok()) {
                return make_ok<pdu>(std::move(result.value()));
            }
#ifdef PACS_WITH_COMMON_SYSTEM
            return make_error<pdu>(pdu_decode_error::malformed_pdu,
                result.error().message);
#else
            return make_error<pdu>(result.error_code(), result.error_message());
#endif
        }
        case 0x03: {
            auto result = decode_associate_rj(data);
            if (result.is_ok()) {
                return make_ok<pdu>(std::move(result.value()));
            }
#ifdef PACS_WITH_COMMON_SYSTEM
            return make_error<pdu>(pdu_decode_error::malformed_pdu,
                result.error().message);
#else
            return make_error<pdu>(result.error_code(), result.error_message());
#endif
        }
        case 0x04: {
            auto result = decode_p_data_tf(data);
            if (result.is_ok()) {
                return make_ok<pdu>(std::move(result.value()));
            }
#ifdef PACS_WITH_COMMON_SYSTEM
            return make_error<pdu>(pdu_decode_error::malformed_pdu,
                result.error().message);
#else
            return make_error<pdu>(result.error_code(), result.error_message());
#endif
        }
        case 0x05: {
            auto result = decode_release_rq(data);
            if (result.is_ok()) {
                return make_ok<pdu>(std::move(result.value()));
            }
#ifdef PACS_WITH_COMMON_SYSTEM
            return make_error<pdu>(pdu_decode_error::malformed_pdu,
                result.error().message);
#else
            return make_error<pdu>(result.error_code(), result.error_message());
#endif
        }
        case 0x06: {
            auto result = decode_release_rp(data);
            if (result.is_ok()) {
                return make_ok<pdu>(std::move(result.value()));
            }
#ifdef PACS_WITH_COMMON_SYSTEM
            return make_error<pdu>(pdu_decode_error::malformed_pdu,
                result.error().message);
#else
            return make_error<pdu>(result.error_code(), result.error_message());
#endif
        }
        case 0x07: {
            auto result = decode_abort(data);
            if (result.is_ok()) {
                return make_ok<pdu>(std::move(result.value()));
            }
#ifdef PACS_WITH_COMMON_SYSTEM
            return make_error<pdu>(pdu_decode_error::malformed_pdu,
                result.error().message);
#else
            return make_error<pdu>(result.error_code(), result.error_message());
#endif
        }
        default:
            return make_error<pdu>(pdu_decode_error::invalid_pdu_type,
                "Unknown PDU type: " + std::to_string(type));
    }
}

// ============================================================================
// A-ASSOCIATE-RJ Decoder
// ============================================================================

DecodeResult<associate_rj> pdu_decoder::decode_associate_rj(
    std::span<const uint8_t> data) {

    auto header_result = validate_pdu_header(data, 0x03);
    if (header_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return make_error<associate_rj>(pdu_decode_error::incomplete_pdu,
            header_result.error().message);
#else
        return make_error<associate_rj>(header_result.error_code(),
            header_result.error_message());
#endif
    }

    // A-ASSOCIATE-RJ is always 10 bytes
    if (data.size() < FIXED_PDU_SIZE) {
        return make_error<associate_rj>(pdu_decode_error::incomplete_pdu);
    }

    associate_rj rj;
    rj.result = static_cast<reject_result>(data[7]);
    rj.source = data[8];
    rj.reason = data[9];

    return make_ok(std::move(rj));
}

// ============================================================================
// A-RELEASE-RQ Decoder
// ============================================================================

DecodeResult<release_rq_pdu> pdu_decoder::decode_release_rq(
    std::span<const uint8_t> data) {

    auto header_result = validate_pdu_header(data, 0x05);
    if (header_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return make_error<release_rq_pdu>(pdu_decode_error::incomplete_pdu,
            header_result.error().message);
#else
        return make_error<release_rq_pdu>(header_result.error_code(),
            header_result.error_message());
#endif
    }

    return make_ok(release_rq_pdu{});
}

// ============================================================================
// A-RELEASE-RP Decoder
// ============================================================================

DecodeResult<release_rp_pdu> pdu_decoder::decode_release_rp(
    std::span<const uint8_t> data) {

    auto header_result = validate_pdu_header(data, 0x06);
    if (header_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return make_error<release_rp_pdu>(pdu_decode_error::incomplete_pdu,
            header_result.error().message);
#else
        return make_error<release_rp_pdu>(header_result.error_code(),
            header_result.error_message());
#endif
    }

    return make_ok(release_rp_pdu{});
}

// ============================================================================
// A-ABORT Decoder
// ============================================================================

DecodeResult<abort_pdu> pdu_decoder::decode_abort(std::span<const uint8_t> data) {
    auto header_result = validate_pdu_header(data, 0x07);
    if (header_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return make_error<abort_pdu>(pdu_decode_error::incomplete_pdu,
            header_result.error().message);
#else
        return make_error<abort_pdu>(header_result.error_code(),
            header_result.error_message());
#endif
    }

    if (data.size() < FIXED_PDU_SIZE) {
        return make_error<abort_pdu>(pdu_decode_error::incomplete_pdu);
    }

    abort_pdu abort;
    abort.source = static_cast<abort_source>(data[8]);
    abort.reason = static_cast<abort_reason>(data[9]);

    return make_ok(std::move(abort));
}

// ============================================================================
// P-DATA-TF Decoder
// ============================================================================

DecodeResult<p_data_tf_pdu> pdu_decoder::decode_p_data_tf(
    std::span<const uint8_t> data) {

    auto header_result = validate_pdu_header(data, 0x04);
    if (header_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return make_error<p_data_tf_pdu>(pdu_decode_error::incomplete_pdu,
            header_result.error().message);
#else
        return make_error<p_data_tf_pdu>(header_result.error_code(),
            header_result.error_message());
#endif
    }

    const uint32_t pdu_length = header_result.value();
    const size_t pdu_end = PDU_HEADER_SIZE + pdu_length;

    p_data_tf_pdu result;
    size_t pos = PDU_HEADER_SIZE;

    while (pos < pdu_end) {
        // Each PDV item has:
        // - 4-byte length
        // - 1-byte presentation context ID
        // - 1-byte message control header
        // - variable data

        if (pos + 4 > pdu_end) {
            return make_error<p_data_tf_pdu>(pdu_decode_error::malformed_pdu,
                "Incomplete PDV item length");
        }

        const uint32_t pdv_item_length = read_uint32_be(data, pos);
        pos += 4;

        if (pdv_item_length < 2) {
            return make_error<p_data_tf_pdu>(pdu_decode_error::malformed_pdu,
                "PDV item length too small");
        }

        if (pos + pdv_item_length > pdu_end) {
            return make_error<p_data_tf_pdu>(pdu_decode_error::buffer_overflow,
                "PDV item exceeds PDU bounds");
        }

        presentation_data_value pdv;
        pdv.context_id = data[pos];
        pos += 1;

        const uint8_t control = data[pos];
        pos += 1;

        pdv.is_command = (control & 0x01) != 0;
        pdv.is_last = (control & 0x02) != 0;

        // Data length = item length - context_id (1) - control (1)
        const size_t data_length = pdv_item_length - 2;
        pdv.data.assign(data.begin() + static_cast<ptrdiff_t>(pos),
                        data.begin() + static_cast<ptrdiff_t>(pos + data_length));
        pos += data_length;

        result.pdvs.push_back(std::move(pdv));
    }

    return make_ok(std::move(result));
}

// ============================================================================
// Variable Items Decoder (for ASSOCIATE-RQ/AC)
// ============================================================================

DecodeResult<std::tuple<
    std::string,
    std::vector<presentation_context_rq>,
    std::vector<presentation_context_ac>,
    user_information
>> pdu_decoder::decode_variable_items(std::span<const uint8_t> data, bool is_rq) {
    std::string application_context;
    std::vector<presentation_context_rq> pcs_rq;
    std::vector<presentation_context_ac> pcs_ac;
    user_information user_info;

    size_t pos = 0;

    while (pos < data.size()) {
        if (pos + 4 > data.size()) {
            return make_error<std::tuple<std::string,
                std::vector<presentation_context_rq>,
                std::vector<presentation_context_ac>,
                user_information>>(pdu_decode_error::malformed_pdu,
                    "Incomplete item header");
        }

        const uint8_t item_type_byte = data[pos];
        // Reserved byte at pos + 1
        const uint16_t item_length = read_uint16_be(data, pos + 2);
        pos += 4;

        if (pos + item_length > data.size()) {
            return make_error<std::tuple<std::string,
                std::vector<presentation_context_rq>,
                std::vector<presentation_context_ac>,
                user_information>>(pdu_decode_error::buffer_overflow,
                    "Item length exceeds buffer");
        }

        switch (item_type_byte) {
            case 0x10:  // Application Context
                application_context = read_uid(data, pos, item_length);
                break;

            case 0x20:  // Presentation Context (RQ)
                if (is_rq) {
                    // Parse presentation context RQ
                    presentation_context_rq pc;
                    pc.id = data[pos];
                    // 3 reserved bytes

                    size_t sub_pos = pos + 4;
                    const size_t pc_end = pos + item_length;

                    while (sub_pos < pc_end) {
                        if (sub_pos + 4 > pc_end) break;

                        const uint8_t sub_type = data[sub_pos];
                        const uint16_t sub_length = read_uint16_be(data, sub_pos + 2);
                        sub_pos += 4;

                        if (sub_pos + sub_length > pc_end) break;

                        if (sub_type == 0x30) {  // Abstract Syntax
                            pc.abstract_syntax = read_uid(data, sub_pos, sub_length);
                        } else if (sub_type == 0x40) {  // Transfer Syntax
                            pc.transfer_syntaxes.push_back(
                                read_uid(data, sub_pos, sub_length));
                        }
                        sub_pos += sub_length;
                    }
                    pcs_rq.push_back(std::move(pc));
                }
                break;

            case 0x21:  // Presentation Context (AC)
                if (!is_rq) {
                    presentation_context_ac pc;
                    pc.id = data[pos];
                    // Reserved byte at pos + 1
                    pc.result = static_cast<presentation_context_result>(data[pos + 2]);
                    // Reserved byte at pos + 3

                    size_t sub_pos = pos + 4;
                    const size_t pc_end = pos + item_length;

                    while (sub_pos < pc_end) {
                        if (sub_pos + 4 > pc_end) break;

                        const uint8_t sub_type = data[sub_pos];
                        const uint16_t sub_length = read_uint16_be(data, sub_pos + 2);
                        sub_pos += 4;

                        if (sub_pos + sub_length > pc_end) break;

                        if (sub_type == 0x40) {  // Transfer Syntax
                            pc.transfer_syntax = read_uid(data, sub_pos, sub_length);
                        }
                        sub_pos += sub_length;
                    }
                    pcs_ac.push_back(std::move(pc));
                }
                break;

            case 0x50: {  // User Information
                auto ui_result = decode_user_info_item(
                    data.subspan(pos, item_length));
                if (ui_result.is_ok()) {
                    user_info = std::move(ui_result.value());
                }
                break;
            }

            default:
                // Skip unknown item types
                break;
        }

        pos += item_length;
    }

    return make_ok(std::make_tuple(
        std::move(application_context),
        std::move(pcs_rq),
        std::move(pcs_ac),
        std::move(user_info)));
}

// ============================================================================
// User Information Decoder
// ============================================================================

DecodeResult<user_information> pdu_decoder::decode_user_info_item(
    std::span<const uint8_t> data) {

    user_information ui;
    size_t pos = 0;

    while (pos < data.size()) {
        if (pos + 4 > data.size()) break;

        const uint8_t sub_type = data[pos];
        // Reserved byte at pos + 1
        const uint16_t sub_length = read_uint16_be(data, pos + 2);
        pos += 4;

        if (pos + sub_length > data.size()) break;

        switch (sub_type) {
            case 0x51:  // Maximum Length
                if (sub_length >= 4) {
                    ui.max_pdu_length = read_uint32_be(data, pos);
                }
                break;

            case 0x52:  // Implementation Class UID
                ui.implementation_class_uid = read_uid(data, pos, sub_length);
                break;

            case 0x55:  // Implementation Version Name
                ui.implementation_version_name = read_uid(data, pos, sub_length);
                break;

            case 0x54: {  // SCP/SCU Role Selection
                if (sub_length >= 4) {
                    const uint16_t uid_length = read_uint16_be(data, pos);
                    // UID is padded to even length in encoder
                    const uint16_t padded_uid_length = uid_length + (uid_length % 2);
                    if (pos + 2 + padded_uid_length + 2 <= pos + sub_length) {
                        scp_scu_role_selection role;
                        role.sop_class_uid = read_uid(data, pos + 2, uid_length);
                        role.scu_role = (data[pos + 2 + padded_uid_length] != 0);
                        role.scp_role = (data[pos + 2 + padded_uid_length + 1] != 0);
                        ui.role_selections.push_back(std::move(role));
                    }
                }
                break;
            }

            default:
                // Skip unknown sub-items
                break;
        }

        pos += sub_length;
    }

    return make_ok(std::move(ui));
}

// ============================================================================
// A-ASSOCIATE-RQ Decoder
// ============================================================================

DecodeResult<associate_rq> pdu_decoder::decode_associate_rq(
    std::span<const uint8_t> data) {

    auto header_result = validate_pdu_header(data, 0x01);
    if (header_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return make_error<associate_rq>(pdu_decode_error::incomplete_pdu,
            header_result.error().message);
#else
        return make_error<associate_rq>(header_result.error_code(),
            header_result.error_message());
#endif
    }

    const uint32_t pdu_length = header_result.value();

    // Check minimum size for ASSOCIATE header
    if (pdu_length < ASSOCIATE_HEADER_SIZE) {
        return make_error<associate_rq>(pdu_decode_error::malformed_pdu,
            "PDU too short for ASSOCIATE-RQ");
    }

    // Check protocol version (bytes 6-7)
    const uint16_t protocol_version = read_uint16_be(data, 6);
    if (protocol_version != DICOM_PROTOCOL_VERSION) {
        return make_error<associate_rq>(pdu_decode_error::invalid_protocol_version,
            "Expected version 1, got " + std::to_string(protocol_version));
    }

    associate_rq rq;

    // Called AE Title (bytes 10-25)
    rq.called_ae_title = read_ae_title(data, 10);

    // Calling AE Title (bytes 26-41)
    rq.calling_ae_title = read_ae_title(data, 26);

    // Reserved bytes 42-73 (32 bytes)
    // Variable items start at byte 74

    const size_t variable_start = PDU_HEADER_SIZE + ASSOCIATE_HEADER_SIZE;
    const size_t variable_length = pdu_length - ASSOCIATE_HEADER_SIZE;

    if (variable_length > 0 && variable_start + variable_length <= data.size()) {
        auto var_result = decode_variable_items(
            data.subspan(variable_start, variable_length), true);

        if (var_result.is_ok()) {
            auto& [app_ctx, pcs_rq, pcs_ac, user_info] = var_result.value();
            rq.application_context = std::move(app_ctx);
            rq.presentation_contexts = std::move(pcs_rq);
            rq.user_info = std::move(user_info);
        }
    }

    return make_ok(std::move(rq));
}

// ============================================================================
// A-ASSOCIATE-AC Decoder
// ============================================================================

DecodeResult<associate_ac> pdu_decoder::decode_associate_ac(
    std::span<const uint8_t> data) {

    auto header_result = validate_pdu_header(data, 0x02);
    if (header_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return make_error<associate_ac>(pdu_decode_error::incomplete_pdu,
            header_result.error().message);
#else
        return make_error<associate_ac>(header_result.error_code(),
            header_result.error_message());
#endif
    }

    const uint32_t pdu_length = header_result.value();

    // Check minimum size
    if (pdu_length < ASSOCIATE_HEADER_SIZE) {
        return make_error<associate_ac>(pdu_decode_error::malformed_pdu,
            "PDU too short for ASSOCIATE-AC");
    }

    // Check protocol version
    const uint16_t protocol_version = read_uint16_be(data, 6);
    if (protocol_version != DICOM_PROTOCOL_VERSION) {
        return make_error<associate_ac>(pdu_decode_error::invalid_protocol_version,
            "Expected version 1, got " + std::to_string(protocol_version));
    }

    associate_ac ac;

    // Called AE Title
    ac.called_ae_title = read_ae_title(data, 10);

    // Calling AE Title
    ac.calling_ae_title = read_ae_title(data, 26);

    // Variable items
    const size_t variable_start = PDU_HEADER_SIZE + ASSOCIATE_HEADER_SIZE;
    const size_t variable_length = pdu_length - ASSOCIATE_HEADER_SIZE;

    if (variable_length > 0 && variable_start + variable_length <= data.size()) {
        auto var_result = decode_variable_items(
            data.subspan(variable_start, variable_length), false);

        if (var_result.is_ok()) {
            auto& [app_ctx, pcs_rq, pcs_ac, user_info] = var_result.value();
            ac.application_context = std::move(app_ctx);
            ac.presentation_contexts = std::move(pcs_ac);
            ac.user_info = std::move(user_info);
        }
    }

    return make_ok(std::move(ac));
}

}  // namespace pacs::network
