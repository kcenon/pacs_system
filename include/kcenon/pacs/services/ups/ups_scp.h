/**
 * @file ups_scp.h
 * @brief The UPS SOP Classes as one service
 *
 * A server dispatches on the negotiated SOP Class, while PS3.4 CC.2 splits the operations
 * of one workitem over UPS Push, Pull, Watch and Query. This service routes each request
 * to the class that implements it and answers 0211H when the context does not carry it.
 */

#ifndef PACS_SERVICES_UPS_SCP_HPP
#define PACS_SERVICES_UPS_SCP_HPP

#include "../scp_service.h"
#include "ups_push_scp.h"
#include "ups_query_scp.h"
#include "ups_watch_scp.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::services {

/**
 * @brief Whether a negotiated UPS SOP Class carries a request (Tables CC.2-1, -2, -3 and -5)
 *
 * @param abstract_syntax The negotiated SOP Class
 * @param request The DIMSE request
 */
[[nodiscard]] bool ups_context_carries(std::string_view abstract_syntax,
                                       const network::dimse::dimse_message& request);

class ups_scp final : public scp_service {
public:
    ups_scp(std::shared_ptr<ups_push_scp> push, std::shared_ptr<ups_watch_scp> watch,
            std::shared_ptr<ups_query_scp> query, std::shared_ptr<di::ILogger> logger = nullptr);

    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    [[nodiscard]] std::string_view service_name() const noexcept override;

private:
    std::shared_ptr<ups_push_scp> push_;
    std::shared_ptr<ups_watch_scp> watch_;
    std::shared_ptr<ups_query_scp> query_;
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_UPS_SCP_HPP
