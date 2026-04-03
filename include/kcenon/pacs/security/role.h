// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file role.h
 * @brief Role definitions for RBAC
 *
 * @copyright Copyright (c) 2025
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <string>
#include <string_view>
#include <optional>

namespace kcenon::pacs::security {

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

} // namespace kcenon::pacs::security
