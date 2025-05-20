#pragma once

#include <string>
#include <map>
#include <optional>
#include <vector>

namespace pacs {
namespace common {

/**
 * @brief Configuration options for DICOM services
 * 
 * This structure holds configuration parameters for DICOM network services,
 * including network settings, AE (Application Entity) information, and
 * timeout values.
 */
struct ServiceConfig {
    /**
     * @brief Application Entity Title
     * 
     * This is the DICOM AE Title used to identify this application in
     * a DICOM network.
     */
    std::string aeTitle = "PACS";
    
    /**
     * @brief Local network port
     * 
     * The port on which the service will listen for incoming connections.
     * Default DICOM port is 11112.
     */
    int localPort = 11112;
    
    /**
     * @brief Maximum number of network associations
     * 
     * The maximum number of concurrent network associations the service
     * will accept. Default is 10.
     */
    int maxAssociations = 10;
    
    /**
     * @brief Association timeout in seconds
     * 
     * Timeout for DICOM association negotiation. Default is 30 seconds.
     */
    int associationTimeout = 30;
    
    /**
     * @brief DIMSE (DICOM Message Service Element) timeout in seconds
     * 
     * Timeout for DICOM message operations. Default is 30 seconds.
     */
    int dimseTimeout = 30;
    
    /**
     * @brief ACSE (Association Control Service Element) timeout in seconds
     * 
     * Timeout for DICOM association control operations. Default is 30 seconds.
     */
    int acseTimeout = 30;
    
    /**
     * @brief Connection timeout in seconds
     * 
     * Timeout for establishing a network connection. Default is 10 seconds.
     */
    int connectionTimeout = 10;
    
    /**
     * @brief Optional data directory path
     * 
     * Directory where service-specific data files will be stored.
     */
    std::optional<std::string> dataDirectory;
    
    /**
     * @brief Optional log directory path
     * 
     * Directory where log files will be stored.
     */
    std::optional<std::string> logDirectory;
    
    /**
     * @brief Optional database directory path
     * 
     * Directory where database files will be stored.
     */
    std::optional<std::string> databaseDirectory;
    
    /**
     * @brief Optional TLS settings
     * 
     * If present, enables TLS encryption for the service.
     */
    std::optional<bool> useTLS;
    
    /**
     * @brief Optional TLS certificate file path
     */
    std::optional<std::string> tlsCertificatePath;
    
    /**
     * @brief Optional TLS private key file path
     */
    std::optional<std::string> tlsPrivateKeyPath;
    
    /**
     * @brief Optional list of allowed remote AE titles
     * 
     * If present, only connections from these AE titles will be accepted.
     */
    std::optional<std::vector<std::string>> allowedRemoteAETitles;
    
    /**
     * @brief Optional key-value map for service-specific configuration
     * 
     * Additional configuration parameters specific to each service.
     */
    std::map<std::string, std::string> serviceSpecificConfig;
    
    /**
     * @brief Optional configuration file path
     * 
     * Path to a JSON configuration file that will be loaded.
     */
    std::optional<std::string> configFilePath;
};

} // namespace common
} // namespace pacs