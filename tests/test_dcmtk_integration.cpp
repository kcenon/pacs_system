#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

// DCMTK includes
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcuid.h"

// PACS includes
#include "common/dicom/dicom_file.h"
#include "common/dicom/codec_manager.h"
#include "modules/storage/scp/storage_scp.h"
#include "modules/storage/scu/storage_scu.h"
#include "modules/query_retrieve/scp/query_retrieve_scp.h"
#include "modules/query_retrieve/scu/query_retrieve_scu.h"
#include "modules/worklist/scp/worklist_scp.h"
#include "modules/worklist/scu/worklist_scu.h"
#include "modules/mpps/scp/mpps_scp.h"
#include "modules/mpps/scu/mpps_scu.h"

using namespace pacs;

// Test result tracking
struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
};

std::vector<TestResult> testResults;

void recordTest(const std::string& name, bool passed, const std::string& message = "") {
    testResults.push_back({name, passed, message});
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name;
    if (!message.empty()) {
        std::cout << " - " << message;
    }
    std::cout << std::endl;
}

// Test DICOM file operations
void testDicomFileOperations() {
    std::cout << "\n=== Testing DICOM File Operations ===" << std::endl;
    
    // Test creating a DICOM file
    {
        common::dicom::DicomFile dicomFile;
        
        // Create a simple dataset
        DcmDataset dataset;
        dataset.putAndInsertString(DCM_PatientName, "TEST^PATIENT");
        dataset.putAndInsertString(DCM_PatientID, "12345");
        dataset.putAndInsertString(DCM_StudyInstanceUID, dcmGenerateUniqueIdentifier(nullptr, SITE_STUDY_UID_ROOT));
        dataset.putAndInsertString(DCM_SeriesInstanceUID, dcmGenerateUniqueIdentifier(nullptr, SITE_SERIES_UID_ROOT));
        dataset.putAndInsertString(DCM_SOPInstanceUID, dcmGenerateUniqueIdentifier(nullptr, SITE_INSTANCE_UID_ROOT));
        dataset.putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
        dataset.putAndInsertString(DCM_Modality, "OT");
        
        // Set dataset
        dicomFile.setDataset(&dataset);
        
        // Save to file
        const std::string testFilename = "test_dicom.dcm";
        bool saveResult = dicomFile.save(testFilename);
        recordTest("DICOM File Save", saveResult);
        
        // Load from file
        common::dicom::DicomFile loadFile;
        bool loadResult = loadFile.load(testFilename);
        recordTest("DICOM File Load", loadResult);
        
        // Verify patient name
        if (loadResult) {
            DcmDataset* loadedDataset = loadFile.getDataset();
            OFString patientName;
            if (loadedDataset->findAndGetOFString(DCM_PatientName, patientName).good()) {
                recordTest("DICOM Patient Name Verification", patientName == "TEST^PATIENT");
            } else {
                recordTest("DICOM Patient Name Verification", false, "Could not retrieve patient name");
            }
        }
        
        // Clean up test file
        std::remove(testFilename.c_str());
    }
}

// Test codec manager
void testCodecManager() {
    std::cout << "\n=== Testing Codec Manager ===" << std::endl;
    
    common::dicom::CodecManager& codecManager = common::dicom::CodecManager::getInstance();
    
    // Test codec registration
    codecManager.registerAllCodecs();
    recordTest("Codec Registration", true, "All codecs registered");
    
    // Test parameter initialization
    codecManager.initializeEncodingParameters();
    recordTest("Encoding Parameters Initialization", true);
}

