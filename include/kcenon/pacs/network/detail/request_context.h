// BSD 3-Clause License. See LICENSE.
#pragma once

#include "kcenon/pacs/network/association.h"

namespace kcenon::pacs::network::detail {

// PS3.7 command identity is command-specific. Never fall back from a missing
// Requested SOP Class to Affected SOP Class (or vice versa) on malformed input.
inline Result<std::string> request_sop_class(
    const association& assoc, uint8_t context_id, const dimse::dimse_message& msg) {
    std::string abstract_syntax;
    for (const auto& context : assoc.accepted_contexts()) {
        if (context.id == context_id) {
            abstract_syntax = context.abstract_syntax;
            break;
        }
    }
    if (abstract_syntax.empty()) {
        return kcenon::common::error_info("Unknown presentation context ID");
    }

    using dimse::command_field;
    bool requested = false;
    switch (msg.command()) {
        case command_field::n_get_rq:
        case command_field::n_set_rq:
        case command_field::n_action_rq:
        case command_field::n_delete_rq:
            requested = true;
            break;
        case command_field::c_echo_rq:
        case command_field::c_store_rq:
        case command_field::c_find_rq:
        case command_field::c_get_rq:
        case command_field::c_move_rq:
        case command_field::n_create_rq:
        case command_field::n_event_report_rq:
            break;
        case command_field::c_cancel_rq:
            // C-CANCEL identifies an operation by context and Message ID Being
            // Responded To, and has no SOP Class UID command element.
            if (msg.affected_sop_class_uid().empty() && msg.requested_sop_class_uid().empty()) {
                return abstract_syntax;
            }
            return kcenon::common::error_info("Unexpected SOP Class UID in C-CANCEL");
        default:
            return kcenon::common::error_info("Cannot dispatch a response or unknown DIMSE command");
    }
    const auto sop_class = requested ? msg.requested_sop_class_uid() : msg.affected_sop_class_uid();
    const auto other = requested ? msg.affected_sop_class_uid() : msg.requested_sop_class_uid();
    if (sop_class.empty() || !other.empty()) {
        return kcenon::common::error_info("Missing or conflicting SOP Class UID command element");
    }
    // PS3.4 H.3.2: a Print Meta SOP presentation context carries its explicitly
    // defined member SOP Classes. Do not treat arbitrary SOP mismatches as meta.
    const bool print_gray = abstract_syntax == "1.2.840.10008.5.1.1.9";
    const bool print_color = abstract_syntax == "1.2.840.10008.5.1.1.18";
    const bool print_member = (print_gray || print_color) &&
        (sop_class == "1.2.840.10008.5.1.1.1" ||
         sop_class == "1.2.840.10008.5.1.1.2" ||
         sop_class == "1.2.840.10008.5.1.1.16" ||
         (print_gray && sop_class == "1.2.840.10008.5.1.1.4") ||
         (print_color && sop_class == "1.2.840.10008.5.1.1.4.1"));
    // PS3.4 CC.2: every UPS is an instance of the UPS Push SOP Class, so a request
    // on a Pull, Watch or Query context, or an event report, still names UPS Push.
    const bool ups_member = sop_class == "1.2.840.10008.5.1.4.34.6.1" &&
        (abstract_syntax == "1.2.840.10008.5.1.4.34.6.2" ||
         abstract_syntax == "1.2.840.10008.5.1.4.34.6.3" ||
         abstract_syntax == "1.2.840.10008.5.1.4.34.6.4" ||
         abstract_syntax == "1.2.840.10008.5.1.4.34.6.5");
    if (sop_class != abstract_syntax && !print_member && !ups_member) {
        return kcenon::common::error_info("Command SOP Class UID does not match presentation context");
    }
    return abstract_syntax;
}

} // namespace kcenon::pacs::network::detail
