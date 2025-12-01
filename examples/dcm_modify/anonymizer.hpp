/**
 * @file anonymizer.hpp
 * @brief DICOM Anonymization - Patient data removal/replacement
 *
 * Implements DICOM PS3.15 compliant anonymization by removing or replacing
 * Protected Health Information (PHI) tags.
 *
 * @see DICOM PS3.15 - Security and System Management Profiles
 * @see Issue #108 - DICOM Modify Sample
 */

#ifndef PACS_DCM_MODIFY_ANONYMIZER_HPP
#define PACS_DCM_MODIFY_ANONYMIZER_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace pacs::dcm_modify {

/**
 * @brief UID mapping for consistent anonymization across related instances
 */
class uid_mapper {
public:
    /**
     * @brief Get or create a replacement UID for the original UID
     * @param original_uid The original UID to map
     * @return The replacement UID (consistent for same original)
     */
    std::string map(const std::string& original_uid) {
        auto it = mapping_.find(original_uid);
        if (it != mapping_.end()) {
            return it->second;
        }

        auto new_uid = generate_uid();
        mapping_[original_uid] = new_uid;
        return new_uid;
    }

    /**
     * @brief Clear all mappings
     */
    void clear() {
        mapping_.clear();
    }

private:
    std::string generate_uid() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        return std::string(uid_root_) + "." + std::to_string(timestamp) + "." +
               std::to_string(++counter);
    }

    std::map<std::string, std::string> mapping_;
    static constexpr const char* uid_root_ = "1.2.826.0.1.3680043.8.1055.2";
};

/**
 * @brief Anonymization options
 */
struct anonymize_options {
    /// Replace UIDs with new generated UIDs
    bool replace_uids{true};

    /// Replace patient name with this value (empty = remove)
    std::string patient_name_replacement{"ANONYMOUS"};

    /// Replace patient ID with this prefix + counter
    std::string patient_id_prefix{"ANON"};

    /// Remove patient birth date
    bool remove_birth_date{true};

    /// Remove patient address
    bool remove_address{true};

    /// Remove referring physician name
    bool remove_referring_physician{true};

    /// Remove institution name
    bool remove_institution{false};

    /// Remove study/series descriptions
    bool remove_descriptions{false};

    /// Keep private tags (vendor-specific)
    bool keep_private_tags{false};
};

/**
 * @brief DICOM Anonymizer - removes or replaces PHI from datasets
 *
 * Implements basic DICOM anonymization as specified in DICOM PS3.15.
 * This includes removal/replacement of:
 * - Patient identifying information
 * - UIDs (to prevent correlation)
 * - Dates (optional shifting)
 * - Free-text fields that may contain PHI
 */
class anonymizer {
public:
    /**
     * @brief Construct an anonymizer with default options
     */
    anonymizer() = default;

    /**
     * @brief Construct an anonymizer with custom options
     * @param opts Anonymization options
     */
    explicit anonymizer(anonymize_options opts) : options_(std::move(opts)) {}

    /**
     * @brief Anonymize a DICOM dataset in place
     * @param dataset The dataset to anonymize
     */
    void anonymize(core::dicom_dataset& dataset) {
        using namespace pacs::core;
        using namespace pacs::encoding;

        // Patient identifying information
        anonymize_patient_info(dataset);

        // UIDs
        if (options_.replace_uids) {
            anonymize_uids(dataset);
        }

        // Dates
        if (options_.remove_birth_date) {
            dataset.remove(tags::patient_birth_date);
        }

        // Referring physician
        if (options_.remove_referring_physician) {
            dataset.remove(tags::referring_physician_name);
        }

        // Institution
        if (options_.remove_institution) {
            dataset.remove(dicom_tag{0x0008, 0x0080});  // InstitutionName
            dataset.remove(dicom_tag{0x0008, 0x0081});  // InstitutionAddress
        }

        // Descriptions (may contain PHI in free text)
        if (options_.remove_descriptions) {
            dataset.remove(tags::study_description);
            dataset.remove(dicom_tag{0x0008, 0x103E});  // SeriesDescription
        }

        // Address and other contact info
        if (options_.remove_address) {
            dataset.remove(dicom_tag{0x0010, 0x1040});  // PatientAddress
            dataset.remove(dicom_tag{0x0010, 0x2154});  // PatientTelephoneNumbers
        }

        // Remove private tags unless explicitly kept
        if (!options_.keep_private_tags) {
            remove_private_tags(dataset);
        }

        // Additional PHI tags
        remove_additional_phi(dataset);
    }

