#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include "common/dicom/storage_client.h"
#include "thread_system/sources/logger/logger.h"

namespace fs = std::filesystem;
using namespace log_module;
using namespace pacs::common::dicom;

void showProgress(int current, int total, const std::string& filename) {
    // Calculate percentage
    int percent = static_cast<int>((static_cast<double>(current) / total) * 100);
    
    // Extract just the filename from the path
    std::string basename = fs::path(filename).filename().string();
    
    // Print progress
    write_information("Sending file %d of %d (%d%%): %s", 
                     current + 1, total, percent, basename.c_str());
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] <files or directories>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --host <hostname>    Hostname or IP address of the DICOM server (default: localhost)" << std::endl;
    std::cout << "  -p, --port <port>        Port of the DICOM server (default: 11112)" << std::endl;
    std::cout << "  -a, --aetitle <aetitle>  Called AE title (default: SIMPLE_STORAGE)" << std::endl;
    std::cout << "  -r, --recursive          Process directories recursively" << std::endl;
    std::cout << "  --help                   Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        // Default parameters
        std::string hostname = "localhost";
        uint16_t port = 11112;
        std::string aeTitle = "SIMPLE_STORAGE";
        bool recursive = false;
        std::vector<std::string> paths;
        
        // Parse command line arguments
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            
            if (arg == "--help") {
                printUsage(argv[0]);
                return 0;
            }
            else if (arg == "-h" || arg == "--host") {
                if (i + 1 < argc) {
                    hostname = argv[++i];
                }
            }
            else if (arg == "-p" || arg == "--port") {
                if (i + 1 < argc) {
                    port = static_cast<uint16_t>(std::stoi(argv[++i]));
                }
            }
            else if (arg == "-a" || arg == "--aetitle") {
                if (i + 1 < argc) {
                    aeTitle = argv[++i];
                }
            }
            else if (arg == "-r" || arg == "--recursive") {
                recursive = true;
            }
            else {
                // Assume it's a file or directory path
                paths.push_back(arg);
            }
        }
        
        if (paths.empty()) {
            std::cerr << "Error: No files or directories specified" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        
        // Initialize logger
        set_title("SIMPLE_STORAGE_CLIENT");
        console_target(log_types::Information | log_types::Error);
        start();
        
        write_information("Starting Simple Storage Client...");
        write_information("Server: %s:%d", hostname.c_str(), port);
        write_information("AE Title: %s", aeTitle.c_str());
        
        // Configure and create the storage client
        auto config = StorageClient::Config::createDefault()
            .withRemoteHost(hostname)
            .withRemotePort(port)
            .withRemoteAETitle(aeTitle)
            .withLocalAETitle("SIMPLE_CLIENT");
        
        StorageClient client(config);
        
        int totalFiles = 0;
        int successCount = 0;
        
        // Process each specified path
        for (const auto& path : paths) {
            if (fs::is_directory(path)) {
                write_information("Processing directory: %s", path.c_str());
                
                // Store directory with progress callback
                auto result = client.storeDirectory(path, recursive, showProgress);
                
                if (result.isSuccess()) {
                    write_information("Successfully sent all files from directory: %s", path.c_str());
                    successCount++;
                } else {
                    write_error("Error storing directory: %s - %s", 
                               path.c_str(), result.getErrorMessage().c_str());
                }
                
                totalFiles++;
            }
            else if (fs::is_regular_file(path)) {
                write_information("Sending file: %s", path.c_str());
                
                // Store individual file
                auto result = client.storeFile(path);
                
                if (result.isSuccess()) {
                    write_information("Successfully sent file: %s", path.c_str());
                    successCount++;
                } else {
                    write_error("Error storing file: %s - %s", 
                               path.c_str(), result.getErrorMessage().c_str());
                }
                
                totalFiles++;
            }
            else {
                write_error("Skipping invalid path: %s", path.c_str());
            }
        }
        
        write_information("Completed: %d of %d operations successful", successCount, totalFiles);
    }
    catch (const std::exception& ex) {
        write_error("Error: %s", ex.what());
        stop();
        return 1;
    }
    
    stop();
    return 0;
}