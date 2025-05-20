#include "sqlite_database.h"
#include <stdexcept>
#include <iostream>

namespace pacs {
namespace core {
namespace database {

SQLiteDatabase::SQLiteDatabase() = default;

SQLiteDatabase::~SQLiteDatabase() {
    if (db_) {
        close();
    }
}

Result<void> SQLiteDatabase::initialize(const std::string& connectionString) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Close any existing connection
    if (db_) {
        auto result = close();
        if (!result.isOk()) {
            return result;
        }
    }
    
    // Open the database
    int rc = sqlite3_open(connectionString.c_str(), &db_);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        return Result<void>::error("Failed to open SQLite database: " + lastError_);
    }
    
    // Enable foreign keys
    rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return Result<void>::error("Failed to enable foreign keys: " + lastError_);
    }
    
    return Result<void>::ok();
}

Result<void> SQLiteDatabase::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return Result<void>::error("No database connection to close");
    }
    
    int rc = sqlite3_close(db_);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return Result<void>::error("Failed to close SQLite database: " + lastError_);
    }
    
    db_ = nullptr;
    return Result<void>::ok();
}

sqlite3_stmt* SQLiteDatabase::prepareStatement(const std::string& query,
                                             const std::map<std::string, std::string>& params) {
    if (!db_) {
        lastError_ = "No database connection";
        return nullptr;
    }
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return nullptr;
    }
    
    // Bind parameters
    for (const auto& [name, value] : params) {
        int index = sqlite3_bind_parameter_index(stmt, name.c_str());
        if (index > 0) {
            rc = sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                lastError_ = sqlite3_errmsg(db_);
                sqlite3_finalize(stmt);
                return nullptr;
            }
        }
    }
    
    return stmt;
}

std::vector<std::string> SQLiteDatabase::getColumnNames(sqlite3_stmt* statement) {
    std::vector<std::string> columnNames;
    
    int columnCount = sqlite3_column_count(statement);
    for (int i = 0; i < columnCount; i++) {
        const char* name = sqlite3_column_name(statement, i);
        columnNames.push_back(name ? name : "");
    }
    
    return columnNames;
}

Result<void> SQLiteDatabase::execute(const std::string& query,
                            const std::map<std::string, std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3_stmt* stmt = prepareStatement(query, params);
    if (!stmt) {
        return Result<void>::error("Failed to prepare statement: " + lastError_);
    }
    
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return Result<void>::error("Failed to execute statement: " + lastError_);
    }
    
    int changes = sqlite3_changes(db_);
    sqlite3_finalize(stmt);
    
    return Result<void>::ok();
}

Result<ResultSet> SQLiteDatabase::query(const std::string& query,
                                     const std::map<std::string, std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3_stmt* stmt = prepareStatement(query, params);
    if (!stmt) {
        return Result<ResultSet>::error("Failed to prepare statement: " + lastError_);
    }
    
    ResultSet results;
    auto columnNames = getColumnNames(stmt);
    
    while (true) {
        int rc = sqlite3_step(stmt);
        
        if (rc == SQLITE_ROW) {
            ResultRow row;
            
            for (int i = 0; i < columnNames.size(); i++) {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                row[columnNames[i]] = text ? text : "";
            }
            
            results.push_back(row);
        }
        else if (rc == SQLITE_DONE) {
            break;
        }
        else {
            lastError_ = sqlite3_errmsg(db_);
            sqlite3_finalize(stmt);
            return Result<ResultSet>::error("Error executing query: " + lastError_);
        }
    }
    
    sqlite3_finalize(stmt);
    return Result<ResultSet>::ok(results);
}

Result<void> SQLiteDatabase::beginTransaction() {
    return execute("BEGIN TRANSACTION;");
}

Result<void> SQLiteDatabase::commitTransaction() {
    return execute("COMMIT;");
}

Result<void> SQLiteDatabase::rollbackTransaction() {
    return execute("ROLLBACK;");
}

bool SQLiteDatabase::isConnected() const {
    return db_ != nullptr;
}

std::string SQLiteDatabase::getLastError() const {
    return lastError_;
}

Result<void> SQLiteDatabase::createTables() {
    // Create studies table
    auto result = execute(R"(
        CREATE TABLE IF NOT EXISTS studies (
            study_instance_uid TEXT PRIMARY KEY,
            patient_id TEXT NOT NULL,
            patient_name TEXT,
            study_date TEXT,
            study_time TEXT,
            accession_number TEXT,
            study_description TEXT,
            modality TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )");
    
    if (!result.isOk()) {
        return result;
    }
    
    // Create series table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS series (
            series_instance_uid TEXT PRIMARY KEY,
            study_instance_uid TEXT NOT NULL,
            series_number TEXT,
            modality TEXT,
            series_description TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (study_instance_uid) REFERENCES studies(study_instance_uid)
        );
    )");
    
    if (!result.isOk()) {
        return result;
    }
    
    // Create instances table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS instances (
            sop_instance_uid TEXT PRIMARY KEY,
            series_instance_uid TEXT NOT NULL,
            sop_class_uid TEXT,
            instance_number TEXT,
            file_path TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (series_instance_uid) REFERENCES series(series_instance_uid)
        );
    )");
    
    if (!result.isOk()) {
        return result;
    }
    
    // Create worklist table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS worklist (
            worklist_id TEXT PRIMARY KEY,
            patient_id TEXT NOT NULL,
            patient_name TEXT,
            accession_number TEXT,
            scheduled_procedure_step_start_date TEXT,
            scheduled_procedure_step_start_time TEXT,
            modality TEXT,
            scheduled_station_aet TEXT,
            scheduled_procedure_step_description TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )");
    
    if (!result.isOk()) {
        return result;
    }
    
    // Create MPPS table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS mpps (
            sop_instance_uid TEXT PRIMARY KEY,
            patient_id TEXT NOT NULL,
            study_instance_uid TEXT,
            performed_procedure_step_start_date TEXT,
            performed_procedure_step_start_time TEXT,
            performed_procedure_step_status TEXT,
            performed_station_aet TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )");
    
    return result;
}

} // namespace database
} // namespace core
} // namespace pacs