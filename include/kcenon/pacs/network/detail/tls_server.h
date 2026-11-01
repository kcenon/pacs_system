// BSD 3-Clause License
// See the LICENSE file in the project root for full license information.
#pragma once

#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/server_config.h"
#include <memory>

namespace kcenon::network::interfaces { class i_protocol_server; }
namespace kcenon::pacs::network::detail {
// PACS adapter for the public network_system server interface. The pinned
// tcp_facade rejects use_ssl; this adapter applies TLS before delivering PDUs.
[[nodiscard]] Result<std::shared_ptr<kcenon::network::interfaces::i_protocol_server>>
make_tls_server(const server_config& config);
}
