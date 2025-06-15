#pragma once

#include "database_interface.h"
#include <memory>
#include <string>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <set>

// Forward declarations
#ifdef HAS_POSTGRESQL
typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;
#else
typedef void PGconn;
typedef void PGresult;
#endif

namespace pacs {
namespace core {
namespace database {

/**
 * @brief PostgreSQL database implementation
 */
class PostgreSQLDatabase : public DatabaseInterface {
public:
    PostgreSQLDatabase();
    ~PostgreSQLDatabase() override;
    
    /**
     * @brief Initialize the database connection
     * @param connectionString PostgreSQL connection string
     * @return Result indicating success or failure
     */
    Result<void> initialize(const std::string& connectionString) override;
    
    /**
     * @brief Close the database connection
     * @return Result object indicating success or failure
     */
    Result<void> close() override;
    
    /**
     * @brief Check if connected to the database
     * @return True if connected
     */
    bool isConnected() const override;
    
    /**
     * @brief Execute a query that returns results (SELECT)
     * @param query SQL query string
     * @param params Query parameters as key-value pairs
     * @return Result object containing query results
     */
    Result<ResultSet> query(const std::string& query, 
                          const std::map<std::string, std::string>& params = {}) override;
    
    /**
     * @brief Execute a query that returns no results (INSERT, UPDATE, DELETE)
     * @param query SQL query string
     * @param params Query parameters as key-value pairs
     * @return Result object indicating success or failure
     */
    Result<void> execute(const std::string& query, 
                        const std::map<std::string, std::string>& params = {}) override;
    
    /**
     * @brief Prepare a SQL statement
     * @param name Statement name
     * @param query SQL query with placeholders ($1, $2, etc.)
     * @return Result indicating success or failure
     */
    Result<void> prepareStatement(const std::string& name, const std::string& query);
    
    /**
     * @brief Execute a prepared statement
     * @param name Statement name
     * @param params Parameter values
     * @return Query result or error
     */
    Result<ResultSet> executePrepared(const std::string& name, 
                                        const std::vector<std::string>& params);
    
    /**
     * @brief Begin a transaction
     * @return Result indicating success or failure
     */
    Result<void> beginTransaction() override;
    
    /**
     * @brief Commit the current transaction
     * @return Result indicating success or failure
     */
    Result<void> commitTransaction() override;
    
    /**
     * @brief Rollback the current transaction
     * @return Result indicating success or failure
     */
    Result<void> rollbackTransaction() override;
    
    /**
     * @brief Get the last error message
     * @return Error message
     */
    std::string getLastError() const override;
    
    /**
     * @brief Create tables for PACS system
     * @return Result indicating success or failure
     */
    Result<void> createTables() override;
    
    /**
     * @brief Check if tables exist
     * @return True if all required tables exist
     */
    bool tablesExist();

private:
    /**
     * @brief Connection pool entry
     */
    struct PooledConnection {
        PGconn* connection;
        bool inUse;
        std::chrono::steady_clock::time_point lastUsed;
    };
    
    /**
     * @brief Get a connection from the pool
     * @return Connection or nullptr if unavailable
     */
    PGconn* getConnection();
    
    /**
     * @brief Return a connection to the pool
     * @param conn Connection to return
     */
    void returnConnection(PGconn* conn);
    
    /**
     * @brief Create a new connection
     * @return New connection or nullptr on failure
     */
    PGconn* createConnection();
    
    /**
     * @brief Check and reconnect if necessary
     * @param conn Connection to check
     * @return True if connection is valid
     */
    bool ensureConnected(PGconn* conn);
    
    /**
     * @brief Convert PGresult to ResultSet
     * @param result PostgreSQL result
     * @return ResultSet
     */
    ResultSet convertResult(PGresult* result);
    
    /**
     * @brief Execute a SQL query (internal method)
     * @param query SQL query string
     * @return Query result or error
     */
    Result<ResultSet> executeQuery(const std::string& query);
    
    /**
     * @brief Execute a SQL statement (internal method)
     * @param statement SQL statement
     * @return Result indicating success or failure
     */
    Result<void> executeStatement(const std::string& statement);
    
    /**
     * @brief Escape string for SQL
     * @param value String to escape
     * @return Escaped string
     */
    std::string escapeString(const std::string& value);
    
    // Connection pool
    std::vector<std::unique_ptr<PooledConnection>> connectionPool_;
    size_t minPoolSize_{2};
    size_t maxPoolSize_{10};
    mutable std::mutex poolMutex_;
    std::condition_variable poolCondition_;
    
    // Connection parameters
    std::string connectionString_;
    bool connected_{false};
    
    // Transaction state
    thread_local static bool inTransaction_;
    thread_local static PGconn* transactionConn_;
    
    // Prepared statements
    std::set<std::string> preparedStatements_;
    std::mutex preparedMutex_;
};

/**
 * @brief PostgreSQL connection parameters
 */
struct PostgreSQLConfig {
    std::string host = "localhost";
    int port = 5432;
    std::string database = "pacs";
    std::string username = "pacs";
    std::string password;
    std::string sslMode = "prefer";  // disable, allow, prefer, require, verify-ca, verify-full
    int connectionTimeout = 10;
    int commandTimeout = 30;
    
    // Connection pool settings
    size_t minPoolSize = 2;
    size_t maxPoolSize = 10;
    
    /**
     * @brief Convert to connection string
     * @return PostgreSQL connection string
     */
    std::string toConnectionString() const;
    
    /**
     * @brief Load from configuration
     * @return PostgreSQL configuration
     */
    static PostgreSQLConfig loadFromConfig();
};

} // namespace database
} // namespace core
} // namespace pacs