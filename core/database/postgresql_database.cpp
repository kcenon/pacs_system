#include "postgresql_database.h"
#include "common/logger/logger.h"
#include "common/config/config_manager.h"

#ifdef HAS_POSTGRESQL
#include <libpq-fe.h>
#endif

#include <sstream>
#include <algorithm>

namespace pacs {
namespace core {
namespace database {

#ifdef HAS_POSTGRESQL
// Thread-local storage for transaction state
thread_local bool PostgreSQLDatabase::inTransaction_ = false;
thread_local PGconn* PostgreSQLDatabase::transactionConn_ = nullptr;

PostgreSQLDatabase::PostgreSQLDatabase() {
    // Constructor
}

PostgreSQLDatabase::~PostgreSQLDatabase() {
    close();
}

Result<void> PostgreSQLDatabase::initialize(const std::string& connectionString) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    if (connected_) {
        return Result<void>::error("Already connected to database");
    }
    
    connectionString_ = connectionString;
    
    try {
        // Create initial connections for the pool
        for (size_t i = 0; i < minPoolSize_; ++i) {
            auto conn = createConnection();
            if (!conn) {
                // Clean up any created connections
                close();
                return Result<void>::error("Failed to create initial connection pool");
            }
            
            auto pooled = std::make_unique<PooledConnection>();
            pooled->connection = conn;
            pooled->inUse = false;
            pooled->lastUsed = std::chrono::steady_clock::now();
            
            connectionPool_.push_back(std::move(pooled));
        }
        
        connected_ = true;
        common::logger::logInfo("Connected to PostgreSQL database with {} connections", minPoolSize_);
        
        return Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return Result<void>::error("Failed to connect to database: " + std::string(ex.what()));
    }
}

Result<void> PostgreSQLDatabase::close() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Close all connections
    for (auto& pooled : connectionPool_) {
        if (pooled->connection) {
            PQfinish(pooled->connection);
        }
    }
    
    connectionPool_.clear();
    connected_ = false;
    
    common::logger::logInfo("Disconnected from PostgreSQL database");
    return Result<void>::ok();
}

bool PostgreSQLDatabase::isConnected() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return connected_ && !connectionPool_.empty();
}

Result<ResultSet> PostgreSQLDatabase::query(const std::string& query, 
                                          const std::map<std::string, std::string>& params) {
    // For now, ignore params and just execute the query
    return executeQuery(query);
}

Result<ResultSet> PostgreSQLDatabase::executeQuery(const std::string& query) {
    if (!isConnected()) {
        return Result<ResultSet>::error("Not connected to database");
    }
    
    PGconn* conn = nullptr;
    
    // Use transaction connection if in transaction
    if (inTransaction_ && transactionConn_) {
        conn = transactionConn_;
    } else {
        conn = getConnection();
        if (!conn) {
            return Result<ResultSet>::error("No available database connections");
        }
    }
    
    // Ensure connection is valid
    if (!ensureConnected(conn)) {
        if (!inTransaction_) {
            returnConnection(conn);
        }
        return Result<ResultSet>::error("Database connection lost");
    }
    
    // Execute query
    PGresult* result = PQexec(conn, query.c_str());
    
    if (!result) {
        std::string error = PQerrorMessage(conn);
        if (!inTransaction_) {
            returnConnection(conn);
        }
        return Result<ResultSet>::error("Query execution failed: " + error);
    }
    
    ExecStatusType status = PQresultStatus(result);
    
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        std::string error = PQresultErrorMessage(result);
        PQclear(result);
        if (!inTransaction_) {
            returnConnection(conn);
        }
        return Result<ResultSet>::error("Query failed: " + error);
    }
    
    // Convert result
    ResultSet queryResult = convertResult(result);
    PQclear(result);
    
    // Return connection to pool if not in transaction
    if (!inTransaction_) {
        returnConnection(conn);
    }
    
    return Result<ResultSet>::ok(std::move(queryResult));
}

Result<void> PostgreSQLDatabase::execute(const std::string& query, 
                                       const std::map<std::string, std::string>& params) {
    // For now, ignore params and just execute the statement
    return executeStatement(query);
}

