/**
 * @file anonymizer.cpp
 * @brief Implementation of DICOM de-identification/anonymization
 *
 * @copyright Copyright (c) 2025
 */

#include "pacs/security/anonymizer.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <random>
#include <sstream>

namespace pacs::security {

using namespace pacs::core;

anonymizer::anonymizer(anonymization_profile profile)
    : profile_{profile} {
    initialize_profile_actions();
}

anonymizer::anonymizer(const anonymizer& other)
    : profile_{other.profile_}
    , custom_actions_{other.custom_actions_}
    , date_offset_{other.date_offset_}
    , encryption_key_{other.encryption_key_}
    , hash_salt_{other.hash_salt_}
    , detailed_reporting_{other.detailed_reporting_} {}

anonymizer::anonymizer(anonymizer&& other) noexcept
    : profile_{other.profile_}
    , custom_actions_{std::move(other.custom_actions_)}
    , date_offset_{other.date_offset_}
    , encryption_key_{std::move(other.encryption_key_)}
    , hash_salt_{std::move(other.hash_salt_)}
    , detailed_reporting_{other.detailed_reporting_} {}

auto anonymizer::operator=(const anonymizer& other) -> anonymizer& {
    if (this != &other) {
        profile_ = other.profile_;
        custom_actions_ = other.custom_actions_;
        date_offset_ = other.date_offset_;
        encryption_key_ = other.encryption_key_;
        hash_salt_ = other.hash_salt_;
        detailed_reporting_ = other.detailed_reporting_;
    }
    return *this;
}

auto anonymizer::operator=(anonymizer&& other) noexcept -> anonymizer& {
    if (this != &other) {
        profile_ = other.profile_;
        custom_actions_ = std::move(other.custom_actions_);
        date_offset_ = other.date_offset_;
        encryption_key_ = std::move(other.encryption_key_);
        hash_salt_ = std::move(other.hash_salt_);
        detailed_reporting_ = other.detailed_reporting_;
    }
    return *this;
}

auto anonymizer::anonymize(dicom_dataset& dataset)
    -> kcenon::common::Result<anonymization_report> {
    uid_mapping temp_mapping;
    return anonymize_with_mapping(dataset, temp_mapping);
}

auto anonymizer::anonymize_with_mapping(
    dicom_dataset& dataset,
    uid_mapping& mapping
) -> kcenon::common::Result<anonymization_report> {
    anonymization_report report;
    report.profile_name = std::string(to_string(profile_));
    report.date_offset = date_offset_;
    report.timestamp = std::chrono::system_clock::now();

    // Get profile actions
    auto profile_actions = get_profile_actions(profile_);

    // Merge with custom actions (custom takes precedence)
    for (const auto& [tag, config] : custom_actions_) {
        profile_actions[tag] = config;
    }

    // Process each tag in the profile
    for (const auto& [tag, config] : profile_actions) {
        if (!dataset.contains(tag)) {
            continue;
        }

        auto record = apply_action(dataset, tag, config, &mapping);
        report.total_tags_processed++;

        switch (config.action) {
            case tag_action::remove:
            case tag_action::remove_or_empty:
                if (record.success) {
                    report.tags_removed++;
                }
                break;
            case tag_action::empty:
                if (record.success) {
                    report.tags_emptied++;
                }
                break;
            case tag_action::replace:
                if (record.success) {
                    report.tags_replaced++;
                }
                break;
            case tag_action::replace_uid:
                if (record.success) {
                    report.uids_replaced++;
                }
                break;
            case tag_action::keep:
                report.tags_kept++;
                break;
            case tag_action::shift_date:
                if (record.success) {
                    report.dates_shifted++;
                }
                break;
            case tag_action::hash:
                if (record.success) {
                    report.values_hashed++;
                }
                break;
            case tag_action::encrypt:
                if (record.success) {
                    report.tags_replaced++;
                }
                break;
        }

        if (!record.success && !record.error_message.empty()) {
            report.errors.push_back(record.error_message);
        }

        if (detailed_reporting_) {
            report.action_records.push_back(std::move(record));
        }
    }

    return report;
}

auto anonymizer::get_profile() const noexcept -> anonymization_profile {
    return profile_;
}

void anonymizer::set_profile(anonymization_profile profile) {
    profile_ = profile;
    initialize_profile_actions();
}

void anonymizer::add_tag_action(dicom_tag tag, tag_action_config config) {
    custom_actions_[tag] = std::move(config);
}

void anonymizer::add_tag_actions(
    const std::map<dicom_tag, tag_action_config>& actions
) {
    for (const auto& [tag, config] : actions) {
        custom_actions_[tag] = config;
    }
}

auto anonymizer::remove_tag_action(dicom_tag tag) -> bool {
    return custom_actions_.erase(tag) > 0;
}

void anonymizer::clear_custom_actions() {
    custom_actions_.clear();
}

auto anonymizer::get_tag_action(dicom_tag tag) const -> tag_action_config {
    auto it = custom_actions_.find(tag);
    if (it != custom_actions_.end()) {
        return it->second;
    }

    auto profile_actions = get_profile_actions(profile_);
    auto profile_it = profile_actions.find(tag);
    if (profile_it != profile_actions.end()) {
        return profile_it->second;
    }

    return tag_action_config::make_keep();
}

void anonymizer::set_date_offset(std::chrono::days offset) {
    date_offset_ = offset;
}

auto anonymizer::get_date_offset() const noexcept
    -> std::optional<std::chrono::days> {
    return date_offset_;
}

void anonymizer::clear_date_offset() {
    date_offset_.reset();
}

auto anonymizer::generate_random_date_offset(
    std::chrono::days min_days,
    std::chrono::days max_days
) -> std::chrono::days {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(
        static_cast<int>(min_days.count()),
        static_cast<int>(max_days.count())
    );
    return std::chrono::days{dist(gen)};
}

auto anonymizer::set_encryption_key(std::span<const std::uint8_t> key)
    -> kcenon::common::VoidResult {
    if (key.size() != 32) {
        return kcenon::common::make_error<std::monostate>(
            1, "Encryption key must be 32 bytes for AES-256", "anonymizer"
        );
    }
    encryption_key_.assign(key.begin(), key.end());
    return kcenon::common::ok();
}

auto anonymizer::has_encryption_key() const noexcept -> bool {
    return !encryption_key_.empty();
}

void anonymizer::set_hash_salt(std::string salt) {
    hash_salt_ = std::move(salt);
}

auto anonymizer::get_hash_salt() const -> std::optional<std::string> {
    return hash_salt_;
}

void anonymizer::set_detailed_reporting(bool enable) {
    detailed_reporting_ = enable;
}

auto anonymizer::is_detailed_reporting() const noexcept -> bool {
    return detailed_reporting_;
}

auto anonymizer::get_profile_actions(anonymization_profile profile)
    -> std::map<dicom_tag, tag_action_config> {
    std::map<dicom_tag, tag_action_config> actions;

    // Common patient identifiers
    auto add_patient_identifiers = [&actions]() {
        actions[tags::patient_name] = tag_action_config::make_replace("ANONYMOUS");
        actions[tags::patient_id] = tag_action_config::make_replace("ANON_ID");
        actions[tags::patient_birth_date] = tag_action_config::make_empty();
        actions[tags::patient_sex] = tag_action_config::make_keep();
        actions[tags::patient_age] = tag_action_config::make_keep();
        actions[tags::patient_address] = tag_action_config::make_remove();
        actions[tags::patient_comments] = tag_action_config::make_remove();
    };

    // Institution identifiers
    auto add_institution_identifiers = [&actions]() {
        actions[tags::institution_name] = tag_action_config::make_empty();
        actions[tags::institution_address] = tag_action_config::make_remove();
        actions[tags::station_name] = tag_action_config::make_empty();
    };

    // Personnel identifiers
    auto add_personnel_identifiers = [&actions]() {
        actions[tags::referring_physician_name] = tag_action_config::make_empty();
        actions[tags::performing_physician_name] = tag_action_config::make_empty();
        actions[tags::name_of_physicians_reading_study] = tag_action_config::make_empty();
        actions[tags::operators_name] = tag_action_config::make_empty();
        actions[tags::scheduled_performing_physician_name] = tag_action_config::make_empty();
    };

    // UID replacements
    auto add_uid_replacements = [&actions]() {
        actions[tags::study_instance_uid] =
            {.action = tag_action::replace_uid};
        actions[tags::series_instance_uid] =
            {.action = tag_action::replace_uid};
        actions[tags::sop_instance_uid] =
            {.action = tag_action::replace_uid};
        actions[tags::frame_of_reference_uid] =
            {.action = tag_action::replace_uid};
    };

    // Study identifiers
    auto add_study_identifiers = [&actions]() {
        actions[tags::accession_number] = tag_action_config::make_empty();
        actions[tags::study_id] = tag_action_config::make_replace("ANON_STUDY");
    };

    // Dates for shifting
    auto add_date_shifting = [&actions]() {
        actions[tags::study_date] = {.action = tag_action::shift_date};
        actions[tags::series_date] = {.action = tag_action::shift_date};
        actions[tags::acquisition_date] = {.action = tag_action::shift_date};
        actions[tags::content_date] = {.action = tag_action::shift_date};
        actions[tags::instance_creation_date] = {.action = tag_action::shift_date};
        actions[tags::patient_birth_date] = {.action = tag_action::shift_date};
    };

    switch (profile) {
        case anonymization_profile::basic:
            add_patient_identifiers();
            add_institution_identifiers();
            add_personnel_identifiers();
            add_uid_replacements();
            add_study_identifiers();
            break;

        case anonymization_profile::clean_pixel:
            add_patient_identifiers();
            add_institution_identifiers();
            add_personnel_identifiers();
            add_uid_replacements();
            add_study_identifiers();
            // Note: Pixel data cleaning requires image processing
            // which is not implemented in this basic version
            break;

        case anonymization_profile::clean_descriptions:
            add_patient_identifiers();
            add_institution_identifiers();
            add_personnel_identifiers();
            add_uid_replacements();
            add_study_identifiers();
            actions[tags::study_description] = tag_action_config::make_empty();
            actions[tags::series_description] = tag_action_config::make_empty();
            break;

        case anonymization_profile::retain_longitudinal:
            add_patient_identifiers();
            add_institution_identifiers();
            add_personnel_identifiers();
            add_uid_replacements();
            add_study_identifiers();
            add_date_shifting();
            break;

        case anonymization_profile::retain_patient_characteristics:
            add_patient_identifiers();
            add_institution_identifiers();
            add_personnel_identifiers();
            add_uid_replacements();
            add_study_identifiers();
            // Keep demographic data
            actions[tags::patient_sex] = tag_action_config::make_keep();
            actions[tags::patient_age] = tag_action_config::make_keep();
            actions[tags::patient_size] = tag_action_config::make_keep();
            actions[tags::patient_weight] = tag_action_config::make_keep();
            break;

        case anonymization_profile::hipaa_safe_harbor:
            add_patient_identifiers();
            add_institution_identifiers();
            add_personnel_identifiers();
            add_uid_replacements();
            add_study_identifiers();
            // Remove all HIPAA identifiers
            for (const auto& tag : get_hipaa_identifier_tags()) {
                if (actions.find(tag) == actions.end()) {
                    actions[tag] = tag_action_config::make_remove();
                }
            }
            // Dates - keep only year or remove
            actions[tags::patient_birth_date] = tag_action_config::make_remove();
            break;

        case anonymization_profile::gdpr_compliant:
            add_patient_identifiers();
            add_institution_identifiers();
            add_personnel_identifiers();
            add_uid_replacements();
            add_study_identifiers();
            // GDPR allows pseudonymization - use hash for linkage
            actions[tags::patient_id] = tag_action_config::make_hash();
            actions[tags::patient_name] = tag_action_config::make_hash();
            break;
    }

    return actions;
}

auto anonymizer::get_hipaa_identifier_tags() -> std::vector<dicom_tag> {
    return hipaa_identifiers::get_all_identifier_tags();
}

auto anonymizer::get_gdpr_personal_data_tags() -> std::vector<dicom_tag> {
    std::vector<dicom_tag> tags;

    // Direct identifiers
    tags.push_back(tags::patient_name);
    tags.push_back(tags::patient_id);
    tags.push_back(tags::patient_birth_date);
    tags.push_back(tags::patient_address);

    // Indirect identifiers
    tags.push_back(tags::study_instance_uid);
    tags.push_back(tags::series_instance_uid);
    tags.push_back(tags::sop_instance_uid);

    // Health data (special category under GDPR)
    tags.push_back(tags::patient_sex);
    tags.push_back(tags::patient_age);
    tags.push_back(tags::patient_size);
    tags.push_back(tags::patient_weight);

    return tags;
}

auto anonymizer::apply_action(
    dicom_dataset& dataset,
    dicom_tag tag,
    const tag_action_config& config,
    uid_mapping* mapping
) -> tag_action_record {
    tag_action_record record;
    record.tag = tag;
    record.action = config.action;

    auto* element = dataset.get(tag);
    if (element == nullptr) {
        record.success = false;
        record.error_message = "Tag not found in dataset";
        return record;
    }

    record.original_value = element->as_string();

    switch (config.action) {
        case tag_action::remove:
        case tag_action::remove_or_empty:
            record.success = dataset.remove(tag);
            break;

        case tag_action::empty:
            dataset.set_string(tag, element->vr(), "");
            record.new_value = "";
            record.success = true;
            break;

        case tag_action::keep:
            record.new_value = record.original_value;
            record.success = true;
            break;

        case tag_action::replace:
            dataset.set_string(tag, element->vr(), config.replacement_value);
            record.new_value = config.replacement_value;
            record.success = true;
            break;

        case tag_action::replace_uid:
            if (mapping != nullptr) {
                auto result = mapping->get_or_create(record.original_value);
                if (result.is_ok()) {
                    dataset.set_string(tag, element->vr(), result.value());
                    record.new_value = result.value();
                    record.success = true;
                } else {
                    record.success = false;
                    record.error_message = "Failed to create UID mapping";
                }
            } else {
                // Generate new UID without mapping
                uid_mapping temp;
                auto result = temp.get_or_create(record.original_value);
                if (result.is_ok()) {
                    dataset.set_string(tag, element->vr(), result.value());
                    record.new_value = result.value();
                    record.success = true;
                } else {
                    record.success = false;
                    record.error_message = "Failed to generate new UID";
                }
            }
            break;

        case tag_action::hash: {
            auto hashed = hash_value(record.original_value);
            dataset.set_string(tag, element->vr(), hashed);
            record.new_value = hashed;
            record.success = true;
            break;
        }

        case tag_action::encrypt: {
            auto result = encrypt_value(record.original_value);
            if (result.is_ok()) {
                dataset.set_string(tag, element->vr(), result.value());
                record.new_value = result.value();
                record.success = true;
            } else {
                record.success = false;
                record.error_message = "Encryption failed: no key configured";
            }
            break;
        }

        case tag_action::shift_date: {
            if (date_offset_.has_value()) {
                auto shifted = shift_date(record.original_value);
                dataset.set_string(tag, element->vr(), shifted);
                record.new_value = shifted;
                record.success = true;
            } else {
                // No offset set - empty the date instead
                dataset.set_string(tag, element->vr(), "");
                record.new_value = "";
                record.success = true;
            }
            break;
        }
    }

    return record;
}

auto anonymizer::shift_date(std::string_view date_string) const -> std::string {
    if (date_string.empty() || !date_offset_.has_value()) {
        return "";
    }

    // Parse DICOM date format (YYYYMMDD)
    if (date_string.length() < 8) {
        return std::string(date_string);
    }

    try {
        int year = std::stoi(std::string(date_string.substr(0, 4)));
        int month = std::stoi(std::string(date_string.substr(4, 2)));
        int day = std::stoi(std::string(date_string.substr(6, 2)));

        // Create time point from date
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;

        auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        // Apply offset
        time_point += date_offset_.value();

        // Convert back to date string
        auto shifted_time = std::chrono::system_clock::to_time_t(time_point);
        std::tm* shifted_tm = std::localtime(&shifted_time);

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << (shifted_tm->tm_year + 1900)
            << std::setw(2) << (shifted_tm->tm_mon + 1)
            << std::setw(2) << shifted_tm->tm_mday;

        return oss.str();

    } catch (const std::exception&) {
        return std::string(date_string);
    }
}

auto anonymizer::hash_value(std::string_view value) const -> std::string {
    // Simple hash implementation using std::hash
    // In production, use a proper cryptographic hash like SHA-256
    std::string to_hash = std::string(value);

    if (hash_salt_.has_value()) {
        to_hash = hash_salt_.value() + to_hash;
    }

    auto hash = std::hash<std::string>{}(to_hash);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}

auto anonymizer::encrypt_value(std::string_view /*value*/) const
    -> kcenon::common::Result<std::string> {
    if (encryption_key_.empty()) {
        return kcenon::common::make_error<std::string>(
            1, "No encryption key configured"
        );
    }

    // Placeholder for actual encryption implementation
    // In production, use AES-256-GCM or similar
    return kcenon::common::make_error<std::string>(
        1, "Encryption not yet implemented"
    );
}

void anonymizer::initialize_profile_actions() {
    // Profile actions are computed on-demand in get_profile_actions()
    // This method can be used for any initialization if needed
}

} // namespace pacs::security
