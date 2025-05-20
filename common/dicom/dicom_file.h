#pragma once

#include <string>
#include <memory>
#include <optional>

#include "dicom_object.h"

// Forward declaration to avoid including DCMTK headers
class DcmFileFormat;

namespace pacs {
namespace common {
namespace dicom {

/**
 * @brief Simple wrapper for DICOM files that hides DCMTK complexity
 * 
 * DicomFile provides a high-level, easy-to-use interface for working with
 * DICOM files without requiring direct knowledge of DCMTK. It handles reading
 * and writing DICOM files and provides access to the contained DICOM data.
 */
class DicomFile {
public:
    /**
     * @brief Default constructor creates an empty DICOM file
     */
    DicomFile();
    
    /**
     * @brief Construct from a DicomObject
     * @param object The DICOM object to store in the file
     */
    explicit DicomFile(const DicomObject& object);
    
    /**
     * @brief Construct from an existing DCMTK file format
     * @param fileFormat Pointer to a DcmFileFormat (ownership is transferred)
     */
    explicit DicomFile(DcmFileFormat* fileFormat);
    
    /**
     * @brief Copy constructor
     */
    DicomFile(const DicomFile& other);
    
    /**
     * @brief Move constructor
     */
    DicomFile(DicomFile&& other) noexcept;
    
    /**
     * @brief Destructor
     */
    ~DicomFile();
    
    /**
     * @brief Copy assignment operator
     */
    DicomFile& operator=(const DicomFile& other);
    
    /**
     * @brief Move assignment operator
     */
    DicomFile& operator=(DicomFile&& other) noexcept;
    
    /**
     * @brief Load a DICOM file from disk
     * @param filename Path to the DICOM file
     * @return True if loading succeeded, false otherwise
     */
    bool load(const std::string& filename);
    
    /**
     * @brief Save the DICOM file to disk
     * @param filename Path where the file should be saved
     * @return True if saving succeeded, false otherwise
     */
    bool save(const std::string& filename) const;
    
    /**
     * @brief Get the DICOM object contained in the file
     * @return The DICOM object
     */
    DicomObject getObject() const;
    
    /**
     * @brief Set the DICOM object contained in the file
     * @param object The DICOM object to store
     */
    void setObject(const DicomObject& object);
    
    /**
     * @brief Get the filename if the file was loaded from disk
     * @return The filename or empty optional if not from disk
     */
    std::optional<std::string> getFilename() const;
    
    /**
     * @brief Generate a filename based on DICOM tags
     * 
     * Creates a filename using SOP Instance UID or other identifiers
     * 
     * @return A suitable filename for the DICOM file
     */
    std::string generateFilename() const;
    
    /**
     * @brief Check if the file contains an image
     * @return True if the file contains image pixel data
     */
    bool hasPixelData() const;
    
    /**
     * @brief Get patient name from the file
     * @return Patient name or empty string if not available
     */
    std::string getPatientName() const;
    
    /**
     * @brief Get study description from the file
     * @return Study description or empty string if not available
     */
    std::string getStudyDescription() const;
    
    /**
     * @brief Get series description from the file
     * @return Series description or empty string if not available
     */
    std::string getSeriesDescription() const;
    
    /**
     * @brief Get SOP instance UID from the file
     * @return SOP instance UID or empty string if not available
     */
    std::string getSOPInstanceUID() const;
    
    /**
     * @brief Get modality from the file
     * @return Modality or empty string if not available
     */
    std::string getModality() const;
    
    /**
     * @brief Access the underlying DCMTK file format
     * @return Pointer to the underlying DcmFileFormat
     * @note This is provided for advanced use cases and should be avoided when possible
     */
    DcmFileFormat* getFileFormat() const;
    
private:
#ifndef DCMTK_NOT_AVAILABLE
    std::unique_ptr<DcmFileFormat> fileFormat_;
#else
    // Placeholder for DCMTK implementation
    void* placeholder = nullptr;
#endif
    std::optional<std::string> filename_;
};

} // namespace dicom
} // namespace common
} // namespace pacs