/**
 * @file anonymization_profile.hpp
 * @brief DICOM de-identification profiles per PS3.15 Annex E
 *
 * This file defines anonymization profiles for DICOM data as specified
 * in DICOM PS3.15 (Security and System Management Profiles) Annex E.
 *
 * @see DICOM PS3.15 Annex E - Attribute Confidentiality Profiles
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pacs::security {

/**
 * @brief DICOM de-identification profiles based on PS3.15 Annex E
 *
 * Each profile defines a set of actions to be performed on specific
 * DICOM attributes to achieve varying levels of de-identification.
 *
 * Profile selection depends on:
 * - Regulatory requirements (HIPAA, GDPR)
 * - Use case (research, clinical trial, data sharing)
 * - Need to preserve clinical utility
 *
 * @see DICOM PS3.15 Annex E Table E.1-1
 */
enum class anonymization_profile : std::uint8_t {
    /**
     * @brief Basic Profile - Remove direct identifiers
     *
     * Removes or empties elements that directly identify the patient:
     * - Patient Name, ID, Birth Date, Address
     * - Accession Number, Institution Name
     * - UIDs are replaced with new values
     *
     * Suitable for: Basic de-identification needs
     */
    basic = 0,

    /**
     * @brief Clean Pixel Data - Remove burned-in annotations
     *
     * Extends basic profile by processing pixel data to remove
     * burned-in patient information in image corners.
     *
     * Suitable for: Images that may contain overlay text
     */
    clean_pixel = 1,

    /**
     * @brief Clean Descriptions - Sanitize text fields
     *
     * Extends basic profile by cleaning free-text fields that
     * may contain identifying information:
     * - Study/Series Description
     * - Patient Comments
     * - Additional Patient History
     *
     * Suitable for: Data with descriptive fields
     */
    clean_descriptions = 2,

    /**
     * @brief Retain Longitudinal - Preserve temporal relationships
     *
     * Maintains date relationships through date shifting rather
     * than zeroing, allowing longitudinal studies to be linked.
     * UIDs are consistently mapped across studies.
     *
     * Suitable for: Research requiring temporal analysis
     */
    retain_longitudinal = 3,

    /**
     * @brief Retain Patient Characteristics
     *
     * Preserves patient demographic information needed for
     * research while removing direct identifiers:
     * - Patient Sex, Age, Size, Weight
     *
     * Suitable for: Clinical research with demographics
     */
    retain_patient_characteristics = 4,

    /**
     * @brief HIPAA Safe Harbor - 18 identifier removal
     *
     * Implements HIPAA Safe Harbor method by removing all
     * 18 categories of identifiers specified in 45 CFR 164.514(b)(2):
     * - Names, addresses, dates (except year)
     * - Phone/fax numbers, email addresses
     * - SSN, medical record numbers, account numbers
     * - Certificate/license numbers, vehicle identifiers
     * - Device identifiers, web URLs, IP addresses
     * - Biometric identifiers, photos, unique codes
     *
     * Suitable for: HIPAA-compliant data sharing in US
     */
    hipaa_safe_harbor = 5,

    /**
     * @brief GDPR Compliant - European data protection
     *
     * Implements GDPR pseudonymization requirements:
     * - All personal data processed per Article 4(5)
     * - Maintains ability to re-identify with separate key
     * - Supports data subject rights (erasure, portability)
     *
     * Suitable for: European Union data processing
     */
    gdpr_compliant = 6
};

/**
 * @brief Convert profile enum to string representation
 * @param profile The anonymization profile
 * @return String name of the profile
 */
[[nodiscard]] constexpr auto to_string(anonymization_profile profile) noexcept
    -> std::string_view {
    switch (profile) {
        case anonymization_profile::basic:
            return "basic";
        case anonymization_profile::clean_pixel:
            return "clean_pixel";
        case anonymization_profile::clean_descriptions:
            return "clean_descriptions";
        case anonymization_profile::retain_longitudinal:
            return "retain_longitudinal";
        case anonymization_profile::retain_patient_characteristics:
            return "retain_patient_characteristics";
        case anonymization_profile::hipaa_safe_harbor:
            return "hipaa_safe_harbor";
        case anonymization_profile::gdpr_compliant:
            return "gdpr_compliant";
    }
    return "unknown";
}

/**
 * @brief Parse profile from string
 * @param name String name of the profile
 * @return Optional containing the profile, or nullopt if invalid
 */
[[nodiscard]] inline auto profile_from_string(std::string_view name)
    -> std::optional<anonymization_profile> {
    if (name == "basic") return anonymization_profile::basic;
    if (name == "clean_pixel") return anonymization_profile::clean_pixel;
    if (name == "clean_descriptions") return anonymization_profile::clean_descriptions;
    if (name == "retain_longitudinal") return anonymization_profile::retain_longitudinal;
    if (name == "retain_patient_characteristics") {
        return anonymization_profile::retain_patient_characteristics;
    }
    if (name == "hipaa_safe_harbor") return anonymization_profile::hipaa_safe_harbor;
    if (name == "gdpr_compliant") return anonymization_profile::gdpr_compliant;
    return std::nullopt;
}

} // namespace pacs::security
