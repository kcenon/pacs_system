// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file ophthalmic_storage.cpp
 * @brief Implementation of Ophthalmic Storage SOP Class utilities
 *
 * @see Issue #829 - Add Ophthalmic SOP Class registration and storage definition
 */

#include "kcenon/pacs/services/sop_classes/ophthalmic_storage.h"

namespace kcenon::pacs::services::sop_classes {

std::vector<std::string> get_ophthalmic_storage_sop_classes() {
    return {
        std::string(ophthalmic_photo_8bit_storage_uid),
        std::string(ophthalmic_photo_16bit_storage_uid),
        std::string(ophthalmic_tomography_storage_uid),
        std::string(wide_field_ophthalmic_photo_storage_uid),
        std::string(ophthalmic_oct_bscan_analysis_storage_uid),
    };
}

bool is_ophthalmic_storage_sop_class(std::string_view uid) noexcept {
    return uid == ophthalmic_photo_8bit_storage_uid ||
           uid == ophthalmic_photo_16bit_storage_uid ||
           uid == ophthalmic_tomography_storage_uid ||
           uid == wide_field_ophthalmic_photo_storage_uid ||
           uid == ophthalmic_oct_bscan_analysis_storage_uid;
}

bool is_valid_ophthalmic_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME1" || value == "MONOCHROME2" ||
           value == "RGB" || value == "YBR_FULL_422" ||
           value == "PALETTE COLOR";
}

}  // namespace kcenon::pacs::services::sop_classes