Result<void> PostgreSQLDatabase::executeStatement(const std::string& statement) {
    auto result = executeQuery(statement);
    
    if (!result.isOk()) {
        return Result<void>::error(result.error());
    }
    
    return Result<void>::ok();
}

Result<void> PostgreSQLDatabase::prepareStatement(const std::string& name, const std::string& query) {
    if (!isConnected()) {
        return Result<void>::error("Not connected to database");
    }
    
    std::lock_guard<std::mutex> lock(preparedMutex_);
    
    // Check if already prepared
    if (preparedStatements_.find(name) != preparedStatements_.end()) {
        return Result<void>::ok();
    }
    
    // Prepare on all connections
    std::lock_guard<std::mutex> poolLock(poolMutex_);
    
    for (auto& pooled : connectionPool_) {
        if (!pooled->connection) continue;
        
        PGresult* result = PQprepare(pooled->connection, name.c_str(), query.c_str(), 0, nullptr);
        
        if (!result) {
            return Result<void>::error("Failed to prepare statement: " + name);
        }
        
        ExecStatusType status = PQresultStatus(result);
        PQclear(result);
        
        if (status != PGRES_COMMAND_OK) {
            return Result<void>::error("Failed to prepare statement: " + name);
        }
    }
    
    preparedStatements_.insert(name);
    
    return Result<void>::ok();
}

Result<ResultSet> PostgreSQLDatabase::executePrepared(const std::string& name, 
                                                         const std::vector<std::string>& params) {
    if (!isConnected()) {
        return Result<ResultSet>::error("Not connected to database");
    }
    
    // Check if prepared
    {
        std::lock_guard<std::mutex> lock(preparedMutex_);
        if (preparedStatements_.find(name) == preparedStatements_.end()) {
            return Result<ResultSet>::error("Statement not prepared: " + name);
        }
    }
    
    PGconn* conn = nullptr;
    
    // Use transaction connection if in transaction
    if (inTransaction_ && transactionConn_) {
        conn = transactionConn_;
    } else {
        conn = getConnection();
        if (!conn) {
            return Result<ResultSet>::error("No available database connections");
        }
    }
    
    // Convert parameters
    std::vector<const char*> paramValues;
    for (const auto& param : params) {
        paramValues.push_back(param.c_str());
    }
    
    // Execute prepared statement
    PGresult* result = PQexecPrepared(conn, name.c_str(), 
                                      static_cast<int>(params.size()),
                                      paramValues.data(),
                                      nullptr, nullptr, 0);
    
    if (!result) {
        std::string error = PQerrorMessage(conn);
        if (!inTransaction_) {
            returnConnection(conn);
        }
        return Result<ResultSet>::error("Prepared statement execution failed: " + error);
    }
    
    ExecStatusType status = PQresultStatus(result);
    
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        std::string error = PQresultErrorMessage(result);
        PQclear(result);
        if (!inTransaction_) {
            returnConnection(conn);
        }
        return Result<ResultSet>::error("Prepared statement failed: " + error);
    }
    
    // Convert result
    ResultSet queryResult = convertResult(result);
    PQclear(result);
    
    // Return connection to pool if not in transaction
    if (!inTransaction_) {
        returnConnection(conn);
    }
    
    return Result<ResultSet>::ok(std::move(queryResult));
}

Result<void> PostgreSQLDatabase::beginTransaction() {
    if (!isConnected()) {
        return Result<void>::error("Not connected to database");
    }
    
    if (inTransaction_) {
        return Result<void>::error("Already in transaction");
    }
    
    // Get a connection for this transaction
    PGconn* conn = getConnection();
    if (!conn) {
        return Result<void>::error("No available database connections");
    }
    
    // Start transaction
    PGresult* result = PQexec(conn, "BEGIN");
    
    if (!result) {
        std::string error = PQerrorMessage(conn);
        returnConnection(conn);
        return Result<void>::error("Failed to begin transaction: " + error);
    }
    
    ExecStatusType status = PQresultStatus(result);
    PQclear(result);
    
    if (status != PGRES_COMMAND_OK) {
        returnConnection(conn);
        return Result<void>::error("Failed to begin transaction");
    }
    
    // Set transaction state
    inTransaction_ = true;
    transactionConn_ = conn;
    
    return Result<void>::ok();
}

