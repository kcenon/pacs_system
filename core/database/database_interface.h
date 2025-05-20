#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

#include "core/result/result.h"

namespace pacs {
namespace core {
namespace database {

/**
 * @brief Database query result row type
 */
using ResultRow = std::map<std::string, std::string>;

/**
 * @brief Database query result set type
 */
using ResultSet = std::vector<ResultRow>;

/**
 * @brief Interface for database operations in PACS system
 * 
 * This interface provides a common set of methods for database
 * operations that can be implemented by different database backends.
 */
class DatabaseInterface {
public:
    virtual ~DatabaseInterface() = default;
    
    /**
     * @brief Initialize the database connection
     * @param connectionString Connection string for the database
     * @return Result object indicating success or failure
     */
    virtual Result<void> initialize(const std::string& connectionString) = 0;
    
    /**
     * @brief Close the database connection
     * @return Result object indicating success or failure
     */
    virtual Result<void> close() = 0;
    
    /**
     * @brief Execute a query that returns no results (INSERT, UPDATE, DELETE)
     * @param query SQL query string
     * @param params Query parameters as key-value pairs
     * @return Result object indicating success or failure with rowsAffected
     */
    virtual Result<void> execute(const std::string& query, 
                          const std::map<std::string, std::string>& params = {}) = 0;
    
    /**
     * @brief Execute a query that returns results (SELECT)
     * @param query SQL query string
     * @param params Query parameters as key-value pairs
     * @return Result object containing query results
     */
    virtual Result<ResultSet> query(const std::string& query, 
                                   const std::map<std::string, std::string>& params = {}) = 0;
    
    /**
     * @brief Begin a transaction
     * @return Result object indicating success or failure
     */
    virtual Result<void> beginTransaction() = 0;
    
    /**
     * @brief Commit a transaction
     * @return Result object indicating success or failure
     */
    virtual Result<void> commitTransaction() = 0;
    
    /**
     * @brief Rollback a transaction
     * @return Result object indicating success or failure
     */
    virtual Result<void> rollbackTransaction() = 0;
    
    /**
     * @brief Check if database is connected
     * @return True if connected, false otherwise
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Get the last error message
     * @return Error message string
     */
    virtual std::string getLastError() const = 0;
    
    /**
     * @brief Create database tables if they don't exist
     * @return Result object indicating success or failure
     */
    virtual Result<void> createTables() = 0;
};

} // namespace database
} // namespace core
} // namespace pacs