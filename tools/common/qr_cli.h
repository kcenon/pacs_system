// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/pacs/core/dicom_dictionary.h"
#include "kcenon/pacs/core/dicom_tag.h"
#include "kcenon/pacs/encoding/vr_type.h"

#include <cstdint>

namespace kcenon::pacs::cli {

/**
 * @brief The VR for a query key given by tag on the command line.
 *
 * Identifiers travel in Explicit VR, so a key sent as UN is opaque to the SCP and
 * cannot match anything. Only a tag the dictionary does not know stays UN.
 */
[[nodiscard]] inline encoding::vr_type key_vr(core::dicom_tag tag) {
    const auto info = core::dicom_dictionary::instance().find(tag);
    return info ? static_cast<encoding::vr_type>(info->vr) : encoding::vr_type::UN;
}

/// Everything but Pending ends a C-GET or C-MOVE, including Warning B000 (Tables C.4-2 and C.4-3).
[[nodiscard]] constexpr bool is_final_retrieve_status(uint16_t status) noexcept {
    return status != 0xFF00 && status != 0xFF01;
}

/**
 * @brief The exit code for a finished retrieve: 0 all done, 1 partial, 2 failed.
 *
 * The status decides, not the counters: a refusal such as A801 has no failed
 * sub-operations, and a cancelled retrieve did not deliver what was asked for.
 */
[[nodiscard]] constexpr int retrieve_exit_code(uint16_t status, uint16_t failed) noexcept {
    const auto kind = status & 0xF000;
    if (kind == 0xA000 || kind == 0xC000 || status == 0xFE00) {
        return 2;
    }
    return kind == 0xB000 || failed > 0 ? 1 : 0;
}

}  // namespace kcenon::pacs::cli
