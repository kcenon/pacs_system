# PACS System SDK Examples

This directory contains example applications demonstrating how to use the PACS System SDK.

## Examples Overview

### 1. Simple Client (`simple_client.cpp`)
Basic example showing how to:
- Initialize PACS server
- Load configuration
- Add modules
- Handle basic operations

### 2. Connection Pool Example (`connection_pool_example.cpp`)
Demonstrates advanced networking features:
- Connection pooling configuration
- Concurrent operations
- Retry policies
- Circuit breaker pattern
- Resilient operations

### 3. Query/Retrieve Example (`query_retrieve_example.cpp`)
Shows DICOM Query/Retrieve operations:
- Patient queries
- Study queries
- Series queries
- Date range queries
- Modality-specific queries

### 4. Worklist Example (`worklist_example.cpp`)
Demonstrates Modality Worklist operations:
- Query scheduled procedures
- Filter by modality, date, AE title
- Handle worklist responses

### 5. Storage SCP Example (`storage_scp_example.cpp`)
Shows how to implement a Storage Service Class Provider:
- Receive DICOM images
- Store to filesystem
- Database indexing
- Handle different SOP classes

## Building the Examples

### Prerequisites
- PACS System SDK installed
- CMake 3.16 or higher
- C++20 compatible compiler
- vcpkg for dependency management

### Build Steps

```bash
# From the examples directory
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
make
```

### Running Examples

```bash
# Simple client
./simple_pacs_client [config_file]

# Connection pool example
./connection_pool_example

# Query/Retrieve example
./query_retrieve_example

# With custom configuration
./simple_pacs_client /path/to/custom_config.json
```

## Configuration

Each example can use a configuration file. See `pacs_example_config.json` for a complete example:

```json
{
  "server": {
    "aeTitle": "EXAMPLE_PACS",
    "port": 11112,
    "maxConnections": 10
  },
  "storage": {
    "rootPath": "./dicom_storage",
    "databasePath": "./pacs_example.db"
  }
}
```

## Common Use Cases

### 1. Setting up a Department PACS

```cpp
// Configure for radiology department
pacs::ServerConfig config;
config.aeTitle = "RAD_PACS";
config.port = 11112;
config.maxConnections = 50;

// Add all necessary modules
server.addModule("storage", storageModule);
server.addModule("worklist", worklistModule);
server.addModule("query_retrieve", qrModule);
server.addModule("mpps", mppsModule);
```

### 2. Integrating with Hospital Information System

```cpp
// Configure database connection
pacs::PostgreSQLConfig dbConfig;
dbConfig.host = "his.hospital.local";
dbConfig.database = "radiology";
dbConfig.username = "pacs_service";

// Enable audit logging for compliance
auditLogger.initialize(pacs::AuditLogger::Backend::Database);
```

### 3. Setting up High Availability

```cpp
// Configure connection pooling
ConnectionPool<DicomConnection>::Config poolConfig;
poolConfig.minSize = 5;
poolConfig.maxSize = 20;
poolConfig.validationInterval = 30;

// Enable resilient operations
ResilientExecutor executor("critical_ops", retryConfig, cbConfig);
```

## Troubleshooting

### Connection Issues
- Check firewall settings for DICOM port (usually 11112)
- Verify AE titles match between systems
- Enable debug logging: `logger::initialize("app", Logger::LogLevel::Debug)`

### Performance Issues
- Monitor connection pool usage
- Check database query performance
- Enable performance monitoring in configuration

### Security
- Always use secure passwords (not hardcoded)
- Enable encryption for sensitive data
- Configure proper user roles and permissions

## Further Reading

- [SDK Usage Guide](../docs/sdk_usage_guide.md)
- [API Reference](../docs/api_reference.md)
- [Security Best Practices](../docs/security_guide.md)
- [Connection Pooling Guide](../docs/connection_pooling.md)