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
 * @file role.hpp
 * @brief Role definitions for RBAC
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <string>
#include <string_view>
#include <optional>

namespace pacs::security {

/**
 * @brief User roles in the PACS system
 */
enum class Role {
    Viewer,         ///< Read-only access to studies
    Technologist,   ///< Can upload/modify studies, but not delete
    Radiologist,    ///< Full clinical access (includes verification)
    Administrator,  ///< User management, system config
    System          ///< Internal system operations
};

/**
 * @brief Convert Role to string
 */
constexpr std::string_view to_string(Role role) {
    switch (role) {
        case Role::Viewer: return "Viewer";
        case Role::Technologist: return "Technologist";
        case Role::Radiologist: return "Radiologist";
        case Role::Administrator: return "Administrator";
        case Role::System: return "System";
    }
    return "Unknown";
}

/**
 * @brief Parse Role from string
 */
inline std::optional<Role> parse_role(std::string_view str) {
    if (str == "Viewer") return Role::Viewer;
    if (str == "Technologist") return Role::Technologist;
    if (str == "Radiologist") return Role::Radiologist;
    if (str == "Administrator") return Role::Administrator;
    if (str == "System") return Role::System;
    return std::nullopt;
}

} // namespace pacs::security
