# PACS System SDK Usage Guide

## Overview

The PACS System SDK provides a comprehensive framework for building hospital-grade PACS applications. This guide demonstrates how to integrate the SDK into your projects.

## Installation

### Using CMake Package

#### From Build Directory

```bash
# Build and install the PACS SDK
cd pacs_system
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
make
make install
```

#### From System Installation

```cmake
# In your project's CMakeLists.txt
find_package(PACSSystem REQUIRED)

add_executable(my_pacs_app main.cpp)
target_link_libraries(my_pacs_app PRIVATE
    pacs::core
    pacs::storage
    pacs::query_retrieve
)
```

### Using vcpkg (Future)

```json
{
  "dependencies": [
    "pacs-system"
  ]
}
```

## Basic Usage

### 1. Initialize PACS Server

```cpp
#include "core/pacs_server.h"
#include "common/config/config_manager.h"
#include "common/logger/log_module.h"

int main() {
    // Initialize logging
    logger::initialize("my_pacs_app", Logger::LogLevel::Info);
    
    // Load configuration
    pacs::ConfigManager configManager;
    configManager.loadConfig("pacs_config.json");
    
    // Get server configuration
    auto serverConfig = configManager.getServerConfig().getValue();
    
    // Create and initialize server
    pacs::PACSServer server(serverConfig);
    
    // Add required modules
    server.addModule("storage", std::make_shared<pacs::StorageSCPModule>());
    server.addModule("worklist", std::make_shared<pacs::WorklistSCPModule>());
    server.addModule("query_retrieve", std::make_shared<pacs::QueryRetrieveSCPModule>());
    
    // Start server
    auto result = server.start();
    if (!result.isOk()) {
        logger::logError("Failed to start server: {}", result.getError());
        return 1;
    }
    
    // Run server
    server.run();
    
    return 0;
}
```

### 2. Configure Security

```cpp
#include "common/security/security_manager.h"
#include "common/security/crypto_manager.h"

// Initialize security manager
pacs::SecurityManager securityManager;

// Create user with secure password
auto passwordResult = securityManager.generateSecurePassword();
auto createResult = securityManager.createUser(
    "doctor_smith", 
    passwordResult.getValue(), 
    pacs::UserRole::Doctor
);

// Enable encryption for sensitive data
pacs::CryptoManager cryptoManager;
cryptoManager.generateKey();  // Generate new encryption key

// Encrypt patient data
std::vector<uint8_t> patientData = loadPatientData();
auto encryptResult = cryptoManager.encrypt(patientData);
```

### 3. Database Configuration

#### SQLite (Default)

```json
{
  "database": {
    "type": "sqlite",
    "path": "/var/lib/pacs/pacs.db"
  }
}
```

#### PostgreSQL (Enterprise)

```json
{
  "database": {
    "type": "postgresql",
    "host": "db.hospital.local",
    "port": 5432,
    "database": "pacs_production",
    "username": "pacs_user",
    "password": "${PACS_DB_PASSWORD}",
    "minPoolSize": 5,
    "maxPoolSize": 20,
    "sslMode": "require"
  }
}
```

### 4. Connection Pooling

```cpp
#include "common/network/dicom_connection_pool.h"

// Configure connection parameters
DicomConnection::Parameters params;
params.remoteHost = "remote-pacs.hospital.local";
params.remotePort = 11112;
params.remoteAeTitle = "MAIN_PACS";
params.localAeTitle = "DEPT_PACS";

// Configure pool
ConnectionPool<DicomConnection>::Config poolConfig;
poolConfig.minSize = 2;
poolConfig.maxSize = 10;

// Get or create pool
auto& poolManager = DicomConnectionPoolManager::getInstance();
auto pool = poolManager.getPool("MAIN_PACS", params, poolConfig);

// Execute operation with automatic connection management
auto result = pool->executeWithConnection("c-find",
    [](DicomConnection* conn) -> core::Result<DcmDataset*> {
        // Perform DICOM query
        return performQuery(conn);
    });
```

### 5. Audit Logging

```cpp
#include "common/audit/audit_logger.h"

// Initialize audit logger
pacs::AuditLogger auditLogger;
auditLogger.initialize(pacs::AuditLogger::Backend::Database);

// Log user actions
auditLogger.logUserLogin("doctor_smith", "192.168.1.100", true);
auditLogger.logPatientDataAccess(
    "doctor_smith",
    "PAT123456",
    "VIEW_IMAGES",
    "SUCCESS"
);

// Log system events
auditLogger.logSystemEvent(
    pacs::AuditEventType::CONFIGURATION_CHANGE,
    "Modified DICOM port",
    "admin"
);
```

## Advanced Features

### Resilient Operations

```cpp
#include "common/network/retry_policy.h"

// Configure retry policy
RetryConfig retryConfig;
retryConfig.maxAttempts = 3;
retryConfig.strategy = RetryStrategy::ExponentialJitter;

// Configure circuit breaker
CircuitBreaker::Config cbConfig;
cbConfig.failureThreshold = 5;
cbConfig.openDuration = std::chrono::seconds(60);

// Create resilient executor
ResilientExecutor executor("image_transfer", retryConfig, cbConfig);

// Execute with automatic retry and circuit breaking
auto result = executor.execute([&]() -> core::Result<void> {
    return transferImageToRemotePACS(sopInstanceUid);
});
```

