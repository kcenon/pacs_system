// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file ct_storage.cpp
 * @brief Implementation of CT Image Storage SOP Class utilities
 *
 * @see Issue #717 - Add CT Image IOD Validator
 * @see Issue #848 - Add CT For Processing SOP Classes
 */

#include "pacs/services/sop_classes/ct_storage.hpp"

namespace kcenon::pacs::services::sop_classes {

std::vector<std::string> get_ct_storage_sop_classes() {
    return {
        std::string(ct_image_storage_uid),
        std::string(enhanced_ct_image_storage_uid),
        std::string(ct_for_processing_image_storage_uid),
    };
}

bool is_ct_storage_sop_class(std::string_view uid) noexcept {
    return uid == ct_image_storage_uid ||
           uid == enhanced_ct_image_storage_uid ||
           uid == ct_for_processing_image_storage_uid;
}

bool is_valid_ct_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME1" || value == "MONOCHROME2";
}

}  // namespace kcenon::pacs::services::sop_classes
