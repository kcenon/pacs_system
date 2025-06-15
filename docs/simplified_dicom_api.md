# Simplified DICOM API Guide

This guide demonstrates how to use the simplified DICOM API to perform common DICOM operations without requiring in-depth knowledge of DCMTK.

## Introduction

The simplified DICOM API provides a high-level interface for working with DICOM data and services, abstracting away the complexity of the underlying DCMTK library. The API is designed to be intuitive, type-safe, and easy to use, even for developers who are not familiar with the DICOM standard.

## Core Components

The API consists of several core components:

1. **DicomObject** - Represents a DICOM dataset
2. **DicomFile** - Represents a DICOM file on disk
3. **DicomTag** - Provides a simple way to work with DICOM tags
4. **StorageClient** - Client for storing DICOM objects on a server
5. **StorageServer** - Server for receiving DICOM objects
6. **DicomError** - Unified error handling system

## Working with DICOM Objects

### Creating a DICOM Object

```cpp
// Create an empty DICOM object
DicomObject obj;

// Set some values
obj.setString(DicomTag::PatientName, "DOE^JOHN");
obj.setString(DicomTag::PatientID, "12345");
obj.setString(DicomTag::Modality, "CT");
```

### Reading Values from a DICOM Object

```cpp
// Get values with type conversion
std::string patientName = obj.getString(DicomTag::PatientName);
std::optional<int> rows = obj.getInt(DicomTag::Rows);
std::optional<double> pixelSpacing = obj.getFloat(DicomTag::PixelSpacing);

// Check if a tag exists
if (obj.hasTag(DicomTag::PixelData)) {
    // Access pixel data...
}

// Get convenient patient/study information
std::string patientId = obj.getPatientID();
std::string studyUid = obj.getStudyInstanceUID();
```

### Working with DICOM Files

```cpp
// Load a DICOM file
DicomFile file;
if (file.load("/path/to/file.dcm")) {
    // Get the DICOM object
    DicomObject obj = file.getObject();
    
    // Display patient info
    std::cout << "Patient: " << obj.getPatientName() << std::endl;
    std::cout << "ID: " << obj.getPatientID() << std::endl;
}

// Create a new DICOM file
DicomObject newObj;
newObj.setString(DicomTag::PatientName, "SMITH^JANE");
newObj.setString(DicomTag::PatientID, "67890");

DicomFile newFile(newObj);
newFile.save("/path/to/output.dcm");
```

## Storage Operations

### Storing DICOM Files (Client)

```cpp
// Configure the storage client
auto config = StorageClient::Config::createDefault()
    .withRemoteHost("dicom-server.example.com")
    .withRemotePort(11112)
    .withRemoteAETitle("STORAGE_SCP");

// Create the client
StorageClient client(config);

// Send a single file
auto result = client.storeFile("/path/to/file.dcm");
if (result.isSuccess()) {
    std::cout << "File sent successfully" << std::endl;
} else {
    std::cerr << "Error: " << result.getErrorMessage() << std::endl;
}

// Send all files in a directory with progress updates
client.storeDirectory("/path/to/dicom/files", true, 
    [](int current, int total, const std::string& filename) {
        std::cout << "Sending file " << current + 1 << " of " << total 
                  << ": " << filename << std::endl;
    });
```

### Receiving DICOM Files (Server)

```cpp
// Configure the storage server
auto config = StorageServer::Config::createDefault()
    .withAETitle("STORAGE_SCP")
    .withPort(11112)
    .withStorageDirectory("/path/to/storage")
    .withFolderOrganization(true);

// Create the server
StorageServer server(config);

// Set callback for received files
server.setStorageCallback([](const StorageServer::StorageEvent& event) {
    std::cout << "Received file from " << event.callingAETitle << std::endl;
    std::cout << "Patient: " << event.patientName << std::endl;
    std::cout << "Stored at: " << event.filename << std::endl;
});

// Start the server
auto result = server.start();
if (result.isSuccess()) {
    std::cout << "Server started on port " << config.port << std::endl;
} else {
    std::cerr << "Error: " << result.getErrorMessage() << std::endl;
}

// Keep the server running
while (server.isRunning()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

## Error Handling

### Using Result Objects

```cpp
// Function that returns a result
DicomVoidResult storeImage(const std::string& filename) {
    DicomFile file;
    if (!file.load(filename)) {
        return makeErrorResult(DicomErrorCode::FileReadError, 
                              "Failed to load DICOM file");
    }
    
    StorageClient client;
    return client.store(file);
}

// Using the result
auto result = storeImage("/path/to/file.dcm");
if (result.isSuccess()) {
    std::cout << "Success!" << std::endl;
} else {
    std::cerr << "Error: " << result.getErrorMessage() << std::endl;
    std::cerr << "Details: " << result.getErrorDetails() << std::endl;
}
```

### Using Exceptions

```cpp
try {
    DicomFile file;
    if (!file.load("/path/to/file.dcm")) {
        throw DicomException(DicomErrorCode::FileReadError, 
                           "Failed to load DICOM file");
    }
    
    // Process file...
} catch (const DicomException& ex) {
    std::cerr << "DICOM Error: " << ex.getMessage() << std::endl;
    std::cerr << "Error Code: " << static_cast<int>(ex.getErrorCode()) << std::endl;
    if (ex.getDetails()) {
        std::cerr << "Details: " << *ex.getDetails() << std::endl;
    }
} catch (const std::exception& ex) {
    std::cerr << "General Error: " << ex.what() << std::endl;
}
```

## Conclusion

The simplified DICOM API provides an intuitive interface for working with DICOM objects and services, hiding the complexity of DCMTK. The API is designed to be easy to use for developers who are not familiar with DICOM, while still providing access to the full power of the underlying library when needed.

For more advanced use cases, you can still access the underlying DCMTK objects through methods like `getDataset()` on `DicomObject` or `getFileFormat()` on `DicomFile`.

## Complete Example: Storage Server

```cpp
#include <iostream>
#include <string>
#include <filesystem>
#include "common/dicom/storage_server.h"

using namespace pacs::common::dicom;

int main() {
    // Create storage directory
    std::string storageDir = "./dicom_storage";
    std::filesystem::create_directories(storageDir);
    
    // Configure server
    auto config = StorageServer::Config::createDefault()
        .withAETitle("MY_STORAGE_SCP")
        .withPort(11112)
        .withStorageDirectory(storageDir)
        .withFolderOrganization(true);
    
    // Create server
    StorageServer server(config);
    
    // Set callback
    server.setStorageCallback([](const StorageServer::StorageEvent& event) {
        std::cout << "Received DICOM object:" << std::endl;
        std::cout << "  Patient: " << event.patientName << std::endl;
        std::cout << "  Study UID: " << event.studyInstanceUID << std::endl;
        std::cout << "  Stored at: " << event.filename << std::endl;
    });
    
    // Start server
    auto result = server.start();
    if (result.isError()) {
        std::cerr << "Failed to start server: " << result.getErrorMessage() << std::endl;
        return 1;
    }
    
    std::cout << "Server started on port " << config.port << std::endl;
    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();
    
    // Stop server
    server.stop();
    std::cout << "Server stopped" << std::endl;
    
    return 0;
}
```