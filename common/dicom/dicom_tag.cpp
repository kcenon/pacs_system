#include "dicom_tag.h"

#ifndef DCMTK_NOT_AVAILABLE
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcdict.h"
#include "dcmtk/dcmdata/dcdicent.h"
#endif

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace pacs {
namespace common {
namespace dicom {

// Initialize static constant DicomTags
const DicomTag DicomTag::PatientName(0x0010, 0x0010);
const DicomTag DicomTag::PatientID(0x0010, 0x0020);
const DicomTag DicomTag::PatientBirthDate(0x0010, 0x0030);
const DicomTag DicomTag::PatientSex(0x0010, 0x0040);
const DicomTag DicomTag::StudyInstanceUID(0x0020, 0x000D);
const DicomTag DicomTag::StudyDate(0x0008, 0x0020);
const DicomTag DicomTag::StudyTime(0x0008, 0x0030);
const DicomTag DicomTag::AccessionNumber(0x0008, 0x0050);
const DicomTag DicomTag::Modality(0x0008, 0x0060);
const DicomTag DicomTag::SeriesInstanceUID(0x0020, 0x000E);
const DicomTag DicomTag::SeriesNumber(0x0020, 0x0011);
const DicomTag DicomTag::SOPInstanceUID(0x0008, 0x0018);
const DicomTag DicomTag::SOPClassUID(0x0008, 0x0016);
const DicomTag DicomTag::InstanceNumber(0x0020, 0x0013);
const DicomTag DicomTag::PixelData(0x7FE0, 0x0010);
const DicomTag DicomTag::Rows(0x0028, 0x0010);
const DicomTag DicomTag::Columns(0x0028, 0x0011);
const DicomTag DicomTag::BitsAllocated(0x0028, 0x0100);
const DicomTag DicomTag::BitsStored(0x0028, 0x0101);
const DicomTag DicomTag::HighBit(0x0028, 0x0102);
const DicomTag DicomTag::PixelRepresentation(0x0028, 0x0103);
const DicomTag DicomTag::SamplesPerPixel(0x0028, 0x0002);
const DicomTag DicomTag::ScheduledProcedureStepSequence(0x0040, 0x0100);
const DicomTag DicomTag::RequestedProcedureID(0x0040, 0x1001);
const DicomTag DicomTag::RequestedProcedureDescription(0x0032, 0x1060);
const DicomTag DicomTag::PerformedProcedureStepStatus(0x0040, 0x0252);

// Initialize tag-to-name map
const std::unordered_map<uint32_t, std::string> DicomTag::tagToNameMap_ = {
    {makeKey(0x0010, 0x0010), "PatientName"},
    {makeKey(0x0010, 0x0020), "PatientID"},
    {makeKey(0x0010, 0x0030), "PatientBirthDate"},
    {makeKey(0x0010, 0x0040), "PatientSex"},
    {makeKey(0x0020, 0x000D), "StudyInstanceUID"},
    {makeKey(0x0008, 0x0020), "StudyDate"},
    {makeKey(0x0008, 0x0030), "StudyTime"},
    {makeKey(0x0008, 0x0050), "AccessionNumber"},
    {makeKey(0x0008, 0x0060), "Modality"},
    {makeKey(0x0020, 0x000E), "SeriesInstanceUID"},
    {makeKey(0x0020, 0x0011), "SeriesNumber"},
    {makeKey(0x0008, 0x0018), "SOPInstanceUID"},
    {makeKey(0x0008, 0x0016), "SOPClassUID"},
    {makeKey(0x0020, 0x0013), "InstanceNumber"},
    {makeKey(0x7FE0, 0x0010), "PixelData"},
    {makeKey(0x0028, 0x0010), "Rows"},
    {makeKey(0x0028, 0x0011), "Columns"},
    {makeKey(0x0028, 0x0100), "BitsAllocated"},
    {makeKey(0x0028, 0x0101), "BitsStored"},
    {makeKey(0x0028, 0x0102), "HighBit"},
    {makeKey(0x0028, 0x0103), "PixelRepresentation"},
    {makeKey(0x0028, 0x0002), "SamplesPerPixel"},
    {makeKey(0x0040, 0x0100), "ScheduledProcedureStepSequence"},
    {makeKey(0x0040, 0x1001), "RequestedProcedureID"},
    {makeKey(0x0032, 0x1060), "RequestedProcedureDescription"},
    {makeKey(0x0040, 0x0252), "PerformedProcedureStepStatus"},
};

// Initialize name-to-tag map
const std::unordered_map<std::string, DicomTag> DicomTag::nameToTagMap_ = {
    {"PatientName", DicomTag(0x0010, 0x0010)},
    {"PatientID", DicomTag(0x0010, 0x0020)},
    {"PatientBirthDate", DicomTag(0x0010, 0x0030)},
    {"PatientSex", DicomTag(0x0010, 0x0040)},
    {"StudyInstanceUID", DicomTag(0x0020, 0x000D)},
    {"StudyDate", DicomTag(0x0008, 0x0020)},
    {"StudyTime", DicomTag(0x0008, 0x0030)},
    {"AccessionNumber", DicomTag(0x0008, 0x0050)},
    {"Modality", DicomTag(0x0008, 0x0060)},
    {"SeriesInstanceUID", DicomTag(0x0020, 0x000E)},
    {"SeriesNumber", DicomTag(0x0020, 0x0011)},
    {"SOPInstanceUID", DicomTag(0x0008, 0x0018)},
    {"SOPClassUID", DicomTag(0x0008, 0x0016)},
    {"InstanceNumber", DicomTag(0x0020, 0x0013)},
    {"PixelData", DicomTag(0x7FE0, 0x0010)},
    {"Rows", DicomTag(0x0028, 0x0010)},
    {"Columns", DicomTag(0x0028, 0x0011)},
    {"BitsAllocated", DicomTag(0x0028, 0x0100)},
    {"BitsStored", DicomTag(0x0028, 0x0101)},
    {"HighBit", DicomTag(0x0028, 0x0102)},
    {"PixelRepresentation", DicomTag(0x0028, 0x0103)},
    {"SamplesPerPixel", DicomTag(0x0028, 0x0002)},
    {"ScheduledProcedureStepSequence", DicomTag(0x0040, 0x0100)},
    {"RequestedProcedureID", DicomTag(0x0040, 0x1001)},
    {"RequestedProcedureDescription", DicomTag(0x0032, 0x1060)},
    {"PerformedProcedureStepStatus", DicomTag(0x0040, 0x0252)},
};

DicomTag::DicomTag(uint16_t group, uint16_t element)
    : group_(group), element_(element) {
}

DicomTag::DicomTag(const DcmTagKey& tagKey) {
#ifndef DCMTK_NOT_AVAILABLE
    group_ = tagKey.getGroup();
    element_ = tagKey.getElement();
#else
    group_ = 0;
    element_ = 0;
#endif
}

bool DicomTag::operator==(const DicomTag& other) const {
    return group_ == other.group_ && element_ == other.element_;
}

bool DicomTag::operator<(const DicomTag& other) const {
    if (group_ != other.group_) {
        return group_ < other.group_;
    }
    return element_ < other.element_;
}

uint16_t DicomTag::getGroup() const {
    return group_;
}

uint16_t DicomTag::getElement() const {
    return element_;
}

std::string DicomTag::toString() const {
    std::stringstream ss;
    ss << "(" << std::hex << std::setfill('0') << std::setw(4) << group_ << ","
       << std::hex << std::setfill('0') << std::setw(4) << element_ << ")";
    return ss.str();
}

std::string DicomTag::getName() const {
    uint32_t key = makeKey(group_, element_);
    auto it = tagToNameMap_.find(key);
    if (it != tagToNameMap_.end()) {
        return it->second;
    }
    
#ifndef DCMTK_NOT_AVAILABLE
    // Try to look up in DCMTK dictionary if not found in our map
    DcmTagKey tagKey(group_, element_);
    const DcmDataDictionary& globalDict = dcmDataDict.rdlock();
    const DcmDictEntry* entry = globalDict.findEntry(tagKey, nullptr);
    if (entry) {
        std::string name = entry->getTagName();
        dcmDataDict.unlock();
        return name;
    }
    dcmDataDict.unlock();
#endif
    
    // Return a formatted string like (gggg,eeee) if no name found
    return toString();
}

DcmTagKey DicomTag::toDcmtkTag() const {
#ifndef DCMTK_NOT_AVAILABLE
    return DcmTagKey(group_, element_);
#else
    // This will never be used in placeholder implementation
    return DcmTagKey();
#endif
}

DicomTag DicomTag::fromName(const std::string& name) {
    auto it = nameToTagMap_.find(name);
    if (it != nameToTagMap_.end()) {
        return it->second;
    }
    
    // Return a default tag (0x0000,0x0000) if not found
    return DicomTag(0, 0);
}

bool DicomTag::isValidName(const std::string& name) {
    return nameToTagMap_.find(name) != nameToTagMap_.end();
}

std::vector<std::string> DicomTag::getAllKnownTagNames() {
    std::vector<std::string> result;
    result.reserve(nameToTagMap_.size());
    
    for (const auto& entry : nameToTagMap_) {
        result.push_back(entry.first);
    }
    
    // Sort the names alphabetically
    std::sort(result.begin(), result.end());
    
    return result;
}

uint32_t DicomTag::makeKey(uint16_t group, uint16_t element) {
    return (static_cast<uint32_t>(group) << 16) | element;
}

} // namespace dicom
} // namespace common
} // namespace pacs