#pragma once

#include <string>
#include <vector>
#include <memory>

#ifndef USE_DCMTK_PLACEHOLDER
#include "dcmtk/dcmdata/dctypes.h"
#endif

namespace pacs {
namespace common {
namespace dicom {

/**
 * @brief Manager for DICOM compression codecs
 * 
 * This class handles registration and management of various DICOM
 * compression/decompression codecs including JPEG, JPEG-LS, JPEG-2000, etc.
 */
class CodecManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the CodecManager instance
     */
    static CodecManager& getInstance();
    
    /**
     * @brief Initialize and register all available codecs
     */
    void initialize();
    
    /**
     * @brief Cleanup and unregister all codecs
     */
    void cleanup();
    
    /**
     * @brief Check if a specific transfer syntax is supported
     * @param transferSyntax Transfer syntax UID
     * @return True if supported, false otherwise
     */
    bool isTransferSyntaxSupported(const std::string& transferSyntax) const;
    
    /**
     * @brief Get list of supported transfer syntaxes
     * @return Vector of transfer syntax UIDs
     */
    std::vector<std::string> getSupportedTransferSyntaxes() const;
    
    /**
     * @brief Get list of supported compressed transfer syntaxes
     * @return Vector of compressed transfer syntax UIDs
     */
    std::vector<std::string> getCompressedTransferSyntaxes() const;
    
    /**
     * @brief Register JPEG codecs (baseline, extended, lossless)
     */
    void registerJPEGCodecs();
    
    /**
     * @brief Register JPEG-LS codecs
     */
    void registerJPEGLSCodecs();
    
    /**
     * @brief Register JPEG-2000 codecs
     */
    void registerJPEG2000Codecs();
    
    /**
     * @brief Register RLE (Run Length Encoding) codec
     */
    void registerRLECodec();
    
    /**
     * @brief Get human-readable name for transfer syntax
     * @param transferSyntax Transfer syntax UID
     * @return Human-readable name
     */
    std::string getTransferSyntaxName(const std::string& transferSyntax) const;
    
    /**
     * @brief Check if transfer syntax is lossy
     * @param transferSyntax Transfer syntax UID
     * @return True if lossy compression, false otherwise
     */
    bool isLossyCompression(const std::string& transferSyntax) const;
    
private:
    CodecManager();
    ~CodecManager();
    
    // Prevent copying
    CodecManager(const CodecManager&) = delete;
    CodecManager& operator=(const CodecManager&) = delete;
    
    bool initialized_;
    bool jpegCodecsRegistered_;
    bool jpegLSCodecsRegistered_;
    bool jpeg2000CodecsRegistered_;
    bool rleCodecRegistered_;
};

} // namespace dicom
} // namespace common
} // namespace pacs