/**
 * @file migration_record.hpp
 * @brief Migration record structure for schema version tracking
 *
 * This file provides the migration_record structure that represents
 * an applied database migration.
 */

#pragma once

#include <string>

namespace pacs::storage {

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

}  // namespace pacs::storage
