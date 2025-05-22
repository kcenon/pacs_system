#include "dicom_object.h"
#include "dicom_tag.h"
#include "dicom_error.h"

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcitem.h"
#include "dcmtk/dcmdata/dcpixel.h"
#include "dcmtk/dcmdata/dcpixseq.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcvr.h"
#include "dcmtk/dcmdata/dcvrcs.h"
#include "dcmtk/dcmdata/dcvrdt.h"
#include "dcmtk/dcmdata/dcvrui.h"
#include "dcmtk/dcmdata/dcvrlo.h"
#include "dcmtk/dcmdata/dcvrpn.h"
#include "dcmtk/dcmdata/dcvrsh.h"
#include "dcmtk/dcmdata/dcvrst.h"
#include "dcmtk/dcmdata/dcvrda.h"
#include "dcmtk/dcmdata/dcvrtm.h"
#include "dcmtk/dcmdata/dcvris.h"
#include "dcmtk/dcmdata/dcvras.h"
#include "dcmtk/dcmdata/dcvrsl.h"
#include "dcmtk/dcmdata/dcvrds.h"
#include "dcmtk/dcmdata/dcvrfl.h"
#include "dcmtk/dcmdata/dcvrfd.h"
#include "dcmtk/dcmdata/dcvrsq.h"

#include <iomanip>
#include <sstream>
#include <algorithm>

