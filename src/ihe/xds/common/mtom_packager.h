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

}  // namespace kcenon::pacs::ihe::xds::detail