Result<void> PostgreSQLDatabase::commitTransaction() {
    if (!isConnected()) {
        return Result<void>::error("Not connected to database");
    }
    
    if (!inTransaction_ || !transactionConn_) {
        return Result<void>::error("Not in transaction");
    }
    
    // Commit transaction
    PGresult* result = PQexec(transactionConn_, "COMMIT");
    
    if (!result) {
        std::string error = PQerrorMessage(transactionConn_);
        return Result<void>::error("Failed to commit transaction: " + error);
    }
    
    ExecStatusType status = PQresultStatus(result);
    PQclear(result);
    
    // Return connection to pool
    returnConnection(transactionConn_);
    
    // Clear transaction state
    inTransaction_ = false;
    transactionConn_ = nullptr;
    
    if (status != PGRES_COMMAND_OK) {
        return Result<void>::error("Failed to commit transaction");
    }
    
    return Result<void>::ok();
}

Result<void> PostgreSQLDatabase::rollbackTransaction() {
    if (!isConnected()) {
        return Result<void>::error("Not connected to database");
    }
    
    if (!inTransaction_ || !transactionConn_) {
        return Result<void>::error("Not in transaction");
    }
    
    // Rollback transaction
    PGresult* result = PQexec(transactionConn_, "ROLLBACK");
    
    if (!result) {
        std::string error = PQerrorMessage(transactionConn_);
        return Result<void>::error("Failed to rollback transaction: " + error);
    }
    
    ExecStatusType status = PQresultStatus(result);
    PQclear(result);
    
    // Return connection to pool
    returnConnection(transactionConn_);
    
    // Clear transaction state
    inTransaction_ = false;
    transactionConn_ = nullptr;
    
    if (status != PGRES_COMMAND_OK) {
        return Result<void>::error("Failed to rollback transaction");
    }
    
    return Result<void>::ok();
}

std::string PostgreSQLDatabase::getLastError() const {
    if (!connectionPool_.empty() && connectionPool_[0]->connection) {
        return PQerrorMessage(connectionPool_[0]->connection);
    }
    return "No error information available";
}

