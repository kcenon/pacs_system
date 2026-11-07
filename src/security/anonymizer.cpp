// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file anonymizer.cpp
 * @brief Implementation of DICOM de-identification/anonymization
 *
 * @copyright Copyright (c) 2025
 */

#include "kcenon/pacs/security/anonymizer.h"
#include "anonymization_sequence_rules.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

#ifdef PACS_WITH_DIGITAL_SIGNATURES
    #include <openssl/evp.h>
    #include <openssl/rand.h>
#endif

namespace kcenon::pacs::security {

using namespace kcenon::pacs::core;

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
    , private_tag_action_{other.private_tag_action_}
    , detailed_reporting_{other.detailed_reporting_} {}

anonymizer::anonymizer(anonymizer&& other) noexcept
    : profile_{other.profile_}
    , custom_actions_{std::move(other.custom_actions_)}
    , date_offset_{other.date_offset_}
    , encryption_key_{std::move(other.encryption_key_)}
    , hash_salt_{std::move(other.hash_salt_)}
    , private_tag_action_{other.private_tag_action_}
    , detailed_reporting_{other.detailed_reporting_} {}

auto anonymizer::operator=(const anonymizer& other) -> anonymizer& {
    if (this != &other) {
        profile_ = other.profile_;
        custom_actions_ = other.custom_actions_;
        date_offset_ = other.date_offset_;
        encryption_key_ = other.encryption_key_;
        hash_salt_ = other.hash_salt_;
        private_tag_action_ = other.private_tag_action_;
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
        private_tag_action_ = other.private_tag_action_;
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

    apply_actions(dataset, profile_actions, mapping, report, 0);

    return report;
}

void anonymizer::record_action(anonymization_report& report,
                               tag_action_record record, bool sequence) {
    ++report.total_tags_processed;
    if (record.success) {
        switch (record.action) {
            case tag_action::remove:
            case tag_action::remove_or_empty: ++report.tags_removed; break;
            case tag_action::empty: ++report.tags_emptied; break;
            case tag_action::replace:
            case tag_action::clean:
            case tag_action::encrypt: ++report.tags_replaced; break;
            case tag_action::replace_uid:
                // A U sequence is a retained container, not itself a UID.
                if (sequence) { ++report.tags_kept; }
                else { ++report.uids_replaced; }
                break;
            case tag_action::keep: ++report.tags_kept; break;
            case tag_action::shift_date: ++report.dates_shifted; break;
            case tag_action::hash: ++report.values_hashed; break;
        }
    } else if (!record.error_message.empty()) {
        report.errors.push_back(record.error_message);
    }
    if (detailed_reporting_) {
        report.action_records.push_back(std::move(record));
    }
}

void anonymizer::apply_actions(
    dicom_dataset& dataset,
    const std::map<dicom_tag, tag_action_config>& actions,
    uid_mapping& mapping, anonymization_report& report, std::size_t depth) {
    // Snapshot tags because actions may erase or replace map elements.
    std::vector<dicom_tag> tags;
    tags.reserve(dataset.size());
    for (const auto& [tag, element] : dataset) {
        tags.push_back(tag);
    }
    for (const auto tag : tags) {
        if ((private_tag_action_ == private_tag_action::remove_all && tag.is_private()) ||
            (private_tag_action_ == private_tag_action::remove_data && tag.is_private_data())) {
            dataset.remove(tag);
            ++report.private_tags_removed;
            continue;
        }
        auto* element = dataset.get(tag);
        const auto found = actions.find(tag);
        if (!element->is_sequence()) {
            // UN may hide a defined-length implicit-VR sequence absent from
            // the dictionary. Never pass opaque bytes through as a K value.
            if (element->vr() == encoding::vr_type::UN && !element->raw_data().empty()) {
                const auto action = found == actions.end() ? tag_action::empty : found->second.action;
                if (action != tag_action::remove && action != tag_action::remove_or_empty &&
                    action != tag_action::empty) {
                    tag_action_record record;
                    record.tag = tag;
                    record.action = action;
                    record.success = false;
                    record.error_message = "Cannot clean unknown representation at " + tag.to_string();
                    dataset.insert(dicom_element{tag, encoding::vr_type::UN});
                    record_action(report, std::move(record));
                    continue;
                }
                if (found == actions.end()) {
                    dataset.insert(dicom_element{tag, encoding::vr_type::UN});
                    tag_action_record record;
                    record.tag = tag;
                    record.action = tag_action::empty;
                    record_action(report, std::move(record));
                    continue;
                }
            }
            if (found != actions.end()) {
                record_action(report, apply_action(dataset, tag, found->second, &mapping));
            }
            continue;
        }

        // Unknown sequences retain the conservative guard at every depth.
        const auto action = found == actions.end() ? tag_action::empty : found->second.action;
        tag_action_record record;
        record.tag = tag;
        record.action = action;
        switch (action) {
            case tag_action::remove:
            case tag_action::remove_or_empty:
                dataset.remove(tag);
                break;
            case tag_action::empty:
                dataset.insert(dicom_element{tag, encoding::vr_type::SQ});
                break;
            case tag_action::replace: {
                // D: a non-empty sequence with a dummy item. IOD-specific
                // mandatory contents still require validation by the caller.
                dicom_element replacement{tag, encoding::vr_type::SQ};
                replacement.sequence_items().emplace_back();
                dataset.insert(std::move(replacement));
                break;
            }
            case tag_action::keep:
            case tag_action::clean:
            case tag_action::replace_uid:
                if (!element->raw_data().empty() ||
                    (depth >= 64 && !element->sequence_items().empty())) {
                    record.success = false;
                    record.error_message = "Cannot clean sequence " + tag.to_string() +
                        (element->raw_data().empty() ? ": nesting limit exceeded" : ": unparsed payload");
                    dataset.insert(dicom_element{tag, encoding::vr_type::SQ});
                } else {
                    for (auto& item : element->sequence_items()) {
                        apply_actions(item, actions, mapping, report, depth + 1);
                    }
                }
                break;
            default:
                record.success = false;
                record.error_message = "Unsupported sequence action at " + tag.to_string();
                dataset.insert(dicom_element{tag, encoding::vr_type::SQ});
                break;
        }
        record_action(report, std::move(record), true);
    }
}

auto anonymizer::get_profile() const noexcept -> anonymization_profile {
    return profile_;
}

void anonymizer::set_profile(anonymization_profile profile) {
    profile_ = profile;
    initialize_profile_actions();
}

void anonymizer::set_private_tag_action(private_tag_action action) {
    private_tag_action_ = action;
}

auto anonymizer::get_private_tag_action() const noexcept
    -> private_tag_action {
    return private_tag_action_;
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
        actions[tags::patient_age] = tag_action_config::make_remove();
        actions[tags::patient_address] = tag_action_config::make_remove();
        actions[tags::patient_comments] = tag_action_config::make_remove();
        for (const auto tag : {dicom_tag{0x0010, 0x0021}, // Issuer of Patient ID
                               dicom_tag{0x0010, 0x1000}, // Other Patient IDs
                               dicom_tag{0x0010, 0x1001}, // Other Patient Names
                               dicom_tag{0x0010, 0x1005}, // Patient Birth Name
                               dicom_tag{0x0010, 0x1060}, // Mother's Birth Name
                               dicom_tag{0x0010, 0x2154}, // Telephone Numbers
                               dicom_tag{0x0010, 0x2155}}) { // Telecom Information
            actions[tag] = tag_action_config::make_remove();
        }
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
        for (const auto tag : {dicom_tag{0x0040, 0x1001}, // Requested Procedure ID
                               dicom_tag{0x0040, 0x0009}, // Scheduled Procedure Step ID
                               dicom_tag{0x0040, 0x0253}}) { // Performed Procedure Step ID
            actions[tag] = tag_action_config::make_empty();
        }
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

    detail::add_sequence_and_uid_actions(actions, profile);
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

    record.original_value = element->as_string().unwrap_or("");

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

        case tag_action::clean:
        case tag_action::replace:
            dataset.set_string(tag, element->vr(), config.replacement_value);
            record.new_value = config.replacement_value;
            record.success = true;
            break;

        case tag_action::replace_uid: {
            // UI may have VM > 1. Each component shares the same mapping with
            // scalar values, nested references, and subsequent files.
            uid_mapping temporary;
            auto& uid_map = mapping != nullptr ? *mapping : temporary;
            std::string replacement;
            std::size_t start = 0;
            while (start < record.original_value.size()) {
                const auto end = record.original_value.find('\\', start);
                const auto uid = record.original_value.substr(start, end - start);
                if (!uid.empty()) {
                    auto result = uid_map.get_or_create(uid);
                    if (result.is_err()) {
                        record.success = false;
                        record.error_message = "Failed to create UID mapping at " + tag.to_string();
                        break;
                    }
                    replacement += result.value();
                }
                if (end == std::string::npos) { break; }
                replacement += '\\';
                start = end + 1;
            }
            if (record.success) {
                dataset.set_string(tag, element->vr(), replacement);
                record.new_value = std::move(replacement);
            }
            break;
        }

        case tag_action::hash: {
            auto hashed = hash_value(record.original_value);
            if (hashed.empty()) {
                record.success = false;
                record.error_message = "Hash computation failed";
            } else {
                dataset.set_string(tag, element->vr(), hashed);
                record.new_value = hashed;
                record.success = true;
            }
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
                record.error_message = result.error().message;
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
        std::tm shifted_tm{};
#if defined(_WIN32)
        localtime_s(&shifted_tm, &shifted_time);
#else
        localtime_r(&shifted_time, &shifted_tm);
#endif

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << (shifted_tm.tm_year + 1900)
            << std::setw(2) << (shifted_tm.tm_mon + 1)
            << std::setw(2) << shifted_tm.tm_mday;

        return oss.str();

    } catch (const std::exception&) {
        return std::string(date_string);
    }
}

auto anonymizer::hash_value(std::string_view value) const -> std::string {
    std::string to_hash = std::string(value);

    if (hash_salt_.has_value()) {
        to_hash = hash_salt_.value() + to_hash;
    }

#ifdef PACS_WITH_DIGITAL_SIGNATURES
    auto* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        return {};
    }

    struct md_ctx_deleter {
        void operator()(EVP_MD_CTX* c) const { EVP_MD_CTX_free(c); }
    };
    std::unique_ptr<EVP_MD_CTX, md_ctx_deleter> ctx_guard(ctx);

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1
        || EVP_DigestUpdate(ctx, to_hash.data(), to_hash.size()) != 1) {
        return {};
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(ctx, digest.data(), &digest_len) != 1) {
        return {};
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
#else
    // Non-cryptographic fallback when OpenSSL is unavailable
    auto hash = std::hash<std::string>{}(to_hash);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
#endif
}

auto anonymizer::encrypt_value(std::string_view value) const
    -> kcenon::common::Result<std::string> {
    if (encryption_key_.empty()) {
        return kcenon::common::make_error<std::string>(
            1, "No encryption key configured");
    }

#ifdef PACS_WITH_DIGITAL_SIGNATURES
    constexpr int iv_length = 12;
    constexpr int tag_length = 16;

    // Generate random IV
    std::array<unsigned char, iv_length> iv{};
    if (RAND_bytes(iv.data(), iv_length) != 1) {
        return kcenon::common::make_error<std::string>(
            2, "Failed to generate random IV");
    }

    // Create cipher context with RAII
    struct cipher_ctx_deleter {
        void operator()(EVP_CIPHER_CTX* c) const { EVP_CIPHER_CTX_free(c); }
    };

    std::unique_ptr<EVP_CIPHER_CTX, cipher_ctx_deleter> ctx(
        EVP_CIPHER_CTX_new());
    if (!ctx) {
        return kcenon::common::make_error<std::string>(
            3, "Failed to create cipher context");
    }

    // Initialize AES-256-GCM
    if (EVP_EncryptInit_ex(
            ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
        != 1) {
        return kcenon::common::make_error<std::string>(
            4, "Failed to initialize AES-256-GCM");
    }

    if (EVP_CIPHER_CTX_ctrl(
            ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv_length, nullptr)
        != 1) {
        return kcenon::common::make_error<std::string>(
            5, "Failed to set IV length");
    }

    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
                           encryption_key_.data(), iv.data())
        != 1) {
        return kcenon::common::make_error<std::string>(
            6, "Failed to set encryption key and IV");
    }

