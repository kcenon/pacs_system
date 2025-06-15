/**
 * @file database_performance_tests.cpp
 * @brief Database-specific performance tests
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#include "performance_test_framework.h"
#include "core/database/database_factory.h"
#include "core/database/database_interface.h"

#include <random>
#include <sstream>

using namespace pacs::perf;

namespace {

// Test data generator
class TestDataGenerator {
public:
    static std::string generatePatientId() {
        static std::atomic<int> counter{0};
        return "PAT" + std::to_string(counter++);
    }
    
    static std::string generateStudyUid() {
        static std::atomic<int> counter{0};
        return "1.2.3.4.5.6.7.8." + std::to_string(counter++);
    }
    
    static std::string generatePatientName() {
        static const std::vector<std::string> firstNames = {
            "John", "Jane", "Bob", "Alice", "Charlie", "Diana"
        };
        static const std::vector<std::string> lastNames = {
            "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia"
        };
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> firstDist(0, firstNames.size() - 1);
        static std::uniform_int_distribution<> lastDist(0, lastNames.size() - 1);
        
        return lastNames[lastDist(gen)] + "^" + firstNames[firstDist(gen)];
    }
};

// Shared database instance
std::shared_ptr<pacs::DatabaseInterface> testDb;

} // anonymous namespace

// Test: Simple INSERT performance
PERF_TEST(DatabaseInsert) {
    if (!testDb) {
        // Create in-memory SQLite database for testing
        pacs::SQLiteConfig config;
        config.path = ":memory:";
        auto dbResult = pacs::DatabaseFactory::create("sqlite", config);
        if (!dbResult.isOk()) {
            return core::Result<void>::error("Failed to create database");
        }
        testDb = dbResult.getValue();
        
        // Create test table
        testDb->executeQuery(
            "CREATE TABLE IF NOT EXISTS patients ("
            "patient_id TEXT PRIMARY KEY,"
            "patient_name TEXT,"
            "birth_date TEXT,"
            "sex TEXT"
            ")", {});
    }
    
    // Insert a patient record
    auto patientId = TestDataGenerator::generatePatientId();
    auto patientName = TestDataGenerator::generatePatientName();
    
    auto result = testDb->executeQuery(
        "INSERT INTO patients (patient_id, patient_name, birth_date, sex) "
        "VALUES (?, ?, ?, ?)",
        {patientId, patientName, "19800101", "M"}
    );
    
    if (!result.isOk()) {
        return core::Result<void>::error("Insert failed: " + result.getError());
    }
    
    return core::Result<void>::ok();
}

// Test: Batch INSERT performance
PERF_TEST(DatabaseBatchInsert) {
    if (!testDb) {
        return core::Result<void>::error("Database not initialized");
    }
    
    // Begin transaction
    auto txResult = testDb->beginTransaction();
    if (!txResult.isOk()) {
        return core::Result<void>::error("Failed to begin transaction");
    }
    
    // Insert 100 records in a batch
    for (int i = 0; i < 100; ++i) {
        auto result = testDb->executeQuery(
            "INSERT OR IGNORE INTO patients (patient_id, patient_name, birth_date, sex) "
            "VALUES (?, ?, ?, ?)",
            {TestDataGenerator::generatePatientId(),
             TestDataGenerator::generatePatientName(),
             "19800101", "M"}
        );
        
        if (!result.isOk()) {
            testDb->rollback();
            return core::Result<void>::error("Batch insert failed");
        }
    }
    
    // Commit transaction
    auto commitResult = testDb->commit();
    if (!commitResult.isOk()) {
        return core::Result<void>::error("Commit failed");
    }
    
    return core::Result<void>::ok();
}

// Test: SELECT query performance
PERF_TEST(DatabaseSelect) {
    if (!testDb) {
        return core::Result<void>::error("Database not initialized");
    }
    
    // Query patients
    auto result = testDb->executeQuery(
        "SELECT patient_id, patient_name FROM patients LIMIT 10",
        {}
    );
    
    if (!result.isOk()) {
        return core::Result<void>::error("Select failed: " + result.getError());
    }
    
    auto rows = result.getValue();
    if (rows.empty()) {
        return core::Result<void>::error("No results returned");
    }
    
    return core::Result<void>::ok();
}

// Test: Complex JOIN query performance
PERF_TEST(DatabaseComplexQuery) {
    if (!testDb) {
        return core::Result<void>::error("Database not initialized");
    }
    
    // Create studies table if not exists
    testDb->executeQuery(
        "CREATE TABLE IF NOT EXISTS studies ("
        "study_uid TEXT PRIMARY KEY,"
        "patient_id TEXT,"
        "study_date TEXT,"
        "study_description TEXT,"
        "FOREIGN KEY(patient_id) REFERENCES patients(patient_id)"
        ")", {}
    );
    
    // Insert some test studies
    for (int i = 0; i < 10; ++i) {
        testDb->executeQuery(
            "INSERT OR IGNORE INTO studies (study_uid, patient_id, study_date, study_description) "
            "VALUES (?, ?, ?, ?)",
            {TestDataGenerator::generateStudyUid(),
             "PAT1", "20240101", "CT Chest"}
        );
    }
    
    // Complex query with JOIN
    auto result = testDb->executeQuery(
        "SELECT p.patient_name, s.study_uid, s.study_date "
        "FROM patients p "
        "JOIN studies s ON p.patient_id = s.patient_id "
        "WHERE s.study_date >= ? "
        "ORDER BY s.study_date DESC "
        "LIMIT 20",
        {"20240101"}
    );
    
    if (!result.isOk()) {
        return core::Result<void>::error("Complex query failed: " + result.getError());
    }
    
    return core::Result<void>::ok();
}

// Test: UPDATE performance
PERF_TEST(DatabaseUpdate) {
    if (!testDb) {
        return core::Result<void>::error("Database not initialized");
    }
    
    // Update patient name
    auto result = testDb->executeQuery(
        "UPDATE patients SET patient_name = ? WHERE patient_id = ?",
        {"UPDATED^NAME", "PAT1"}
    );
    
    if (!result.isOk()) {
        return core::Result<void>::error("Update failed: " + result.getError());
    }
    
    return core::Result<void>::ok();
}

// Test: DELETE performance
PERF_TEST(DatabaseDelete) {
    if (!testDb) {
        return core::Result<void>::error("Database not initialized");
    }
    
    // Delete old studies
    auto result = testDb->executeQuery(
        "DELETE FROM studies WHERE study_date < ?",
        {"20230101"}
    );
    
    if (!result.isOk()) {
        return core::Result<void>::error("Delete failed: " + result.getError());
    }
    
    return core::Result<void>::ok();
}

// Test: Index performance
PERF_TEST(DatabaseIndexedQuery) {
    if (!testDb) {
        return core::Result<void>::error("Database not initialized");
    }
    
    // Create index if not exists
    testDb->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_study_date ON studies(study_date)",
        {}
    );
    
    // Query using indexed column
    auto result = testDb->executeQuery(
        "SELECT study_uid FROM studies WHERE study_date = ?",
        {"20240101"}
    );
    
    if (!result.isOk()) {
        return core::Result<void>::error("Indexed query failed: " + result.getError());
    }
    
    return core::Result<void>::ok();
}

// Test: Concurrent database access
PERF_TEST(DatabaseConcurrentAccess) {
    if (!testDb) {
        return core::Result<void>::error("Database not initialized");
    }
    
    const size_t numThreads = 4;
    std::vector<std::future<bool>> futures;
    
    for (size_t i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [i]() {
            // Each thread performs different operations
            if (i % 4 == 0) {
                // INSERT
                auto result = testDb->executeQuery(
                    "INSERT OR IGNORE INTO patients (patient_id, patient_name) VALUES (?, ?)",
                    {TestDataGenerator::generatePatientId(), TestDataGenerator::generatePatientName()}
                );
                return result.isOk();
            } else if (i % 4 == 1) {
                // SELECT
                auto result = testDb->executeQuery(
                    "SELECT COUNT(*) FROM patients", {}
                );
                return result.isOk();
            } else if (i % 4 == 2) {
                // UPDATE
                auto result = testDb->executeQuery(
                    "UPDATE patients SET birth_date = ? WHERE patient_id LIKE 'PAT%' LIMIT 1",
                    {"19900101"}
                );
                return result.isOk();
            } else {
                // Complex query
                auto result = testDb->executeQuery(
                    "SELECT p.patient_id, COUNT(s.study_uid) as study_count "
                    "FROM patients p "
                    "LEFT JOIN studies s ON p.patient_id = s.patient_id "
                    "GROUP BY p.patient_id "
                    "LIMIT 10", {}
                );
                return result.isOk();
            }
        }));
    }
    
    // Wait for all operations
    for (auto& future : futures) {
        if (!future.get()) {
            return core::Result<void>::error("Concurrent operation failed");
        }
    }
    
    return core::Result<void>::ok();
}

// Main test runner
int main(int argc, char* argv[]) {
    pacs::perf::PerfTestRunner runner;
    
    // Register all tests
    REGISTER_PERF_TEST(DatabaseInsert);
    REGISTER_PERF_TEST(DatabaseBatchInsert);
    REGISTER_PERF_TEST(DatabaseSelect);
    REGISTER_PERF_TEST(DatabaseComplexQuery);
    REGISTER_PERF_TEST(DatabaseUpdate);
    REGISTER_PERF_TEST(DatabaseDelete);
    REGISTER_PERF_TEST(DatabaseIndexedQuery);
    REGISTER_PERF_TEST(DatabaseConcurrentAccess);
    
    // Configure test parameters
    pacs::perf::PerfTestConfig config;
    config.iterations = 1000;
    config.warmupIterations = 100;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations" && i + 1 < argc) {
            config.iterations = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            config.outputFormat = argv[++i];
        }
    }
    
    // Run tests
    logger::initialize("db_perf_tests", Logger::LogLevel::Info);
    logger::logInfo("Running database performance tests");
    
    auto results = runner.runAll(config);
    
    // Generate report
    if (config.outputFormat == "json") {
        runner.generateReport(results, "db_perf_results.json");
    } else if (config.outputFormat == "csv") {
        runner.generateReport(results, "db_perf_results.csv");
    } else {
        runner.generateReport(results);
    }
    
    // Cleanup
    testDb.reset();
    
    // Check for failures
    bool allPassed = std::all_of(results.begin(), results.end(),
                                 [](const auto& r) { return r.passed; });
    
    return allPassed ? 0 : 1;
}