/**
 * @file standard_tags_data.cpp
 * @brief Standard DICOM tags from PS3.6 Data Dictionary
 *
 * This file contains the standard DICOM tag definitions as specified
 * in DICOM PS3.6. The data is organized by group for easier maintenance.
 *
 * Note: This file includes commonly used tags. For complete PS3.6 coverage,
 * additional tags can be generated from the official DICOM standard XML.
 *
 * @see DICOM PS3.6 - Data Dictionary
 */

#include "pacs/core/tag_info.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <array>
#include <span>

namespace pacs::core {

using VR = pacs::encoding::vr_type;

// Helper to create VR value
constexpr auto vr(VR v) -> uint16_t {
    return static_cast<uint16_t>(v);
}

// Helper for common VM patterns
constexpr value_multiplicity vm_1{1, 1};
constexpr value_multiplicity vm_1_n{1, std::nullopt};
constexpr value_multiplicity vm_2{2, 2};
constexpr value_multiplicity vm_2_n{2, std::nullopt};
constexpr value_multiplicity vm_3{3, 3};
constexpr value_multiplicity vm_6{6, 6};

// Standard DICOM tags organized by group
// clang-format off
static constexpr std::array standard_tags = {
    // ========================================================================
    // Command Group (0x0000) - DIMSE messages
    // ========================================================================
    tag_info{dicom_tag{0x0000, 0x0000}, vr(VR::UL), vm_1, "CommandGroupLength", "Command Group Length", false},
    tag_info{dicom_tag{0x0000, 0x0002}, vr(VR::UI), vm_1, "AffectedSOPClassUID", "Affected SOP Class UID", false},
    tag_info{dicom_tag{0x0000, 0x0003}, vr(VR::UI), vm_1, "RequestedSOPClassUID", "Requested SOP Class UID", false},
    tag_info{dicom_tag{0x0000, 0x0100}, vr(VR::US), vm_1, "CommandField", "Command Field", false},
    tag_info{dicom_tag{0x0000, 0x0110}, vr(VR::US), vm_1, "MessageID", "Message ID", false},
    tag_info{dicom_tag{0x0000, 0x0120}, vr(VR::US), vm_1, "MessageIDBeingRespondedTo", "Message ID Being Responded To", false},
    tag_info{dicom_tag{0x0000, 0x0600}, vr(VR::AE), vm_1, "MoveDestination", "Move Destination", false},
    tag_info{dicom_tag{0x0000, 0x0700}, vr(VR::US), vm_1, "Priority", "Priority", false},
    tag_info{dicom_tag{0x0000, 0x0800}, vr(VR::US), vm_1, "CommandDataSetType", "Command Data Set Type", false},
    tag_info{dicom_tag{0x0000, 0x0900}, vr(VR::US), vm_1, "Status", "Status", false},
    tag_info{dicom_tag{0x0000, 0x1000}, vr(VR::UI), vm_1, "AffectedSOPInstanceUID", "Affected SOP Instance UID", false},
    tag_info{dicom_tag{0x0000, 0x1020}, vr(VR::US), vm_1, "NumberOfRemainingSuboperations", "Number of Remaining Sub-operations", false},
    tag_info{dicom_tag{0x0000, 0x1021}, vr(VR::US), vm_1, "NumberOfCompletedSuboperations", "Number of Completed Sub-operations", false},
    tag_info{dicom_tag{0x0000, 0x1022}, vr(VR::US), vm_1, "NumberOfFailedSuboperations", "Number of Failed Sub-operations", false},
    tag_info{dicom_tag{0x0000, 0x1023}, vr(VR::US), vm_1, "NumberOfWarningSuboperations", "Number of Warning Sub-operations", false},

    // ========================================================================
    // File Meta Information (0x0002)
    // ========================================================================
    tag_info{dicom_tag{0x0002, 0x0000}, vr(VR::UL), vm_1, "FileMetaInformationGroupLength", "File Meta Information Group Length", false},
    tag_info{dicom_tag{0x0002, 0x0001}, vr(VR::OB), vm_1, "FileMetaInformationVersion", "File Meta Information Version", false},
    tag_info{dicom_tag{0x0002, 0x0002}, vr(VR::UI), vm_1, "MediaStorageSOPClassUID", "Media Storage SOP Class UID", false},
    tag_info{dicom_tag{0x0002, 0x0003}, vr(VR::UI), vm_1, "MediaStorageSOPInstanceUID", "Media Storage SOP Instance UID", false},
    tag_info{dicom_tag{0x0002, 0x0010}, vr(VR::UI), vm_1, "TransferSyntaxUID", "Transfer Syntax UID", false},
    tag_info{dicom_tag{0x0002, 0x0012}, vr(VR::UI), vm_1, "ImplementationClassUID", "Implementation Class UID", false},
    tag_info{dicom_tag{0x0002, 0x0013}, vr(VR::SH), vm_1, "ImplementationVersionName", "Implementation Version Name", false},
    tag_info{dicom_tag{0x0002, 0x0016}, vr(VR::AE), vm_1, "SourceApplicationEntityTitle", "Source Application Entity Title", false},
    tag_info{dicom_tag{0x0002, 0x0017}, vr(VR::AE), vm_1, "SendingApplicationEntityTitle", "Sending Application Entity Title", false},
    tag_info{dicom_tag{0x0002, 0x0018}, vr(VR::AE), vm_1, "ReceivingApplicationEntityTitle", "Receiving Application Entity Title", false},
    tag_info{dicom_tag{0x0002, 0x0100}, vr(VR::UI), vm_1, "PrivateInformationCreatorUID", "Private Information Creator UID", false},
    tag_info{dicom_tag{0x0002, 0x0102}, vr(VR::OB), vm_1, "PrivateInformation", "Private Information", false},

    // ========================================================================
    // SOP Common Module (0x0008)
    // ========================================================================
    tag_info{dicom_tag{0x0008, 0x0005}, vr(VR::CS), vm_1_n, "SpecificCharacterSet", "Specific Character Set", false},
    tag_info{dicom_tag{0x0008, 0x0006}, vr(VR::SQ), vm_1, "LanguageCodeSequence", "Language Code Sequence", false},
    tag_info{dicom_tag{0x0008, 0x0008}, vr(VR::CS), vm_2_n, "ImageType", "Image Type", false},
    tag_info{dicom_tag{0x0008, 0x0012}, vr(VR::DA), vm_1, "InstanceCreationDate", "Instance Creation Date", false},
    tag_info{dicom_tag{0x0008, 0x0013}, vr(VR::TM), vm_1, "InstanceCreationTime", "Instance Creation Time", false},
    tag_info{dicom_tag{0x0008, 0x0014}, vr(VR::UI), vm_1, "InstanceCreatorUID", "Instance Creator UID", false},
    tag_info{dicom_tag{0x0008, 0x0016}, vr(VR::UI), vm_1, "SOPClassUID", "SOP Class UID", false},
    tag_info{dicom_tag{0x0008, 0x0018}, vr(VR::UI), vm_1, "SOPInstanceUID", "SOP Instance UID", false},
    tag_info{dicom_tag{0x0008, 0x001A}, vr(VR::UI), vm_1_n, "RelatedGeneralSOPClassUID", "Related General SOP Class UID", false},
    tag_info{dicom_tag{0x0008, 0x001B}, vr(VR::UI), vm_1, "OriginalSpecializedSOPClassUID", "Original Specialized SOP Class UID", false},
    tag_info{dicom_tag{0x0008, 0x0020}, vr(VR::DA), vm_1, "StudyDate", "Study Date", false},
    tag_info{dicom_tag{0x0008, 0x0021}, vr(VR::DA), vm_1, "SeriesDate", "Series Date", false},
    tag_info{dicom_tag{0x0008, 0x0022}, vr(VR::DA), vm_1, "AcquisitionDate", "Acquisition Date", false},
    tag_info{dicom_tag{0x0008, 0x0023}, vr(VR::DA), vm_1, "ContentDate", "Content Date", false},
    tag_info{dicom_tag{0x0008, 0x0030}, vr(VR::TM), vm_1, "StudyTime", "Study Time", false},
    tag_info{dicom_tag{0x0008, 0x0031}, vr(VR::TM), vm_1, "SeriesTime", "Series Time", false},
    tag_info{dicom_tag{0x0008, 0x0032}, vr(VR::TM), vm_1, "AcquisitionTime", "Acquisition Time", false},
    tag_info{dicom_tag{0x0008, 0x0033}, vr(VR::TM), vm_1, "ContentTime", "Content Time", false},
    tag_info{dicom_tag{0x0008, 0x0050}, vr(VR::SH), vm_1, "AccessionNumber", "Accession Number", false},
    tag_info{dicom_tag{0x0008, 0x0052}, vr(VR::CS), vm_1, "QueryRetrieveLevel", "Query/Retrieve Level", false},
    tag_info{dicom_tag{0x0008, 0x0054}, vr(VR::AE), vm_1_n, "RetrieveAETitle", "Retrieve AE Title", false},
    tag_info{dicom_tag{0x0008, 0x0056}, vr(VR::CS), vm_1, "InstanceAvailability", "Instance Availability", false},
    tag_info{dicom_tag{0x0008, 0x0058}, vr(VR::UI), vm_1_n, "FailedSOPInstanceUIDList", "Failed SOP Instance UID List", false},
    tag_info{dicom_tag{0x0008, 0x0060}, vr(VR::CS), vm_1, "Modality", "Modality", false},
    tag_info{dicom_tag{0x0008, 0x0061}, vr(VR::CS), vm_1_n, "ModalitiesInStudy", "Modalities in Study", false},
    tag_info{dicom_tag{0x0008, 0x0062}, vr(VR::UI), vm_1_n, "SOPClassesInStudy", "SOP Classes in Study", false},
    tag_info{dicom_tag{0x0008, 0x0064}, vr(VR::CS), vm_1, "ConversionType", "Conversion Type", false},
    tag_info{dicom_tag{0x0008, 0x0068}, vr(VR::CS), vm_1, "PresentationIntentType", "Presentation Intent Type", false},
    tag_info{dicom_tag{0x0008, 0x0070}, vr(VR::LO), vm_1, "Manufacturer", "Manufacturer", false},
    tag_info{dicom_tag{0x0008, 0x0080}, vr(VR::LO), vm_1, "InstitutionName", "Institution Name", false},
    tag_info{dicom_tag{0x0008, 0x0081}, vr(VR::ST), vm_1, "InstitutionAddress", "Institution Address", false},
    tag_info{dicom_tag{0x0008, 0x0082}, vr(VR::SQ), vm_1, "InstitutionCodeSequence", "Institution Code Sequence", false},
    tag_info{dicom_tag{0x0008, 0x0090}, vr(VR::PN), vm_1, "ReferringPhysicianName", "Referring Physician's Name", false},
    tag_info{dicom_tag{0x0008, 0x0092}, vr(VR::ST), vm_1, "ReferringPhysicianAddress", "Referring Physician's Address", false},
    tag_info{dicom_tag{0x0008, 0x0094}, vr(VR::SH), vm_1_n, "ReferringPhysicianTelephoneNumbers", "Referring Physician's Telephone Numbers", false},
    tag_info{dicom_tag{0x0008, 0x0096}, vr(VR::SQ), vm_1, "ReferringPhysicianIdentificationSequence", "Referring Physician Identification Sequence", false},
    tag_info{dicom_tag{0x0008, 0x0100}, vr(VR::SH), vm_1, "CodeValue", "Code Value", false},
    tag_info{dicom_tag{0x0008, 0x0102}, vr(VR::SH), vm_1, "CodingSchemeDesignator", "Coding Scheme Designator", false},
    tag_info{dicom_tag{0x0008, 0x0103}, vr(VR::SH), vm_1, "CodingSchemeVersion", "Coding Scheme Version", false},
    tag_info{dicom_tag{0x0008, 0x0104}, vr(VR::LO), vm_1, "CodeMeaning", "Code Meaning", false},
    tag_info{dicom_tag{0x0008, 0x0105}, vr(VR::CS), vm_1, "MappingResource", "Mapping Resource", false},
    tag_info{dicom_tag{0x0008, 0x0106}, vr(VR::DT), vm_1, "ContextGroupVersion", "Context Group Version", false},
    tag_info{dicom_tag{0x0008, 0x010F}, vr(VR::CS), vm_1, "ContextIdentifier", "Context Identifier", false},
    tag_info{dicom_tag{0x0008, 0x0110}, vr(VR::SQ), vm_1, "CodingSchemeIdentificationSequence", "Coding Scheme Identification Sequence", false},
    tag_info{dicom_tag{0x0008, 0x1010}, vr(VR::SH), vm_1, "StationName", "Station Name", false},
    tag_info{dicom_tag{0x0008, 0x1030}, vr(VR::LO), vm_1, "StudyDescription", "Study Description", false},
    tag_info{dicom_tag{0x0008, 0x103E}, vr(VR::LO), vm_1, "SeriesDescription", "Series Description", false},
    tag_info{dicom_tag{0x0008, 0x1040}, vr(VR::LO), vm_1, "InstitutionalDepartmentName", "Institutional Department Name", false},
    tag_info{dicom_tag{0x0008, 0x1048}, vr(VR::PN), vm_1_n, "PhysiciansOfRecord", "Physician(s) of Record", false},
    tag_info{dicom_tag{0x0008, 0x1050}, vr(VR::PN), vm_1_n, "PerformingPhysicianName", "Performing Physician's Name", false},
    tag_info{dicom_tag{0x0008, 0x1060}, vr(VR::PN), vm_1_n, "NameOfPhysiciansReadingStudy", "Name of Physician(s) Reading Study", false},
    tag_info{dicom_tag{0x0008, 0x1070}, vr(VR::PN), vm_1_n, "OperatorsName", "Operators' Name", false},
    tag_info{dicom_tag{0x0008, 0x1080}, vr(VR::LO), vm_1_n, "AdmittingDiagnosesDescription", "Admitting Diagnoses Description", false},
    tag_info{dicom_tag{0x0008, 0x1084}, vr(VR::SQ), vm_1, "AdmittingDiagnosesCodeSequence", "Admitting Diagnoses Code Sequence", false},
    tag_info{dicom_tag{0x0008, 0x1090}, vr(VR::LO), vm_1, "ManufacturerModelName", "Manufacturer's Model Name", false},
    tag_info{dicom_tag{0x0008, 0x1110}, vr(VR::SQ), vm_1, "ReferencedStudySequence", "Referenced Study Sequence", false},
    tag_info{dicom_tag{0x0008, 0x1111}, vr(VR::SQ), vm_1, "ReferencedPerformedProcedureStepSequence", "Referenced Performed Procedure Step Sequence", false},
    tag_info{dicom_tag{0x0008, 0x1115}, vr(VR::SQ), vm_1, "ReferencedSeriesSequence", "Referenced Series Sequence", false},
    tag_info{dicom_tag{0x0008, 0x1120}, vr(VR::SQ), vm_1, "ReferencedPatientSequence", "Referenced Patient Sequence", false},
    tag_info{dicom_tag{0x0008, 0x1125}, vr(VR::SQ), vm_1, "ReferencedVisitSequence", "Referenced Visit Sequence", false},
    tag_info{dicom_tag{0x0008, 0x1140}, vr(VR::SQ), vm_1, "ReferencedImageSequence", "Referenced Image Sequence", false},
    tag_info{dicom_tag{0x0008, 0x1150}, vr(VR::UI), vm_1, "ReferencedSOPClassUID", "Referenced SOP Class UID", false},
    tag_info{dicom_tag{0x0008, 0x1155}, vr(VR::UI), vm_1, "ReferencedSOPInstanceUID", "Referenced SOP Instance UID", false},
    tag_info{dicom_tag{0x0008, 0x2111}, vr(VR::ST), vm_1, "DerivationDescription", "Derivation Description", false},
    tag_info{dicom_tag{0x0008, 0x2112}, vr(VR::SQ), vm_1, "SourceImageSequence", "Source Image Sequence", false},

    // ========================================================================
    // Patient Module (0x0010)
    // ========================================================================
    tag_info{dicom_tag{0x0010, 0x0010}, vr(VR::PN), vm_1, "PatientName", "Patient's Name", false},
    tag_info{dicom_tag{0x0010, 0x0020}, vr(VR::LO), vm_1, "PatientID", "Patient ID", false},
    tag_info{dicom_tag{0x0010, 0x0021}, vr(VR::LO), vm_1, "IssuerOfPatientID", "Issuer of Patient ID", false},
    tag_info{dicom_tag{0x0010, 0x0022}, vr(VR::CS), vm_1, "TypeOfPatientID", "Type of Patient ID", false},
    tag_info{dicom_tag{0x0010, 0x0024}, vr(VR::SQ), vm_1, "IssuerOfPatientIDQualifiersSequence", "Issuer of Patient ID Qualifiers Sequence", false},
    tag_info{dicom_tag{0x0010, 0x0030}, vr(VR::DA), vm_1, "PatientBirthDate", "Patient's Birth Date", false},
    tag_info{dicom_tag{0x0010, 0x0032}, vr(VR::TM), vm_1, "PatientBirthTime", "Patient's Birth Time", false},
    tag_info{dicom_tag{0x0010, 0x0040}, vr(VR::CS), vm_1, "PatientSex", "Patient's Sex", false},
    tag_info{dicom_tag{0x0010, 0x0050}, vr(VR::SQ), vm_1, "PatientInsurancePlanCodeSequence", "Patient's Insurance Plan Code Sequence", false},
    tag_info{dicom_tag{0x0010, 0x0101}, vr(VR::SQ), vm_1, "PatientPrimaryLanguageCodeSequence", "Patient's Primary Language Code Sequence", false},
    tag_info{dicom_tag{0x0010, 0x0102}, vr(VR::SQ), vm_1, "PatientPrimaryLanguageModifierCodeSequence", "Patient's Primary Language Modifier Code Sequence", false},
    tag_info{dicom_tag{0x0010, 0x1000}, vr(VR::LO), vm_1_n, "OtherPatientIDs", "Other Patient IDs", true},  // Retired
    tag_info{dicom_tag{0x0010, 0x1001}, vr(VR::PN), vm_1_n, "OtherPatientNames", "Other Patient Names", false},
    tag_info{dicom_tag{0x0010, 0x1002}, vr(VR::SQ), vm_1, "OtherPatientIDsSequence", "Other Patient IDs Sequence", false},
    tag_info{dicom_tag{0x0010, 0x1005}, vr(VR::PN), vm_1, "PatientBirthName", "Patient's Birth Name", false},
    tag_info{dicom_tag{0x0010, 0x1010}, vr(VR::AS), vm_1, "PatientAge", "Patient's Age", false},
    tag_info{dicom_tag{0x0010, 0x1020}, vr(VR::DS), vm_1, "PatientSize", "Patient's Size", false},
    tag_info{dicom_tag{0x0010, 0x1030}, vr(VR::DS), vm_1, "PatientWeight", "Patient's Weight", false},
    tag_info{dicom_tag{0x0010, 0x1040}, vr(VR::LO), vm_1, "PatientAddress", "Patient's Address", false},
    tag_info{dicom_tag{0x0010, 0x2000}, vr(VR::LO), vm_1_n, "MedicalAlerts", "Medical Alerts", false},
    tag_info{dicom_tag{0x0010, 0x2110}, vr(VR::LO), vm_1_n, "Allergies", "Allergies", false},
    tag_info{dicom_tag{0x0010, 0x2150}, vr(VR::LO), vm_1, "CountryOfResidence", "Country of Residence", false},
    tag_info{dicom_tag{0x0010, 0x2152}, vr(VR::LO), vm_1, "RegionOfResidence", "Region of Residence", false},
    tag_info{dicom_tag{0x0010, 0x2154}, vr(VR::SH), vm_1_n, "PatientTelephoneNumbers", "Patient's Telephone Numbers", false},
    tag_info{dicom_tag{0x0010, 0x2160}, vr(VR::SH), vm_1, "EthnicGroup", "Ethnic Group", false},
    tag_info{dicom_tag{0x0010, 0x2180}, vr(VR::SH), vm_1, "Occupation", "Occupation", false},
    tag_info{dicom_tag{0x0010, 0x21A0}, vr(VR::CS), vm_1, "SmokingStatus", "Smoking Status", false},
    tag_info{dicom_tag{0x0010, 0x21B0}, vr(VR::LT), vm_1, "AdditionalPatientHistory", "Additional Patient History", false},
    tag_info{dicom_tag{0x0010, 0x21C0}, vr(VR::US), vm_1, "PregnancyStatus", "Pregnancy Status", false},
    tag_info{dicom_tag{0x0010, 0x21D0}, vr(VR::DA), vm_1, "LastMenstrualDate", "Last Menstrual Date", false},
    tag_info{dicom_tag{0x0010, 0x21F0}, vr(VR::LO), vm_1, "PatientReligiousPreference", "Patient's Religious Preference", false},
    tag_info{dicom_tag{0x0010, 0x4000}, vr(VR::LT), vm_1, "PatientComments", "Patient Comments", false},

    // ========================================================================
    // Study and Series Identification (0x0020)
    // ========================================================================
    tag_info{dicom_tag{0x0020, 0x000D}, vr(VR::UI), vm_1, "StudyInstanceUID", "Study Instance UID", false},
    tag_info{dicom_tag{0x0020, 0x000E}, vr(VR::UI), vm_1, "SeriesInstanceUID", "Series Instance UID", false},
    tag_info{dicom_tag{0x0020, 0x0010}, vr(VR::SH), vm_1, "StudyID", "Study ID", false},
    tag_info{dicom_tag{0x0020, 0x0011}, vr(VR::IS), vm_1, "SeriesNumber", "Series Number", false},
    tag_info{dicom_tag{0x0020, 0x0012}, vr(VR::IS), vm_1, "AcquisitionNumber", "Acquisition Number", false},
    tag_info{dicom_tag{0x0020, 0x0013}, vr(VR::IS), vm_1, "InstanceNumber", "Instance Number", false},
    tag_info{dicom_tag{0x0020, 0x0020}, vr(VR::CS), vm_2, "PatientOrientation", "Patient Orientation", false},
    tag_info{dicom_tag{0x0020, 0x0032}, vr(VR::DS), vm_3, "ImagePositionPatient", "Image Position (Patient)", false},
    tag_info{dicom_tag{0x0020, 0x0037}, vr(VR::DS), vm_6, "ImageOrientationPatient", "Image Orientation (Patient)", false},
    tag_info{dicom_tag{0x0020, 0x0052}, vr(VR::UI), vm_1, "FrameOfReferenceUID", "Frame of Reference UID", false},
    tag_info{dicom_tag{0x0020, 0x0060}, vr(VR::CS), vm_1, "Laterality", "Laterality", false},
    tag_info{dicom_tag{0x0020, 0x0062}, vr(VR::CS), vm_1, "ImageLaterality", "Image Laterality", false},
    tag_info{dicom_tag{0x0020, 0x0100}, vr(VR::IS), vm_1, "TemporalPositionIdentifier", "Temporal Position Identifier", false},
    tag_info{dicom_tag{0x0020, 0x0105}, vr(VR::IS), vm_1, "NumberOfTemporalPositions", "Number of Temporal Positions", false},
    tag_info{dicom_tag{0x0020, 0x0110}, vr(VR::DS), vm_1, "TemporalResolution", "Temporal Resolution", false},
    tag_info{dicom_tag{0x0020, 0x0200}, vr(VR::UI), vm_1, "SynchronizationFrameOfReferenceUID", "Synchronization Frame of Reference UID", false},
    tag_info{dicom_tag{0x0020, 0x1040}, vr(VR::LO), vm_1, "PositionReferenceIndicator", "Position Reference Indicator", false},
    tag_info{dicom_tag{0x0020, 0x1041}, vr(VR::DS), vm_1, "SliceLocation", "Slice Location", false},
    tag_info{dicom_tag{0x0020, 0x1200}, vr(VR::IS), vm_1, "NumberOfPatientRelatedStudies", "Number of Patient Related Studies", false},
    tag_info{dicom_tag{0x0020, 0x1202}, vr(VR::IS), vm_1, "NumberOfPatientRelatedSeries", "Number of Patient Related Series", false},
    tag_info{dicom_tag{0x0020, 0x1204}, vr(VR::IS), vm_1, "NumberOfPatientRelatedInstances", "Number of Patient Related Instances", false},
    tag_info{dicom_tag{0x0020, 0x1206}, vr(VR::IS), vm_1, "NumberOfStudyRelatedSeries", "Number of Study Related Series", false},
    tag_info{dicom_tag{0x0020, 0x1208}, vr(VR::IS), vm_1, "NumberOfStudyRelatedInstances", "Number of Study Related Instances", false},
    tag_info{dicom_tag{0x0020, 0x1209}, vr(VR::IS), vm_1, "NumberOfSeriesRelatedInstances", "Number of Series Related Instances", false},
    tag_info{dicom_tag{0x0020, 0x4000}, vr(VR::LT), vm_1, "ImageComments", "Image Comments", false},

    // ========================================================================
    // Image Pixel Module (0x0028)
    // ========================================================================
    tag_info{dicom_tag{0x0028, 0x0002}, vr(VR::US), vm_1, "SamplesPerPixel", "Samples per Pixel", false},
    tag_info{dicom_tag{0x0028, 0x0003}, vr(VR::US), vm_1, "SamplesPerPixelUsed", "Samples per Pixel Used", false},
    tag_info{dicom_tag{0x0028, 0x0004}, vr(VR::CS), vm_1, "PhotometricInterpretation", "Photometric Interpretation", false},
    tag_info{dicom_tag{0x0028, 0x0006}, vr(VR::US), vm_1, "PlanarConfiguration", "Planar Configuration", false},
    tag_info{dicom_tag{0x0028, 0x0008}, vr(VR::IS), vm_1, "NumberOfFrames", "Number of Frames", false},
    tag_info{dicom_tag{0x0028, 0x0009}, vr(VR::AT), vm_1_n, "FrameIncrementPointer", "Frame Increment Pointer", false},
    tag_info{dicom_tag{0x0028, 0x0010}, vr(VR::US), vm_1, "Rows", "Rows", false},
    tag_info{dicom_tag{0x0028, 0x0011}, vr(VR::US), vm_1, "Columns", "Columns", false},
    tag_info{dicom_tag{0x0028, 0x0030}, vr(VR::DS), vm_2, "PixelSpacing", "Pixel Spacing", false},
    tag_info{dicom_tag{0x0028, 0x0034}, vr(VR::IS), vm_2, "PixelAspectRatio", "Pixel Aspect Ratio", false},
    tag_info{dicom_tag{0x0028, 0x0100}, vr(VR::US), vm_1, "BitsAllocated", "Bits Allocated", false},
    tag_info{dicom_tag{0x0028, 0x0101}, vr(VR::US), vm_1, "BitsStored", "Bits Stored", false},
    tag_info{dicom_tag{0x0028, 0x0102}, vr(VR::US), vm_1, "HighBit", "High Bit", false},
    tag_info{dicom_tag{0x0028, 0x0103}, vr(VR::US), vm_1, "PixelRepresentation", "Pixel Representation", false},
    tag_info{dicom_tag{0x0028, 0x0106}, vr(VR::US), vm_1, "SmallestImagePixelValue", "Smallest Image Pixel Value", false},
    tag_info{dicom_tag{0x0028, 0x0107}, vr(VR::US), vm_1, "LargestImagePixelValue", "Largest Image Pixel Value", false},
    tag_info{dicom_tag{0x0028, 0x0108}, vr(VR::US), vm_1, "SmallestPixelValueInSeries", "Smallest Pixel Value in Series", false},
    tag_info{dicom_tag{0x0028, 0x0109}, vr(VR::US), vm_1, "LargestPixelValueInSeries", "Largest Pixel Value in Series", false},
    tag_info{dicom_tag{0x0028, 0x0120}, vr(VR::US), vm_1, "PixelPaddingValue", "Pixel Padding Value", false},
    tag_info{dicom_tag{0x0028, 0x0121}, vr(VR::US), vm_1, "PixelPaddingRangeLimit", "Pixel Padding Range Limit", false},
    tag_info{dicom_tag{0x0028, 0x0300}, vr(VR::CS), vm_1, "QualityControlImage", "Quality Control Image", false},
    tag_info{dicom_tag{0x0028, 0x0301}, vr(VR::CS), vm_1, "BurnedInAnnotation", "Burned In Annotation", false},
    tag_info{dicom_tag{0x0028, 0x1050}, vr(VR::DS), vm_1_n, "WindowCenter", "Window Center", false},
    tag_info{dicom_tag{0x0028, 0x1051}, vr(VR::DS), vm_1_n, "WindowWidth", "Window Width", false},
    tag_info{dicom_tag{0x0028, 0x1052}, vr(VR::DS), vm_1, "RescaleIntercept", "Rescale Intercept", false},
    tag_info{dicom_tag{0x0028, 0x1053}, vr(VR::DS), vm_1, "RescaleSlope", "Rescale Slope", false},
    tag_info{dicom_tag{0x0028, 0x1054}, vr(VR::LO), vm_1, "RescaleType", "Rescale Type", false},
    tag_info{dicom_tag{0x0028, 0x1055}, vr(VR::LO), vm_1_n, "WindowCenterWidthExplanation", "Window Center & Width Explanation", false},
    tag_info{dicom_tag{0x0028, 0x1056}, vr(VR::CS), vm_1, "VOILUTFunction", "VOI LUT Function", false},
    tag_info{dicom_tag{0x0028, 0x1101}, vr(VR::US), vm_3, "RedPaletteColorLookupTableDescriptor", "Red Palette Color Lookup Table Descriptor", false},
    tag_info{dicom_tag{0x0028, 0x1102}, vr(VR::US), vm_3, "GreenPaletteColorLookupTableDescriptor", "Green Palette Color Lookup Table Descriptor", false},
    tag_info{dicom_tag{0x0028, 0x1103}, vr(VR::US), vm_3, "BluePaletteColorLookupTableDescriptor", "Blue Palette Color Lookup Table Descriptor", false},
    tag_info{dicom_tag{0x0028, 0x1199}, vr(VR::UI), vm_1, "PaletteColorLookupTableUID", "Palette Color Lookup Table UID", false},
    tag_info{dicom_tag{0x0028, 0x1201}, vr(VR::OW), vm_1, "RedPaletteColorLookupTableData", "Red Palette Color Lookup Table Data", false},
    tag_info{dicom_tag{0x0028, 0x1202}, vr(VR::OW), vm_1, "GreenPaletteColorLookupTableData", "Green Palette Color Lookup Table Data", false},
    tag_info{dicom_tag{0x0028, 0x1203}, vr(VR::OW), vm_1, "BluePaletteColorLookupTableData", "Blue Palette Color Lookup Table Data", false},
    tag_info{dicom_tag{0x0028, 0x2110}, vr(VR::CS), vm_1, "LossyImageCompression", "Lossy Image Compression", false},
    tag_info{dicom_tag{0x0028, 0x2112}, vr(VR::DS), vm_1_n, "LossyImageCompressionRatio", "Lossy Image Compression Ratio", false},
    tag_info{dicom_tag{0x0028, 0x2114}, vr(VR::CS), vm_1_n, "LossyImageCompressionMethod", "Lossy Image Compression Method", false},
    tag_info{dicom_tag{0x0028, 0x3000}, vr(VR::SQ), vm_1, "ModalityLUTSequence", "Modality LUT Sequence", false},
    tag_info{dicom_tag{0x0028, 0x3010}, vr(VR::SQ), vm_1, "VOILUTSequence", "VOI LUT Sequence", false},

    // ========================================================================
    // Scheduled Procedure Step (0x0040)
    // ========================================================================
    tag_info{dicom_tag{0x0040, 0x0001}, vr(VR::AE), vm_1_n, "ScheduledStationAETitle", "Scheduled Station AE Title", false},
    tag_info{dicom_tag{0x0040, 0x0002}, vr(VR::DA), vm_1, "ScheduledProcedureStepStartDate", "Scheduled Procedure Step Start Date", false},
    tag_info{dicom_tag{0x0040, 0x0003}, vr(VR::TM), vm_1, "ScheduledProcedureStepStartTime", "Scheduled Procedure Step Start Time", false},
    tag_info{dicom_tag{0x0040, 0x0004}, vr(VR::DA), vm_1, "ScheduledProcedureStepEndDate", "Scheduled Procedure Step End Date", false},
    tag_info{dicom_tag{0x0040, 0x0005}, vr(VR::TM), vm_1, "ScheduledProcedureStepEndTime", "Scheduled Procedure Step End Time", false},
    tag_info{dicom_tag{0x0040, 0x0006}, vr(VR::PN), vm_1, "ScheduledPerformingPhysicianName", "Scheduled Performing Physician's Name", false},
    tag_info{dicom_tag{0x0040, 0x0007}, vr(VR::LO), vm_1, "ScheduledProcedureStepDescription", "Scheduled Procedure Step Description", false},
    tag_info{dicom_tag{0x0040, 0x0008}, vr(VR::SQ), vm_1, "ScheduledProtocolCodeSequence", "Scheduled Protocol Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0x0009}, vr(VR::SH), vm_1, "ScheduledProcedureStepID", "Scheduled Procedure Step ID", false},
    tag_info{dicom_tag{0x0040, 0x000A}, vr(VR::SQ), vm_1, "StageCodeSequence", "Stage Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0x000B}, vr(VR::SQ), vm_1, "ScheduledPerformingPhysicianIdentificationSequence", "Scheduled Performing Physician Identification Sequence", false},
    tag_info{dicom_tag{0x0040, 0x0010}, vr(VR::SH), vm_1_n, "ScheduledStationName", "Scheduled Station Name", false},
    tag_info{dicom_tag{0x0040, 0x0011}, vr(VR::SH), vm_1, "ScheduledProcedureStepLocation", "Scheduled Procedure Step Location", false},
    tag_info{dicom_tag{0x0040, 0x0012}, vr(VR::LO), vm_1, "PreMedication", "Pre-Medication", false},
    tag_info{dicom_tag{0x0040, 0x0020}, vr(VR::CS), vm_1, "ScheduledProcedureStepStatus", "Scheduled Procedure Step Status", false},
    tag_info{dicom_tag{0x0040, 0x0100}, vr(VR::SQ), vm_1, "ScheduledProcedureStepSequence", "Scheduled Procedure Step Sequence", false},
    tag_info{dicom_tag{0x0040, 0x0220}, vr(VR::SQ), vm_1, "ReferencedNonImageCompositeSOPInstanceSequence", "Referenced Non-Image Composite SOP Instance Sequence", false},
    tag_info{dicom_tag{0x0040, 0x0241}, vr(VR::AE), vm_1, "PerformedStationAETitle", "Performed Station AE Title", false},
    tag_info{dicom_tag{0x0040, 0x0242}, vr(VR::SH), vm_1, "PerformedStationName", "Performed Station Name", false},
    tag_info{dicom_tag{0x0040, 0x0243}, vr(VR::SH), vm_1, "PerformedLocation", "Performed Location", false},
    tag_info{dicom_tag{0x0040, 0x0244}, vr(VR::DA), vm_1, "PerformedProcedureStepStartDate", "Performed Procedure Step Start Date", false},
    tag_info{dicom_tag{0x0040, 0x0245}, vr(VR::TM), vm_1, "PerformedProcedureStepStartTime", "Performed Procedure Step Start Time", false},
    tag_info{dicom_tag{0x0040, 0x0250}, vr(VR::DA), vm_1, "PerformedProcedureStepEndDate", "Performed Procedure Step End Date", false},
    tag_info{dicom_tag{0x0040, 0x0251}, vr(VR::TM), vm_1, "PerformedProcedureStepEndTime", "Performed Procedure Step End Time", false},
    tag_info{dicom_tag{0x0040, 0x0252}, vr(VR::CS), vm_1, "PerformedProcedureStepStatus", "Performed Procedure Step Status", false},
    tag_info{dicom_tag{0x0040, 0x0253}, vr(VR::SH), vm_1, "PerformedProcedureStepID", "Performed Procedure Step ID", false},
    tag_info{dicom_tag{0x0040, 0x0254}, vr(VR::LO), vm_1, "PerformedProcedureStepDescription", "Performed Procedure Step Description", false},
    tag_info{dicom_tag{0x0040, 0x0255}, vr(VR::LO), vm_1, "PerformedProcedureTypeDescription", "Performed Procedure Type Description", false},
    tag_info{dicom_tag{0x0040, 0x0260}, vr(VR::SQ), vm_1, "PerformedProtocolCodeSequence", "Performed Protocol Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0x0270}, vr(VR::SQ), vm_1, "ScheduledStepAttributesSequence", "Scheduled Step Attributes Sequence", false},
    tag_info{dicom_tag{0x0040, 0x0275}, vr(VR::SQ), vm_1, "RequestAttributesSequence", "Request Attributes Sequence", false},
    tag_info{dicom_tag{0x0040, 0x0280}, vr(VR::ST), vm_1, "CommentsOnThePerformedProcedureStep", "Comments on the Performed Procedure Step", false},
    tag_info{dicom_tag{0x0040, 0x0340}, vr(VR::SQ), vm_1, "PerformedSeriesSequence", "Performed Series Sequence", false},
    tag_info{dicom_tag{0x0040, 0x1001}, vr(VR::SH), vm_1, "RequestedProcedureID", "Requested Procedure ID", false},
    tag_info{dicom_tag{0x0040, 0x1002}, vr(VR::LO), vm_1, "ReasonForTheRequestedProcedure", "Reason for the Requested Procedure", false},
    tag_info{dicom_tag{0x0040, 0x1003}, vr(VR::SH), vm_1, "RequestedProcedurePriority", "Requested Procedure Priority", false},
    tag_info{dicom_tag{0x0040, 0x1004}, vr(VR::LO), vm_1, "PatientTransportArrangements", "Patient Transport Arrangements", false},
    tag_info{dicom_tag{0x0040, 0x1005}, vr(VR::LO), vm_1, "RequestedProcedureLocation", "Requested Procedure Location", false},
    tag_info{dicom_tag{0x0040, 0x1008}, vr(VR::LO), vm_1, "ConfidentialityCode", "Confidentiality Code", false},
    tag_info{dicom_tag{0x0040, 0x1009}, vr(VR::SH), vm_1, "ReportingPriority", "Reporting Priority", false},
    tag_info{dicom_tag{0x0040, 0x100A}, vr(VR::SQ), vm_1, "ReasonForRequestedProcedureCodeSequence", "Reason for Requested Procedure Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0x1010}, vr(VR::PN), vm_1_n, "NamesOfIntendedRecipientsOfResults", "Names of Intended Recipients of Results", false},
    tag_info{dicom_tag{0x0040, 0x1011}, vr(VR::SQ), vm_1, "IntendedRecipientsOfResultsIdentificationSequence", "Intended Recipients of Results Identification Sequence", false},
    tag_info{dicom_tag{0x0040, 0x1012}, vr(VR::SQ), vm_1, "ReasonForPerformedProcedureCodeSequence", "Reason For Performed Procedure Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0x2001}, vr(VR::LO), vm_1, "ReasonForTheImagingServiceRequest", "Reason for the Imaging Service Request", true},  // Retired
    tag_info{dicom_tag{0x0040, 0x2004}, vr(VR::DA), vm_1, "IssueDateOfImagingServiceRequest", "Issue Date of Imaging Service Request", false},
    tag_info{dicom_tag{0x0040, 0x2005}, vr(VR::TM), vm_1, "IssueTimeOfImagingServiceRequest", "Issue Time of Imaging Service Request", false},
    tag_info{dicom_tag{0x0040, 0x2008}, vr(VR::PN), vm_1, "OrderEnteredBy", "Order Entered By", false},
    tag_info{dicom_tag{0x0040, 0x2009}, vr(VR::SH), vm_1, "OrderEntererLocation", "Order Enterer's Location", false},
    tag_info{dicom_tag{0x0040, 0x2010}, vr(VR::SH), vm_1, "OrderCallbackPhoneNumber", "Order Callback Phone Number", false},
    tag_info{dicom_tag{0x0040, 0x2016}, vr(VR::LO), vm_1, "PlacerOrderNumberImagingServiceRequest", "Placer Order Number / Imaging Service Request", false},
    tag_info{dicom_tag{0x0040, 0x2017}, vr(VR::LO), vm_1, "FillerOrderNumberImagingServiceRequest", "Filler Order Number / Imaging Service Request", false},
    tag_info{dicom_tag{0x0040, 0x2400}, vr(VR::LT), vm_1, "ImagingServiceRequestComments", "Imaging Service Request Comments", false},
    tag_info{dicom_tag{0x0040, 0x3001}, vr(VR::LO), vm_1, "ConfidentialityConstraintOnPatientDataDescription", "Confidentiality Constraint on Patient Data Description", false},
    tag_info{dicom_tag{0x0040, 0xA010}, vr(VR::CS), vm_1, "RelationshipType", "Relationship Type", false},
    tag_info{dicom_tag{0x0040, 0xA027}, vr(VR::LO), vm_1, "VerifyingOrganization", "Verifying Organization", false},
    tag_info{dicom_tag{0x0040, 0xA030}, vr(VR::DT), vm_1, "VerificationDateTime", "Verification Date Time", false},
    tag_info{dicom_tag{0x0040, 0xA032}, vr(VR::DT), vm_1, "ObservationDateTime", "Observation DateTime", false},
    tag_info{dicom_tag{0x0040, 0xA040}, vr(VR::CS), vm_1, "ValueType", "Value Type", false},
    tag_info{dicom_tag{0x0040, 0xA043}, vr(VR::SQ), vm_1, "ConceptNameCodeSequence", "Concept Name Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA050}, vr(VR::CS), vm_1, "ContinuityOfContent", "Continuity Of Content", false},
    tag_info{dicom_tag{0x0040, 0xA073}, vr(VR::SQ), vm_1, "VerifyingObserverSequence", "Verifying Observer Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA075}, vr(VR::PN), vm_1, "VerifyingObserverName", "Verifying Observer Name", false},
    tag_info{dicom_tag{0x0040, 0xA088}, vr(VR::SQ), vm_1, "VerifyingObserverIdentificationCodeSequence", "Verifying Observer Identification Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA120}, vr(VR::DT), vm_1, "DateTime", "DateTime", false},
    tag_info{dicom_tag{0x0040, 0xA121}, vr(VR::DA), vm_1, "Date", "Date", false},
    tag_info{dicom_tag{0x0040, 0xA122}, vr(VR::TM), vm_1, "Time", "Time", false},
    tag_info{dicom_tag{0x0040, 0xA123}, vr(VR::PN), vm_1, "PersonName", "Person Name", false},
    tag_info{dicom_tag{0x0040, 0xA124}, vr(VR::UI), vm_1, "UID", "UID", false},
    tag_info{dicom_tag{0x0040, 0xA130}, vr(VR::CS), vm_1, "TemporalRangeType", "Temporal Range Type", false},
    tag_info{dicom_tag{0x0040, 0xA132}, vr(VR::UL), vm_1_n, "ReferencedSamplePositions", "Referenced Sample Positions", false},
    tag_info{dicom_tag{0x0040, 0xA136}, vr(VR::US), vm_1_n, "ReferencedFrameNumbers", "Referenced Frame Numbers", false},
    tag_info{dicom_tag{0x0040, 0xA138}, vr(VR::DS), vm_1_n, "ReferencedTimeOffsets", "Referenced Time Offsets", false},
    tag_info{dicom_tag{0x0040, 0xA13A}, vr(VR::DT), vm_1_n, "ReferencedDateTime", "Referenced DateTime", false},
    tag_info{dicom_tag{0x0040, 0xA160}, vr(VR::UT), vm_1, "TextValue", "Text Value", false},
    tag_info{dicom_tag{0x0040, 0xA168}, vr(VR::SQ), vm_1, "ConceptCodeSequence", "Concept Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA170}, vr(VR::SQ), vm_1, "PurposeOfReferenceCodeSequence", "Purpose of Reference Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA180}, vr(VR::US), vm_1, "AnnotationGroupNumber", "Annotation Group Number", false},
    tag_info{dicom_tag{0x0040, 0xA195}, vr(VR::SQ), vm_1, "ModifierCodeSequence", "Modifier Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA300}, vr(VR::SQ), vm_1, "MeasuredValueSequence", "Measured Value Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA30A}, vr(VR::DS), vm_1_n, "NumericValue", "Numeric Value", false},
    tag_info{dicom_tag{0x0040, 0xA360}, vr(VR::SQ), vm_1, "PredecessorDocumentsSequence", "Predecessor Documents Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA370}, vr(VR::SQ), vm_1, "ReferencedRequestSequence", "Referenced Request Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA372}, vr(VR::SQ), vm_1, "PerformedProcedureCodeSequence", "Performed Procedure Code Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA375}, vr(VR::SQ), vm_1, "CurrentRequestedProcedureEvidenceSequence", "Current Requested Procedure Evidence Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA385}, vr(VR::SQ), vm_1, "PertinentOtherEvidenceSequence", "Pertinent Other Evidence Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA390}, vr(VR::SQ), vm_1, "HL7StructuredDocumentReferenceSequence", "HL7 Structured Document Reference Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA491}, vr(VR::CS), vm_1, "CompletionFlag", "Completion Flag", false},
    tag_info{dicom_tag{0x0040, 0xA492}, vr(VR::LO), vm_1, "CompletionFlagDescription", "Completion Flag Description", false},
    tag_info{dicom_tag{0x0040, 0xA493}, vr(VR::CS), vm_1, "VerificationFlag", "Verification Flag", false},
    tag_info{dicom_tag{0x0040, 0xA504}, vr(VR::SQ), vm_1, "ContentTemplateSequence", "Content Template Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA525}, vr(VR::SQ), vm_1, "IdenticalDocumentsSequence", "Identical Documents Sequence", false},
    tag_info{dicom_tag{0x0040, 0xA730}, vr(VR::SQ), vm_1, "ContentSequence", "Content Sequence", false},

    // ========================================================================
    // Device Information (0x0050)
    // ========================================================================
    tag_info{dicom_tag{0x0050, 0x0004}, vr(VR::CS), vm_1, "CalibrationImage", "Calibration Image", false},
    tag_info{dicom_tag{0x0050, 0x0010}, vr(VR::SQ), vm_1, "DeviceSequence", "Device Sequence", false},
    tag_info{dicom_tag{0x0050, 0x0014}, vr(VR::DS), vm_1, "DeviceLength", "Device Length", false},
    tag_info{dicom_tag{0x0050, 0x0016}, vr(VR::DS), vm_1, "DeviceDiameter", "Device Diameter", false},
    tag_info{dicom_tag{0x0050, 0x0017}, vr(VR::CS), vm_1, "DeviceDiameterUnits", "Device Diameter Units", false},
    tag_info{dicom_tag{0x0050, 0x0018}, vr(VR::DS), vm_1, "DeviceVolume", "Device Volume", false},
    tag_info{dicom_tag{0x0050, 0x0019}, vr(VR::DS), vm_1, "InterMarkerDistance", "Inter-Marker Distance", false},
    tag_info{dicom_tag{0x0050, 0x0020}, vr(VR::LO), vm_1, "DeviceDescription", "Device Description", false},

    // ========================================================================
    // Pixel Data (0x7FE0)
    // ========================================================================
    tag_info{dicom_tag{0x7FE0, 0x0010}, vr(VR::OW), vm_1, "PixelData", "Pixel Data", false},
    tag_info{dicom_tag{0x7FE0, 0x0020}, vr(VR::OW), vm_1, "CoefficientsSDVN", "Coefficients SDVN", true},  // Retired
    tag_info{dicom_tag{0x7FE0, 0x0030}, vr(VR::OW), vm_1, "CoefficientsSDHN", "Coefficients SDHN", true},  // Retired
    tag_info{dicom_tag{0x7FE0, 0x0040}, vr(VR::OW), vm_1, "CoefficientsSDDN", "Coefficients SDDN", true},  // Retired

    // ========================================================================
    // Item Delimiters (0xFFFE)
    // ========================================================================
    tag_info{dicom_tag{0xFFFE, 0xE000}, vr(VR::UN), vm_1, "Item", "Item", false},
    tag_info{dicom_tag{0xFFFE, 0xE00D}, vr(VR::UN), vm_1, "ItemDelimitationItem", "Item Delimitation Item", false},
    tag_info{dicom_tag{0xFFFE, 0xE0DD}, vr(VR::UN), vm_1, "SequenceDelimitationItem", "Sequence Delimitation Item", false},
};
// clang-format on

auto get_standard_tags() -> std::span<const tag_info> {
    return standard_tags;
}

}  // namespace pacs::core
