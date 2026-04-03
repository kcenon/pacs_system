// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file mr_storage.cpp
 * @brief Implementation of MR Image Storage SOP Class utilities
 *
 * @see Issue #718 - Add MR Image IOD Validator
 */

#include "pacs/services/sop_classes/mr_storage.hpp"

namespace kcenon::pacs::services::sop_classes {

std::vector<std::string> get_mr_storage_sop_classes() {
    return {
        std::string(mr_image_storage_uid),
        std::string(enhanced_mr_image_storage_uid),
    };
}

bool is_mr_storage_sop_class(std::string_view uid) noexcept {
    return uid == mr_image_storage_uid ||
           uid == enhanced_mr_image_storage_uid;
}

bool is_valid_mr_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME1" || value == "MONOCHROME2";
}

}  // namespace kcenon::pacs::services::sop_classes
