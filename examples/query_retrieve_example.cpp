/**
 * @file query_retrieve_example.cpp
 * @brief Example demonstrating DICOM Query/Retrieve operations
 *
 * Copyright (c) 2024 PACS System
 * Licensed under BSD License
 */

#include <iostream>
#include <memory>
#include <vector>

#include "core/result.h"
#include "common/logger/log_module.h"
#include "modules/query_retrieve/query_retrieve_scp_module.h"
#include "common/dicom_util.h"

// Example query builder
std::unique_ptr<DcmDataset> buildPatientQuery(const std::string& patientId) {
    auto query = std::make_unique<DcmDataset>();
    
    // Query level
    query->putAndInsertString(DCM_QueryRetrieveLevel, "PATIENT");
    
    // Patient ID (can use wildcards)
    if (!patientId.empty()) {
        query->putAndInsertString(DCM_PatientID, patientId.c_str());
    }
    
    // Return keys (what we want back)
    query->putAndInsertString(DCM_PatientName, "");
    query->putAndInsertString(DCM_PatientBirthDate, "");
    query->putAndInsertString(DCM_PatientSex, "");
    
    return query;
}

std::unique_ptr<DcmDataset> buildStudyQuery(const std::string& patientId,
                                             const std::string& studyDate) {
    auto query = std::make_unique<DcmDataset>();
    
    // Query level
    query->putAndInsertString(DCM_QueryRetrieveLevel, "STUDY");
    
    // Search criteria
    if (!patientId.empty()) {
        query->putAndInsertString(DCM_PatientID, patientId.c_str());
    }
    if (!studyDate.empty()) {
        query->putAndInsertString(DCM_StudyDate, studyDate.c_str());
    }
    
    // Return keys
    query->putAndInsertString(DCM_StudyInstanceUID, "");
    query->putAndInsertString(DCM_StudyDescription, "");
    query->putAndInsertString(DCM_StudyTime, "");
    query->putAndInsertString(DCM_AccessionNumber, "");
    query->putAndInsertString(DCM_NumberOfStudyRelatedSeries, "");
    query->putAndInsertString(DCM_NumberOfStudyRelatedInstances, "");
    
    return query;
}

std::unique_ptr<DcmDataset> buildSeriesQuery(const std::string& studyInstanceUID) {
    auto query = std::make_unique<DcmDataset>();
    
    // Query level
    query->putAndInsertString(DCM_QueryRetrieveLevel, "SERIES");
    
    // Study Instance UID (required for series query)
    query->putAndInsertString(DCM_StudyInstanceUID, studyInstanceUID.c_str());
    
    // Return keys
    query->putAndInsertString(DCM_SeriesInstanceUID, "");
    query->putAndInsertString(DCM_SeriesDescription, "");
    query->putAndInsertString(DCM_SeriesNumber, "");
    query->putAndInsertString(DCM_Modality, "");
    query->putAndInsertString(DCM_NumberOfSeriesRelatedInstances, "");
    
    return query;
}

void displayQueryResults(const std::vector<DcmDataset*>& results,
                         const std::string& level) {
    logger::logInfo("Found {} {} results", results.size(), level);
    
    for (size_t i = 0; i < results.size(); ++i) {
        logger::logInfo("{} {}:", level, i + 1);
        
        if (level == "PATIENT") {
            OFString patientName, patientId, birthDate, sex;
            results[i]->findAndGetOFString(DCM_PatientName, patientName);
            results[i]->findAndGetOFString(DCM_PatientID, patientId);
            results[i]->findAndGetOFString(DCM_PatientBirthDate, birthDate);
            results[i]->findAndGetOFString(DCM_PatientSex, sex);
            
            logger::logInfo("  Name: {}", patientName.c_str());
            logger::logInfo("  ID: {}", patientId.c_str());
            logger::logInfo("  Birth Date: {}", birthDate.c_str());
            logger::logInfo("  Sex: {}", sex.c_str());
        }
        else if (level == "STUDY") {
            OFString studyUID, description, date, time, accession;
            results[i]->findAndGetOFString(DCM_StudyInstanceUID, studyUID);
            results[i]->findAndGetOFString(DCM_StudyDescription, description);
            results[i]->findAndGetOFString(DCM_StudyDate, date);
            results[i]->findAndGetOFString(DCM_StudyTime, time);
            results[i]->findAndGetOFString(DCM_AccessionNumber, accession);
            
            logger::logInfo("  Study UID: {}", studyUID.c_str());
            logger::logInfo("  Description: {}", description.c_str());
            logger::logInfo("  Date/Time: {} {}", date.c_str(), time.c_str());
            logger::logInfo("  Accession: {}", accession.c_str());
        }
        else if (level == "SERIES") {
            OFString seriesUID, description, modality, number;
            results[i]->findAndGetOFString(DCM_SeriesInstanceUID, seriesUID);
            results[i]->findAndGetOFString(DCM_SeriesDescription, description);
            results[i]->findAndGetOFString(DCM_Modality, modality);
            results[i]->findAndGetOFString(DCM_SeriesNumber, number);
            
            logger::logInfo("  Series UID: {}", seriesUID.c_str());
            logger::logInfo("  Description: {}", description.c_str());
            logger::logInfo("  Modality: {}", modality.c_str());
            logger::logInfo("  Number: {}", number.c_str());
        }
    }
}