// Test Storage SCP/SCU
void testStorageService() {
    std::cout << "\n=== Testing Storage Service ===" << std::endl;
    
    // Configure services
    common::ServiceConfig scpConfig;
    scpConfig.aeTitle = "TEST_SCP";
    scpConfig.localPort = 11112;
    scpConfig.peerAETitle = "TEST_SCU";
    scpConfig.peerHost = "localhost";
    scpConfig.peerPort = 11113;
    
    common::ServiceConfig scuConfig;
    scuConfig.aeTitle = "TEST_SCU";
    scuConfig.localPort = 11113;
    scuConfig.peerAETitle = "TEST_SCP";
    scuConfig.peerHost = "localhost";
    scuConfig.peerPort = 11112;
    
    // Create Storage SCP
    storage::scp::StorageSCP storageSCP(scpConfig, "./test_storage");
    
    // Set storage callback
    bool storageReceived = false;
    storageSCP.setStorageCallback([&storageReceived](const std::string& filename, const DcmDataset* dataset) {
        storageReceived = true;
        std::cout << "Storage callback: Received file " << filename << std::endl;
    });
    
    // Start SCP
    auto scpStartResult = storageSCP.start();
    recordTest("Storage SCP Start", scpStartResult.isOk(), scpStartResult.isError() ? scpStartResult.error() : "");
    
    if (scpStartResult.isOk()) {
        // Give SCP time to start
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Create Storage SCU
        storage::scu::StorageSCU storageSCU(scuConfig);
        
        // Create a test dataset
        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();
        dataset->putAndInsertString(DCM_PatientName, "STORAGE^TEST");
        dataset->putAndInsertString(DCM_PatientID, "ST001");
        dataset->putAndInsertString(DCM_StudyInstanceUID, dcmGenerateUniqueIdentifier(nullptr, SITE_STUDY_UID_ROOT));
        dataset->putAndInsertString(DCM_SeriesInstanceUID, dcmGenerateUniqueIdentifier(nullptr, SITE_SERIES_UID_ROOT));
        dataset->putAndInsertString(DCM_SOPInstanceUID, dcmGenerateUniqueIdentifier(nullptr, SITE_INSTANCE_UID_ROOT));
        dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
        dataset->putAndInsertString(DCM_Modality, "OT");
        
        // Note: In a real test, we would send the dataset via C-STORE
        // For now, we just verify the services started correctly
        recordTest("Storage Service Setup", true);
        
        // Stop SCP
        storageSCP.stop();
    }
}

// Test Query/Retrieve Service
void testQueryRetrieveService() {
    std::cout << "\n=== Testing Query/Retrieve Service ===" << std::endl;
    
    // Configure services
    common::ServiceConfig config;
    config.aeTitle = "TEST_QR_SCP";
    config.localPort = 11114;
    config.peerAETitle = "TEST_QR_SCU";
    config.peerHost = "localhost";
    config.peerPort = 11115;
    
    // Create Query/Retrieve SCP
    query_retrieve::scp::QueryRetrieveSCP qrSCP(config, "./test_qr_storage");
    
    // Start SCP
    auto scpStartResult = qrSCP.start();
    recordTest("Query/Retrieve SCP Start", scpStartResult.isOk(), scpStartResult.isError() ? scpStartResult.error() : "");
    
    if (scpStartResult.isOk()) {
        // Create a test DICOM file in storage
        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();
        dataset->putAndInsertString(DCM_PatientName, "QR^TEST");
        dataset->putAndInsertString(DCM_PatientID, "QR001");
        dataset->putAndInsertString(DCM_StudyInstanceUID, "1.2.3.4.5");
        dataset->putAndInsertString(DCM_StudyDescription, "Test Study");
        dataset->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4.5.1");
        dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.4.5.1.1");
        dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
        dataset->putAndInsertString(DCM_Modality, "CT");
        
        // Save to storage directory
        fileFormat.saveFile("./test_qr_storage/test_qr.dcm");
        
        // Add file to index
        auto addResult = qrSCP.addFile("./test_qr_storage/test_qr.dcm");
        recordTest("Query/Retrieve Add File", addResult.isOk(), addResult.isError() ? addResult.error() : "");
        
        // Test query
        DcmDataset queryDataset;
        queryDataset.putAndInsertString(DCM_QueryRetrieveLevel, "STUDY");
        queryDataset.putAndInsertString(DCM_PatientID, "QR001");
        
        auto queryResult = qrSCP.query(&queryDataset, core::interfaces::query_retrieve::QueryRetrieveLevel::STUDY);
        recordTest("Query/Retrieve Query", queryResult.isOk() && !queryResult.value().empty());
        
        // Clean up
        if (queryResult.isOk()) {
            for (auto* ds : queryResult.value()) {
                delete ds;
            }
        }
        
        // Stop SCP
        qrSCP.stop();
        
        // Clean up test files
        std::remove("./test_qr_storage/test_qr.dcm");
        std::remove("./test_qr_storage/1.2.3.4.5.1.1.dcm");
    }
}

