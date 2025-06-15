#include "dicom_util.h"

#include <iostream>
#include <sstream>

namespace pacs {
namespace common {

std::string DicomUtil::datasetToString(const DcmDataset* dataset) {
#ifdef USE_DCMTK_PLACEHOLDER
    return "[DCMTK not available]";
#else
    if (!dataset) {
        return "";
    }
    
    std::stringstream ss;
    // Create a non-const copy for printing
    DcmDataset copy(*dataset);
    copy.print(ss);
    return ss.str();
#endif
}

std::unique_ptr<DcmFileFormat> DicomUtil::loadDicomFile(const std::string& filename) {
#ifdef USE_DCMTK_PLACEHOLDER
    return nullptr;
#else
    auto fileFormat = std::make_unique<DcmFileFormat>();
    OFCondition cond = fileFormat->loadFile(filename.c_str());
    
    if (cond.bad()) {
        return nullptr;
    }
    
    return fileFormat;
#endif
}

bool DicomUtil::saveDicomFile(DcmDataset* dataset, const std::string& filename) {
#ifdef USE_DCMTK_PLACEHOLDER
    return false;
#else
    if (!dataset) {
        return false;
    }
    
    DcmFileFormat fileFormat;
    // Create a copy of the dataset and assign it to the fileFormat
    DcmDataset* newDataset = new DcmDataset(*dataset);
    fileFormat.getDataset()->copyFrom(*newDataset);
    delete newDataset;
    
    OFCondition cond = fileFormat.saveFile(filename.c_str(), EXS_LittleEndianExplicit);
    
    return cond.good();
#endif
}

std::string DicomUtil::getElementValue(const DcmDataset* dataset, const DcmTagKey& tag) {
#ifdef USE_DCMTK_PLACEHOLDER
    return "[DCMTK not available]";
#else
    if (!dataset) {
        return "";
    }
    
    // Create a non-const copy for DCMTK API
    DcmDataset copy(*dataset);
    DcmElement* element = nullptr;
    OFCondition cond = copy.findAndGetElement(tag, element);
    if (cond.good() && element) {
        OFString valueStr;
        element->getOFString(valueStr, 0);
        return valueStr.c_str();
    }
    
    return "";
#endif
}

} // namespace common
} // namespace pacs