#include "database_manager.h"
#include "sqlite_database.h"
#include "postgresql_database.h"
#include <stdexcept>

namespace pacs {
namespace core {
namespace database {

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

Result<void> DatabaseManager::initialize(DatabaseType type, const std::string& connectionString) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return Result<void>::error("Database already initialized");
    }
    
    // Create the appropriate database implementation based on type
    try {
        switch (type) {
            case DatabaseType::SQLite:
                database_ = std::make_shared<SQLiteDatabase>();
                break;
            case DatabaseType::PostgreSQL:
                database_ = std::make_shared<PostgreSQLDatabase>();
                break;
            default:
                return Result<void>::error("Unsupported database type");
        }
        
        // Initialize the database
        auto result = database_->initialize(connectionString);
        if (!result.isOk()) {
            return result;
        }
        
        // Create tables if needed
        result = database_->createTables();
        if (!result.isOk()) {
            return result;
        }
        
        initialized_ = true;
        return Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return Result<void>::error("Failed to initialize database: " + std::string(ex.what()));
    }
}

std::shared_ptr<DatabaseInterface> DatabaseManager::getDatabase() {
    if (!initialized_) {
        throw std::runtime_error("Database not initialized");
    }
    return database_;
}

bool DatabaseManager::isInitialized() const {
    return initialized_;
}

Result<void> DatabaseManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return Result<void>::error("Database not initialized");
    }
    
    auto result = database_->close();
    if (result.isOk()) {
        initialized_ = false;
        database_.reset();
    }
    
    return result;
}

} // namespace database
} // namespace core
} // namespace pacs