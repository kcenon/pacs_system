// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file ophthalmic_storage.cpp
 * @brief Implementation of Ophthalmic Storage SOP Class utilities
 *
 * @see Issue #829 - Add Ophthalmic SOP Class registration and storage definition
 */

#include "pacs/services/sop_classes/ophthalmic_storage.hpp"

namespace pacs::services::sop_classes {

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

}  // namespace pacs::services::sop_classes
