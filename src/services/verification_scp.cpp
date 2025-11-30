/**
 * @file verification_scp.cpp
 * @brief Implementation of the Verification SCP service
 */

#include "pacs/services/verification_scp.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

std::vector<std::string> verification_scp::supported_sop_classes() const {
    return {std::string(verification_sop_class_uid)};
}

network::Result<std::monostate> verification_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify the message is a C-ECHO request
    if (request.command() != command_field::c_echo_rq) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            std::string("Expected C-ECHO-RQ but received ") +
            std::string(to_string(request.command())));
#else
        return std::string("Expected C-ECHO-RQ but received ") +
               std::string(to_string(request.command()));
#endif
    }

    // Build C-ECHO response using the factory function
    auto response = make_c_echo_rsp(
        request.message_id(),
        status_success,
        verification_sop_class_uid
    );

    // Send the response
    return assoc.send_dimse(context_id, response);
}

std::string_view verification_scp::service_name() const noexcept {
    return "Verification SCP";
}

}  // namespace pacs::services
