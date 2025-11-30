#ifndef PACS_NETWORK_PDU_TYPES_HPP
#define PACS_NETWORK_PDU_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace pacs::network {

/**
 * @brief PDU (Protocol Data Unit) types as defined in DICOM PS3.8.
 *
 * These values represent the type field in PDU headers.
 */
enum class pdu_type : uint8_t {
    associate_rq = 0x01,  ///< A-ASSOCIATE-RQ (Association Request)
    associate_ac = 0x02,  ///< A-ASSOCIATE-AC (Association Accept)
    associate_rj = 0x03,  ///< A-ASSOCIATE-RJ (Association Reject)
    p_data_tf = 0x04,     ///< P-DATA-TF (Data Transfer)
    release_rq = 0x05,    ///< A-RELEASE-RQ (Release Request)
    release_rp = 0x06,    ///< A-RELEASE-RP (Release Response)
    abort = 0x07,         ///< A-ABORT (Abort)
};

/**
 * @brief Item types used in variable items of PDUs.
 */
enum class item_type : uint8_t {
    application_context = 0x10,     ///< Application Context Item
    presentation_context_rq = 0x20, ///< Presentation Context Item (RQ)
    presentation_context_ac = 0x21, ///< Presentation Context Item (AC)
    user_information = 0x50,        ///< User Information Item
    abstract_syntax = 0x30,         ///< Abstract Syntax Sub-item
    transfer_syntax = 0x40,         ///< Transfer Syntax Sub-item
    maximum_length = 0x51,          ///< Maximum Length Sub-item
    implementation_class_uid = 0x52,    ///< Implementation Class UID Sub-item
    implementation_version_name = 0x55, ///< Implementation Version Name Sub-item
    async_operations_window = 0x53,     ///< Asynchronous Operations Window Sub-item
    scp_scu_role_selection = 0x54,      ///< SCP/SCU Role Selection Sub-item
    sop_class_extended_negotiation = 0x56,      ///< SOP Class Extended Negotiation
    sop_class_common_extended_negotiation = 0x57, ///< SOP Class Common Extended Negotiation
    user_identity_rq = 0x58,        ///< User Identity RQ Sub-item
    user_identity_ac = 0x59,        ///< User Identity AC Sub-item
};

/**
 * @brief Result values for A-ASSOCIATE-AC presentation context.
 */
enum class presentation_context_result : uint8_t {
    acceptance = 0,                   ///< Accepted
    user_rejection = 1,               ///< User-rejection
    no_reason = 2,                    ///< No reason (provider rejection)
    abstract_syntax_not_supported = 3,///< Abstract-syntax-not-supported
    transfer_syntaxes_not_supported = 4, ///< Transfer-syntaxes-not-supported
};

/**
 * @brief Abort source values.
 */
enum class abort_source : uint8_t {
    service_user = 0,       ///< DICOM UL service-user
    reserved = 1,           ///< Reserved
    service_provider = 2,   ///< DICOM UL service-provider (ACSE)
};

/**
 * @brief Abort reason values when source is service-provider.
 */
enum class abort_reason : uint8_t {
    not_specified = 0,          ///< Reason not specified
    unrecognized_pdu = 1,       ///< Unrecognized PDU
    unexpected_pdu = 2,         ///< Unexpected PDU
    reserved = 3,               ///< Reserved
    unrecognized_pdu_parameter = 4, ///< Unrecognized PDU parameter
    unexpected_pdu_parameter = 5,   ///< Unexpected PDU parameter
    invalid_pdu_parameter = 6,      ///< Invalid PDU parameter value
};

/**
 * @brief Reject result values.
 */
enum class reject_result : uint8_t {
    rejected_permanent = 1,  ///< Rejected-permanent
    rejected_transient = 2,  ///< Rejected-transient
};

/**
 * @brief Reject source values.
 */
enum class reject_source : uint8_t {
    service_user = 1,                   ///< DICOM UL service-user
    service_provider_acse = 2,          ///< DICOM UL service-provider (ACSE)
    service_provider_presentation = 3,  ///< DICOM UL service-provider (Presentation)
};

/**
 * @brief Reject reason values when source is service-user.
 */
enum class reject_reason_user : uint8_t {
    no_reason = 1,           ///< No reason given
    application_context_not_supported = 2, ///< Application-context-name not supported
    calling_ae_not_recognized = 3, ///< Calling-AE-title not recognized
    called_ae_not_recognized = 7,  ///< Called-AE-title not recognized
};

/**
 * @brief Reject reason values when source is service-provider (ACSE).
 */
enum class reject_reason_provider_acse : uint8_t {
    no_reason = 1,           ///< No reason given
    protocol_version_not_supported = 2, ///< Protocol-version not supported
};

/**
 * @brief Reject reason values when source is service-provider (Presentation).
 */
enum class reject_reason_provider_presentation : uint8_t {
    temporary_congestion = 1, ///< Temporary congestion
    local_limit_exceeded = 2, ///< Local limit exceeded
};

/**
 * @brief Presentation Data Value (PDV) item for P-DATA-TF.
 */
struct presentation_data_value {
    uint8_t context_id;          ///< Presentation Context ID (odd number 1-255)
    bool is_command;             ///< true if Command message, false if Data
    bool is_last;                ///< true if last fragment
    std::vector<uint8_t> data;   ///< Fragment data

