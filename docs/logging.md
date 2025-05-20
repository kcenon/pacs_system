# PACS System Logging

This document explains the logging system used in the PACS application.

## Overview

The PACS logging system is built on top of the thread_system logger, providing a simplified interface and integration with the configuration system. It includes features such as:

- Multiple log levels (Exception, Error, Info, Debug, Trace)
- Multiple output targets (console, file, callback)
- Automatic log file rotation
- RAII-based function entry/exit logging
- Configuration integration

## Log Levels

The PACS system defines the following log levels, in order of severity:

1. **Exception**: Used for logging caught exceptions
2. **Error**: Used for error conditions that don't trigger exceptions
3. **Info**: Used for important operational information
4. **Debug**: Used for detailed debug information
5. **Trace**: Used for very detailed execution tracing

Each output target (console, file) can be configured with a different minimum log level.

## Configuration

The logging system is integrated with the PACS configuration system. The following configuration options are available:

- `log.level.console`: Minimum log level for console output (default: "INFO")
- `log.level.file`: Minimum log level for file output (default: "DEBUG")
- `log.max.files`: Maximum number of log files to keep (default: 10)
- `log.max.lines`: Maximum number of lines per log file (default: 10000)

## Using the Logger

### Basic Logging

```cpp
#include "common/logger/logger.h"

using namespace pacs::common::logger;

// Log an exception
try {
    throw std::runtime_error("Something went wrong");
}
catch (const std::exception& ex) {
    logException("Caught exception: {}", ex.what());
}

// Log an error
logError("Failed to connect to server: {}", errorCode);

// Log information
logInfo("Processing file: {}", filename);

// Log debug information
logDebug("Variable value: {}", value);

// Log trace information
logTrace("Entering function with param: {}", param);
```

### Automatic Function Entry/Exit Logging

You can use the `PACS_FUNCTION_LOG` macro to automatically log function entry and exit:

```cpp
void someFunction() {
    PACS_FUNCTION_LOG; // Will log entry and exit with timing
    
    // Function code...
}
```

### Initializing the Logger

The logger should be initialized through the LoggingService:

```cpp
#include "common/logger/logging_service.h"

auto& loggingService = pacs::common::logger::LoggingService::getInstance();
auto result = loggingService.initialize("APPLICATION_NAME");

if (result.has_value()) {
    std::cerr << "Failed to initialize logging: " << *result << std::endl;
    return 1;
}
```

### Shutting Down the Logger

Always shut down the logger at application exit:

```cpp
loggingService.shutdown();
```

## Log Output Format

Log entries include:

1. Timestamp (ISO 8601 format)
2. Log level
3. Message

Example:

```
2025-05-20 12:34:56.789 [INFO] PACS Server starting up
2025-05-20 12:34:56.790 [INFO] Initializing thread manager with 4 threads and 2 priority levels
2025-05-20 12:34:56.791 [INFO] Initializing database at ./data/db/pacs.db
2025-05-20 12:34:56.795 [SEQUENCE] > Entering function: connectToServer
2025-05-20 12:34:57.123 [SEQUENCE] < Exiting function: connectToServer (duration: 328ms)
```

## Best Practices

1. **Choose the Appropriate Log Level**: Use the correct log level for each message to ensure proper filtering.
2. **Include Contextual Information**: Include enough context in log messages to understand what happened.
3. **Use Structured Logging**: Use the formatting capabilities to create structured logs.
4. **Log Exceptions**: Always log exceptions, including stack traces if available.
5. **Use the Function Logger**: Use the PACS_FUNCTION_LOG macro for important functions to track execution flow.
6. **Review Log Files Regularly**: Set up log rotation and review logs for errors and issues.

## Implementation Details

The logging system is implemented in the following files:

- `common/logger/logger.h`: Main logger interface
- `common/logger/logger.cpp`: Implementation of the logger interface
- `common/logger/logging_service.h`: Service for managing the logger
- `common/logger/logging_service.cpp`: Implementation of the logging service

The logging system uses the thread_system logger internally, which provides:

- Thread-safe logging
- Log message queuing
- Multiple output targets
- Log file rotation