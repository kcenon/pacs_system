# Simplified DICOM Storage Example

This sample demonstrates how to use the simplified DICOM abstraction layer to create a Storage SCP and SCU without needing to understand the details of DCMTK.

## Overview

The simplified DICOM API provides a high-level, intuitive interface for DICOM operations without requiring in-depth knowledge of the DICOM standard or the DCMTK library. This sample includes:

1. A simple Storage Server (SCP) that receives DICOM files
2. A simple Storage Client (SCU) that sends DICOM files

## Storage Server

The Storage Server (`simple_storage_server`) demonstrates:

- Creating a Storage SCP with minimal configuration
- Using builder pattern for readable configuration
- Setting up a callback for handling received files
- Starting and running the server

## Storage Client

The Storage Client (`simple_storage_client`) demonstrates:

- Creating a Storage SCU with minimal configuration
- Sending individual DICOM files
- Sending all DICOM files in a directory
- Providing progress updates during file transfer
- Handling command-line arguments

## Building

```bash
# In the build directory
cmake --build . --target simple_storage_server
cmake --build . --target simple_storage_client
```

## Running the Sample

### Starting the Server

```bash
# Start the Storage SCP
./bin/simple_storage_server
```

The server will start and listen for incoming DICOM Storage requests on port 11112.

### Sending Files with the Client

```bash
# Send a single DICOM file
./bin/simple_storage_client /path/to/dicom/file.dcm

# Send all files in a directory
./bin/simple_storage_client /path/to/dicom/directory

# Send to a remote server
./bin/simple_storage_client --host remote-server.example.com /path/to/dicom/files

# Send with custom port and AE title
./bin/simple_storage_client --port 11113 --aetitle PACS_SERVER /path/to/dicom/files

# Send with recursive directory traversal
./bin/simple_storage_client --recursive /path/to/dicom/directory
```

## Code Comparison

The simplified API requires significantly less code compared to using DCMTK directly:

**Traditional DCMTK (over 100 lines):**
```cpp
// Complex setup code omitted
// Association parameters
T_ASC_Parameters *params;
ASC_createAssociationParameters(&params, ASC_MAXIMUMPDUSIZE);
ASC_setAPTitles(params, "SIMPLE_CLIENT", "SIMPLE_STORAGE", NULL);

// Host and port setup
DIC_NODENAME remoteName;
strcpy(remoteName, "localhost");
T_ASC_NetworkRole role = NET_REQUESTOR;
ASC_setTransportLayerType(params, false);
// ... many more initialization steps ...

// Configure presentation contexts
std::vector<OFString> sopClasses;
// ... configuration for many SOP classes ...
for (const auto& sopClass : sopClasses) {
    ASC_addPresentationContext(params, presentationContextId, 
                             sopClass.c_str(), transferSyntaxes, numTransferSyntaxes);
    presentationContextId += 2;
}

// Establish association
T_ASC_Association *assoc;
OFCondition cond = ASC_requestAssociation(net, params, &assoc);
if (cond.bad()) {
    // ... error handling ...
}
// ... many more steps for actual C-STORE operation ...
```

**Simplified API (just a few lines):**
```cpp
// Create and configure storage client
auto config = StorageClient::Config::createDefault()
    .withRemoteHost("localhost")
    .withRemotePort(11112)
    .withRemoteAETitle("SIMPLE_STORAGE");

StorageClient client(config);

// Send a DICOM file
auto result = client.storeFile("/path/to/file.dcm");
if (result.isSuccess()) {
    std::cout << "File sent successfully" << std::endl;
} else {
    std::cerr << "Error: " << result.getErrorMessage() << std::endl;
}
```

## Benefits of the Simplified API

1. **Reduced Learning Curve**: Developers don't need to understand DICOM protocols or DCMTK's complex API
2. **Higher Productivity**: Common operations can be done with fewer lines of code
3. **Better Error Handling**: Clear error messages with typed error codes
4. **Improved Readability**: Clean, intuitive API with builder pattern for configuration
5. **Safer Memory Management**: Smart pointers and RAII throughout the API