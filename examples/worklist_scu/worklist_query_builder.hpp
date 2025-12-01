/**
 * @file worklist_query_builder.hpp
 * @brief DICOM Modality Worklist Query Dataset Builder
 *
 * Provides a fluent interface for building MWL C-FIND query datasets
 * with proper tag initialization for Scheduled Procedure Step attributes.
 *
 * @see Issue #104 - Worklist SCU Sample
 * @see DICOM PS3.4 Section K - Basic Worklist Management Service Class
 */

#ifndef WORKLIST_SCU_WORKLIST_QUERY_BUILDER_HPP
#define WORKLIST_SCU_WORKLIST_QUERY_BUILDER_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/services/worklist_scp.hpp"

#include <string>
#include <string_view>

namespace worklist_scu {

/**
 * @brief Fluent builder for constructing MWL query datasets
 *
 * This class provides a convenient way to build query datasets for Modality
 * Worklist C-FIND operations. It handles the complexity of the Scheduled
 * Procedure Step Sequence and automatically sets required return keys.
 *
 * @example
 * @code
 * auto query = worklist_query_builder()
 *     .modality("CT")
 *     .scheduled_date("20241215")
 *     .scheduled_station_ae("CT_SCANNER_01")
 *     .build();
 * @endcode
 */
class worklist_query_builder {
public:
    worklist_query_builder() = default;

    // =========================================================================
    // Patient Demographics
    // =========================================================================

    /**
     * @brief Set patient name search criteria (supports wildcards)
     * @param name Patient name pattern (e.g., "DOE^JOHN" or "DOE^*")
     */
    worklist_query_builder& patient_name(std::string_view name) {
        patient_name_ = name;
        return *this;
    }

    /**
     * @brief Set patient ID search criteria
     * @param id Patient ID pattern
     */
    worklist_query_builder& patient_id(std::string_view id) {
        patient_id_ = id;
        return *this;
    }

    /**
     * @brief Set patient birth date criteria
     * @param date Birth date in DICOM DA format (YYYYMMDD or range)
     */
    worklist_query_builder& patient_birth_date(std::string_view date) {
        patient_birth_date_ = date;
        return *this;
    }

    /**
     * @brief Set patient sex criteria
     * @param sex Patient sex (M, F, O)
     */
    worklist_query_builder& patient_sex(std::string_view sex) {
        patient_sex_ = sex;
        return *this;
    }

    // =========================================================================
    // Scheduled Procedure Step Attributes
    // =========================================================================

    /**
     * @brief Set modality criteria
     * @param mod Modality code (CT, MR, US, XR, etc.)
     */
    worklist_query_builder& modality(std::string_view mod) {
        modality_ = mod;
        return *this;
    }

    /**
     * @brief Set scheduled date criteria (supports ranges)
     * @param date Scheduled date in DICOM DA format (YYYYMMDD or range)
     */
    worklist_query_builder& scheduled_date(std::string_view date) {
        scheduled_date_ = date;
        return *this;
    }

    /**
     * @brief Set scheduled time criteria
     * @param time Scheduled time in DICOM TM format
     */
    worklist_query_builder& scheduled_time(std::string_view time) {
        scheduled_time_ = time;
        return *this;
    }

    /**
     * @brief Set scheduled station AE title criteria
     * @param ae_title Station AE title (e.g., "CT_SCANNER_01")
     */
    worklist_query_builder& scheduled_station_ae(std::string_view ae_title) {
        scheduled_station_ae_ = ae_title;
        return *this;
    }

    /**
     * @brief Set scheduled performing physician name criteria
     * @param name Physician name pattern
     */
    worklist_query_builder& scheduled_physician(std::string_view name) {
        scheduled_physician_ = name;
        return *this;
    }

    // =========================================================================
    // Study/Request Attributes
    // =========================================================================

    /**
     * @brief Set accession number criteria
     * @param accession Accession number pattern
     */
    worklist_query_builder& accession_number(std::string_view accession) {
        accession_number_ = accession;
        return *this;
    }

