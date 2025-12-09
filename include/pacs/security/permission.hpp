/**
 * @file permission.hpp
 * @brief Permission definitions for RBAC
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pacs::security {

/**
 * @brief Categories of resources requiring protection
 */
enum class ResourceType {
  Study,    ///< DICOM studies/series/instances
  Metadata, ///< Patient/Study metadata
  System,   ///< System configuration and services
  Audit,    ///< Audit logs
  User,     ///< User management
  Role,     ///< Role management
  Series,   ///< DICOM Series
  Image     ///< DICOM Image
};

/**
 * @brief specific actions that can be performed
 */
namespace Action {
constexpr std::uint32_t None = 0;
constexpr std::uint32_t Read = 1 << 0;
constexpr std::uint32_t Write = 1 << 1; // Create/Update
constexpr std::uint32_t Delete = 1 << 2;
constexpr std::uint32_t Export = 1 << 3;  // Download/Move
constexpr std::uint32_t Execute = 1 << 4; // Run commands/tools

constexpr std::uint32_t All = 0xFFFFFFFF;
constexpr std::uint32_t ReadWrite = Read | Write;
constexpr std::uint32_t Full = Read | Write | Delete | Export | Execute;
} // namespace Action

/**
 * @brief Represents a permission grant
 */
struct Permission {
  ResourceType resource;
  std::uint32_t actions; // Bitmask of Action flags

  constexpr bool has_action(std::uint32_t action_mask) const {
    return (actions & action_mask) == action_mask;
  }

  bool operator==(const Permission &other) const = default;
};

} // namespace pacs::security
