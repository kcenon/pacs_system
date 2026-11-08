/**
 * @file ups_scp.cpp
 * @brief The UPS SOP Classes as one service
 */

#include "kcenon/pacs/services/ups/ups_scp.h"

#include "kcenon/pacs/network/dimse/command_field.h"
#include "kcenon/pacs/network/dimse/status_codes.h"

namespace kcenon::pacs::services {

namespace {

using network::dimse::command_field;

[[nodiscard]] std::string_view negotiated_syntax(const network::association& assoc, uint8_t context_id) {
    for (const auto& context : assoc.accepted_contexts()) {
        if (context.id == context_id) {
            return context.abstract_syntax;
        }
    }
    return {};
}

}  // namespace

bool ups_context_carries(std::string_view abstract_syntax, const network::dimse::dimse_message& request) {
    const bool push = abstract_syntax == ups_push_sop_class_uid;
    const bool pull = abstract_syntax == ups_pull_sop_class_uid;
    const bool watch = abstract_syntax == ups_watch_sop_class_uid;
    const bool query = abstract_syntax == ups_query_find_sop_class_uid;

    switch (request.command()) {
        case command_field::n_create_rq:
            return push;
        case command_field::n_set_rq:
            return pull;
        case command_field::n_get_rq:
            return push || pull || watch || query;
        case command_field::c_find_rq:
        case command_field::c_cancel_rq:
            return pull || watch || query;
        case command_field::n_action_rq: {
            const auto action = request.action_type_id();
            if (!action) {
                return false;
            }
            if (*action == ups_action_change_state) {
                return pull;
            }
            if (*action == ups_action_request_cancel) {
                return push || watch;
            }
            if (*action == ups_watch_action_subscribe || *action == ups_watch_action_unsubscribe ||
                *action == ups_watch_action_suspend_global) {
                return watch;
            }
            // An action type no class defines reaches a service, which answers 0123H.
            return true;
        }
        default:
            return false;
    }
}

ups_scp::ups_scp(std::shared_ptr<ups_push_scp> push, std::shared_ptr<ups_watch_scp> watch,
                 std::shared_ptr<ups_query_scp> query, std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger))
    , push_(std::move(push))
    , watch_(std::move(watch))
    , query_(std::move(query)) {}

std::vector<std::string> ups_scp::supported_sop_classes() const {
    return {std::string(ups_push_sop_class_uid), std::string(ups_pull_sop_class_uid),
            std::string(ups_watch_sop_class_uid), std::string(ups_query_find_sop_class_uid)};
}

network::Result<std::monostate> ups_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    if (!ups_context_carries(negotiated_syntax(assoc, context_id), request)) {
        dimse_message response{get_response_command(request.command()), 0};
        response.set_message_id_responded_to(request.message_id());
        const auto named = request.command() == command_field::n_create_rq || request.command() == command_field::c_find_rq
                               ? request.affected_sop_class_uid()
                               : request.requested_sop_class_uid();
        response.set_affected_sop_class_uid(named);
        if (const auto action = request.action_type_id(); action && request.command() == command_field::n_action_rq) {
            response.set_action_type_id(*action);
        }
        response.set_status(status_error_unrecognized_operation);
        return assoc.send_dimse(context_id, response);
    }

    switch (request.command()) {
        case command_field::c_find_rq:
            return query_->handle_message(assoc, context_id, request);
        case command_field::n_action_rq: {
            const auto action = request.action_type_id();
            if (action && (*action == ups_watch_action_subscribe || *action == ups_watch_action_unsubscribe ||
                           *action == ups_watch_action_suspend_global)) {
                return watch_->handle_message(assoc, context_id, request);
            }
            return push_->handle_message(assoc, context_id, request);
        }
        default:
            return push_->handle_message(assoc, context_id, request);
    }
}

std::string_view ups_scp::service_name() const noexcept {
    return "UPS SCP";
}

}  // namespace kcenon::pacs::services