    /**
     * @brief Set requested procedure ID criteria
     * @param id Requested procedure ID
     */
    worklist_query_builder& requested_procedure_id(std::string_view id) {
        requested_procedure_id_ = id;
        return *this;
    }

    /**
     * @brief Set study instance UID criteria
     * @param uid Study Instance UID
     */
    worklist_query_builder& study_instance_uid(std::string_view uid) {
        study_instance_uid_ = uid;
        return *this;
    }

    // =========================================================================
    // Build
    // =========================================================================

    /**
     * @brief Build the worklist query dataset
     * @return Configured DICOM dataset ready for MWL C-FIND
     *
     * @note This implementation uses a flat dataset structure without nested
     * sequences, as the current dicom_dataset implementation does not support
     * DICOM sequences. The Scheduled Procedure Step attributes are included
     * directly in the main dataset.
     */
    [[nodiscard]] pacs::core::dicom_dataset build() const {
        using namespace pacs::core;
        using namespace pacs::encoding;

        dicom_dataset ds;

        // Patient demographics (return keys with optional search criteria)
        ds.set_string(tags::patient_name, vr_type::PN, patient_name_);
        ds.set_string(tags::patient_id, vr_type::LO, patient_id_);

        if (!patient_birth_date_.empty()) {
            ds.set_string(tags::patient_birth_date, vr_type::DA, patient_birth_date_);
        } else {
            ds.set_string(tags::patient_birth_date, vr_type::DA, "");
        }

        if (!patient_sex_.empty()) {
            ds.set_string(tags::patient_sex, vr_type::CS, patient_sex_);
        } else {
            ds.set_string(tags::patient_sex, vr_type::CS, "");
        }

        // Study-level return keys
        ds.set_string(tags::study_instance_uid, vr_type::UI, study_instance_uid_);
        ds.set_string(tags::accession_number, vr_type::SH, accession_number_);
        ds.set_string(tags::referring_physician_name, vr_type::PN, "");

        // Requested Procedure attributes
        ds.set_string(tags::requested_procedure_id, vr_type::SH, requested_procedure_id_);
        ds.set_string(tags::study_description, vr_type::LO, "");

        // Scheduled Procedure Step attributes (flat structure)
        // Station AE Title (search criteria and return key)
        ds.set_string(tags::scheduled_station_ae_title, vr_type::AE, scheduled_station_ae_);

        // Scheduled date/time
        ds.set_string(tags::scheduled_procedure_step_start_date, vr_type::DA, scheduled_date_);
        ds.set_string(tags::scheduled_procedure_step_start_time, vr_type::TM, scheduled_time_);

        // Modality
        ds.set_string(tags::modality, vr_type::CS, modality_);

        // Performing physician
        ds.set_string(tags::scheduled_performing_physician_name, vr_type::PN, scheduled_physician_);

        // Additional return keys
        ds.set_string(tags::scheduled_procedure_step_id, vr_type::SH, "");
        ds.set_string(tags::scheduled_procedure_step_description, vr_type::LO, "");
        ds.set_string(tags::scheduled_station_name, vr_type::SH, "");
        ds.set_string(tags::scheduled_procedure_step_location, vr_type::SH, "");

        return ds;
    }

private:

    // Patient demographics
    std::string patient_name_;
    std::string patient_id_;
    std::string patient_birth_date_;
    std::string patient_sex_;

    // Scheduled Procedure Step attributes
    std::string modality_;
    std::string scheduled_date_;
    std::string scheduled_time_;
    std::string scheduled_station_ae_;
    std::string scheduled_physician_;

    // Study/Request attributes
    std::string accession_number_;
    std::string requested_procedure_id_;
    std::string study_instance_uid_;
};

}  // namespace worklist_scu

#endif  // WORKLIST_SCU_WORKLIST_QUERY_BUILDER_HPP
