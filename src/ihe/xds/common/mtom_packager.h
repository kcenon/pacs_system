// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file mtom_packager.h
 * @brief MTOM / XOP multipart/related packager for ITI-41.
 *
 * Packages a signed SOAP envelope plus one document binary part per
 * xop:Include into a multipart/related body per RFC 2387 and W3C MTOM.
 *
 * This is an internal header.
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/pacs/ihe/xds/errors.h>

#include <cstdint>
#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds::detail {

/**
 * @brief One attachment part, mapped 1:1 to an xop:Include in the envelope.
 *
 * content_id must match the "cid:" href used in the envelope
 * (without the "cid:" prefix). mime_type is the Content-Type of the
 * attached bytes.
 */
struct mtom_part {
    std::string content_id;
    std::string mime_type;
    std::vector<std::uint8_t> content;
};

/**
 * @brief Fully assembled MTOM request body.
 *
 * content_type is the full HTTP Content-Type header value to set on the
 * request, including the boundary and start/start-info parameters.
 * body is the raw bytes of the multipart/related body.
 */
struct packaged_mtom {
    std::string content_type;
    std::string body;
};

/**
 * @brief Assemble a multipart/related MTOM body.
 *
 * @p envelope_xml is the signed SOAP 1.2 envelope; @p parts are the
 * document binaries referenced from the envelope by xop:Include.
 *
 * The boundary is generated from a thread-local PRNG and guaranteed not
 * to appear in any of the inputs via a fallback re-seed if a collision is
 * detected.
 */
kcenon::common::Result<packaged_mtom> package_mtom(
    const std::string& envelope_xml, const std::vector<mtom_part>& parts);

/**
 * @brief Result of parsing an inbound multipart/related MTOM response.
 *
 * root_xml is the SOAP envelope that constitutes the root part (the one
 * whose Content-ID matches the start parameter or, when absent, the first
 * part). parts is every attachment, keyed on the bare content-id with the
 * angle brackets stripped.
 */
struct parsed_mtom {
    std::string root_xml;
    std::vector<mtom_part> parts;
};

/**
 * @brief Parse a multipart/related MTOM response body.
 *
 * Prefers the boundary parameter from @p content_type_header when it is
 * non-empty and carries a `boundary="..."` (or bare `boundary=...`)
 * attribute. Falls back to sniffing the first `--<boundary>` marker in
 * @p body when the header is absent or malformed. The 8 MiB response cap
 * in http_client.cpp bounds the walk in either path.
 *
 * The body is split into parts, each part is stripped of its headers, and
 * the first encountered part is treated as the root envelope.
 *
 * Non-MTOM responses (the repository returned plain SOAP with no
 * multipart framing) are detected and returned with root_xml set to the
 * whole body and parts empty, so the caller can still invoke the signer
 * and response parser uniformly. This matches the fallback behavior of
 * Apache CXF repositories that omit MTOM when no attachments are present.
 */
kcenon::common::Result<parsed_mtom> parse_mtom_response(
    const std::string& content_type_header, const std::string& body);

}  // namespace kcenon::pacs::ihe::xds::detail
