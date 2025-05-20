#include <iostream>
#include <filesystem>
#include <string>

#include "core/database/database_manager.h"

using namespace pacs::core::database;

void printResultSet(const ResultSet& results) {
    if (results.empty()) {
        std::cout << "No results found." << std::endl;
        return;
    }
    
    // Print headers
    for (const auto& [key, _] : results[0]) {
        std::cout << key << "\t";
    }
    std::cout << std::endl;
    
    // Print separator line
    for (const auto& [key, _] : results[0]) {
        std::cout << std::string(key.length(), '-') << "\t";
    }
    std::cout << std::endl;
    
    // Print rows
    for (const auto& row : results) {
        for (const auto& [_, value] : row) {
            std::cout << value << "\t";
        }
        std::cout << std::endl;
    }
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "PACS Database Sample" << std::endl;
        std::cout << "===================" << std::endl << std::endl;
        
        // Create data directory if it doesn't exist
        std::filesystem::create_directories("./data");
        
        // Initialize database
        std::string dbPath = "./data/pacs.db";
        std::cout << "Initializing SQLite database at " << dbPath << std::endl;
        
        auto& dbManager = DatabaseManager::getInstance();
        auto result = dbManager.initialize(DatabaseType::SQLite, dbPath);
        
        if (!result.isSuccess()) {
            std::cerr << "Failed to initialize database: " << result.getMessage() << std::endl;
            return 1;
        }
        
        std::cout << "Database initialized successfully." << std::endl << std::endl;
        
        // Get database interface
        auto db = dbManager.getDatabase();
        
        // Insert sample data
        std::cout << "Inserting sample study..." << std::endl;
        
        result = db->beginTransaction();
        if (!result.isSuccess()) {
            std::cerr << "Failed to begin transaction: " << result.getMessage() << std::endl;
            return 1;
        }
        
        // Insert study
        std::string studyUid = "1.2.826.0.1.3680043.8.498.10010193774384923176560230966164592";
        result = db->execute(
            "INSERT OR REPLACE INTO studies "
            "(study_instance_uid, patient_id, patient_name, study_date, study_time, "
            "accession_number, study_description, modality) "
            "VALUES (:uid, :pid, :pname, :date, :time, :acc, :desc, :mod)",
            {
                {":uid", studyUid},
                {":pid", "PAT001"},
                {":pname", "Doe^John"},
                {":date", "20250520"},
                {":time", "120000"},
                {":acc", "ACC001"},
                {":desc", "Chest X-Ray"},
                {":mod", "CR"}
            }
        );
        
        if (!result.isSuccess()) {
            std::cerr << "Failed to insert study: " << result.getMessage() << std::endl;
            db->rollbackTransaction();
            return 1;
        }
        
        // Insert series
        std::string seriesUid = "1.2.826.0.1.3680043.8.498.45787401941419891429861528689208429");
        result = db->execute(
            "INSERT OR REPLACE INTO series "
            "(series_instance_uid, study_instance_uid, series_number, "
            "modality, series_description) "
            "VALUES (:uid, :study_uid, :num, :mod, :desc)",
            {
                {":uid", seriesUid},
                {":study_uid", studyUid},
                {":num", "1"},
                {":mod", "CR"},
                {":desc", "PA and Lateral"}
            }
        );
        
        if (!result.isSuccess()) {
            std::cerr << "Failed to insert series: " << result.getMessage() << std::endl;
            db->rollbackTransaction();
            return 1;
        }
        
        // Insert instance
        result = db->execute(
            "INSERT OR REPLACE INTO instances "
            "(sop_instance_uid, series_instance_uid, sop_class_uid, "
            "instance_number, file_path) "
            "VALUES (:uid, :series_uid, :class_uid, :num, :path)",
            {
                {":uid", "1.2.826.0.1.3680043.8.498.12714725698140322629866383307389559"},
                {":series_uid", seriesUid},
                {":class_uid", "1.2.840.10008.5.1.4.1.1.1"}, // CR Image Storage
                {":num", "1"},
                {":path", "/path/to/image.dcm"}
            }
        );
        
        if (!result.isSuccess()) {
            std::cerr << "Failed to insert instance: " << result.getMessage() << std::endl;
            db->rollbackTransaction();
            return 1;
        }
        
        result = db->commitTransaction();
        if (!result.isSuccess()) {
            std::cerr << "Failed to commit transaction: " << result.getMessage() << std::endl;
            db->rollbackTransaction();
            return 1;
        }
        
        std::cout << "Sample data inserted successfully." << std::endl << std::endl;
        
        // Query data
        std::cout << "Querying studies..." << std::endl;
        auto queryResult = db->query("SELECT * FROM studies");
        
        if (!queryResult.isSuccess()) {
            std::cerr << "Failed to query studies: " << queryResult.getMessage() << std::endl;
            return 1;
        }
        
        printResultSet(queryResult.getData());
        std::cout << std::endl;
        
        // Query data with join
        std::cout << "Querying studies with series..." << std::endl;
        queryResult = db->query(
            "SELECT s.patient_name, s.study_date, s.study_description, "
            "se.series_description, se.modality "
            "FROM studies s "
            "JOIN series se ON s.study_instance_uid = se.study_instance_uid"
        );
        
        if (!queryResult.isSuccess()) {
            std::cerr << "Failed to query studies with series: " << queryResult.getMessage() << std::endl;
            return 1;
        }
        
        printResultSet(queryResult.getData());
        std::cout << std::endl;
        
        // Close database
        std::cout << "Shutting down database..." << std::endl;
        result = dbManager.shutdown();
        
        if (!result.isSuccess()) {
            std::cerr << "Failed to shutdown database: " << result.getMessage() << std::endl;
            return 1;
        }
        
        std::cout << "Database shut down successfully." << std::endl;
        
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}