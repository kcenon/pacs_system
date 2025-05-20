#pragma once

#include <memory>
#include <string>

#include "tls_config.h"
#include "core/result/result.h"

// Check if DCMTK is built with OpenSSL
#ifdef DCMTK_WITH_OPENSSL
#define WITH_OPENSSL
#endif

// Forward declarations for DCMTK types (avoiding direct inclusion)
#ifdef WITH_OPENSSL
class DcmTLSTransportLayer;
#else
// Dummy class for when OpenSSL is not available
class DcmTLSTransportLayer {};
#endif

struct T_ASC_Network;
struct T_ASC_Parameters;

namespace pacs {
namespace common {
namespace security {

/**
 * @brief Adapter for DCMTK TLS functionality
 * 
 * This class provides an adapter between the PACS security system and
 * DCMTK's TLS implementation, allowing the application to use a consistent
 * configuration interface while ensuring compatibility with DCMTK.
 */
class DcmtkTlsAdapter {
public:
    /**
     * @brief Constructor
     * @param config TLS configuration
     */
    explicit DcmtkTlsAdapter(const TLSConfig& config);
    
    /**
     * @brief Destructor
     */
    ~DcmtkTlsAdapter();
    
    /**
     * @brief Initialize the TLS layer
     * @param role True for server, false for client
     * @return Result object indicating success or failure
     */
    core::Result<void> initialize(bool isServer);
    
    /**
     * @brief Apply TLS settings to a network object
     * @param net DCMTK network object
     * @return Result object indicating success or failure
     */
    core::Result<void> applyToNetwork(T_ASC_Network* net);
    
    /**
     * @brief Apply TLS settings to association parameters
     * @param params DCMTK association parameters
     * @return Result object indicating success or failure
     */
    core::Result<void> applyToAssociation(T_ASC_Parameters* params);
    
    /**
     * @brief Check if TLS is enabled
     * @return True if TLS is enabled, false otherwise
     */
    bool isEnabled() const;
    
    /**
     * @brief Get the DCMTK TLS transport layer
     * @return Pointer to DCMTK TLS transport layer
     */
    DcmTLSTransportLayer* getTLSLayer();

private:
    TLSConfig config_;
    std::unique_ptr<DcmTLSTransportLayer> tlsLayer_;
    bool initialized_ = false;
    bool isServer_ = false;
    
    /**
     * @brief Convert PACS TLS protocol version to DCMTK representation
     * @param version PACS TLS protocol version
     * @return String representation for DCMTK
     */
    std::string getProtocolString(TLSProtocolVersion version);
};

} // namespace security
} // namespace common
} // namespace pacs