    // Encrypt
    std::vector<unsigned char> ciphertext(value.size() + EVP_MAX_BLOCK_LENGTH);
    int out_len = 0;
    if (EVP_EncryptUpdate(
            ctx.get(), ciphertext.data(), &out_len,
            reinterpret_cast<const unsigned char*>(value.data()),
            static_cast<int>(value.size()))
        != 1) {
        return kcenon::common::make_error<std::string>(
            7, "Encryption failed");
    }
    int ciphertext_len = out_len;

    // Finalize
    if (EVP_EncryptFinal_ex(
            ctx.get(), ciphertext.data() + out_len, &out_len)
        != 1) {
        return kcenon::common::make_error<std::string>(
            8, "Encryption finalization failed");
    }
    ciphertext_len += out_len;

    // Get authentication tag
    std::array<unsigned char, tag_length> tag{};
    if (EVP_CIPHER_CTX_ctrl(
            ctx.get(), EVP_CTRL_GCM_GET_TAG, tag_length, tag.data())
        != 1) {
        return kcenon::common::make_error<std::string>(
            9, "Failed to get authentication tag");
    }

    // Encode as hex: IV || ciphertext || tag
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < iv_length; ++i) {
        oss << std::setw(2) << static_cast<int>(iv[i]);
    }
    for (int i = 0; i < ciphertext_len; ++i) {
        oss << std::setw(2) << static_cast<int>(ciphertext[i]);
    }
    for (int i = 0; i < tag_length; ++i) {
        oss << std::setw(2) << static_cast<int>(tag[i]);
    }

    return kcenon::common::Result<std::string>(oss.str());
#else
    (void)value;
    return kcenon::common::make_error<std::string>(
        1, "Encryption requires OpenSSL (build with PACS_WITH_DIGITAL_SIGNATURES)");
#endif
}

void anonymizer::initialize_profile_actions() {
    // Profile actions are computed on-demand in get_profile_actions()
    // This method can be used for any initialization if needed
}

} // namespace kcenon::pacs::security
