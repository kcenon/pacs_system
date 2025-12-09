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
