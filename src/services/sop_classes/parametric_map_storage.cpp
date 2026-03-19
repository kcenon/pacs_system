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
 * @file parametric_map_storage.cpp
 * @brief Implementation of Parametric Map Storage SOP Class utilities
 *
 * @see Issue #833 - Add Parametric Map Storage SOP Class registration
 */

#include "pacs/services/sop_classes/parametric_map_storage.hpp"

#include <array>

namespace kcenon::pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_parametric_map_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
        // JPEG 2000 Lossless (preserves float data integrity)
        "1.2.840.10008.1.2.4.90",
        // HTJ2K Lossless (high-throughput lossless)
        "1.2.840.10008.1.2.4.201",
        // RLE Lossless
        "1.2.840.10008.1.2.5"
    };
}

// =============================================================================
// Pixel Value Representation
// =============================================================================

uint16_t get_bits_allocated(pixel_value_representation repr) noexcept {
    switch (repr) {
        case pixel_value_representation::float32:
            return 32;
        case pixel_value_representation::float64:
            return 64;
    }
    return 32;
}

pixel_value_representation
parse_pixel_value_representation(uint16_t bits_allocated) noexcept {
    if (bits_allocated == 64) {
        return pixel_value_representation::float64;
    }
    return pixel_value_representation::float32;
}

bool is_valid_parametric_map_bits_allocated(uint16_t bits_allocated) noexcept {
    return bits_allocated == 32 || bits_allocated == 64;
}

// =============================================================================
// SOP Class Information
// =============================================================================

namespace {

constexpr std::array<parametric_map_sop_class_info, 1> pmap_sop_classes = {{
    {
        parametric_map_storage_uid,
        "Parametric Map Storage",
        "Voxel-level quantitative parameter maps (ADC, perfusion, T1/T2)",
        false
    }
}};

}  // namespace

std::vector<std::string> get_parametric_map_storage_sop_classes() {
    return {
        std::string(parametric_map_storage_uid)
    };
}

const parametric_map_sop_class_info*
get_parametric_map_sop_class_info(std::string_view uid) noexcept {
    if (uid == parametric_map_storage_uid) {
        return &pmap_sop_classes[0];
    }
    return nullptr;
}

bool is_parametric_map_storage_sop_class(std::string_view uid) noexcept {
    return uid == parametric_map_storage_uid;
}

// =============================================================================
// Photometric Interpretation Validation
// =============================================================================

bool is_valid_parametric_map_photometric(std::string_view value) noexcept {
    return value == "MONOCHROME2";
}

}  // namespace kcenon::pacs::services::sop_classes
