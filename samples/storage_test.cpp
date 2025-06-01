#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>

#include "common/dicom/dicom_file.h"
#include "common/dicom/dicom_object.h"
#include "common/dicom/storage_server.h"
#include "common/dicom/storage_client.h"
#include "common/dicom/codec_manager.h"

using namespace pacs::common::dicom;
namespace fs = std::filesystem;

void createTestDicomFile(const std::string& filename, int instanceNumber) {
    DicomObject obj;
    
    // Set patient information
    obj.setString(DicomTag::PatientName, "Test^Patient^" + std::to_string(instanceNumber));
    obj.setString(DicomTag::PatientID, "TEST" + std::to_string(1000 + instanceNumber));
    obj.setString(DicomTag::PatientBirthDate, "19800101");
    obj.setString(DicomTag::PatientSex, "M");
    
    // Set study information
    obj.setString(DicomTag::StudyInstanceUID, "1.2.3.4.5.6.7.8.9." + std::to_string(instanceNumber));
    obj.setString(DicomTag::StudyDate, "20240315");
    obj.setString(DicomTag::StudyTime, "12" + std::to_string(10 + instanceNumber) + "00");
    obj.setString(DicomTag::AccessionNumber, "ACC" + std::to_string(1000 + instanceNumber));
    obj.setString(DicomTag::Modality, "CT");
    
    // Set series information
    obj.setString(DicomTag::SeriesInstanceUID, "1.2.3.4.5.6.7.8.9." + std::to_string(instanceNumber) + ".1");
    obj.setInt(DicomTag::SeriesNumber, 1);
    
    // Set instance information
    obj.setString(DicomTag::SOPInstanceUID, "1.2.3.4.5.6.7.8.9." + std::to_string(instanceNumber) + ".1.1");
    obj.setString(DicomTag::SOPClassUID, "1.2.840.10008.5.1.4.1.1.2"); // CT Image Storage
    obj.setInt(DicomTag::InstanceNumber, instanceNumber);
    
    // Create and save file
    DicomFile file(obj);
    if (!file.save(filename)) {
        std::cerr << "Failed to create test file: " << filename << std::endl;
    } else {
        std::cout << "Created test file: " << filename << std::endl;
    }
}

void testStorageServer() {
    std::cout << "\n=== Storage Server Test ===\n";
    
    // Configure server
    StorageServer::Config serverConfig = StorageServer::Config::createDefault()
        .withAETitle("TEST_SCP")
        .withPort(11112)
        .withStorageDirectory("./test_storage")
        .withOrganizeFolders(true);
    
    // Create server
    StorageServer server(serverConfig);
    
    // Set storage callback
    server.setStorageCallback([](const std::string& sopInstanceUID, const DicomObject* object, 
                                const std::string& filename) {
        std::cout << "Server received file: " << filename << std::endl;
        std::cout << "  SOP Instance UID: " << sopInstanceUID << std::endl;
        if (object) {
            std::cout << "  Patient Name: " << object->getPatientName() << std::endl;
            std::cout << "  Study Date: " << object->getStudyDate() << std::endl;
            std::cout << "  Modality: " << object->getModality() << std::endl;
        }
    });
    
    // Start server
    auto result = server.start();
    if (result.isError()) {
        std::cerr << "Failed to start server: " << result.errorMessage() << std::endl;
        return;
    }
    
    std::cout << "Storage server started on port " << serverConfig.port << std::endl;
    std::cout << "Press Enter to stop server..." << std::endl;
    
    // Wait for user input
    std::cin.get();
    
    // Stop server
    server.stop();
    std::cout << "Storage server stopped\n";
}

