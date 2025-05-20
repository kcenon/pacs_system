#pragma once

#include "database_interface.h"
#include <sqlite3.h>
#include <string>
#include <map>
#include <mutex>

namespace pacs {
namespace core {
namespace database {

/**
 * @brief SQLite implementation of the DatabaseInterface
 * 
 * This class provides a SQLite-specific implementation of the database
 * interface for the PACS system. It handles SQLite-specific connection,
 * query execution, and transaction management.
 */
class SQLiteDatabase : public DatabaseInterface {
public:
    /**
     * @brief Constructor
     */
    SQLiteDatabase();
    
    /**
     * @brief Destructor
     */
    ~SQLiteDatabase() override;
    
    /**
     * @brief Initialize the SQLite database connection
     * @param connectionString Path to SQLite database file
     * @return Result object indicating success or failure
     */
    Result<void> initialize(const std::string& connectionString) override;
    
    /**
     * @brief Close the SQLite database connection
     * @return Result object indicating success or failure
     */
    Result<void> close() override;
    
    /**
     * @brief Execute a query that returns no results (INSERT, UPDATE, DELETE)
     * @param query SQL query string
     * @param params Query parameters as key-value pairs
     * @return Result object indicating success or failure with rowsAffected
     */
    Result<void> execute(const std::string& query, 
                 const std::map<std::string, std::string>& params = {}) override;
    
    /**
     * @brief Execute a query that returns results (SELECT)
     * @param query SQL query string
     * @param params Query parameters as key-value pairs
     * @return Result object containing query results
     */
    Result<ResultSet> query(const std::string& query, 
                          const std::map<std::string, std::string>& params = {}) override;
    
    /**
     * @brief Begin a transaction
     * @return Result object indicating success or failure
     */
    Result<void> beginTransaction() override;
    
    /**
     * @brief Commit a transaction
     * @return Result object indicating success or failure
     */
    Result<void> commitTransaction() override;
    
    /**
     * @brief Rollback a transaction
     * @return Result object indicating success or failure
     */
    Result<void> rollbackTransaction() override;
    
    /**
     * @brief Check if database is connected
     * @return True if connected, false otherwise
     */
    bool isConnected() const override;
    
    /**
     * @brief Get the last error message
     * @return Error message string
     */
    std::string getLastError() const override;
    
    /**
     * @brief Create SQLite-specific database tables
     * @return Result object indicating success or failure
     */
    Result<void> createTables() override;

private:
    /**
     * @brief Prepare a statement with parameters
     * @param query SQL query string
     * @param params Query parameters
     * @return Prepared statement or nullptr on error
     */
    sqlite3_stmt* prepareStatement(const std::string& query, 
                                  const std::map<std::string, std::string>& params);
    
    /**
     * @brief Get column names from a prepared statement
     * @param statement Prepared statement
     * @return Vector of column names
     */
    std::vector<std::string> getColumnNames(sqlite3_stmt* statement);

    sqlite3* db_ = nullptr;
    std::string lastError_;
    std::mutex mutex_;
};

} // namespace database
} // namespace core
} // namespace pacs