    /**
     * @brief Get the UID mapper for consistent UID replacement
     * @return Reference to the UID mapper
     */
    uid_mapper& get_uid_mapper() { return uid_mapper_; }

    /**
     * @brief Get the current options
     * @return Reference to the options
     */
    const anonymize_options& options() const { return options_; }

    /**
     * @brief Set new options
     * @param opts New options
     */
    void set_options(anonymize_options opts) { options_ = std::move(opts); }

private:
    void anonymize_patient_info(core::dicom_dataset& dataset) {
        using namespace pacs::core;
        using namespace pacs::encoding;

        // Patient Name
        if (!options_.patient_name_replacement.empty()) {
            dataset.set_string(tags::patient_name, vr_type::PN,
                               options_.patient_name_replacement);
        } else {
            dataset.remove(tags::patient_name);
        }

        // Patient ID
        std::string patient_id = options_.patient_id_prefix +
                                 std::to_string(++patient_counter_);
        dataset.set_string(tags::patient_id, vr_type::LO, patient_id);

        // Patient's Birth Name, Mother's Maiden Name
        dataset.remove(dicom_tag{0x0010, 0x1005});  // PatientBirthName
        dataset.remove(dicom_tag{0x0010, 0x1060});  // PatientMotherBirthName

        // Other patient IDs
        dataset.remove(dicom_tag{0x0010, 0x1000});  // OtherPatientIDs
        dataset.remove(dicom_tag{0x0010, 0x1001});  // OtherPatientNames
    }

    void anonymize_uids(core::dicom_dataset& dataset) {
        using namespace pacs::core;
        using namespace pacs::encoding;

        // Study Instance UID
        auto study_uid = dataset.get_string(tags::study_instance_uid);
        if (!study_uid.empty()) {
            dataset.set_string(tags::study_instance_uid, vr_type::UI,
                               uid_mapper_.map(study_uid));
        }

        // Series Instance UID
        auto series_uid = dataset.get_string(tags::series_instance_uid);
        if (!series_uid.empty()) {
            dataset.set_string(tags::series_instance_uid, vr_type::UI,
                               uid_mapper_.map(series_uid));
        }

        // SOP Instance UID
        auto sop_uid = dataset.get_string(tags::sop_instance_uid);
        if (!sop_uid.empty()) {
            dataset.set_string(tags::sop_instance_uid, vr_type::UI,
                               uid_mapper_.map(sop_uid));
        }

        // Frame of Reference UID
        auto frame_uid = dataset.get_string(dicom_tag{0x0020, 0x0052});
        if (!frame_uid.empty()) {
            dataset.set_string(dicom_tag{0x0020, 0x0052}, vr_type::UI,
                               uid_mapper_.map(frame_uid));
        }
    }

    void remove_private_tags(core::dicom_dataset& dataset) {
        std::vector<core::dicom_tag> private_tags;

        for (const auto& [tag, element] : dataset) {
            if (tag.is_private()) {
                private_tags.push_back(tag);
            }
        }

        for (const auto& tag : private_tags) {
            dataset.remove(tag);
        }
    }

    void remove_additional_phi(core::dicom_dataset& dataset) {
        // Accession Number (may be linked to hospital records)
        dataset.remove(core::dicom_tag{0x0008, 0x0050});

        // Patient comments
        dataset.remove(core::dicom_tag{0x0010, 0x4000});

        // Study comments
        dataset.remove(core::dicom_tag{0x0032, 0x4000});

        // Requested Procedure Description
        dataset.remove(core::dicom_tag{0x0032, 0x1060});

        // Performed Procedure Step Description
        dataset.remove(core::dicom_tag{0x0040, 0x0254});

        // Device Serial Number
        dataset.remove(core::dicom_tag{0x0018, 0x1000});
    }

    anonymize_options options_;
    uid_mapper uid_mapper_;
    std::atomic<uint64_t> patient_counter_{0};
};

}  // namespace pacs::dcm_modify

#endif  // PACS_DCM_MODIFY_ANONYMIZER_HPP
