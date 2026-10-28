// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file endpoint_uuid.h
 * @brief Resource ID generator shared by the REST endpoints
 *
 * Used by the annotation, measurement, key image, and viewer state
 * endpoints for new resource IDs.
 *
 * This is an internal header.
 */

#pragma once

#include <random>
#include <string>

namespace kcenon::pacs::web::endpoints::detail {

/**
 * @brief Generate a random (version 4) UUID for a new resource ID
 *
 * Crow runs handlers on concurrent threads, so each thread owns its engine
 * (seeded once from std::random_device) instead of sharing one.
 *
 * @return Lowercase "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx" string
 */
inline std::string generate_uuid() {
  static thread_local std::mt19937 gen(std::random_device{}());
  static thread_local std::uniform_int_distribution<> dis(0, 15);
  static constexpr char hex[] = "0123456789abcdef";

  std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
  for (char &c : uuid) {
    if (c == 'x') {
      c = hex[dis(gen)];
    } else if (c == 'y') {
      c = hex[(dis(gen) & 0x3) | 0x8];
    }
  }
  return uuid;
}

} // namespace kcenon::pacs::web::endpoints::detail
