#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>

#include "dicom_object.h"
#include "dicom_file.h"
#include "dicom_error.h"

namespace pacs {
namespace common {
namespace dicom {

/**
 * @brief Server for DICOM storage operations (C-STORE SCP)
 * 
 * This class provides a simplified interface for receiving DICOM objects
 * from remote DICOM clients (Storage SCUs). It handles all the complexity
 * of DICOM associations, presentation contexts, and transfer syntax negotiation.
 */
class StorageServer {
public:
    /**
     * @brief Configuration for storage server
     */
    struct Config {
        std::string aeTitle = "STORAGE_SCP";   ///< AE title
        uint16_t port = 11112;                 ///< Listen port
        std::string storageDirectory = "./";   ///< Directory to store received files
        bool organizeFolders = true;           ///< Organize files into patient/study/series folders
        bool allowAnyAETitle = false;          ///< Accept associations from any AE title
        std::vector<std::string> allowedAEPeers; ///< List of allowed peer AE titles
        bool useTLS = false;                   ///< Use TLS for connections
        
        /**
         * @brief Create a configuration with default values
         * @return Default configuration
         */
        static Config createDefault() {
            return Config();
        }
        
        /**
         * @brief Builder-style method to set the AE title
         * @param title The AE title
         * @return Reference to this object for chaining
         */
        Config& withAETitle(const std::string& title) {
            aeTitle = title;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set the port
         * @param listenPort The port to listen on
         * @return Reference to this object for chaining
         */
        Config& withPort(uint16_t listenPort) {
            port = listenPort;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set the storage directory
         * @param directory Directory to store received files
         * @return Reference to this object for chaining
         */
        Config& withStorageDirectory(const std::string& directory) {
            storageDirectory = directory;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set folder organization
         * @param organize Whether to organize files into folders
         * @return Reference to this object for chaining
         */
        Config& withFolderOrganization(bool organize) {
            organizeFolders = organize;
            return *this;
        }
        
        /**
         * @brief Builder-style method to allow any AE title to connect
         * @param allow Whether to allow any AE title
         * @return Reference to this object for chaining
         */
        Config& withAllowAnyAETitle(bool allow) {
            allowAnyAETitle = allow;
            return *this;
        }
        
        /**
         * @brief Builder-style method to set allowed peer AE titles
         * @param peers List of allowed peer AE titles
         * @return Reference to this object for chaining
         */
        Config& withAllowedPeers(const std::vector<std::string>& peers) {
            allowedAEPeers = peers;
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
     * @brief Event data for storage callbacks
     */
    struct StorageEvent {
        std::string sopClassUID;       ///< SOP Class UID of the received object
        std::string sopInstanceUID;    ///< SOP Instance UID of the received object
        std::string patientID;         ///< Patient ID
        std::string patientName;       ///< Patient name
        std::string studyInstanceUID;  ///< Study Instance UID
        std::string seriesInstanceUID; ///< Series Instance UID
        std::string modality;          ///< Modality
        std::string filename;          ///< Path where the file was stored
        std::string callingAETitle;    ///< AE title of the calling SCU
        DicomObject object;            ///< The received DICOM object
    };
    
    /**
     * @brief Callback function type for storage events
     */
    using StorageCallback = std::function<void(const StorageEvent& event)>;
    
    /**
     * @brief Constructor
     * @param config Configuration for the storage server
     */
    explicit StorageServer(const Config& config = Config::createDefault());
    
    /**
     * @brief Destructor
     */
    ~StorageServer();
    
    /**
     * @brief Start the server
     * @return DicomVoidResult indicating success or failure
     */
    DicomVoidResult start();
    
    /**
     * @brief Stop the server
     */
    void stop();
    
    /**
     * @brief Check if the server is running
     * @return True if the server is running
     */
    bool isRunning() const;
    
    /**
     * @brief Set the storage callback
     * @param callback The callback function
     */
    void setStorageCallback(StorageCallback callback);
    
    /**
     * @brief Update the server configuration
     * @param config The new configuration
     * @note The server must be restarted for the changes to take effect
     */
    void setConfig(const Config& config);
    
    /**
     * @brief Get the current configuration
     * @return The current configuration
     */
    const Config& getConfig() const;
    
private:
    Config config_;
    StorageCallback callback_;
    std::atomic<bool> running_;
    std::thread serverThread_;
    
    // Implementation details hidden in cpp file
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dicom
} // namespace common
} // namespace pacs