Result<void> PostgreSQLDatabase::createTables() {
    if (!isConnected()) {
        return Result<void>::error("Not connected to database");
    }
    
    // Begin transaction
    auto txResult = beginTransaction();
    if (!txResult.isOk()) {
        return txResult;
    }
    
    // Create tables
    std::vector<std::string> createStatements = {
        // Studies table
        R"(CREATE TABLE IF NOT EXISTS studies (
            study_instance_uid VARCHAR(64) PRIMARY KEY,
            patient_id VARCHAR(64),
            patient_name VARCHAR(256),
            study_date DATE,
            study_time TIME,
            study_description TEXT,
            accession_number VARCHAR(64),
            referring_physician VARCHAR(256),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        ))",
        
        // Series table
        R"(CREATE TABLE IF NOT EXISTS series (
            series_instance_uid VARCHAR(64) PRIMARY KEY,
            study_instance_uid VARCHAR(64) REFERENCES studies(study_instance_uid) ON DELETE CASCADE,
            modality VARCHAR(16),
            series_number INTEGER,
            series_date DATE,
            series_time TIME,
            series_description TEXT,
            body_part_examined VARCHAR(64),
            patient_position VARCHAR(16),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        ))",
        
        // Instances table
        R"(CREATE TABLE IF NOT EXISTS instances (
            sop_instance_uid VARCHAR(64) PRIMARY KEY,
            series_instance_uid VARCHAR(64) REFERENCES series(series_instance_uid) ON DELETE CASCADE,
            sop_class_uid VARCHAR(64),
            instance_number INTEGER,
            storage_path TEXT,
            file_size BIGINT,
            transfer_syntax_uid VARCHAR(64),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        ))",
        
        // Worklist table
        R"(CREATE TABLE IF NOT EXISTS worklist (
            id SERIAL PRIMARY KEY,
            accession_number VARCHAR(64) UNIQUE,
            patient_id VARCHAR(64),
            patient_name VARCHAR(256),
            patient_birth_date DATE,
            patient_sex CHAR(1),
            study_instance_uid VARCHAR(64),
            requested_procedure_id VARCHAR(64),
            requested_procedure_description TEXT,
            scheduled_procedure_step_id VARCHAR(64),
            scheduled_procedure_step_description TEXT,
            scheduled_station_ae_title VARCHAR(16),
            scheduled_start_date DATE,
            scheduled_start_time TIME,
            modality VARCHAR(16),
            performing_physician VARCHAR(256),
            status VARCHAR(32) DEFAULT 'SCHEDULED',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        ))",
        
        // MPPS table
        R"(CREATE TABLE IF NOT EXISTS mpps (
            mpps_sop_instance_uid VARCHAR(64) PRIMARY KEY,
            scheduled_procedure_step_id VARCHAR(64),
            performed_procedure_step_id VARCHAR(64),
            performed_procedure_step_start_date DATE,
            performed_procedure_step_start_time TIME,
            performed_procedure_step_end_date DATE,
            performed_procedure_step_end_time TIME,
            performed_procedure_step_status VARCHAR(32),
            performed_procedure_step_description TEXT,
            performed_protocol_code_sequence TEXT,
            performed_series_sequence TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        ))",
        
        // Create indexes
        "CREATE INDEX IF NOT EXISTS idx_studies_patient_id ON studies(patient_id)",
        "CREATE INDEX IF NOT EXISTS idx_studies_study_date ON studies(study_date)",
        "CREATE INDEX IF NOT EXISTS idx_studies_accession_number ON studies(accession_number)",
        "CREATE INDEX IF NOT EXISTS idx_series_study_uid ON series(study_instance_uid)",
        "CREATE INDEX IF NOT EXISTS idx_series_modality ON series(modality)",
        "CREATE INDEX IF NOT EXISTS idx_instances_series_uid ON instances(series_instance_uid)",
        "CREATE INDEX IF NOT EXISTS idx_worklist_patient_id ON worklist(patient_id)",
        "CREATE INDEX IF NOT EXISTS idx_worklist_scheduled_date ON worklist(scheduled_start_date)",
        "CREATE INDEX IF NOT EXISTS idx_worklist_status ON worklist(status)",
        
        // Create update triggers
        R"(CREATE OR REPLACE FUNCTION update_updated_at_column()
        RETURNS TRIGGER AS $$
        BEGIN
            NEW.updated_at = CURRENT_TIMESTAMP;
            RETURN NEW;
        END;
        $$ language 'plpgsql')",
        
        "CREATE TRIGGER update_studies_updated_at BEFORE UPDATE ON studies FOR EACH ROW EXECUTE FUNCTION update_updated_at_column()",
        "CREATE TRIGGER update_series_updated_at BEFORE UPDATE ON series FOR EACH ROW EXECUTE FUNCTION update_updated_at_column()",
        "CREATE TRIGGER update_instances_updated_at BEFORE UPDATE ON instances FOR EACH ROW EXECUTE FUNCTION update_updated_at_column()",
        "CREATE TRIGGER update_worklist_updated_at BEFORE UPDATE ON worklist FOR EACH ROW EXECUTE FUNCTION update_updated_at_column()",
        "CREATE TRIGGER update_mpps_updated_at BEFORE UPDATE ON mpps FOR EACH ROW EXECUTE FUNCTION update_updated_at_column()"
    };
    
    // Execute create statements
    for (const auto& statement : createStatements) {
        auto result = executeStatement(statement);
        if (!result.isOk()) {
            rollbackTransaction();
            return Result<void>::error("Failed to create tables: " + result.error());
        }
    }
    
    // Commit transaction
    return commitTransaction();
}

bool PostgreSQLDatabase::tablesExist() {
    if (!isConnected()) {
        return false;
    }
    
    // Check if all required tables exist
    const std::vector<std::string> requiredTables = {
        "studies", "series", "instances", "worklist", "mpps"
    };
    
    for (const auto& table : requiredTables) {
        std::string query = "SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = '" + table + "')";
        
        auto result = executeQuery(query);
        if (!result.isOk() || result.value().empty()) {
            return false;
        }
        
        // ResultSet is vector<ResultRow> where ResultRow is map<string, string>
        const auto& firstRow = result.value()[0];
        auto existsIt = firstRow.find("exists");
        if (existsIt == firstRow.end() || existsIt->second != "t") {
            return false;
        }
    }
    
    return true;
}

