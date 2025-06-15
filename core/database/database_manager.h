#pragma once

#include <string>
#include <memory>
#include <mutex>

#include "database_interface.h"

namespace pacs {
namespace core {
namespace database {

/**
 * @brief Database type enumeration
 */
enum class DatabaseType {
    SQLite,
    PostgreSQL,
    MySQL,
    MongoDB
};

/**
 * @brief Singleton manager for database access
 * 
 * This class provides a singleton interface for accessing the database
 * throughout the PACS application. It allows for centralizing configuration
 * and providing a common access point for database operations.
 */
class DatabaseManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the DatabaseManager singleton
     */
    static DatabaseManager& getInstance();

    /**
     * @brief Initialize the database manager
     * @param type Type of database to use
     * @param connectionString Connection string for the database
     * @return Result object indicating success or failure
     */
    Result<void> initialize(DatabaseType type, const std::string& connectionString);

    /**
     * @brief Get the database interface
     * @return Shared pointer to the database interface
     */
    std::shared_ptr<DatabaseInterface> getDatabase();

    /**
     * @brief Check if database is initialized
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const;

    /**
     * @brief Shutdown the database manager and close connections
     * @return Result object indicating success or failure
     */
    Result<void> shutdown();

private:
    // Private constructor for singleton pattern
    DatabaseManager() = default;
    
    // Delete copy and move constructors and assignment operators
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&&) = delete;
    DatabaseManager& operator=(DatabaseManager&&) = delete;

    std::shared_ptr<DatabaseInterface> database_;
    std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace database
} // namespace core
} // namespace pacs