namespace pacs {
namespace common {
namespace dicom {

DicomObject::DicomObject() {
    dataset_ = std::make_unique<DcmDataset>();
}

DicomObject::DicomObject(DcmDataset* dataset) {
    dataset_ = std::unique_ptr<DcmDataset>(dataset);
}

DicomObject::DicomObject(const DicomObject& other) {
    if (other.dataset_) {
        dataset_ = std::make_unique<DcmDataset>(*(other.dataset_));
    } else {
        dataset_ = std::make_unique<DcmDataset>();
    }
}

DicomObject::DicomObject(DicomObject&& other) noexcept {
    dataset_ = std::move(other.dataset_);
}

DicomObject::~DicomObject() = default;

DicomObject& DicomObject::operator=(const DicomObject& other) {
    if (this != &other) {
        if (other.dataset_) {
            dataset_ = std::make_unique<DcmDataset>(*(other.dataset_));
        } else {
            dataset_ = std::make_unique<DcmDataset>();
        }
    }
    return *this;
}

DicomObject& DicomObject::operator=(DicomObject&& other) noexcept {
    if (this != &other) {
        dataset_ = std::move(other.dataset_);
    }
    return *this;
}

bool DicomObject::hasTag(const DicomTag& tag) const {
    if (!dataset_) {
        return false;
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    DcmElement* element = nullptr;
    OFCondition result = dataset_->findAndGetElement(tagKey, element);
    return result.good() && element != nullptr;
}

std::string DicomObject::getString(const DicomTag& tag) const {
    if (!dataset_) {
        return "";
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    OFString value;
    OFCondition result = dataset_->findAndGetOFString(tagKey, value);
    if (result.good()) {
        return std::string(value.c_str());
    }
    return "";
}

std::optional<int> DicomObject::getInt(const DicomTag& tag) const {
    if (!dataset_) {
        return std::nullopt;
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    long int value = 0;
    OFCondition result = dataset_->findAndGetLongInt(tagKey, value);
    if (result.good()) {
        return static_cast<int>(value);
    }
    return std::nullopt;
}

std::optional<double> DicomObject::getFloat(const DicomTag& tag) const {
    if (!dataset_) {
        return std::nullopt;
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    double value = 0.0;
    OFCondition result = dataset_->findAndGetFloat64(tagKey, value);
    if (result.good()) {
        return value;
    }
    return std::nullopt;
}

std::vector<DicomObject> DicomObject::getSequence(const DicomTag& tag) const {
    std::vector<DicomObject> result;
    
    if (!dataset_) {
        return result;
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    DcmSequenceOfItems* sequence = nullptr;
    OFCondition cond = dataset_->findAndGetSequence(tagKey, sequence);
    
    if (cond.good() && sequence != nullptr) {
        unsigned long numItems = sequence->card();
        for (unsigned long i = 0; i < numItems; ++i) {
            DcmItem* item = sequence->getItem(i);
            if (item != nullptr) {
                // Convert DcmItem to DcmDataset (item is a subclass of DcmDataset)
                DcmDataset* dataset = dynamic_cast<DcmDataset*>(item->clone());
                if (dataset != nullptr) {
                    result.emplace_back(dataset);
                }
            }
        }
    }
    
    return result;
}

void DicomObject::setString(const DicomTag& tag, const std::string& value) {
    if (!dataset_) {
        dataset_ = std::make_unique<DcmDataset>();
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    dataset_->putAndInsertString(tagKey, value.c_str());
}

void DicomObject::setInt(const DicomTag& tag, int value) {
    if (!dataset_) {
        dataset_ = std::make_unique<DcmDataset>();
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    dataset_->putAndInsertLongInt(tagKey, static_cast<long int>(value));
}

void DicomObject::setFloat(const DicomTag& tag, double value) {
    if (!dataset_) {
        dataset_ = std::make_unique<DcmDataset>();
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    dataset_->putAndInsertFloat64(tagKey, value);
}

void DicomObject::setSequence(const DicomTag& tag, const std::vector<DicomObject>& sequence) {
    if (!dataset_) {
        dataset_ = std::make_unique<DcmDataset>();
    }
    
    DcmTagKey tagKey = tag.toDcmtkTag();
    DcmSequenceOfItems* dcmSequence = new DcmSequenceOfItems(tagKey);
    
    for (const auto& obj : sequence) {
        DcmDataset* objDataset = obj.getDataset();
        if (objDataset != nullptr) {
            DcmItem* item = new DcmItem();
            item->copyFrom(*objDataset);
            dcmSequence->append(item);
        }
    }
    
    dataset_->insert(dcmSequence, true);
}

std::string DicomObject::toString() const {
    if (!dataset_) {
        return "[Empty DICOM object]";
    }
    
    std::ostringstream ss;
    dataset_->print(ss);
    return ss.str();
}

DcmDataset* DicomObject::getDataset() const {
    return dataset_.get();
}

DicomObject DicomObject::clone() const {
    return DicomObject(*this);
}

std::vector<DicomTag> DicomObject::getAllTags() const {
    std::vector<DicomTag> tags;
    
    if (!dataset_) {
        return tags;
    }
    
    for (unsigned long i = 0; i < dataset_->card(); ++i) {
        DcmElement* element = dataset_->getElement(i);
        if (element) {
            DcmTagKey tagKey = element->getTag();
            tags.emplace_back(DicomTag(tagKey));
        }
    }
    
    return tags;
}

std::string DicomObject::getPatientName() const {
    return getString(DicomTag::PatientName);
}

std::string DicomObject::getPatientID() const {
    return getString(DicomTag::PatientID);
}

std::string DicomObject::getStudyInstanceUID() const {
    return getString(DicomTag::StudyInstanceUID);
}

std::string DicomObject::getSeriesInstanceUID() const {
    return getString(DicomTag::SeriesInstanceUID);
}

std::string DicomObject::getSOPInstanceUID() const {
    return getString(DicomTag::SOPInstanceUID);
}

std::string DicomObject::getSOPClassUID() const {
    return getString(DicomTag::SOPClassUID);
}

std::string DicomObject::getModality() const {
    return getString(DicomTag::Modality);
}

std::string DicomObject::getAccessionNumber() const {
    return getString(DicomTag::AccessionNumber);
}

std::string DicomObject::getStudyDate() const {
    return getString(DicomTag::StudyDate);
}

std::string DicomObject::getStudyTime() const {
    return getString(DicomTag::StudyTime);
}

DcmElement* DicomObject::getElement(const DcmTagKey& tagKey) const {
    if (!dataset_) {
        return nullptr;
    }
    
    DcmElement* element = nullptr;
    OFCondition result = dataset_->findAndGetElement(tagKey, element);
    if (result.good()) {
        return element;
    }
    return nullptr;
}

} // namespace dicom
} // namespace common
} // namespace pacs