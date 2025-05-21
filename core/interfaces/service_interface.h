#pragma once

#include "result.h"
#include <string>
#include <memory>

namespace pacs {
namespace core {
namespace interfaces {

/**
 * @class ServiceInterface
 * @brief Base interface for all PACS service components
 * 
 * This interface defines the basic operations that all PACS services should support,
 * including initialization, startup, shutdown, and health checking. It provides
 * a consistent interface for managing service lifecycle across the PACS system.
 */
class ServiceInterface {
public:
    /**
     * @brief Virtual destructor to ensure proper cleanup of derived classes
     */
    virtual ~ServiceInterface() = default;
    
    /**
     * @brief Initialize the service with configuration
     * @param configPath Path to the configuration file or configuration data
     * @return Result indicating success or failure with error message
     */
    virtual Result<void> initialize(const std::string& configPath) = 0;
    
    /**
     * @brief Start the service
     * @return Result indicating success or failure with error message
     */
    virtual Result<void> start() = 0;
    
    /**
     * @brief Stop the service
     * @param graceful If true, complete pending operations before stopping
     * @return Result indicating success or failure with error message
     */
    virtual Result<void> stop(bool graceful = true) = 0;
    
    /**
     * @brief Check if the service is running
     * @return True if the service is running, false otherwise
     */
    virtual bool isRunning() const = 0;
    
    /**
     * @brief Check the health of the service
     * @return Result containing health status information or error
     */
    virtual Result<std::string> checkHealth() = 0;
    
    /**
     * @brief Get the name of the service
     * @return The service name
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Get the current version of the service
     * @return The service version
     */
    virtual std::string getVersion() const = 0;
};

/**
 * @class DicomServiceInterface
 * @brief Specialized interface for DICOM services
 * 
 * This interface extends ServiceInterface with DICOM-specific functionality.
 * It adds methods to manage DICOM connectivity, association handling, and
 * DICOM-specific service capabilities.
 */
class DicomServiceInterface : public ServiceInterface {
public:
    /**
     * @brief Get the AE Title (Application Entity Title) of the service
     * @return The AE Title
     */
    virtual std::string getAETitle() const = 0;
    
    /**
     * @brief Set the AE Title for the service
     * @param aeTitle The new AE Title
     * @return Result indicating success or failure
     */
    virtual Result<void> setAETitle(const std::string& aeTitle) = 0;
    
    /**
     * @brief Get the port number the service is listening on
     * @return The port number, or 0 if not applicable
     */
    virtual int getPort() const = 0;
    
    /**
     * @brief Set the port for the service to listen on
     * @param port The port number
     * @return Result indicating success or failure
     */
    virtual Result<void> setPort(int port) = 0;
    
    /**
     * @brief Get the list of supported SOP classes
     * @return List of supported SOP class UIDs
     */
    virtual std::vector<std::string> getSupportedSOPClasses() const = 0;
    
    /**
     * @brief Check if a specific SOP class is supported
     * @param sopClassUID The SOP Class UID to check
     * @return True if the SOP class is supported
     */
    virtual bool isSOPClassSupported(const std::string& sopClassUID) const = 0;
};

/**
 * @class DatabaseServiceInterface
 * @brief Interface for database service components
 * 
 * This interface defines operations for database services within the PACS system,
 * including connection management, transaction handling, and basic CRUD operations.
 */
class DatabaseServiceInterface : public ServiceInterface {
public:
    /**
     * @brief Connect to the database
     * @return Result indicating success or failure
     */
    virtual Result<void> connect() = 0;
    
    /**
     * @brief Disconnect from the database
     * @return Result indicating success or failure
     */
    virtual Result<void> disconnect() = 0;
    
    /**
     * @brief Check if connected to the database
     * @return True if connected
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Begin a transaction
     * @return Result containing a transaction ID or error
     */
    virtual Result<std::string> beginTransaction() = 0;
    
    /**
     * @brief Commit a transaction
     * @param transactionId The ID of the transaction to commit
     * @return Result indicating success or failure
     */
    virtual Result<void> commitTransaction(const std::string& transactionId) = 0;
    
    /**
     * @brief Rollback a transaction
     * @param transactionId The ID of the transaction to rollback
     * @return Result indicating success or failure
     */
    virtual Result<void> rollbackTransaction(const std::string& transactionId) = 0;
    
    /**
     * @brief Execute a query
     * @param query The query to execute
     * @param params The parameters for the query
     * @return Result containing query results or error
     */
    virtual Result<std::string> executeQuery(
        const std::string& query, 
        const std::vector<std::string>& params = {}) = 0;
};

/**
 * @class StorageServiceInterface
 * @brief Interface for storage service components
 * 
 * This interface defines operations for file storage services within the PACS system,
 * including file management, DICOM object storage, and retrieval.
 */
class StorageServiceInterface : public ServiceInterface {
public:
    /**
     * @brief Store a file
     * @param filePath Path to the file to store
     * @param metadata Metadata about the file
     * @return Result containing the stored file ID or error
     */
    virtual Result<std::string> storeFile(
        const std::string& filePath, 
        const std::map<std::string, std::string>& metadata) = 0;
    
    /**
     * @brief Retrieve a file
     * @param fileId ID of the file to retrieve
     * @param destinationPath Path where the file should be saved
     * @return Result indicating success or failure
     */
    virtual Result<void> retrieveFile(
        const std::string& fileId, 
        const std::string& destinationPath) = 0;
    
    /**
     * @brief Delete a file
     * @param fileId ID of the file to delete
     * @return Result indicating success or failure
     */
    virtual Result<void> deleteFile(const std::string& fileId) = 0;
    
    /**
     * @brief Check if a file exists
     * @param fileId ID of the file to check
     * @return True if the file exists
     */
    virtual bool fileExists(const std::string& fileId) const = 0;
    
    /**
     * @brief Get metadata for a file
     * @param fileId ID of the file
     * @return Result containing the file metadata or error
     */
    virtual Result<std::map<std::string, std::string>> getFileMetadata(
        const std::string& fileId) = 0;
};

} // namespace interfaces
} // namespace core
} // namespace pacs