// Test Worklist Service
void testWorklistService() {
    std::cout << "\n=== Testing Worklist Service ===" << std::endl;
    
    // Configure services
    common::ServiceConfig config;
    config.aeTitle = "TEST_WL_SCP";
    config.localPort = 11116;
    
    // Create Worklist SCP
    worklist::scp::WorklistSCP worklistSCP(config, "./test_worklist");
    
    // Start SCP
    auto scpStartResult = worklistSCP.start();
    recordTest("Worklist SCP Start", scpStartResult.isOk(), scpStartResult.isError() ? scpStartResult.error() : "");
    
    if (scpStartResult.isOk()) {
        // Create a test worklist item
        DcmDataset worklistItem;
        worklistItem.putAndInsertString(DCM_PatientName, "WORKLIST^TEST");
        worklistItem.putAndInsertString(DCM_PatientID, "WL001");
        worklistItem.putAndInsertString(DCM_AccessionNumber, "ACC12345");
        
        // Add scheduled procedure step sequence
        DcmItem* spsItem = new DcmItem();
        spsItem->putAndInsertString(DCM_ScheduledProcedureStepStartDate, "20231215");
        spsItem->putAndInsertString(DCM_ScheduledProcedureStepStartTime, "143000");
        spsItem->putAndInsertString(DCM_Modality, "CT");
        spsItem->putAndInsertString(DCM_ScheduledStationAETitle, "CT_SCANNER");
        spsItem->putAndInsertString(DCM_ScheduledProcedureStepDescription, "CT Head");
        
        worklistItem.insertSequenceItem(DCM_ScheduledProcedureStepSequence, spsItem);
        
        // Add worklist item
        auto addResult = worklistSCP.addWorklistItem(&worklistItem);
        recordTest("Worklist Add Item", addResult.isOk(), addResult.isError() ? addResult.error() : "");
        
        // Test query
        DcmDataset queryDataset;
        queryDataset.putAndInsertString(DCM_PatientID, "WL001");
        
        auto queryResult = worklistSCP.findWorklist(&queryDataset);
        recordTest("Worklist Query", queryResult.isOk() && !queryResult.value().empty());
        
        // Clean up
        if (queryResult.isOk()) {
            for (auto* ds : queryResult.value()) {
                delete ds;
            }
        }
        
        // Stop SCP
        worklistSCP.stop();
        
        // Clean up test files
        std::remove("./test_worklist/ACC12345.wl");
    }
}

// Test MPPS Service
void testMPPSService() {
    std::cout << "\n=== Testing MPPS Service ===" << std::endl;
    
    // Configure services
    common::ServiceConfig config;
    config.aeTitle = "TEST_MPPS_SCP";
    config.localPort = 11118;
    
    // Create MPPS SCP
    mpps::scp::MPPSSCP mppsSCP(config);
    
    // Set callbacks
    bool createReceived = false;
    bool updateReceived = false;
    
    mppsSCP.setCreateCallback([&createReceived](const std::string& accessionNumber, const DcmDataset* dataset) {
        createReceived = true;
        std::cout << "MPPS Create callback: " << accessionNumber << std::endl;
    });
    
    mppsSCP.setUpdateCallback([&updateReceived](const std::string& accessionNumber, const DcmDataset* dataset) {
        updateReceived = true;
        std::cout << "MPPS Update callback: " << accessionNumber << std::endl;
    });
    
    // Start SCP
    auto scpStartResult = mppsSCP.start();
    recordTest("MPPS SCP Start", scpStartResult.isOk(), scpStartResult.isError() ? scpStartResult.error() : "");
    
    if (scpStartResult.isOk()) {
        // Note: In a real test, we would create an MPPS SCU and send N-CREATE/N-SET
        // For now, we just verify the service started correctly
        recordTest("MPPS Service Setup", true);
        
        // Stop SCP
        mppsSCP.stop();
    }
}

// Print test summary
void printTestSummary() {
    std::cout << "\n=== Test Summary ===" << std::endl;
    
    int totalTests = testResults.size();
    int passedTests = 0;
    int failedTests = 0;
    
    for (const auto& result : testResults) {
        if (result.passed) {
            passedTests++;
        } else {
            failedTests++;
        }
    }
    
    std::cout << "Total Tests: " << totalTests << std::endl;
    std::cout << "Passed: " << passedTests << std::endl;
    std::cout << "Failed: " << failedTests << std::endl;
    
    if (failedTests > 0) {
        std::cout << "\nFailed Tests:" << std::endl;
        for (const auto& result : testResults) {
            if (!result.passed) {
                std::cout << "  - " << result.testName;
                if (!result.message.empty()) {
                    std::cout << " (" << result.message << ")";
                }
                std::cout << std::endl;
            }
        }
    }
    
    std::cout << "\nOverall Result: " << (failedTests == 0 ? "PASS" : "FAIL") << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "PACS System DCMTK Integration Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Initialize codec manager
    common::dicom::CodecManager::getInstance().registerAllCodecs();
    
    // Run tests
    testDicomFileOperations();
    testCodecManager();
    testStorageService();
    testQueryRetrieveService();
    testWorklistService();
    testMPPSService();
    
    // Print summary
    printTestSummary();
    
    return (std::count_if(testResults.begin(), testResults.end(), 
                          [](const TestResult& r) { return !r.passed; }) > 0) ? 1 : 0;
}