/**
 * @file tag_action.cpp
 * @brief Implementation of HIPAA identifier tag collections
 *
 * @copyright Copyright (c) 2025
 */

#include "pacs/security/tag_action.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

namespace pacs::security::hipaa_identifiers {

using namespace pacs::core;

auto get_name_tags() -> std::vector<dicom_tag> {
    return {
        tags::patient_name,
        tags::referring_physician_name,
        tags::performing_physician_name,
        tags::name_of_physicians_reading_study,
        tags::operators_name,
        tags::scheduled_performing_physician_name
    };
}

auto get_geographic_tags() -> std::vector<dicom_tag> {
    return {
        tags::institution_name,
        tags::institution_address,
        tags::patient_address,
        tags::station_name
    };
}

auto get_date_tags() -> std::vector<dicom_tag> {
    return {
        tags::patient_birth_date,
        tags::study_date,
        tags::series_date,
        tags::acquisition_date,
        tags::content_date,
        tags::instance_creation_date,
        tags::scheduled_procedure_step_start_date,
        tags::scheduled_procedure_step_end_date,
        tags::performed_procedure_step_start_date
    };
}

auto get_communication_tags() -> std::vector<dicom_tag> {
    // Phone, fax, email - not typically in standard DICOM tags
    // These would be in private tags or extended attributes
    return {};
}

auto get_unique_id_tags() -> std::vector<dicom_tag> {
    return {
        tags::patient_id,
        tags::issuer_of_patient_id,
        tags::accession_number,
        tags::study_id,
        tags::requested_procedure_id,
        tags::scheduled_procedure_step_id,
        tags::performed_procedure_step_id
    };
}

auto get_all_identifier_tags() -> std::vector<dicom_tag> {
    std::vector<dicom_tag> all_tags;

    auto names = get_name_tags();
    auto geo = get_geographic_tags();
    auto dates = get_date_tags();
    auto comm = get_communication_tags();
    auto ids = get_unique_id_tags();

    all_tags.reserve(names.size() + geo.size() + dates.size() +
                     comm.size() + ids.size());

    all_tags.insert(all_tags.end(), names.begin(), names.end());
    all_tags.insert(all_tags.end(), geo.begin(), geo.end());
    all_tags.insert(all_tags.end(), dates.begin(), dates.end());
    all_tags.insert(all_tags.end(), comm.begin(), comm.end());
    all_tags.insert(all_tags.end(), ids.begin(), ids.end());

    return all_tags;
}

} // namespace pacs::security::hipaa_identifiers
