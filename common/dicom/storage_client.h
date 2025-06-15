#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "dicom_object.h"
#include "dicom_file.h"
#include "dicom_error.h"

namespace pacs {
namespace common {
namespace dicom {

/**
 * @brief Client for DICOM storage operations (C-STORE SCU)
 * 
 * This class provides a simplified interface for storing DICOM objects
 * on a remote DICOM server (Storage SCP). It handles all the complexity
 * of DICOM associations, presentation contexts, and transfer syntax negotiation.
 */
class StorageClient {
public:
    /**
     * @brief Configuration for storage client
     */
    struct Config {
        std::string localAETitle = "STORAGE_SCU";  ///< Local AE title
        std::string remoteAETitle = "STORAGE_SCP"; ///< Remote AE title
        std::string remoteHost = "localhost";      ///< Remote host name or IP
        uint16_t remotePort = 11112;               ///< Remote port
        uint16_t connectTimeout = 30;              ///< Connection timeout in seconds
        uint16_t dimseTimeout = 30;                ///< DIMSE timeout in seconds
        bool useTLS = false;                       ///< Use TLS for connection
        
        /**
         * @brief Create a configuration with default values
         * @return Default configuration
         */
        static Config createDefault() {
            return Config();
        }
        
        /**
         * @brief Builder-style method to set the local AE title
         * @param aeTitle The local AE title
         * @return Reference to this object for chaining
         */
        Config& withLocalAETitle(const std::string& aeTitle) {
            localAETitle = aeTitle;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set the remote AE title
         * @param aeTitle The remote AE title
         * @return Reference to this object for chaining
         */
        Config& withRemoteAETitle(const std::string& aeTitle) {
            remoteAETitle = aeTitle;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set the remote host
         * @param host The remote host name or IP
         * @return Reference to this object for chaining
         */
        Config& withRemoteHost(const std::string& host) {
            remoteHost = host;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set the remote port
         * @param port The remote port
         * @return Reference to this object for chaining
         */
        Config& withRemotePort(uint16_t port) {
            remotePort = port;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set connection timeout
         * @param timeout Timeout in seconds
         * @return Reference to this object for chaining
         */
        Config& withConnectTimeout(uint16_t timeout) {
            connectTimeout = timeout;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set DIMSE timeout
         * @param timeout Timeout in seconds
         * @return Reference to this object for chaining
         */
        Config& withDimseTimeout(uint16_t timeout) {
            dimseTimeout = timeout;
            return *this;
        }
        
        /**
         * @brief Builder-style method to enable/disable TLS
         * @param enable Whether to enable TLS
         * @return Reference to this object for chaining
         */
        Config& withTLS(bool enable) {
            useTLS = enable;
            return *this;
        }
    };
    
    /**
     * @brief Progress callback function type
     * @param currentIndex Current file index (0-based)
     * @param totalCount Total number of files
     * @param filename Current filename
     */
    using ProgressCallback = std::function<void(int currentIndex, int totalCount, const std::string& filename)>;
    
    /**
     * @brief Constructor
     * @param config Configuration for the storage client
     */
    explicit StorageClient(const Config& config = Config::createDefault());
    
    /**
     * @brief Destructor
     */
    ~StorageClient();
    
    /**
     * @brief Store a single DICOM object
     * @param object The DICOM object to store
     * @return DicomVoidResult indicating success or failure
     */
    DicomVoidResult store(const DicomObject& object);
    
    /**
     * @brief Store a single DICOM file
     * @param file The DICOM file to store
     * @return DicomVoidResult indicating success or failure
     */
    DicomVoidResult store(const DicomFile& file);
    
    /**
     * @brief Store a DICOM file from disk
     * @param filename Path to the DICOM file
     * @return DicomVoidResult indicating success or failure
     */
    DicomVoidResult storeFile(const std::string& filename);
    
    /**
     * @brief Store multiple DICOM files from disk
     * @param filenames Paths to the DICOM files
     * @param progressCallback Optional callback for progress updates
     * @return DicomVoidResult indicating success or failure
     */
    DicomVoidResult storeFiles(const std::vector<std::string>& filenames, 
                             ProgressCallback progressCallback = nullptr);
    
    /**
     * @brief Store all DICOM files in a directory
     * @param directory Path to the directory
     * @param recursive Whether to search subdirectories
     * @param progressCallback Optional callback for progress updates
     * @return DicomVoidResult indicating success or failure
     */
    DicomVoidResult storeDirectory(const std::string& directory, bool recursive = false,
                                 ProgressCallback progressCallback = nullptr);
    
    /**
     * @brief Update the client configuration
     * @param config The new configuration
     */
    void setConfig(const Config& config);
    
    /**
     * @brief Get the current configuration
     * @return The current configuration
     */
    const Config& getConfig() const;
    
private:
    Config config_;
    
    // Implementation details hidden in cpp file
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom
} // namespace common
} // namespace pacs