### Custom Modules

```cpp
class CustomModule : public pacs::ModuleInterface {
public:
    core::Result<void> init() override {
        // Initialize module
        return core::Result<void>::ok();
    }
    
    core::Result<void> start() override {
        // Start module services
        return core::Result<void>::ok();
    }
    
    void stop() override {
        // Stop module services
    }
    
    std::string getName() const override {
        return "custom_module";
    }
};

// Register module
server.addModule("custom", std::make_shared<CustomModule>());
```

### Performance Monitoring

```cpp
#include "common/performance.h"

// Monitor operation performance
{
    ScopedTimer timer("dicom_query");
    auto result = performDicomQuery(dataset);
}

// Access metrics
auto metrics = PerformanceMonitor::getInstance().getMetrics();
for (const auto& [operation, stats] : metrics) {
    logger::logInfo("{}: avg={:.2f}ms, max={:.2f}ms, count={}",
                    operation, stats.avgDuration, stats.maxDuration, stats.count);
}
```

## Configuration Reference

### Complete Configuration Example

```json
{
  "server": {
    "aeTitle": "HOSPITAL_PACS",
    "port": 11112,
    "maxConnections": 100,
    "timeout": 30000,
    "threadPoolSize": 20
  },
  "security": {
    "requireAuthentication": true,
    "passwordPolicy": {
      "minLength": 12,
      "requireUppercase": true,
      "requireNumbers": true,
      "requireSpecialChars": true
    },
    "sessionTimeout": 3600,
    "maxFailedAttempts": 3
  },
  "database": {
    "type": "postgresql",
    "host": "localhost",
    "port": 5432,
    "database": "pacs",
    "username": "pacs_user",
    "minPoolSize": 5,
    "maxPoolSize": 20
  },
  "storage": {
    "rootPath": "/var/lib/pacs/dicom",
    "compressionLevel": 6,
    "encryption": {
      "enabled": true,
      "algorithm": "AES-256-GCM",
      "keyRotationDays": 90
    }
  },
  "audit": {
    "enabled": true,
    "backend": "database",
    "retentionDays": 2555
  },
  "network": {
    "connection_pools": {
      "default": {
        "min_size": 2,
        "max_size": 10,
        "max_idle_time": 300
      }
    },
    "retry": {
      "max_attempts": 3,
      "initial_delay_ms": 1000,
      "strategy": "exponential_jitter"
    }
  }
}
```

## Building Applications

### Example CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyPACSApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find PACS SDK
find_package(PACSSystem REQUIRED)

# Main application
add_executable(my_pacs_app
    src/main.cpp
    src/custom_module.cpp
    src/integration.cpp
)

target_link_libraries(my_pacs_app PRIVATE
    pacs::core
    pacs::storage
    pacs::query_retrieve
    pacs::worklist
)

# Tests
enable_testing()
add_executable(my_pacs_tests
    tests/test_main.cpp
    tests/test_integration.cpp
)

target_link_libraries(my_pacs_tests PRIVATE
    pacs::core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME MyPACSTests COMMAND my_pacs_tests)
```

## Deployment

### Docker

```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    libpq5 \
    libssl3 \
    libxml2

# Copy PACS application
COPY build/bin/my_pacs_app /usr/local/bin/
COPY config/pacs_config.json /etc/pacs/

# Create data directories
RUN mkdir -p /var/lib/pacs/dicom /var/log/pacs

# Run as non-root user
USER pacs

EXPOSE 11112

CMD ["/usr/local/bin/my_pacs_app", "/etc/pacs/pacs_config.json"]
```

### System Service

```ini
[Unit]
Description=Hospital PACS Server
After=network.target postgresql.service

[Service]
Type=simple
User=pacs
Group=pacs
ExecStart=/usr/local/bin/my_pacs_app /etc/pacs/pacs_config.json
Restart=always
RestartSec=5

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/pacs /var/log/pacs

[Install]
WantedBy=multi-user.target
```

## Troubleshooting

### Common Issues

1. **Connection Pool Exhaustion**
   ```cpp
   // Increase pool size
   poolConfig.maxSize = 20;
   
   // Monitor pool usage
   auto stats = pool->getPoolStats();
   if (stats.availableSize == 0) {
       logger::logWarning("Connection pool exhausted");
   }
   ```

2. **Database Connection Failures**
   ```cpp
   // Enable connection retry
   dbConfig.enableRetry = true;
   dbConfig.retryAttempts = 3;
   dbConfig.retryDelay = std::chrono::seconds(1);
   ```

3. **Memory Usage**
   ```cpp
   // Enable memory limits
   server.setMemoryLimit(4 * 1024 * 1024 * 1024); // 4GB
   
   // Monitor memory usage
   auto memStats = server.getMemoryStats();
   logger::logInfo("Memory usage: {}MB", memStats.usedMB);
   ```

## Support

- GitHub Issues: https://github.com/yourorg/pacs-system/issues
- Documentation: https://docs.yourorg.com/pacs-system
- Enterprise Support: support@yourorg.com