PGconn* PostgreSQLDatabase::getConnection() {
    std::unique_lock<std::mutex> lock(poolMutex_);
    
    // Find available connection
    for (auto& pooled : connectionPool_) {
        if (!pooled->inUse) {
            pooled->inUse = true;
            pooled->lastUsed = std::chrono::steady_clock::now();
            return pooled->connection;
        }
    }
    
    // Create new connection if pool not at max size
    if (connectionPool_.size() < maxPoolSize_) {
        auto conn = createConnection();
        if (conn) {
            auto pooled = std::make_unique<PooledConnection>();
            pooled->connection = conn;
            pooled->inUse = true;
            pooled->lastUsed = std::chrono::steady_clock::now();
            
            connectionPool_.push_back(std::move(pooled));
            return conn;
        }
    }
    
    // Wait for available connection
    poolCondition_.wait(lock, [this] {
        return std::any_of(connectionPool_.begin(), connectionPool_.end(),
                           [](const auto& pooled) { return !pooled->inUse; });
    });
    
    // Try again
    for (auto& pooled : connectionPool_) {
        if (!pooled->inUse) {
            pooled->inUse = true;
            pooled->lastUsed = std::chrono::steady_clock::now();
            return pooled->connection;
        }
    }
    
    return nullptr;
}

void PostgreSQLDatabase::returnConnection(PGconn* conn) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    for (auto& pooled : connectionPool_) {
        if (pooled->connection == conn) {
            pooled->inUse = false;
            pooled->lastUsed = std::chrono::steady_clock::now();
            poolCondition_.notify_one();
            return;
        }
    }
}

PGconn* PostgreSQLDatabase::createConnection() {
    PGconn* conn = PQconnectdb(connectionString_.c_str());
    
    if (!conn || PQstatus(conn) != CONNECTION_OK) {
        if (conn) {
            common::logger::logError("Failed to connect to PostgreSQL: {}", PQerrorMessage(conn));
            PQfinish(conn);
        }
        return nullptr;
    }
    
    // Set client encoding to UTF8
    PQsetClientEncoding(conn, "UTF8");
    
    // Prepare statements on new connection
    std::lock_guard<std::mutex> lock(preparedMutex_);
    for (const auto& name : preparedStatements_) {
        // Re-prepare statements on new connection
        // Note: This would need the actual query stored
    }
    
    return conn;
}

bool PostgreSQLDatabase::ensureConnected(PGconn* conn) {
    if (!conn) {
        return false;
    }
    
    // Check connection status
    ConnStatusType status = PQstatus(conn);
    
    if (status == CONNECTION_OK) {
        return true;
    }
    
    // Try to reset connection
    PQreset(conn);
    
    // Check again
    status = PQstatus(conn);
    return status == CONNECTION_OK;
}

ResultSet PostgreSQLDatabase::convertResult(PGresult* result) {
    ResultSet queryResult;
    
    if (!result) {
        return queryResult;
    }
    
    int numRows = PQntuples(result);
    int numCols = PQnfields(result);
    
    // Get column names
    std::vector<std::string> columnNames;
    for (int i = 0; i < numCols; ++i) {
        columnNames.push_back(PQfname(result, i));
    }
    
    // Get row data
    for (int row = 0; row < numRows; ++row) {
        ResultRow rowData;
        
        for (int col = 0; col < numCols; ++col) {
            std::string value;
            if (PQgetisnull(result, row, col)) {
                value = "";
            } else {
                value = PQgetvalue(result, row, col);
            }
            rowData[columnNames[col]] = value;
        }
        
        queryResult.push_back(std::move(rowData));
    }
    
    return queryResult;
}

std::string PostgreSQLDatabase::escapeString(const std::string& value) {
    if (!isConnected() || connectionPool_.empty()) {
        return value;
    }
    
    // Get a connection for escaping
    PGconn* conn = connectionPool_[0]->connection;
    
    // Allocate buffer for escaped string (worst case: 2n+1)
    std::vector<char> buffer(value.length() * 2 + 1);
    
    int error = 0;
    PQescapeStringConn(conn, buffer.data(), value.c_str(), value.length(), &error);
    
    if (error) {
        common::logger::logError("Failed to escape string");
        return value;
    }
    
    return std::string(buffer.data());
}