int main() {
    // Initialize logger
    logger::initialize("query_retrieve_example", Logger::LogLevel::Info);
    
    logger::logInfo("DICOM Query/Retrieve Example");
    logger::logInfo("=============================");
    
    try {
        // Create query/retrieve module
        auto qrModule = std::make_shared<pacs::QueryRetrieveSCPModule>();
        
        // Initialize module (normally done by PACS server)
        auto initResult = qrModule->init();
        if (!initResult.isOk()) {
            logger::logError("Failed to initialize Q/R module: {}", 
                             initResult.getError());
            return 1;
        }
        
        // Example 1: Query for all patients
        logger::logInfo("\nExample 1: Query all patients");
        auto patientQuery = buildPatientQuery("*");  // Wildcard search
        
        std::vector<DcmDataset*> patientResults;
        // In real usage, this would be done through DICOM network
        // For demonstration, we'll simulate the results
        
        // Simulate some patient results
        auto patient1 = new DcmDataset();
        patient1->putAndInsertString(DCM_PatientName, "DOE^JOHN");
        patient1->putAndInsertString(DCM_PatientID, "PAT001");
        patient1->putAndInsertString(DCM_PatientBirthDate, "19700101");
        patient1->putAndInsertString(DCM_PatientSex, "M");
        patientResults.push_back(patient1);
        
        auto patient2 = new DcmDataset();
        patient2->putAndInsertString(DCM_PatientName, "SMITH^JANE");
        patient2->putAndInsertString(DCM_PatientID, "PAT002");
        patient2->putAndInsertString(DCM_PatientBirthDate, "19800515");
        patient2->putAndInsertString(DCM_PatientSex, "F");
        patientResults.push_back(patient2);
        
        displayQueryResults(patientResults, "PATIENT");
        
        // Example 2: Query studies for a specific patient
        logger::logInfo("\nExample 2: Query studies for patient PAT001");
        auto studyQuery = buildStudyQuery("PAT001", "");  // All dates
        
        std::vector<DcmDataset*> studyResults;
        
        // Simulate study results
        auto study1 = new DcmDataset();
        study1->putAndInsertString(DCM_StudyInstanceUID, "1.2.3.4.5.6.7.8.9");
        study1->putAndInsertString(DCM_StudyDescription, "Chest X-Ray");
        study1->putAndInsertString(DCM_StudyDate, "20240101");
        study1->putAndInsertString(DCM_StudyTime, "143000");
        study1->putAndInsertString(DCM_AccessionNumber, "ACC001");
        studyResults.push_back(study1);
        
        displayQueryResults(studyResults, "STUDY");
        
        // Example 3: Query series for a specific study
        logger::logInfo("\nExample 3: Query series for study");
        auto seriesQuery = buildSeriesQuery("1.2.3.4.5.6.7.8.9");
        
        std::vector<DcmDataset*> seriesResults;
        
        // Simulate series results
        auto series1 = new DcmDataset();
        series1->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4.5.6.7.8.9.1");
        series1->putAndInsertString(DCM_SeriesDescription, "PA View");
        series1->putAndInsertString(DCM_Modality, "CR");
        series1->putAndInsertString(DCM_SeriesNumber, "1");
        seriesResults.push_back(series1);
        
        auto series2 = new DcmDataset();
        series2->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4.5.6.7.8.9.2");
        series2->putAndInsertString(DCM_SeriesDescription, "Lateral View");
        series2->putAndInsertString(DCM_Modality, "CR");
        series2->putAndInsertString(DCM_SeriesNumber, "2");
        seriesResults.push_back(series2);
        
        displayQueryResults(seriesResults, "SERIES");
        
        // Example 4: Date range query
        logger::logInfo("\nExample 4: Query studies by date range");
        auto dateRangeQuery = buildStudyQuery("", "20240101-20240131");
        logger::logInfo("Query for studies between 2024-01-01 and 2024-01-31");
        
        // Example 5: Modality specific query
        logger::logInfo("\nExample 5: Query by modality");
        auto modalityQuery = std::make_unique<DcmDataset>();
        modalityQuery->putAndInsertString(DCM_QueryRetrieveLevel, "STUDY");
        modalityQuery->putAndInsertString(DCM_ModalitiesInStudy, "CT\\MR");
        modalityQuery->putAndInsertString(DCM_StudyInstanceUID, "");
        modalityQuery->putAndInsertString(DCM_StudyDescription, "");
        logger::logInfo("Query for CT and MR studies");
        
        // Cleanup
        for (auto ds : patientResults) delete ds;
        for (auto ds : studyResults) delete ds;
        for (auto ds : seriesResults) delete ds;
        
        // Stop module
        qrModule->stop();
        
        logger::logInfo("\nQuery/Retrieve example completed");
        
    } catch (const std::exception& e) {
        logger::logError("Exception: {}", e.what());
        return 1;
    }
    
    return 0;
}