void testStorageClient() {
    std::cout << "\n=== Storage Client Test ===\n";
    
    // Initialize codec manager
    CodecManager::getInstance().initialize();
    
    // Create test directory
    fs::create_directories("./test_files");
    
    // Create test files
    std::vector<std::string> testFiles;
    for (int i = 1; i <= 3; ++i) {
        std::string filename = "./test_files/test_" + std::to_string(i) + ".dcm";
        createTestDicomFile(filename, i);
        testFiles.push_back(filename);
    }
    
    // Configure client
    StorageClient::Config clientConfig = StorageClient::Config::createDefault()
        .withAETitle("TEST_SCU")
        .withRemoteAETitle("TEST_SCP")
        .withRemoteHost("localhost")
        .withRemotePort(11112);
    
    // Create client
    StorageClient client(clientConfig);
    
    // Test single file storage
    std::cout << "\nStoring single file..." << std::endl;
    auto result = client.storeFile(testFiles[0]);
    if (result.isError()) {
        std::cerr << "Failed to store file: " << result.errorMessage() << std::endl;
    } else {
        std::cout << "Successfully stored file: " << testFiles[0] << std::endl;
    }
    
    // Test multiple files storage
    std::cout << "\nStoring multiple files..." << std::endl;
    int completed = 0;
    result = client.storeFiles(testFiles, [&completed, &testFiles](int current, int total) {
        completed = current;
        std::cout << "Progress: " << current << "/" << total 
                  << " (" << (current * 100 / total) << "%)" << std::endl;
    });
    
    if (result.isError()) {
        std::cerr << "Failed to store files: " << result.errorMessage() << std::endl;
    } else {
        std::cout << "Successfully stored " << completed << " files" << std::endl;
    }
    
    // Test directory storage
    std::cout << "\nStoring directory..." << std::endl;
    result = client.storeDirectory("./test_files", true, [](int current, int total) {
        std::cout << "Directory progress: " << current << "/" << total << std::endl;
    });
    
    if (result.isError()) {
        std::cerr << "Failed to store directory: " << result.errorMessage() << std::endl;
    } else {
        std::cout << "Successfully stored directory" << std::endl;
    }
    
    // Cleanup codec manager
    CodecManager::getInstance().cleanup();
}

void testCodecManager() {
    std::cout << "\n=== Codec Manager Test ===\n";
    
    auto& codecManager = CodecManager::getInstance();
    codecManager.initialize();
    
    std::cout << "Supported transfer syntaxes:" << std::endl;
    auto syntaxes = codecManager.getSupportedTransferSyntaxes();
    for (const auto& syntax : syntaxes) {
        std::cout << "  " << syntax << " - " << codecManager.getTransferSyntaxName(syntax);
        if (codecManager.isLossyCompression(syntax)) {
            std::cout << " (lossy)";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\nCompressed transfer syntaxes:" << std::endl;
    auto compressed = codecManager.getCompressedTransferSyntaxes();
    for (const auto& syntax : compressed) {
        std::cout << "  " << codecManager.getTransferSyntaxName(syntax) << std::endl;
    }
    
    codecManager.cleanup();
}

int main(int argc, char* argv[]) {
    std::cout << "DICOM Storage Test Program\n";
    std::cout << "==========================\n";
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <test_type>" << std::endl;
        std::cout << "  test_type: server, client, codec, all" << std::endl;
        return 1;
    }
    
    std::string testType = argv[1];
    
    if (testType == "server") {
        testStorageServer();
    }
    else if (testType == "client") {
        // Give server time to start if running separately
        std::this_thread::sleep_for(std::chrono::seconds(2));
        testStorageClient();
    }
    else if (testType == "codec") {
        testCodecManager();
    }
    else if (testType == "all") {
        // Run server in background thread
        std::thread serverThread([]() {
            StorageServer::Config config = StorageServer::Config::createDefault()
                .withPort(11112)
                .withStorageDirectory("./test_storage");
            
            StorageServer server(config);
            server.setStorageCallback([](const std::string& uid, const DicomObject* obj, 
                                       const std::string& file) {
                std::cout << "[Server] Received: " << file << std::endl;
            });
            
            if (server.start().isOk()) {
                std::cout << "Test server started on port 11112" << std::endl;
                // Keep server running for 30 seconds
                std::this_thread::sleep_for(std::chrono::seconds(30));
                server.stop();
                std::cout << "Test server stopped" << std::endl;
            }
        });
        
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Run tests
        testCodecManager();
        testStorageClient();
        
        // Wait for server thread to finish
        serverThread.join();
    }
    else {
        std::cerr << "Unknown test type: " << testType << std::endl;
        return 1;
    }
    
    std::cout << "\nTest completed." << std::endl;
    return 0;
}