// PostgreSQLConfig implementation
std::string PostgreSQLConfig::toConnectionString() const {
    std::stringstream ss;
    
    ss << "host=" << host;
    ss << " port=" << port;
    ss << " dbname=" << database;
    ss << " user=" << username;
    
    if (!password.empty()) {
        ss << " password=" << password;
    }
    
    ss << " sslmode=" << sslMode;
    ss << " connect_timeout=" << connectionTimeout;
    ss << " options='-c statement_timeout=" << (commandTimeout * 1000) << "'";
    
    return ss.str();
}

PostgreSQLConfig PostgreSQLConfig::loadFromConfig() {
    PostgreSQLConfig config;
    
    auto& configManager = common::config::ConfigManager::getInstance();
    
    config.host = configManager.getValue("database.postgresql.host", "localhost");
    config.port = std::stoi(configManager.getValue("database.postgresql.port", "5432"));
    config.database = configManager.getValue("database.postgresql.database", "pacs");
    config.username = configManager.getValue("database.postgresql.username", "pacs");
    config.password = configManager.getValue("database.postgresql.password", "");
    config.sslMode = configManager.getValue("database.postgresql.ssl_mode", "prefer");
    config.connectionTimeout = std::stoi(configManager.getValue("database.postgresql.connection_timeout", "10"));
    config.commandTimeout = std::stoi(configManager.getValue("database.postgresql.command_timeout", "30"));
    
    config.minPoolSize = std::stoul(configManager.getValue("database.postgresql.min_pool_size", "2"));
    config.maxPoolSize = std::stoul(configManager.getValue("database.postgresql.max_pool_size", "10"));
    
    return config;
}

#else // !HAS_POSTGRESQL

// Thread-local storage for transaction state (dummy)
thread_local bool PostgreSQLDatabase::inTransaction_ = false;
thread_local void* PostgreSQLDatabase::transactionConn_ = nullptr;

// Dummy implementations when PostgreSQL is not available
PostgreSQLDatabase::PostgreSQLDatabase() {}
PostgreSQLDatabase::~PostgreSQLDatabase() {}

Result<void> PostgreSQLDatabase::initialize(const std::string&) {
    return Result<void>::error("PostgreSQL support not compiled in");
}

Result<void> PostgreSQLDatabase::close() {
    return Result<void>::ok();
}

bool PostgreSQLDatabase::isConnected() const {
    return false;
}

Result<ResultSet> PostgreSQLDatabase::query(const std::string&, const std::map<std::string, std::string>&) {
    return Result<ResultSet>::error("PostgreSQL support not compiled in");
}

Result<ResultSet> PostgreSQLDatabase::executeQuery(const std::string&) {
    return Result<ResultSet>::error("PostgreSQL support not compiled in");
}

Result<void> PostgreSQLDatabase::beginTransaction() {
    return Result<void>::error("PostgreSQL support not compiled in");
}

Result<void> PostgreSQLDatabase::commitTransaction() {
    return Result<void>::error("PostgreSQL support not compiled in");
}

Result<void> PostgreSQLDatabase::rollbackTransaction() {
    return Result<void>::error("PostgreSQL support not compiled in");
}

std::string PostgreSQLDatabase::getLastError() const {
    return "PostgreSQL support not compiled in";
}

Result<void> PostgreSQLDatabase::createTables() {
    return Result<void>::error("PostgreSQL support not compiled in");
}

std::string PostgreSQLDatabase::escapeString(const std::string& str) {
    return str;
}

bool PostgreSQLDatabase::ensureConnected(PGconn*) {
    return false;
}

PGconn* PostgreSQLDatabase::createConnection() {
    return nullptr;
}

PGconn* PostgreSQLDatabase::getConnection() {
    return nullptr;
}

void PostgreSQLDatabase::returnConnection(PGconn*) {}

ResultSet PostgreSQLDatabase::convertResult(PGresult*) {
    return ResultSet();
}

#endif // HAS_POSTGRESQL

} // namespace database
} // namespace core
} // namespace pacs