    presentation_data_value() = default;
    presentation_data_value(uint8_t id, bool command, bool last, std::vector<uint8_t> d)
        : context_id(id), is_command(command), is_last(last), data(std::move(d)) {}
};

/**
 * @brief Presentation Context for A-ASSOCIATE-RQ.
 */
struct presentation_context_rq {
    uint8_t id;                           ///< Presentation Context ID (odd number 1-255)
    std::string abstract_syntax;          ///< Abstract Syntax UID (SOP Class)
    std::vector<std::string> transfer_syntaxes; ///< Proposed Transfer Syntaxes

    presentation_context_rq() : id(0) {}
    presentation_context_rq(uint8_t context_id, std::string abs_syntax,
                           std::vector<std::string> ts_list)
        : id(context_id)
        , abstract_syntax(std::move(abs_syntax))
        , transfer_syntaxes(std::move(ts_list)) {}
};

/**
 * @brief Presentation Context for A-ASSOCIATE-AC.
 */
struct presentation_context_ac {
    uint8_t id;                              ///< Presentation Context ID
    presentation_context_result result;       ///< Result/Reason
    std::string transfer_syntax;              ///< Accepted Transfer Syntax UID

    presentation_context_ac()
        : id(0), result(presentation_context_result::acceptance) {}
    presentation_context_ac(uint8_t context_id, presentation_context_result res,
                           std::string ts = "")
        : id(context_id), result(res), transfer_syntax(std::move(ts)) {}
};

/**
 * @brief SCP/SCU Role Selection Sub-item.
 */
struct scp_scu_role_selection {
    std::string sop_class_uid;  ///< SOP Class UID
    bool scu_role;              ///< SCU-role (true if supported)
    bool scp_role;              ///< SCP-role (true if supported)

    scp_scu_role_selection() : scu_role(false), scp_role(false) {}
    scp_scu_role_selection(std::string uid, bool scu, bool scp)
        : sop_class_uid(std::move(uid)), scu_role(scu), scp_role(scp) {}
};

/**
 * @brief User Information for A-ASSOCIATE-RQ/AC.
 */
struct user_information {
    uint32_t max_pdu_length;              ///< Maximum Length of P-DATA-TF PDUs
    std::string implementation_class_uid; ///< Implementation Class UID
    std::string implementation_version_name; ///< Implementation Version Name (optional)
    std::vector<scp_scu_role_selection> role_selections; ///< Role selections (optional)

    user_information() : max_pdu_length(0) {}
};

/**
 * @brief A-ASSOCIATE-RQ PDU data.
 */
struct associate_rq {
    std::string called_ae_title;                  ///< Called AE Title (16 chars max)
    std::string calling_ae_title;                 ///< Calling AE Title (16 chars max)
    std::string application_context;              ///< Application Context Name UID
    std::vector<presentation_context_rq> presentation_contexts; ///< Presentation Contexts
    user_information user_info;                   ///< User Information

    associate_rq() = default;
};

/**
 * @brief A-ASSOCIATE-AC PDU data.
 */
struct associate_ac {
    std::string called_ae_title;                  ///< Called AE Title (16 chars max)
    std::string calling_ae_title;                 ///< Calling AE Title (16 chars max)
    std::string application_context;              ///< Application Context Name UID
    std::vector<presentation_context_ac> presentation_contexts; ///< Presentation Contexts
    user_information user_info;                   ///< User Information

    associate_ac() = default;
};

/**
 * @brief A-ASSOCIATE-RJ PDU data.
 */
struct associate_rj {
    reject_result result;    ///< Result (1=permanent, 2=transient)
    uint8_t source;          ///< Source
    uint8_t reason;          ///< Reason/Diagnostic

    associate_rj()
        : result(reject_result::rejected_permanent), source(0), reason(0) {}
    associate_rj(reject_result res, uint8_t src, uint8_t rsn)
        : result(res), source(src), reason(rsn) {}
};

/// @name Conversion Functions
/// @{

/**
 * @brief Converts a pdu_type to its string representation.
 */
[[nodiscard]] constexpr const char* to_string(pdu_type type) noexcept {
    switch (type) {
        case pdu_type::associate_rq: return "A-ASSOCIATE-RQ";
        case pdu_type::associate_ac: return "A-ASSOCIATE-AC";
        case pdu_type::associate_rj: return "A-ASSOCIATE-RJ";
        case pdu_type::p_data_tf: return "P-DATA-TF";
        case pdu_type::release_rq: return "A-RELEASE-RQ";
        case pdu_type::release_rp: return "A-RELEASE-RP";
        case pdu_type::abort: return "A-ABORT";
        default: return "UNKNOWN";
    }
}

/// @}

/// @name Constants
/// @{

/// Default DICOM Application Context Name (PS3.7)
constexpr const char* DICOM_APPLICATION_CONTEXT = "1.2.840.10008.3.1.1.1";

/// DICOM Protocol Version
constexpr uint16_t DICOM_PROTOCOL_VERSION = 0x0001;

/// AE Title length (fixed 16 characters, space-padded)
constexpr size_t AE_TITLE_LENGTH = 16;

/// Maximum PDU length recommended by DICOM (16384 bytes)
constexpr uint32_t DEFAULT_MAX_PDU_LENGTH = 16384;

/// Maximum PDU length that can be negotiated (0 = unlimited)
constexpr uint32_t UNLIMITED_MAX_PDU_LENGTH = 0;

/// @}

}  // namespace pacs::network

#endif  // PACS_NETWORK_PDU_TYPES_HPP
