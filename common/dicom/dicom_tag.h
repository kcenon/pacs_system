#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>

// Forward declaration to avoid including DCMTK headers
class DcmTagKey;

namespace pacs {
namespace common {
namespace dicom {

/**
 * @brief Representation of a DICOM tag
 * 
 * DicomTag provides a simple, user-friendly way to work with DICOM tags
 * without requiring direct knowledge of DCMTK or the DICOM standard.
 * It includes predefined constants for common tags and methods to create
 * tags from group/element numbers or names.
 */
class DicomTag {
public:
    /**
     * @brief Construct a tag from group and element numbers
     * @param group The group number (e.g., 0x0010 for patient)
     * @param element The element number (e.g., 0x0010 for patient name)
     */
    DicomTag(uint16_t group, uint16_t element);
    
    /**
     * @brief Construct a tag from a DCMTK tag key
     * @param tagKey The DCMTK tag key
     */
    explicit DicomTag(const DcmTagKey& tagKey);
    
    /**
     * @brief Copy constructor
     */
    DicomTag(const DicomTag& other) = default;
    
    /**
     * @brief Move constructor
     */
    DicomTag(DicomTag&& other) noexcept = default;
    
    /**
     * @brief Destructor
     */
    ~DicomTag() = default;
    
    /**
     * @brief Copy assignment operator
     */
    DicomTag& operator=(const DicomTag& other) = default;
    
    /**
     * @brief Move assignment operator
     */
    DicomTag& operator=(DicomTag&& other) noexcept = default;
    
    /**
     * @brief Equality operator
     */
    bool operator==(const DicomTag& other) const;
    
    /**
     * @brief Less than operator for ordering
     */
    bool operator<(const DicomTag& other) const;
    
    /**
     * @brief Get the group number
     * @return The group number
     */
    uint16_t getGroup() const;
    
    /**
     * @brief Get the element number
     * @return The element number
     */
    uint16_t getElement() const;
    
    /**
     * @brief Get the tag as a string in format (gggg,eeee)
     * @return String representation of the tag
     */
    std::string toString() const;
    
    /**
     * @brief Get the tag name if known
     * @return The tag name or empty string if unknown
     */
    std::string getName() const;
    
    /**
     * @brief Convert to a DCMTK tag key
     * @return DcmTagKey representation
     */
    DcmTagKey toDcmtkTag() const;
    
    /**
     * @brief Create a tag from its name
     * @param name The tag name (e.g., "PatientName")
     * @return The DicomTag or a default tag if name is unknown
     */
    static DicomTag fromName(const std::string& name);
    
    /**
     * @brief Check if a tag name exists
     * @param name The tag name to check
     * @return True if the name corresponds to a known tag
     */
    static bool isValidName(const std::string& name);
    
    /**
     * @brief Get all known tag names
     * @return Vector of all known tag names
     */
    static std::vector<std::string> getAllKnownTagNames();
    
    // Common DICOM tags as constants
    static const DicomTag PatientName;            // (0010,0010)
    static const DicomTag PatientID;              // (0010,0020)
    static const DicomTag PatientBirthDate;       // (0010,0030)
    static const DicomTag PatientSex;             // (0010,0040)
    static const DicomTag StudyInstanceUID;       // (0020,000D)
    static const DicomTag StudyDate;              // (0008,0020)
    static const DicomTag StudyTime;              // (0008,0030)
    static const DicomTag AccessionNumber;        // (0008,0050)
    static const DicomTag Modality;               // (0008,0060)
    static const DicomTag SeriesInstanceUID;      // (0020,000E)
    static const DicomTag SeriesNumber;           // (0020,0011)
    static const DicomTag SOPInstanceUID;         // (0008,0018)
    static const DicomTag SOPClassUID;            // (0008,0016)
    static const DicomTag InstanceNumber;         // (0020,0013)
    static const DicomTag PixelData;              // (7FE0,0010)
    static const DicomTag Rows;                   // (0028,0010)
    static const DicomTag Columns;                // (0028,0011)
    static const DicomTag BitsAllocated;          // (0028,0100)
    static const DicomTag BitsStored;             // (0028,0101)
    static const DicomTag HighBit;                // (0028,0102)
    static const DicomTag PixelRepresentation;    // (0028,0103)
    static const DicomTag SamplesPerPixel;        // (0028,0002)
    static const DicomTag ScheduledProcedureStepSequence; // (0040,0100)
    static const DicomTag RequestedProcedureID;   // (0040,1001)
    static const DicomTag RequestedProcedureDescription; // (0032,1060)
    static const DicomTag PerformedProcedureStepStatus; // (0040,0252)
    
private:
    uint16_t group_;
    uint16_t element_;
    
    // Static map of name to tag for fromName lookup
    static const std::unordered_map<std::string, DicomTag> nameToTagMap_;
    
    // Static map of tag to name for getName lookup
    static const std::unordered_map<uint32_t, std::string> tagToNameMap_;
    
    // Helper to create a unique key from group and element
    static uint32_t makeKey(uint16_t group, uint16_t element);
};

} // namespace dicom
} // namespace common
} // namespace pacs