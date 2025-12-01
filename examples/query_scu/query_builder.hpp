/**
 * @file query_builder.hpp
 * @brief DICOM Query Dataset Builder
 *
 * Provides a fluent interface for building C-FIND query datasets
 * with proper tag initialization for different query levels.
 *
 * @see Issue #102 - Query SCU Sample
 * @see DICOM PS3.4 Section C.6 - Query/Retrieve Information Model
 */

#ifndef QUERY_SCU_QUERY_BUILDER_HPP
#define QUERY_SCU_QUERY_BUILDER_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/services/query_scp.hpp"

#include <string>
#include <string_view>

namespace query_scu {

/**
 * @brief Fluent builder for constructing DICOM query datasets
 *
 * This class provides a convenient way to build query datasets for C-FIND
 * operations. It automatically sets required tags and handles query level
 * configuration.
 *
 * @example
 * @code
 * auto query = query_builder()
 *     .level(query_level::study)
 *     .patient_name("DOE^*")
 *     .study_date("20240101-20241231")
 *     .modality("CT")
 *     .build();
 * @endcode
 */
class query_builder {
public:
    using query_level = pacs::services::query_level;

    query_builder() = default;

    /**
     * @brief Set the query/retrieve level
     * @param lvl The query level (PATIENT, STUDY, SERIES, IMAGE)
     * @return Reference to this builder for chaining
     */
    query_builder& level(query_level lvl) {
        level_ = lvl;
        return *this;
    }

    // =========================================================================
    // Patient Level Attributes
    // =========================================================================

    /**
     * @brief Set patient name search criteria (supports wildcards)
     * @param name Patient name pattern (e.g., "DOE^JOHN" or "DOE^*")
     */
    query_builder& patient_name(std::string_view name) {
        patient_name_ = name;
        return *this;
    }

    /**
     * @brief Set patient ID search criteria
     * @param id Patient ID pattern
     */
    query_builder& patient_id(std::string_view id) {
        patient_id_ = id;
        return *this;
    }

    /**
     * @brief Set patient birth date criteria
     * @param date Birth date in DICOM DA format (YYYYMMDD or range)
     */
    query_builder& patient_birth_date(std::string_view date) {
        patient_birth_date_ = date;
        return *this;
    }

    /**
     * @brief Set patient sex criteria
     * @param sex Patient sex (M, F, O)
     */
    query_builder& patient_sex(std::string_view sex) {
        patient_sex_ = sex;
        return *this;
    }

    // =========================================================================
    // Study Level Attributes
    // =========================================================================

    /**
     * @brief Set study date criteria (supports ranges)
     * @param date Study date in DICOM DA format (YYYYMMDD, YYYYMMDD-YYYYMMDD)
     */
    query_builder& study_date(std::string_view date) {
        study_date_ = date;
        return *this;
    }

    /**
     * @brief Set study time criteria
     * @param time Study time in DICOM TM format
     */
    query_builder& study_time(std::string_view time) {
        study_time_ = time;
        return *this;
    }

    /**
     * @brief Set accession number criteria
     * @param accession Accession number pattern
     */
    query_builder& accession_number(std::string_view accession) {
        accession_number_ = accession;
        return *this;
    }

    /**
     * @brief Set study instance UID criteria
     * @param uid Study Instance UID
     */
    query_builder& study_instance_uid(std::string_view uid) {
        study_instance_uid_ = uid;
        return *this;
    }

    /**
     * @brief Set study ID criteria
     * @param id Study ID pattern
     */
    query_builder& study_id(std::string_view id) {
        study_id_ = id;
        return *this;
    }

    /**
     * @brief Set study description criteria
     * @param desc Study description pattern
     */
    query_builder& study_description(std::string_view desc) {
        study_description_ = desc;
        return *this;
    }

    // =========================================================================
    // Series Level Attributes
    // =========================================================================

    /**
     * @brief Set modality criteria
     * @param mod Modality code (CT, MR, US, XR, etc.)
     */
    query_builder& modality(std::string_view mod) {
        modality_ = mod;
        return *this;
    }

    /**
     * @brief Set series instance UID criteria
     * @param uid Series Instance UID
     */
    query_builder& series_instance_uid(std::string_view uid) {
        series_instance_uid_ = uid;
        return *this;
    }

    /**
     * @brief Set series number criteria
     * @param num Series number
     */
    query_builder& series_number(std::string_view num) {
        series_number_ = num;
        return *this;
    }

    /**
     * @brief Set series description criteria
     * @param desc Series description pattern
     */
    query_builder& series_description(std::string_view desc) {
        series_description_ = desc;
        return *this;
    }

    // =========================================================================
    // Instance Level Attributes
    // =========================================================================

    /**
     * @brief Set SOP instance UID criteria
     * @param uid SOP Instance UID
     */
    query_builder& sop_instance_uid(std::string_view uid) {
        sop_instance_uid_ = uid;
        return *this;
    }

    /**
     * @brief Set instance number criteria
     * @param num Instance number
     */
    query_builder& instance_number(std::string_view num) {
        instance_number_ = num;
        return *this;
    }

    // =========================================================================
    // Build
    // =========================================================================

