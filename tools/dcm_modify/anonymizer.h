// BSD 3-Clause License. See LICENSE.
/**
 * @file anonymizer.h
 * @brief Compatibility options for the shared recursive security anonymizer.
 *
 * These presets are not a claim of complete PS3.15 or regulatory compliance.
 */
#ifndef PACS_DCM_MODIFY_ANONYMIZER_HPP
#define PACS_DCM_MODIFY_ANONYMIZER_HPP

#include "kcenon/pacs/security/anonymizer.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include <atomic>
#include <stdexcept>
#include <utility>

namespace kcenon::pacs::dcm_modify {

class uid_mapper {
public:
    // Retain the legacy string-returning interface. Mapping failures are never
    // silently replaced with the original UID.
    std::string map(const std::string& original_uid) {
        auto result = mapping_.get_or_create(original_uid);
        if (result.is_err()) {
            throw std::runtime_error("Unable to map UID");
        }
        return result.value();
    }
    void clear() { mapping_.clear(); }
    security::uid_mapping& mapping() { return mapping_; }
private:
    security::uid_mapping mapping_;
};

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
 * Uses security::anonymizer for all traversal, sequence actions, UID mappings,
 * and private-tag policy. Options apply at every retained sequence depth.
 */
class anonymizer {
public:
    anonymizer() = default;
    explicit anonymizer(anonymize_options opts) : options_(std::move(opts)) {}

    [[nodiscard]] auto anonymize(core::dicom_dataset& dataset)
        -> kcenon::common::Result<security::anonymization_report> {
        using namespace core;
        using security::tag_action_config;
        security::anonymizer engine;
        engine.set_private_tag_action(options_.keep_private_tags
            ? security::private_tag_action::keep : security::private_tag_action::remove_all);
        engine.add_tag_action(tags::patient_name, options_.patient_name_replacement.empty()
            ? tag_action_config::make_remove()
            : tag_action_config::make_replace(options_.patient_name_replacement));
        engine.add_tag_action(tags::patient_id, tag_action_config::make_replace(
            options_.patient_id_prefix + std::to_string(++patient_counter_)));

        const auto remove_or_keep = [&engine](dicom_tag tag, bool remove) {
            engine.add_tag_action(tag, remove ? tag_action_config::make_remove()
                                             : tag_action_config::make_keep());
        };
        remove_or_keep(tags::patient_birth_date, options_.remove_birth_date);
        remove_or_keep(tags::referring_physician_name, options_.remove_referring_physician);
        for (auto tag : {tags::patient_address, dicom_tag{0x0010, 0x2154}}) {
            remove_or_keep(tag, options_.remove_address);
        }
        for (auto tag : {tags::institution_name, tags::institution_address}) {
            remove_or_keep(tag, options_.remove_institution);
        }
        for (auto tag : {tags::study_description, tags::series_description}) {
            remove_or_keep(tag, options_.remove_descriptions);
        }
        // Legacy extra removals, now also applied inside retained items.
        for (auto tag : {tags::accession_number, dicom_tag{0x0032, 0x4000},
                         dicom_tag{0x0032, 0x1060}, dicom_tag{0x0040, 0x0254},
                         dicom_tag{0x0018, 0x1000}}) {
            engine.add_tag_action(tag, tag_action_config::make_remove());
        }
        if (!options_.replace_uids) {
            for (const auto& [tag, config] : security::anonymizer::get_profile_actions(
                     security::anonymization_profile::basic)) {
                if (config.action == security::tag_action::replace_uid) {
                    engine.add_tag_action(tag, tag_action_config::make_keep());
                }
            }
        }
        // Callers must also check result.value().is_successful() before saving.
        return engine.anonymize_with_mapping(dataset, uid_mapper_.mapping());
    }

    uid_mapper& get_uid_mapper() { return uid_mapper_; }
    const anonymize_options& options() const { return options_; }
    void set_options(anonymize_options opts) { options_ = std::move(opts); }

private:
    anonymize_options options_;
    uid_mapper uid_mapper_;
    std::atomic<uint64_t> patient_counter_{0};
};

} // namespace kcenon::pacs::dcm_modify
#endif
