# PACS System Configuration

This document explains how to configure the PACS system using the configuration system.

## Configuration Sources

The PACS system uses a hierarchical configuration system with the following sources, in order of precedence:

1. Command-line arguments
2. Environment variables 
3. Configuration file
4. Default values

## Configuration File

The configuration file is a JSON file with the following structure:

```json
{
    "service": {
        "aeTitle": "PACS",
        "localPort": 11112,
        "maxAssociations": 10,
        "associationTimeout": 30,
        "dimseTimeout": 30,
        "acseTimeout": 30,
        "connectionTimeout": 10,
        "dataDirectory": "./data",
        "logDirectory": "./logs",
        "databaseDirectory": "./data/db",
        "useTLS": false,
        "serviceSpecificConfig": {
            "STORAGE_MAX_PDU_SIZE": "16384",
            "WORKLIST_FILE_PATH": "./data/worklist/modality.wl",
            "QUERY_RETRIEVE_CACHE_SIZE": "100"
        }
    },
    "config": {
        "thread.pool.size": "4",
        "thread.priority.levels": "3",
        "log.level": "INFO",
        "log.console": "true",
        "log.file": "true",
        "storage.compress": "false"
    }
}
```

## Running with a Configuration File

To run the PACS server with a configuration file, use the following command:

```bash
./pacs_server /path/to/config.json
```

If no configuration file is specified, the system looks for a file named `pacs_default_config.json` in the current directory.

## Environment Variables

The following environment variables can be used to configure the PACS system:

- `PACS_AE_TITLE`: The AE Title of the PACS server
- `PACS_LOCAL_PORT`: The port on which the PACS server listens
- `PACS_MAX_ASSOCIATIONS`: The maximum number of associations
- `PACS_ASSOCIATION_TIMEOUT`: The association timeout in seconds
- `PACS_DIMSE_TIMEOUT`: The DIMSE timeout in seconds
- `PACS_ACSE_TIMEOUT`: The ACSE timeout in seconds
- `PACS_CONNECTION_TIMEOUT`: The connection timeout in seconds
- `PACS_DATA_DIRECTORY`: The directory for DICOM data
- `PACS_LOG_DIRECTORY`: The directory for logs
- `PACS_DATABASE_DIRECTORY`: The directory for the database
- `PACS_USE_TLS`: Whether to use TLS (1/0, true/false)
- `PACS_CONFIG_FILE_PATH`: Path to the configuration file

## Directory Structure

The PACS system uses the following directory structure:

- `{dataDirectory}`: Root directory for all data
  - `/storage`: DICOM storage files
  - `/worklist`: Worklist files
  - `/db`: Database files
- `{logDirectory}`: Directory for log files

## Default Values

If no configuration is provided, the system uses the following default values:

- AE Title: "PACS"
- Local Port: 11112
- Max Associations: 10
- Association Timeout: 30 seconds
- DIMSE Timeout: 30 seconds
- ACSE Timeout: 30 seconds
- Connection Timeout: 10 seconds
- Data Directory: "./data"
- Log Directory: "./logs"
- Database Directory: "./data/db"
- Use TLS: false
- Thread Pool Size: 4
- Thread Priority Levels: 2
- Log Level: "INFO"
- Log to Console: true
- Log to File: true

## Programmatic Configuration

The configuration system can also be used programmatically:

```cpp
#include "common/config/config_manager.h"

// Get the configuration manager
auto& configManager = pacs::common::config::ConfigManager::getInstance();

// Initialize with a configuration file
configManager.initialize("/path/to/config.json");

// Get the service configuration
const auto& config = configManager.getServiceConfig();

// Get a configuration value
std::string logLevel = configManager.getValue("log.level", "INFO");

// Set a configuration value
configManager.setValue("storage.compress", "true");

// Save the configuration to a file
configManager.saveToFile("/path/to/new_config.json");
```