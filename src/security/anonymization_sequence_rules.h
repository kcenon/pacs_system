// BSD 3-Clause License. See LICENSE.
#pragma once

#include "kcenon/pacs/security/anonymization_profile.h"
#include "kcenon/pacs/security/tag_action.h"
#include <map>

namespace kcenon::pacs::security::detail {

// PS3.15 Table E.1-1, retrieved 2026-11-01:
// https://dicom.nema.org/medical/dicom/current/output/chtml/part15/chapter_E.html
// This is the sequence/instance-UID subset, not a complete conformance table.
// Without IOD type information, X/Z and X/Z/D select Z, X/D selects X,
// and image-reference X/Z/U* selects U to retain remapped references.
inline void add_sequence_and_uid_actions(
    std::map<core::dicom_tag, tag_action_config>& actions,
    anonymization_profile profile) {
    struct sequence_rule {
        core::dicom_tag tag;
        tag_action basic;
        bool clean_descriptions;
    };
    static constexpr sequence_rule sequences[] = {
        {{0x0040, 0x0555}, tag_action::empty, false}, // Acquisition Context Sequence: X/Z
        {{0x0040, 0x4035}, tag_action::remove, false}, // Actual Human Performers Sequence: X
        {{0x0008, 0x1084}, tag_action::remove, true}, // Admitting Diagnoses Code Sequence: X
        {{0x0040, 0xA078}, tag_action::remove, false}, // Author Observer Sequence: X
        {{0x0008, 0x009D}, tag_action::remove, false}, // Consulting Physician Identification Sequence: X
        {{0x0070, 0x0086}, tag_action::remove, false}, // Content Creator's Identification Code Sequence: X
        {{0x0040, 0xA730}, tag_action::replace, false}, // Content Sequence: D
        {{0x0040, 0xA07C}, tag_action::remove, false}, // Custodial Organization Sequence: X
        {{0xFFFA, 0xFFFA}, tag_action::remove, false}, // Digital Signatures Sequence: X
        {{0x0010, 0x2161}, tag_action::remove, false}, // Ethnic Group Code Sequence: X
        {{0x0034, 0x0001}, tag_action::replace, false}, // Flow Identifier Sequence: D
        {{0x0010, 0x0044}, tag_action::remove, false}, // Gender Identity Code Sequence: X
        {{0x0010, 0x0041}, tag_action::remove, false}, // Gender Identity Sequence: X
        {{0x0070, 0x0001}, tag_action::replace, false}, // Graphic Annotation Sequence: D
        {{0x0008, 0x1304}, tag_action::remove, true}, // Histological Diagnoses Code Sequence: X
        {{0x0088, 0x0200}, tag_action::remove, false}, // Icon Image Sequence (see Note 11): X
        {{0x0008, 0x1041}, tag_action::remove, false}, // Institutional Department Type Code Sequence: X
        {{0x0008, 0x0082}, tag_action::empty, false}, // Institution Code Sequence: X/Z/D
        {{0x0040, 0x1011}, tag_action::remove, false}, // Intended Recipients of Results Identification Sequence: X
        {{0x4008, 0x0111}, tag_action::remove, false}, // Interpretation Approver Sequence: X
        {{0x0038, 0x0014}, tag_action::remove, false}, // Issuer of Admission ID Sequence: X
        {{0x0038, 0x0064}, tag_action::remove, false}, // Issuer of Service Episode ID Sequence: X
        {{0x0040, 0x0513}, tag_action::empty, false}, // Issuer of the Container Identifier Sequence: Z
        {{0x0040, 0x0562}, tag_action::empty, false}, // Issuer of the Specimen Identifier Sequence: Z
        {{0x0400, 0x0550}, tag_action::remove, false}, // Modified Attributes Sequence: X
        {{0x0400, 0x0551}, tag_action::remove, false}, // Nonconforming Modified Attributes Sequence: X
        {{0x0008, 0x1072}, tag_action::remove, false}, // Operator Identification Sequence: X/D
        {{0x0400, 0x0561}, tag_action::remove, false}, // Original Attributes Sequence: X
        {{0x0012, 0x0023}, tag_action::remove, false}, // Other Clinical Trial Protocol IDs Sequence: X
        {{0x0010, 0x1002}, tag_action::remove, false}, // Other Patient IDs Sequence: X
        {{0x0040, 0xA07A}, tag_action::remove, false}, // Participant Sequence: X
        {{0x0010, 0x0050}, tag_action::remove, false}, // Patient's Insurance Plan Code Sequence: X
        {{0x0010, 0x0101}, tag_action::remove, false}, // Patient's Primary Language Code Sequence: X
        {{0x0010, 0x0102}, tag_action::remove, false}, // Patient's Primary Language Modifier Code Sequence: X
        {{0x0040, 0x4030}, tag_action::remove, false}, // Performed Station Geographic Location Code Sequence: X
        {{0x0040, 0x4028}, tag_action::remove, false}, // Performed Station Name Code Sequence: X
        {{0x0008, 0x1052}, tag_action::remove, false}, // Performing Physician Identification Sequence: X
        {{0x0040, 0x1101}, tag_action::replace, false}, // Person Identification Code Sequence: D
        {{0x0010, 0x0011}, tag_action::remove, false}, // Person Names to Use Sequence: X
        {{0x0008, 0x1049}, tag_action::remove, false}, // Physician(s) of Record Identification Sequence: X
        {{0x0008, 0x1062}, tag_action::remove, false}, // Physician(s) Reading Study Identification Sequence: X
        {{0x3010, 0x0081}, tag_action::empty, true}, // Prescription Notes Sequence: Z
        {{0x0008, 0x1302}, tag_action::remove, true}, // Primary Diagnosis Code Sequence: X
        {{0x0008, 0x1301}, tag_action::remove, true}, // Principal Diagnosis Code Sequence: X
        {{0x0010, 0x0015}, tag_action::remove, false}, // Pronoun Code Sequence: X
        {{0x0040, 0x100A}, tag_action::remove, true}, // Reason for Requested Procedure Code Sequence: X
        {{0x0032, 0x1067}, tag_action::remove, true}, // Reason for Visit Code Sequence: X
        {{0x0400, 0x0402}, tag_action::remove, false}, // Referenced Digital Signature Sequence: X
        {{0x0008, 0x1140}, tag_action::replace_uid, false}, // Referenced Image Sequence: X/Z/U*
        {{0x0038, 0x0004}, tag_action::remove, false}, // Referenced Patient Alias Sequence: X
        {{0x0010, 0x1100}, tag_action::remove, false}, // Referenced Patient Photo Sequence: X
        {{0x0008, 0x1120}, tag_action::remove, false}, // Referenced Patient Sequence: X
        {{0x0008, 0x1111}, tag_action::empty, false}, // Referenced Performed Procedure Step Sequence: X/Z/D
        {{0x0400, 0x0403}, tag_action::remove, false}, // Referenced SOP Instance MAC Sequence: X
        {{0x0008, 0x1110}, tag_action::empty, false}, // Referenced Study Sequence: X/Z
        {{0x0008, 0x0096}, tag_action::remove, false}, // Referring Physician Identification Sequence: X
        {{0x0040, 0x0275}, tag_action::remove, true}, // Request Attributes Sequence: X
        {{0x4008, 0x0118}, tag_action::remove, false}, // Results Distribution List Sequence: X
        {{0x3006, 0x004D}, tag_action::remove, false}, // ROI Creator Sequence: X
        {{0x3006, 0x004E}, tag_action::remove, false}, // ROI Interpreter Sequence: X
        {{0x0040, 0x4034}, tag_action::remove, false}, // Scheduled Human Performers Sequence: X
        {{0x0040, 0x000B}, tag_action::remove, false}, // Scheduled Performing Physician Identification Sequence: X
        {{0x0040, 0x4027}, tag_action::remove, false}, // Scheduled Station Geographic Location Code Sequence: X
        {{0x0040, 0x4025}, tag_action::remove, false}, // Scheduled Station Name Code Sequence: X
        {{0x0008, 0x1303}, tag_action::remove, true}, // Secondary Diagnoses Code Sequence: X
        {{0x0010, 0x0046}, tag_action::remove, false}, // Sex Parameters for Clinical Use Category Code Sequence: X
        {{0x0010, 0x0043}, tag_action::remove, false}, // Sex Parameters for Clinical Use Category Sequence: X
        {{0x0008, 0x2112}, tag_action::replace_uid, false}, // Source Image Sequence: X/Z/U*
        {{0x0040, 0x0610}, tag_action::empty, false}, // Specimen Preparation Sequence: Z
        {{0x0010, 0x0014}, tag_action::remove, false}, // Third Person Pronouns Sequence: X
        {{0x0018, 0x5011}, tag_action::remove, false}, // Transducer Identification Sequence: X
        {{0x0018, 0x100A}, tag_action::remove, false}, // UDI Sequence: X
        {{0x0040, 0xA358}, tag_action::remove, false}, // Verbal Source Identifier Code Sequence (Trial): X
        {{0x0040, 0xA088}, tag_action::empty, false}, // Verifying Observer Identification Code Sequence: Z
        {{0x0040, 0xA073}, tag_action::replace, false}, // Verifying Observer Sequence: D
        {{0x0040, 0xB020}, tag_action::remove, false}, // Waveform Annotation Sequence: X/D
    };
    for (const auto& rule : sequences) {
        actions[rule.tag] = {.action =
            profile == anonymization_profile::clean_descriptions && rule.clean_descriptions
                ? tag_action::clean : rule.basic};
    }

    // Explicit retention of clinical/reference containers not listed in E.1-1.
    // Their items still undergo the same recursive cleaning as any K sequence.
    for (const auto tag : {core::dicom_tag{0x0008, 0x1115}, // Referenced Series
                           core::dicom_tag{0x0008, 0x2218}, // Anatomic Region
                           core::dicom_tag{0x0008, 0x2220}, // Anatomic Region Modifier
                           core::dicom_tag{0x0008, 0x2228}, // Primary Anatomic Structure
                           core::dicom_tag{0x0008, 0x2230}}) {
        actions[tag] = tag_action_config::make_keep();
    }

    static constexpr core::dicom_tag instance_uids[] = {
        {0x0008, 0x0017}, // Acquisition UID
        {0x0020, 0x9161}, // Concatenation UID
        {0x3010, 0x0006}, // Conceptual Volume UID
        {0x3010, 0x0013}, // Constituent Conceptual Volume UID
        {0x0018, 0x1002}, // Device UID
        {0x0400, 0x0100}, // Digital Signature UID
        {0x0020, 0x9164}, // Dimension Organization UID
        {0x300A, 0x0013}, // Dose Reference UID
        {0x3010, 0x006E}, // Dosimetric Objective UID
        {0x0008, 0x0058}, // Failed SOP Instance UID List
        {0x0070, 0x031A}, // Fiducial UID
        {0x0020, 0x0052}, // Frame of Reference UID
        {0x0008, 0x0014}, // Instance Creator UID
        {0x0008, 0x3010}, // Irradiation Event UID
        {0x0028, 0x1214}, // Large Palette Color Lookup Table UID
        {0x0018, 0x100B}, // Manufacturer's Device Class UID
        {0x0002, 0x0003}, // Media Storage SOP Instance UID
        {0x003A, 0x0310}, // Multiplex Group UID
        {0x0040, 0xA402}, // Observation Subject UID (Trial)
        {0x0040, 0xA171}, // Observation UID
        {0x0028, 0x1199}, // Palette Color Lookup Table UID
        {0x300A, 0x0650}, // Patient Setup UID
        {0x0070, 0x1101}, // Presentation Display Collection UID
        {0x0070, 0x1102}, // Presentation Sequence Collection UID
        {0x0008, 0x0019}, // Pyramid UID
        {0x3010, 0x000B}, // Referenced Conceptual Volume UID
        {0x300A, 0x0083}, // Referenced Dose Reference UID
        {0x3010, 0x006F}, // Referenced Dosimetric Objective UID
        {0x3010, 0x0031}, // Referenced Fiducials UID
        {0x3006, 0x0024}, // Referenced Frame of Reference UID
        {0x0040, 0x4023}, // Referenced General Purpose Scheduled Procedure Step Transaction UID
        {0x0040, 0xA172}, // Referenced Observation UID (Trial)
        {0x0008, 0x1155}, // Referenced SOP Instance UID
        {0x0004, 0x1511}, // Referenced SOP Instance UID in File
        {0x300A, 0x0785}, // Referenced Treatment Position Group UID
        {0x3006, 0x00C2}, // Related Frame of Reference UID
        {0x0000, 0x1001}, // Requested SOP Instance UID
        {0x3010, 0x003B}, // RT Treatment Phase UID
        {0x0020, 0x000E}, // Series Instance UID
        {0x0008, 0x0018}, // SOP Instance UID
        {0x3010, 0x0015}, // Source Conceptual Volume UID
        {0x0064, 0x0003}, // Source Frame of Reference UID
        {0x0040, 0x0554}, // Specimen UID
        {0x0088, 0x0140}, // Storage Media File-set UID
        {0x0020, 0x000D}, // Study Instance UID
        {0x0020, 0x0200}, // Synchronization Frame of Reference UID
        {0x300A, 0x0054}, // Table Top Position Alignment UID
        {0x0018, 0x2042}, // Target UID
        {0x0040, 0xDB0D}, // Template Extension Creator UID
        {0x0040, 0xDB0C}, // Template Extension Organization UID
        {0x0062, 0x0021}, // Tracking UID
        {0x0008, 0x1195}, // Transaction UID
        {0x300A, 0x0609}, // Treatment Position Group UID
        {0x300A, 0x0700}, // Treatment Session UID
        {0x0040, 0xA124}, // UID
    };
    for (const auto tag : instance_uids) {
        actions[tag] = {.action = tag_action::replace_uid};
    }
}

} // namespace kcenon::pacs::security::detail
