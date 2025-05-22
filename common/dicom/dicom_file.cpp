#include "dicom_file.h"
#include "dicom_tag.h"
#include "dicom_error.h"

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcmetinf.h"
#include "dcmtk/dcmdata/dcdict.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "codec_manager.h"

#include <filesystem>
#include <sstream>
#include <chrono>

namespace fs = std::filesystem;

namespace pacs {
namespace common {
namespace dicom {

DicomFile::DicomFile() {
    fileFormat_ = std::make_unique<DcmFileFormat>();
}

DicomFile::DicomFile(const DicomObject& object) {
    fileFormat_ = std::make_unique<DcmFileFormat>();
    DcmDataset* dataset = object.getDataset();
    if (dataset) {
        fileFormat_->getDataset()->copyFrom(*dataset);
    }
}

DicomFile::DicomFile(DcmFileFormat* fileFormat) {
    fileFormat_ = std::unique_ptr<DcmFileFormat>(fileFormat);
}

DicomFile::DicomFile(const DicomFile& other) {
    if (other.fileFormat_) {
        fileFormat_ = std::make_unique<DcmFileFormat>(*other.fileFormat_);
    } else {
        fileFormat_ = std::make_unique<DcmFileFormat>();
    }
    
    filename_ = other.filename_;
}

DicomFile::DicomFile(DicomFile&& other) noexcept {
    fileFormat_ = std::move(other.fileFormat_);
    filename_ = std::move(other.filename_);
}

DicomFile::~DicomFile() = default;

DicomFile& DicomFile::operator=(const DicomFile& other) {
    if (this != &other) {
        if (other.fileFormat_) {
            fileFormat_ = std::make_unique<DcmFileFormat>(*other.fileFormat_);
        } else {
            fileFormat_ = std::make_unique<DcmFileFormat>();
        }
        
        filename_ = other.filename_;
    }
    return *this;
}

DicomFile& DicomFile::operator=(DicomFile&& other) noexcept {
    if (this != &other) {
        fileFormat_ = std::move(other.fileFormat_);
        filename_ = std::move(other.filename_);
    }
    return *this;
}

bool DicomFile::load(const std::string& filename) {
    if (!fileFormat_) {
        fileFormat_ = std::make_unique<DcmFileFormat>();
    }
    
    OFCondition result = fileFormat_->loadFile(filename.c_str());
    if (result.good()) {
        filename_ = filename;
        return true;
    }
    
    return false;
}

bool DicomFile::save(const std::string& filename) const {
    if (!fileFormat_) {
        return false;
    }
    
    // Create directory if it doesn't exist
    fs::path path(filename);
    fs::path dir = path.parent_path();
    if (!dir.empty() && !fs::exists(dir)) {
        std::error_code ec;
        if (!fs::create_directories(dir, ec) && ec) {
            return false;
        }
    }
    
    // Default to Little Endian Explicit if not specified
    E_TransferSyntax xfer = EXS_LittleEndianExplicit;
    
    OFCondition result = fileFormat_->saveFile(filename.c_str(), xfer);
    return result.good();
}

DicomObject DicomFile::getObject() const {
    if (!fileFormat_ || !fileFormat_->getDataset()) {
        return DicomObject();
    }
    
    // Create a copy of the dataset
    DcmDataset* dataset = new DcmDataset(*fileFormat_->getDataset());
    return DicomObject(dataset);
}

void DicomFile::setObject(const DicomObject& object) {
    if (!fileFormat_) {
        fileFormat_ = std::make_unique<DcmFileFormat>();
    }
    
    DcmDataset* dataset = object.getDataset();
    if (dataset) {
        fileFormat_->getDataset()->copyFrom(*dataset);
    }
}

std::optional<std::string> DicomFile::getFilename() const {
    return filename_;
}

std::string DicomFile::generateFilename() const {
    if (!fileFormat_ || !fileFormat_->getDataset()) {
        return "unknown.dcm";
    }
    
    // Try to get SOP Instance UID for the filename
    DicomObject obj = getObject();
    std::string sopInstanceUID = obj.getSOPInstanceUID();
    
    if (!sopInstanceUID.empty()) {
        // Replace dots with underscores for the filename
        std::replace(sopInstanceUID.begin(), sopInstanceUID.end(), '.', '_');
        return sopInstanceUID + ".dcm";
    }
    
    // If no SOP Instance UID, try patient and study info
    std::string patientID = obj.getPatientID();
    std::string studyDate = obj.getStudyDate();
    std::string modality = obj.getModality();
    
    if (!patientID.empty() && !studyDate.empty() && !modality.empty()) {
        return patientID + "_" + studyDate + "_" + modality + ".dcm";
    }
    
    // Fallback to a generic name
    return "dicom_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".dcm";
}

bool DicomFile::hasPixelData() const {
    if (!fileFormat_ || !fileFormat_->getDataset()) {
        return false;
    }
    
    DicomObject obj = getObject();
    return obj.hasTag(DicomTag::PixelData);
}

std::string DicomFile::getPatientName() const {
    return getObject().getPatientName();
}

std::string DicomFile::getStudyDescription() const {
    return getObject().getString(DicomTag(0x0008, 0x1030)); // Study Description
}

std::string DicomFile::getSeriesDescription() const {
    return getObject().getString(DicomTag(0x0008, 0x103E)); // Series Description
}

std::string DicomFile::getSOPInstanceUID() const {
    return getObject().getSOPInstanceUID();
}

std::string DicomFile::getModality() const {
    return getObject().getModality();
}

DcmFileFormat* DicomFile::getFileFormat() const {
    return fileFormat_.get();
}

} // namespace dicom
} // namespace common
} // namespace pacs