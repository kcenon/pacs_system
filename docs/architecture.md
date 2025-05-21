# PACS System Architecture

## Overview

The PACS System is built with a modular architecture that separates concerns and enables flexible deployment configurations. This document outlines the architectural principles and components of the system.

## Architectural Principles

1. **Modularity**: Each DICOM service is implemented as a separate module
2. **Separation of Concerns**: Clear separation between interface and implementation
3. **Concurrency**: Thread-based design for handling multiple operations
4. **Scalability**: Components can be deployed individually or as an integrated system
5. **Extensibility**: Easy addition of new DICOM services or enhancements

## Component Architecture

### Core Components

- **Thread System**: Submodule providing thread pools for concurrent operations
- **Result Handling**: Standardized approach to error and success conditions
- **Service Interfaces**: Abstract definitions for all DICOM services
- **Common Utilities**: Shared functionality for all components

### DICOM Service Modules

Each DICOM service module is implemented with both SCP (Server) and SCU (Client) components:

1. **Storage Service**
   - Storage SCP: Receives and stores DICOM objects
   - Storage SCU: Sends DICOM objects to remote systems

2. **Worklist Service**
   - Worklist SCP: Provides worklist information to modalities
   - Worklist SCU: Queries worklist from remote systems

3. **Query/Retrieve Service**
   - Query/Retrieve SCP: Handles queries and retrieval requests
   - Query/Retrieve SCU: Performs queries and retrieval operations

4. **MPPS Service**
   - MPPS SCP: Receives procedure step information
   - MPPS SCU: Sends procedure step information to remote systems

### Applications

- **PACS Server**: Integrated server implementing all SCP components
- **Sample Applications**: Individual applications demonstrating each service

## Data Flow

1. **Storage Flow**:
   - Imaging modality → Storage SCU → Network → Storage SCP → File System

2. **Worklist Flow**:
   - HIS/RIS → Worklist SCP → Network → Worklist SCU → Modality

3. **Query/Retrieve Flow**:
   - Workstation → Q/R SCU → Network → Q/R SCP → Database → File System

4. **MPPS Flow**:
   - Modality → MPPS SCU → Network → MPPS SCP → Database/Workflow System

## Technical Architecture

- **Language**: C++20
- **Build System**: CMake
- **Dependency Management**: vcpkg
- **Threading Model**: Thread pool system with job queues
  - Using dedicated thread_system submodule
  - Support for priority-based thread pools
- **Network Communication**: DICOM protocol over TCP/IP
- **File Storage**: File system-based with configurable paths
- **Logging**: Thread-safe logging with multiple targets
- **Error Handling**: Result pattern for consistent error management

## Thread System Architecture

The thread_system submodule provides:

1. **Thread Pool**: Primary mechanism for concurrent task execution
2. **Priority Thread Pool**: Task execution with priority levels
3. **Job Queue**: Thread-safe queue for pending tasks
4. **Worker Threads**: Execution contexts for tasks
5. **Job Abstraction**: Common interface for all executable tasks

This system is used via direct integration rather than through intermediate adapters, ensuring optimal performance and minimal overhead.

## Service Interface Architecture

The core/interfaces directory defines abstract interfaces for all DICOM services:

1. **ServiceInterface**: Base interface for all services
2. **DicomServiceInterface**: DICOM-specific service interface
3. **Module-specific interfaces**: Specialized interfaces for each DICOM service

These interfaces ensure consistent behavior across implementations and facilitate testing and future enhancements.

## Deployment Architecture

The system supports multiple deployment configurations:

1. **Standalone Server**: All services running in a single process
2. **Distributed Services**: Each service running as a separate process
3. **Microservice Architecture**: Each service deployed independently
4. **Hybrid Deployment**: Mix of standalone and distributed components

## Security Architecture

Future security enhancements will include:

1. **Transport Layer Security**: TLS for all network communications
2. **Authentication**: User and system authentication
3. **Authorization**: Role-based access control for operations
4. **Audit Logging**: Comprehensive logging of security events
5. **Data Protection**: Encryption of sensitive data

## Conclusion

The PACS System architecture provides a solid foundation for a complete medical imaging solution. Its modular design allows for flexible deployment options and future enhancements while maintaining compliance with DICOM standards. The recent optimization to use the thread_system directly improves both code clarity and performance.