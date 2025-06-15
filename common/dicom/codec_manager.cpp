#include "codec_manager.h"

#ifndef USE_DCMTK_PLACEHOLDER
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcrledrg.h"     // RLE decoder
#include "dcmtk/dcmdata/dcrleerg.h"     // RLE encoder
#include "dcmtk/dcmjpeg/djdecode.h"     // JPEG decoder
#include "dcmtk/dcmjpeg/djencode.h"     // JPEG encoder
#include "dcmtk/dcmjpls/djdecode.h"     // JPEG-LS decoder
#include "dcmtk/dcmjpls/djencode.h"     // JPEG-LS encoder

// JPEG-2000 support (if available)
#ifdef WITH_JPEG2K
#include "dcmtk/dcmjp2k/djp2kdec.h"     // JPEG-2000 decoder
#include "dcmtk/dcmjp2k/djp2kenc.h"     // JPEG-2000 encoder
#endif
#endif // USE_DCMTK_PLACEHOLDER

#include "../logger/logger.h"

#ifdef USE_DCMTK_PLACEHOLDER
// Define placeholder UIDs when DCMTK is not available
#define UID_LittleEndianImplicitTransferSyntax "1.2.840.10008.1.2"
#define UID_LittleEndianExplicitTransferSyntax "1.2.840.10008.1.2.1"
#define UID_BigEndianExplicitTransferSyntax "1.2.840.10008.1.2.2"
#define UID_JPEGProcess1TransferSyntax "1.2.840.10008.1.2.4.50"
#define UID_JPEGProcess2_4TransferSyntax "1.2.840.10008.1.2.4.51"
#define UID_JPEGProcess14TransferSyntax "1.2.840.10008.1.2.4.57"
#define UID_JPEGProcess14SV1TransferSyntax "1.2.840.10008.1.2.4.70"
#define UID_JPEGLSLosslessTransferSyntax "1.2.840.10008.1.2.4.80"
#define UID_JPEGLSNearLosslessTransferSyntax "1.2.840.10008.1.2.4.81"
#define UID_JPEG2000LosslessOnlyTransferSyntax "1.2.840.10008.1.2.4.90"
#define UID_JPEG2000TransferSyntax "1.2.840.10008.1.2.4.91"
#define UID_RLELosslessTransferSyntax "1.2.840.10008.1.2.5"
#endif

// Create shorter aliases for UIDs
#ifdef USE_DCMTK_PLACEHOLDER
// When DCMTK is not available, use the literal UID values
#define UID_JPEGProcess1 "1.2.840.10008.1.2.4.50"
#define UID_JPEGProcess2_4 "1.2.840.10008.1.2.4.51"
#define UID_JPEGProcess14 "1.2.840.10008.1.2.4.70"
#define UID_JPEGProcess14SV1 "1.2.840.10008.1.2.4.80"
#define UID_JPEGLSLossless "1.2.840.10008.1.2.4.80"
#define UID_JPEGLSLossy "1.2.840.10008.1.2.4.81"
#define UID_JPEG2000LosslessOnly "1.2.840.10008.1.2.4.90"
#define UID_JPEG2000 "1.2.840.10008.1.2.4.91"
#define UID_RLELossless "1.2.840.10008.1.2.5"
#else
// When DCMTK is available, use the DCMTK constants
#define UID_JPEGProcess1 UID_JPEGProcess1TransferSyntax
#define UID_JPEGProcess2_4 UID_JPEGProcess2_4TransferSyntax
#define UID_JPEGProcess14 UID_JPEGProcess14TransferSyntax
#define UID_JPEGProcess14SV1 UID_JPEGProcess14SV1TransferSyntax
#define UID_JPEGLSLossless UID_JPEGLSLosslessTransferSyntax
#define UID_JPEGLSLossy UID_JPEGLSLossyTransferSyntax
#define UID_JPEG2000LosslessOnly UID_JPEG2000LosslessOnlyTransferSyntax
#define UID_JPEG2000 UID_JPEG2000TransferSyntax
#define UID_RLELossless UID_RLELosslessTransferSyntax
#endif

