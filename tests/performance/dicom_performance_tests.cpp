/**
 * @file dicom_performance_tests.cpp
 * @brief DICOM-specific performance tests
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#include "performance_test_framework.h"
#include "common/dicom_util.h"
#include "modules/storage/storage_scp_module.h"
#include "modules/query_retrieve/query_retrieve_scp_module.h"
#include "common/network/dicom_connection_pool.h"

#include <random>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcfilefo.h>

using namespace pacs::perf;

namespace {

// Helper to create test DICOM dataset
std::unique_ptr<DcmDataset> createTestDataset(size_t size = 1024) {
    auto dataset = std::make_unique<DcmDataset>();
    
    // Basic patient/study info
    dataset->putAndInsertString(DCM_PatientName, "TEST^PATIENT");
    dataset->putAndInsertString(DCM_PatientID, "TEST123");
    dataset->putAndInsertString(DCM_StudyInstanceUID, "1.2.3.4.5.6.7.8.9");
    dataset->putAndInsertString(DCM_SeriesInstanceUID, "1.2.3.4.5.6.7.8.9.1");
    dataset->putAndInsertString(DCM_SOPInstanceUID, "1.2.3.4.5.6.7.8.9.1.1");
    dataset->putAndInsertString(DCM_Modality, "CT");
    
    // Add pixel data of specified size
    std::vector<uint8_t> pixelData(size);
    std::generate(pixelData.begin(), pixelData.end(), 
                  []() { return rand() % 256; });
    
    dataset->putAndInsertUint8Array(DCM_PixelData, 
                                     pixelData.data(), 
                                     pixelData.size());
    
    return dataset;
}

} // anonymous namespace

// Test: DICOM dataset parsing performance
PERF_TEST(DicomDatasetParsing) {
    // Create a test file in memory
    static std::vector<uint8_t> dicomData;
    
    if (dicomData.empty()) {
        auto dataset = createTestDataset(1024 * 1024); // 1MB
        DcmFileFormat fileFormat(dataset.get());
        
        // Serialize to memory
        size_t bufSize = 0;
        fileFormat.calcElementLength(EXS_LittleEndianExplicit, EET_ExplicitLength);
        bufSize = fileFormat.getLength(EXS_LittleEndianExplicit, EET_ExplicitLength);
        dicomData.resize(bufSize);
        
        DcmOutputBufferStream stream(dicomData.data(), bufSize);
        fileFormat.write(stream, EXS_LittleEndianExplicit, EET_ExplicitLength, nullptr);
    }
    
    // Parse the dataset
    DcmInputBufferStream stream(dicomData.data(), dicomData.size());
    DcmFileFormat fileFormat;
    OFCondition status = fileFormat.read(stream);
    
    if (!status.good()) {
        return core::Result<void>::error("Failed to parse DICOM dataset");
    }
    
    return core::Result<void>::ok();
}

// Test: DICOM query performance
PERF_TEST(DicomQuery) {
    static std::vector<std::unique_ptr<DcmDataset>> datasets;
    
    // Setup: Create test datasets
    if (datasets.empty()) {
        for (int i = 0; i < 1000; ++i) {
            auto dataset = std::make_unique<DcmDataset>();
            dataset->putAndInsertString(DCM_PatientName, 
                                        ("PATIENT^" + std::to_string(i)).c_str());
            dataset->putAndInsertString(DCM_PatientID, 
                                        ("PAT" + std::to_string(i)).c_str());
            dataset->putAndInsertString(DCM_StudyDate, "20240101");
            datasets.push_back(std::move(dataset));
        }
    }
    
    // Query for specific patient
    int found = 0;
    for (const auto& dataset : datasets) {
        OFString patientName;
        dataset->findAndGetOFString(DCM_PatientName, patientName);
        if (patientName.find("PATIENT^500") != OFString_npos) {
            found++;
        }
    }
    
    return core::Result<void>::ok();
}

// Test: Connection pool performance
PERF_TEST(ConnectionPoolBorrow) {
    static std::shared_ptr<ConnectionPool<DicomConnection>> pool;
    
    if (!pool) {
        DicomConnection::Parameters params;
        params.remoteHost = "127.0.0.1";
        params.remotePort = 11112;
        params.remoteAeTitle = "TEST_SCP";
        params.localAeTitle = "TEST_SCU";
        
        ConnectionPool<DicomConnection>::Config config;
        config.minSize = 5;
        config.maxSize = 10;
        
        pool = std::make_shared<ConnectionPool<DicomConnection>>(
            []() -> core::Result<std::unique_ptr<DicomConnection>> {
                // Mock connection creation
                return core::Result<std::unique_ptr<DicomConnection>>::ok(
                    std::make_unique<DicomConnection>());
            },
            config
        );
    }
    
    // Borrow and return connection
    auto borrowResult = pool->borrowConnection();
    if (!borrowResult.isOk()) {
        return core::Result<void>::error("Failed to borrow connection");
    }
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    
    // Connection automatically returned when out of scope
    return core::Result<void>::ok();
}

// Test: DICOM store performance
PERF_TEST(DicomStore) {
    static pacs::StorageSCPModule storage;
    static bool initialized = false;
    
    if (!initialized) {
        auto initResult = storage.init();
        if (!initResult.isOk()) {
            return core::Result<void>::error("Failed to initialize storage");
        }
        initialized = true;
    }
    
    // Create test dataset
    auto dataset = createTestDataset(10 * 1024); // 10KB
    
    // Generate unique SOP Instance UID
    static std::atomic<int> counter{0};
    std::string sopUid = "1.2.3.4.5.6.7.8.9.1." + std::to_string(counter++);
    dataset->putAndInsertString(DCM_SOPInstanceUID, sopUid.c_str());
    
    // Store dataset (mock storage)
    // In real implementation, this would write to disk/database
    
    return core::Result<void>::ok();
}

// Test: Concurrent DICOM operations
PERF_TEST(ConcurrentDicomOperations) {
    const size_t numOperations = 10;
    std::vector<std::future<bool>> futures;
    
    // Launch concurrent operations
    for (size_t i = 0; i < numOperations; ++i) {
        futures.push_back(std::async(std::launch::async, [i]() {
            auto dataset = createTestDataset(1024);
            
            // Simulate various operations
            OFString value;
            dataset->findAndGetOFString(DCM_PatientName, value);
            dataset->putAndInsertString(DCM_StudyDescription, 
                                        ("Study " + std::to_string(i)).c_str());
            
            return true;
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

// Test: Large dataset handling
PERF_TEST(LargeDicomDataset) {
    // Create large dataset (simulating CT scan with many slices)
    auto dataset = createTestDataset(10 * 1024 * 1024); // 10MB
    
    // Perform typical operations
    OFString patientName, studyUID;
    dataset->findAndGetOFString(DCM_PatientName, patientName);
    dataset->findAndGetOFString(DCM_StudyInstanceUID, studyUID);
    
    // Calculate dataset size
    DcmFileFormat fileFormat(dataset.get());
    fileFormat.calcElementLength(EXS_LittleEndianExplicit, EET_ExplicitLength);
    size_t size = fileFormat.getLength(EXS_LittleEndianExplicit, EET_ExplicitLength);
    
    if (size == 0) {
        return core::Result<void>::error("Invalid dataset size");
    }
    
    return core::Result<void>::ok();
}

// Main test runner
int main(int argc, char* argv[]) {
    pacs::perf::PerfTestRunner runner;
    
    // Register all tests
    REGISTER_PERF_TEST(DicomDatasetParsing);
    REGISTER_PERF_TEST(DicomQuery);
    REGISTER_PERF_TEST(ConnectionPoolBorrow);
    REGISTER_PERF_TEST(DicomStore);
    REGISTER_PERF_TEST(ConcurrentDicomOperations);
    REGISTER_PERF_TEST(LargeDicomDataset);
    
    // Configure test parameters
    pacs::perf::PerfTestConfig config;
    config.iterations = 1000;
    config.warmupIterations = 100;
    config.concurrentThreads = 1;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations" && i + 1 < argc) {
            config.iterations = std::stoi(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            config.concurrentThreads = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            config.outputFormat = argv[++i];
        }
    }
    
    // Run tests
    logger::initialize("dicom_perf_tests", Logger::LogLevel::Info);
    logger::logInfo("Running DICOM performance tests");
    logger::logInfo("Configuration: {} iterations, {} threads",
                    config.iterations, config.concurrentThreads);
    
    auto results = runner.runAll(config);
    
    // Generate report
    if (config.outputFormat == "json") {
        runner.generateReport(results, "dicom_perf_results.json");
    } else if (config.outputFormat == "csv") {
        runner.generateReport(results, "dicom_perf_results.csv");
    } else {
        runner.generateReport(results);
    }
    
    // Check for failures
    bool allPassed = std::all_of(results.begin(), results.end(),
                                 [](const auto& r) { return r.passed; });
    
    return allPassed ? 0 : 1;
}