    /**
     * @brief Build the query dataset
     * @return Configured DICOM dataset ready for C-FIND
     */
    [[nodiscard]] pacs::core::dicom_dataset build() const {
        using namespace pacs::core;
        using namespace pacs::encoding;

        dicom_dataset ds;

        // Set Query/Retrieve Level (required)
        ds.set_string(tags::query_retrieve_level, vr_type::CS,
                      std::string(pacs::services::to_string(level_)));

        // Add return keys and search criteria based on level
        switch (level_) {
            case query_level::patient:
                add_patient_keys(ds);
                break;
            case query_level::study:
                add_patient_keys(ds);
                add_study_keys(ds);
                break;
            case query_level::series:
                add_patient_keys(ds);
                add_study_keys(ds);
                add_series_keys(ds);
                break;
            case query_level::image:
                add_patient_keys(ds);
                add_study_keys(ds);
                add_series_keys(ds);
                add_instance_keys(ds);
                break;
        }

        return ds;
    }

private:
    void add_patient_keys(pacs::core::dicom_dataset& ds) const {
        using namespace pacs::core;
        using namespace pacs::encoding;

        // Patient Name - always include as return key
        ds.set_string(tags::patient_name, vr_type::PN, patient_name_);

        // Patient ID
        ds.set_string(tags::patient_id, vr_type::LO, patient_id_);

        // Patient Birth Date
        if (!patient_birth_date_.empty()) {
            ds.set_string(tags::patient_birth_date, vr_type::DA, patient_birth_date_);
        } else {
            ds.set_string(tags::patient_birth_date, vr_type::DA, "");
        }

        // Patient Sex
        if (!patient_sex_.empty()) {
            ds.set_string(tags::patient_sex, vr_type::CS, patient_sex_);
        } else {
            ds.set_string(tags::patient_sex, vr_type::CS, "");
        }
    }

    void add_study_keys(pacs::core::dicom_dataset& ds) const {
        using namespace pacs::core;
        using namespace pacs::encoding;

        // Study Instance UID - required for Study level
        ds.set_string(tags::study_instance_uid, vr_type::UI, study_instance_uid_);

        // Study Date
        ds.set_string(tags::study_date, vr_type::DA, study_date_);

        // Study Time
        if (!study_time_.empty()) {
            ds.set_string(tags::study_time, vr_type::TM, study_time_);
        } else {
            ds.set_string(tags::study_time, vr_type::TM, "");
        }

        // Accession Number
        ds.set_string(tags::accession_number, vr_type::SH, accession_number_);

        // Study ID
        if (!study_id_.empty()) {
            ds.set_string(tags::study_id, vr_type::SH, study_id_);
        } else {
            ds.set_string(tags::study_id, vr_type::SH, "");
        }

        // Study Description
        ds.set_string(tags::study_description, vr_type::LO, study_description_);

        // Referring Physician's Name (return key)
        ds.set_string(tags::referring_physician_name, vr_type::PN, "");

        // Number of Study Related Series (return key)
        ds.set_string(tags::number_of_study_related_series, vr_type::IS, "");

        // Number of Study Related Instances (return key)
        ds.set_string(tags::number_of_study_related_instances, vr_type::IS, "");

        // Modalities in Study (return key)
        ds.set_string(tags::modalities_in_study, vr_type::CS, modality_);
    }

    void add_series_keys(pacs::core::dicom_dataset& ds) const {
        using namespace pacs::core;
        using namespace pacs::encoding;

        // Series Instance UID - required for Series level
        ds.set_string(tags::series_instance_uid, vr_type::UI, series_instance_uid_);

        // Modality
        ds.set_string(tags::modality, vr_type::CS, modality_);

        // Series Number
        if (!series_number_.empty()) {
            ds.set_string(tags::series_number, vr_type::IS, series_number_);
        } else {
            ds.set_string(tags::series_number, vr_type::IS, "");
        }

        // Series Description
        ds.set_string(tags::series_description, vr_type::LO, series_description_);

        // Number of Series Related Instances (return key)
        ds.set_string(tags::number_of_series_related_instances, vr_type::IS, "");
    }

    void add_instance_keys(pacs::core::dicom_dataset& ds) const {
        using namespace pacs::core;
        using namespace pacs::encoding;

        // SOP Instance UID - required for Instance level
        ds.set_string(tags::sop_instance_uid, vr_type::UI, sop_instance_uid_);

        // SOP Class UID (return key)
        ds.set_string(tags::sop_class_uid, vr_type::UI, "");

        // Instance Number
        if (!instance_number_.empty()) {
            ds.set_string(tags::instance_number, vr_type::IS, instance_number_);
        } else {
            ds.set_string(tags::instance_number, vr_type::IS, "");
        }
    }

    // Query level
    query_level level_{query_level::study};

    // Patient level
    std::string patient_name_;
    std::string patient_id_;
    std::string patient_birth_date_;
    std::string patient_sex_;

    // Study level
    std::string study_date_;
    std::string study_time_;
    std::string accession_number_;
    std::string study_instance_uid_;
    std::string study_id_;
    std::string study_description_;

    // Series level
    std::string modality_;
    std::string series_instance_uid_;
    std::string series_number_;
    std::string series_description_;

    // Instance level
    std::string sop_instance_uid_;
    std::string instance_number_;
};

}  // namespace query_scu

#endif  // QUERY_SCU_QUERY_BUILDER_HPP
