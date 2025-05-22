#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <variant>
#include <map>
#include <functional>

// Forward declarations to avoid including DCMTK headers
class DcmDataset;
class DcmElement;
class DcmTagKey;

namespace pacs {
namespace common {
namespace dicom {

// Forward declarations
class DicomElement;
class DicomTag;

/**
 * @brief Simple wrapper for DICOM object that hides DCMTK complexity
 * 
 * DicomObject provides a high-level, easy-to-use interface for working with
 * DICOM data without requiring direct knowledge of DCMTK. It encapsulates
 * a DcmDataset and provides methods for accessing and manipulating DICOM elements.
 */
class DicomObject {
public:
    /**
     * @brief Default constructor creates an empty DICOM object
     */
    DicomObject();
    
    /**
     * @brief Construct from an existing DCMTK dataset
     * @param dataset Pointer to a DcmDataset (ownership is transferred)
     */
    explicit DicomObject(DcmDataset* dataset);
    
    /**
     * @brief Copy constructor
     */
    DicomObject(const DicomObject& other);
    
    /**
     * @brief Move constructor
     */
    DicomObject(DicomObject&& other) noexcept;
    
    /**
     * @brief Destructor
     */
    ~DicomObject();
    
    /**
     * @brief Copy assignment operator
     */
    DicomObject& operator=(const DicomObject& other);
    
    /**
     * @brief Move assignment operator
     */
    DicomObject& operator=(DicomObject&& other) noexcept;
    
    /**
     * @brief Check if the object contains a specific tag
     * @param tag The DICOM tag to check for
     * @return True if the tag exists, false otherwise
     */
    bool hasTag(const DicomTag& tag) const;
    
    /**
     * @brief Get a string value for a specific tag
     * @param tag The DICOM tag to get
     * @return The string value or empty string if tag doesn't exist
     */
    std::string getString(const DicomTag& tag) const;
    
    /**
     * @brief Get an integer value for a specific tag
     * @param tag The DICOM tag to get
     * @return Optional containing the integer value, or empty if tag doesn't exist or isn't an integer
     */
    std::optional<int> getInt(const DicomTag& tag) const;
    
    /**
     * @brief Get a floating point value for a specific tag
     * @param tag The DICOM tag to get
     * @return Optional containing the float value, or empty if tag doesn't exist or isn't a float
     */
    std::optional<double> getFloat(const DicomTag& tag) const;
    
    /**
     * @brief Get a sequence (nested DICOM objects) for a specific tag
     * @param tag The DICOM tag for the sequence
     * @return Vector of DicomObjects in the sequence, empty if tag doesn't exist or isn't a sequence
     */
    std::vector<DicomObject> getSequence(const DicomTag& tag) const;
    
    /**
     * @brief Set a string value for a specific tag
     * @param tag The DICOM tag to set
     * @param value The string value to set
     */
    void setString(const DicomTag& tag, const std::string& value);
    
    /**
     * @brief Set an integer value for a specific tag
     * @param tag The DICOM tag to set
     * @param value The integer value to set
     */
    void setInt(const DicomTag& tag, int value);
    
    /**
     * @brief Set a floating point value for a specific tag
     * @param tag The DICOM tag to set
     * @param value The float value to set
     */
    void setFloat(const DicomTag& tag, double value);
    
    /**
     * @brief Set a sequence (nested DICOM objects) for a specific tag
     * @param tag The DICOM tag for the sequence
     * @param sequence Vector of DicomObjects to set in the sequence
     */
    void setSequence(const DicomTag& tag, const std::vector<DicomObject>& sequence);
    
    /**
     * @brief Get the DICOM object as a formatted string
     * @return String representation of the DICOM object
     */
    std::string toString() const;
    
    /**
     * @brief Access the underlying DCMTK dataset
     * @return Pointer to the underlying DcmDataset
     * @note This is provided for advanced use cases and should be avoided when possible
     */
    DcmDataset* getDataset() const;
    
    /**
     * @brief Create a copy of this DICOM object
     * @return New DicomObject containing a copy of this object's data
     */
    DicomObject clone() const;
    
    /**
     * @brief Get all tags in this DICOM object
     * @return Vector of all DicomTags in this object
     */
    std::vector<DicomTag> getAllTags() const;
    
    /**
     * @brief Get patient name
     * @return Patient name or empty string if not available
     */
    std::string getPatientName() const;
    
    /**
     * @brief Get patient ID
     * @return Patient ID or empty string if not available
     */
    std::string getPatientID() const;
    
    /**
     * @brief Get study instance UID
     * @return Study instance UID or empty string if not available
     */
    std::string getStudyInstanceUID() const;
    
    /**
     * @brief Get series instance UID
     * @return Series instance UID or empty string if not available
     */
    std::string getSeriesInstanceUID() const;
    
    /**
     * @brief Get SOP instance UID
     * @return SOP instance UID or empty string if not available
     */
    std::string getSOPInstanceUID() const;
    
    /**
     * @brief Get SOP class UID
     * @return SOP class UID or empty string if not available
     */
    std::string getSOPClassUID() const;
    
    /**
     * @brief Get modality
     * @return Modality or empty string if not available
     */
    std::string getModality() const;
    
    /**
     * @brief Get accession number
     * @return Accession number or empty string if not available
     */
    std::string getAccessionNumber() const;
    
    /**
     * @brief Get study date
     * @return Study date or empty string if not available
     */
    std::string getStudyDate() const;
    
    /**
     * @brief Get study time
     * @return Study time or empty string if not available
     */
    std::string getStudyTime() const;
    
private:
    std::unique_ptr<DcmDataset> dataset_;
    
    // Helper method to get a DCMTK element
    DcmElement* getElement(const DcmTagKey& tagKey) const;
};

} // namespace dicom
} // namespace common
} // namespace pacs