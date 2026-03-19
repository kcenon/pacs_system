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
 * @file permission.hpp
 * @brief Permission definitions for RBAC
 *
 * @copyright Copyright (c) 2025
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace kcenon::pacs::security {

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

} // namespace kcenon::pacs::security