namespace pacs {
namespace common {
namespace dicom {

CodecManager::CodecManager() 
    : initialized_(false)
    , jpegCodecsRegistered_(false)
    , jpegLSCodecsRegistered_(false)
    , jpeg2000CodecsRegistered_(false)
    , rleCodecRegistered_(false) {
}

CodecManager::~CodecManager() {
    cleanup();
}

CodecManager& CodecManager::getInstance() {
    static CodecManager instance;
    return instance;
}

void CodecManager::initialize() {
    if (initialized_) {
        return;
    }
    
    pacs::common::logger::logInfo("Initializing DICOM codec manager");
    
    // Register all available codecs
    registerJPEGCodecs();
    registerJPEGLSCodecs();
    registerJPEG2000Codecs();
    registerRLECodec();
    
    initialized_ = true;
    pacs::common::logger::logInfo("DICOM codec manager initialized successfully");
}

void CodecManager::cleanup() {
    if (!initialized_) {
        return;
    }
    
    pacs::common::logger::logInfo("Cleaning up DICOM codec manager");
    
#ifndef USE_DCMTK_PLACEHOLDER
    // Deregister codecs in reverse order
    if (rleCodecRegistered_) {
        DcmRLEDecoderRegistration::cleanup();
        DcmRLEEncoderRegistration::cleanup();
        rleCodecRegistered_ = false;
    }
    
#ifdef WITH_JPEG2K
    if (jpeg2000CodecsRegistered_) {
        DJPEG2KDecoderRegistration::cleanup();
        DJPEG2KEncoderRegistration::cleanup();
        jpeg2000CodecsRegistered_ = false;
    }
#endif
    
    if (jpegLSCodecsRegistered_) {
        DJLSDecoderRegistration::cleanup();
        DJLSEncoderRegistration::cleanup();
        jpegLSCodecsRegistered_ = false;
    }
    
    if (jpegCodecsRegistered_) {
        DJDecoderRegistration::cleanup();
        DJEncoderRegistration::cleanup();
        jpegCodecsRegistered_ = false;
    }
#else
    // When DCMTK is not available, just reset the flags
    rleCodecRegistered_ = false;
    jpeg2000CodecsRegistered_ = false;
    jpegLSCodecsRegistered_ = false;
    jpegCodecsRegistered_ = false;
#endif
    
    initialized_ = false;
    pacs::common::logger::logInfo("DICOM codec manager cleaned up");
}

void CodecManager::registerJPEGCodecs() {
    if (jpegCodecsRegistered_) {
        return;
    }
    
#ifdef USE_DCMTK_PLACEHOLDER
    pacs::common::logger::logInfo("JPEG codec registration skipped - DCMTK not available");
    jpegCodecsRegistered_ = true;
#else
    try {
        // Register JPEG decoders
        DJDecoderRegistration::registerCodecs();
        
        // Register JPEG encoders
        DJEncoderRegistration::registerCodecs();
        
        jpegCodecsRegistered_ = true;
        pacs::common::logger::logInfo("JPEG codecs registered successfully");
    }
    catch (const std::exception& e) {
        pacs::common::logger::logError("Failed to register JPEG codecs: %s", e.what());
    }
#endif
}

void CodecManager::registerJPEGLSCodecs() {
    if (jpegLSCodecsRegistered_) {
        return;
    }
    
#ifdef USE_DCMTK_PLACEHOLDER
    pacs::common::logger::logInfo("JPEG-LS codec registration skipped - DCMTK not available");
    jpegLSCodecsRegistered_ = true;
#else
    try {
        // Register JPEG-LS decoders
        DJLSDecoderRegistration::registerCodecs();
        
        // Register JPEG-LS encoders
        DJLSEncoderRegistration::registerCodecs();
        
        jpegLSCodecsRegistered_ = true;
        pacs::common::logger::logInfo("JPEG-LS codecs registered successfully");
    }
    catch (const std::exception& e) {
        pacs::common::logger::logError("Failed to register JPEG-LS codecs: %s", e.what());
    }
#endif
}

void CodecManager::registerJPEG2000Codecs() {
    if (jpeg2000CodecsRegistered_) {
        return;
    }
    
#ifdef WITH_JPEG2K
    try {
        // Register JPEG-2000 decoders
        DJPEG2KDecoderRegistration::registerCodecs();
        
        // Register JPEG-2000 encoders
        DJPEG2KEncoderRegistration::registerCodecs();
        
        jpeg2000CodecsRegistered_ = true;
        write_information("JPEG-2000 codecs registered successfully");
    }
    catch (const std::exception& e) {
        write_error("Failed to register JPEG-2000 codecs: {}", e.what());
    }
#else
    pacs::common::logger::logInfo("JPEG-2000 support not available (compile with WITH_JPEG2K)");
#endif
}

void CodecManager::registerRLECodec() {
    if (rleCodecRegistered_) {
        return;
    }
    
#ifdef USE_DCMTK_PLACEHOLDER
    write_information("RLE codec registration skipped - DCMTK not available");
    rleCodecRegistered_ = true;
#else
    try {
        // Register RLE decoder
        DcmRLEDecoderRegistration::registerCodecs();
        
        // Register RLE encoder
        DcmRLEEncoderRegistration::registerCodecs();
        
        rleCodecRegistered_ = true;
        pacs::common::logger::logInfo("RLE codec registered successfully");
    }
    catch (const std::exception& e) {
        pacs::common::logger::logError("Failed to register RLE codec: %s", e.what());
    }
#endif
}

bool CodecManager::isTransferSyntaxSupported(const std::string& transferSyntax) const {
    // Check uncompressed transfer syntaxes
    if (transferSyntax == UID_LittleEndianImplicitTransferSyntax ||
        transferSyntax == UID_LittleEndianExplicitTransferSyntax ||
        transferSyntax == UID_BigEndianExplicitTransferSyntax) {
        return true;
    }
    
    // Check JPEG
    if (jpegCodecsRegistered_) {
        if (transferSyntax == UID_JPEGProcess1 ||
            transferSyntax == UID_JPEGProcess2_4 ||
            transferSyntax == UID_JPEGProcess14 ||
            transferSyntax == UID_JPEGProcess14SV1) {
            return true;
        }
    }
    
    // Check JPEG-LS
    if (jpegLSCodecsRegistered_) {
        if (transferSyntax == UID_JPEGLSLossless ||
            transferSyntax == UID_JPEGLSLossy) {
            return true;
        }
    }
    
    // Check JPEG-2000
    if (jpeg2000CodecsRegistered_) {
        if (transferSyntax == UID_JPEG2000LosslessOnly ||
            transferSyntax == UID_JPEG2000) {
            return true;
        }
    }
    
    // Check RLE
    if (rleCodecRegistered_) {
        if (transferSyntax == UID_RLELossless) {
            return true;
        }
    }
    
    return false;
}

std::vector<std::string> CodecManager::getSupportedTransferSyntaxes() const {
    std::vector<std::string> syntaxes;
    
    // Always support uncompressed
    syntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
    syntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
    syntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
    
    // Add compressed syntaxes
    auto compressed = getCompressedTransferSyntaxes();
    syntaxes.insert(syntaxes.end(), compressed.begin(), compressed.end());
    
    return syntaxes;
}

std::vector<std::string> CodecManager::getCompressedTransferSyntaxes() const {
    std::vector<std::string> syntaxes;
    
    if (jpegCodecsRegistered_) {
        syntaxes.push_back(UID_JPEGProcess1);          // Baseline
        syntaxes.push_back(UID_JPEGProcess2_4);        // Extended
        syntaxes.push_back(UID_JPEGProcess14);         // Lossless
        syntaxes.push_back(UID_JPEGProcess14SV1);      // Lossless SV1
    }
    
    if (jpegLSCodecsRegistered_) {
        syntaxes.push_back(UID_JPEGLSLossless);
        syntaxes.push_back(UID_JPEGLSLossy);
    }
    
    if (jpeg2000CodecsRegistered_) {
        syntaxes.push_back(UID_JPEG2000LosslessOnly);
        syntaxes.push_back(UID_JPEG2000);
    }
    
    if (rleCodecRegistered_) {
        syntaxes.push_back(UID_RLELossless);
    }
    
    return syntaxes;
}

std::string CodecManager::getTransferSyntaxName(const std::string& transferSyntax) const {
    if (transferSyntax == UID_LittleEndianImplicitTransferSyntax) {
        return "Little Endian Implicit";
    }
    else if (transferSyntax == UID_LittleEndianExplicitTransferSyntax) {
        return "Little Endian Explicit";
    }
    else if (transferSyntax == UID_BigEndianExplicitTransferSyntax) {
        return "Big Endian Explicit";
    }
    else if (transferSyntax == UID_JPEGProcess1) {
        return "JPEG Baseline (Process 1)";
    }
    else if (transferSyntax == UID_JPEGProcess2_4) {
        return "JPEG Extended (Process 2 & 4)";
    }
    else if (transferSyntax == UID_JPEGProcess14) {
        return "JPEG Lossless (Process 14)";
    }
    else if (transferSyntax == UID_JPEGProcess14SV1) {
        return "JPEG Lossless, First-Order Prediction";
    }
    else if (transferSyntax == UID_JPEGLSLossless) {
        return "JPEG-LS Lossless";
    }
    else if (transferSyntax == UID_JPEGLSLossy) {
        return "JPEG-LS Lossy";
    }
    else if (transferSyntax == UID_JPEG2000LosslessOnly) {
        return "JPEG 2000 Lossless Only";
    }
    else if (transferSyntax == UID_JPEG2000) {
        return "JPEG 2000";
    }
    else if (transferSyntax == UID_RLELossless) {
        return "RLE Lossless";
    }
    
    return "Unknown Transfer Syntax";
}

bool CodecManager::isLossyCompression(const std::string& transferSyntax) const {
    // These are lossy compression transfer syntaxes
    return (transferSyntax == UID_JPEGProcess1 ||        // JPEG Baseline
            transferSyntax == UID_JPEGProcess2_4 ||      // JPEG Extended
            transferSyntax == UID_JPEGLSLossy ||         // JPEG-LS Lossy
            transferSyntax == UID_JPEG2000);             // JPEG 2000 (can be lossy)
}

} // namespace dicom
} // namespace common
} // namespace pacs