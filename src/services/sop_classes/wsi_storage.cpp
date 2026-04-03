// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file wsi_storage.cpp
 * @brief Implementation of WSI Image Storage SOP Class utilities
 *
 * @see Issue #825 - Add WSI SOP Class registration and storage definition
 */

#include "pacs/services/sop_classes/wsi_storage.hpp"

namespace kcenon::pacs::services::sop_classes {

std::vector<std::string> get_wsi_storage_sop_classes() {
    return {
        std::string(wsi_image_storage_uid),
    };
}

bool is_wsi_storage_sop_class(std::string_view uid) noexcept {
    return uid == wsi_image_storage_uid;
}

bool is_valid_wsi_photometric(std::string_view value) noexcept {
    return value == "RGB" || value == "YBR_FULL_422" ||
           value == "YBR_ICT" || value == "YBR_RCT" ||
           value == "MONOCHROME2";
}

}  // namespace kcenon::pacs::services::sop_classes
