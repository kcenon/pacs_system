// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file migration_record.hpp
 * @brief Migration record structure for schema version tracking
 *
 * This file provides the migration_record structure that represents
 * an applied database migration.
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <string>

namespace kcenon::pacs::storage {

/**
 * @brief Represents a record of an applied database migration
 *
 * Each migration_record contains information about a schema version
 * that has been applied to the database, including when it was applied.
 */
struct migration_record {
    int version;              ///< Schema version number
    std::string description;  ///< Description of the migration
    std::string applied_at;   ///< Timestamp when migration was applied
};

}  // namespace kcenon::pacs::storage
