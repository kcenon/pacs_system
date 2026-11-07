// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file qr_index.cpp
 * @brief Answer Query/Retrieve requests from the index database
 */

#include "kcenon/pacs/services/qr_index.h"

#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/utf8_text.h"
#include "kcenon/pacs/encoding/vr_type.h"
#include "kcenon/pacs/services/qr_matching.h"
#include "kcenon/pacs/storage/index_database.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::services {

namespace {

namespace tags = core::tags;
using encoding::vr_type;

struct key_rule {
    core::dicom_tag tag;
    vr_type vr;
    key_matching matching;
};

/// One candidate entity's stored values, by the tag a key would name.
using row_values = std::map<core::dicom_tag, std::string>;

const std::vector<key_rule> patient_rules{
    {tags::patient_id, vr_type::LO, key_matching::text},
    {tags::patient_name, vr_type::PN, key_matching::person_name},
    {tags::patient_birth_date, vr_type::DA, key_matching::date},
    {tags::patient_sex, vr_type::CS, key_matching::text},
};
const std::vector<key_rule> study_rules{
    {tags::study_instance_uid, vr_type::UI, key_matching::uid},
    {tags::study_date, vr_type::DA, key_matching::date},
    {tags::study_time, vr_type::TM, key_matching::time},
    {tags::accession_number, vr_type::SH, key_matching::text},
    {tags::study_id, vr_type::SH, key_matching::text},
    {tags::referring_physician_name, vr_type::PN, key_matching::person_name},
    {tags::study_description, vr_type::LO, key_matching::text},
    {tags::modalities_in_study, vr_type::CS, key_matching::text},
};
const std::vector<key_rule> series_rules{
    {tags::series_instance_uid, vr_type::UI, key_matching::uid},
    {tags::modality, vr_type::CS, key_matching::text},
    {tags::series_number, vr_type::IS, key_matching::number},
    {tags::series_description, vr_type::LO, key_matching::text},
};
const std::vector<key_rule> image_rules{
    {tags::sop_instance_uid, vr_type::UI, key_matching::uid},
    {tags::instance_number, vr_type::IS, key_matching::number},
    {tags::sop_class_uid, vr_type::UI, key_matching::uid},
};

[[nodiscard]] std::vector<key_rule> rules_for(qr_model model, query_level level) {
    switch (level) {
        case query_level::patient:
            return patient_rules;
        case query_level::study: {
            auto rules = study_rules;
            if (model == qr_model::study_root) {
                // Study Root has no PATIENT level, so these keys belong to STUDY (Table C.6-5).
                rules.insert(rules.end(), patient_rules.begin(), patient_rules.end());
            }
            return rules;
        }
        case query_level::series:
            return series_rules;
        case query_level::image:
            return image_rules;
    }
    return {};
}

/// The level whose Unique Key is the tag, or nothing when it is not a Unique Key.
[[nodiscard]] std::optional<query_level> unique_key_level(core::dicom_tag tag) {
    if (tag == tags::patient_id) return query_level::patient;
    if (tag == tags::study_instance_uid) return query_level::study;
    if (tag == tags::series_instance_uid) return query_level::series;
    if (tag == tags::sop_instance_uid) return query_level::image;
    return std::nullopt;
}

/// Unique Keys of the levels above the query level, top down (C.4.1.2.1).
[[nodiscard]] std::vector<core::dicom_tag> keys_above(qr_model model, query_level level) {
    std::vector<core::dicom_tag> above;
    if (level != query_level::patient && model != qr_model::study_root) {
        above.push_back(tags::patient_id);
    }
    if (level == query_level::series || level == query_level::image) {
        above.push_back(tags::study_instance_uid);
    }
    if (level == query_level::image) {
        above.push_back(tags::series_instance_uid);
    }
    return above;
}

[[nodiscard]] std::string trimmed_value(const core::dicom_dataset& keys, core::dicom_tag tag) {
    auto value = keys.get_string(tag, "");
    const auto first = value.find_first_not_of(" \0", 0, 2);
    if (first == std::string::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(" \0", std::string::npos, 2)-first+1);
}

[[nodiscard]] std::vector<std::string> split_values(const std::string& text) {
    std::vector<std::string> values;
    std::string::size_type start = 0;
    for (;;) {
        const auto delimiter = text.find('\\', start);
        auto value = text.substr(start, delimiter == std::string::npos ? delimiter : delimiter-start);
        if (!value.empty()) {
            values.push_back(std::move(value));
        }
        if (delimiter == std::string::npos) {
            return values;
        }
        start = delimiter+1;
    }
}

/// A single non-empty value, which is what a Unique Key above the level must hold.
[[nodiscard]] std::optional<std::string> single_value(const core::dicom_dataset& keys, core::dicom_tag tag) {
    auto value = trimmed_value(keys, tag);
    if (value.empty() || value.find('\\') != std::string::npos) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] row_values values_of(const storage::patient_record& patient) {
    return {{tags::patient_id, patient.patient_id},
            {tags::patient_name, patient.patient_name},
            {tags::patient_birth_date, patient.birth_date},
            {tags::patient_sex, patient.sex}};
}

[[nodiscard]] row_values values_of(const storage::study_record& study) {
    return {{tags::study_instance_uid, study.study_uid},
            {tags::study_date, study.study_date},
            {tags::study_time, study.study_time},
            {tags::accession_number, study.accession_number},
            {tags::study_id, study.study_id},
            {tags::referring_physician_name, study.referring_physician},
            {tags::study_description, study.study_description},
            {tags::modalities_in_study, study.modalities_in_study}};
}

[[nodiscard]] row_values values_of(const storage::series_record& series) {
    return {{tags::series_instance_uid, series.series_uid},
            {tags::modality, series.modality},
            {tags::series_number, series.series_number ? std::to_string(*series.series_number) : std::string{}},
            {tags::series_description, series.series_description}};
}

[[nodiscard]] row_values values_of(const storage::instance_record& instance) {
    return {{tags::sop_instance_uid, instance.sop_uid},
            {tags::instance_number, instance.instance_number ? std::to_string(*instance.instance_number) : std::string{}},
            {tags::sop_class_uid, instance.sop_class_uid}};
}

/// The studies of one exact Patient ID. list_studies matches a pattern, so the patient decides.
[[nodiscard]] kcenon::pacs::Result<std::vector<storage::study_record>> studies_of_patient(storage::index_database& database,
                                                                                         const std::string& patient_id) {
    auto listed = database.list_studies(patient_id);
    if (listed.is_err()) {
        return listed.error();
    }
    auto studies = std::move(listed.value());
    const auto patient = database.find_patient(patient_id);
    std::erase_if(studies, [&patient](const storage::study_record& study) {
        return !patient || study.patient_pk != patient->pk;
    });
    return studies;
}

/// Whether the study exists where the Identifier's upper keys put it.
[[nodiscard]] kcenon::pacs::Result<bool> study_under_patient(storage::index_database& database, qr_model model,
                                                             const std::optional<std::string>& patient_id,
                                                             const std::string& study_uid) {
    if (model == qr_model::study_root) {
        return true;
    }
    auto studies = studies_of_patient(database, *patient_id);
    if (studies.is_err()) {
        return studies.error();
    }
    return std::any_of(studies.value().begin(), studies.value().end(),
                       [&study_uid](const storage::study_record& study) { return study.study_uid == study_uid; });
}

[[nodiscard]] kcenon::pacs::Result<std::vector<storage::series_record>> series_of_study(storage::index_database& database,
                                                                                        const std::string& study_uid) {
    return database.list_series(study_uid);
}

/// Everything the Identifier's keys need, resolved once for all candidates.
class find_evaluation {
public:
    find_evaluation(const find_request& request, core::dicom_dataset keys)
        : request_(request), keys_(std::move(keys)), rules_(rules_for(request.model, request.level)) {
        for (const auto& [tag, element] : keys_) {
            if (tag == tags::query_retrieve_level || tag == tags::specific_character_set || tag == tags::retrieve_ae_title ||
                tag.element() == 0x0000 || is_upper_key(tag)) {
                continue;
            }
            const auto rule = std::find_if(rules_.begin(), rules_.end(), [tag = tag](const key_rule& candidate) { return candidate.tag == tag; });
            if (rule == rules_.end()) {
                ignored_ = true;
            } else {
                requested_.push_back(*rule);
            }
        }
    }

    [[nodiscard]] bool optional_keys_ignored() const noexcept { return ignored_; }

    void consider(const row_values& values, std::vector<core::dicom_dataset>& matches) const {
        for (const auto& rule : requested_) {
            const auto stored = values.find(rule.tag);
            const std::string empty;
            if (!key_matches(rule.matching, keys_.get_string(rule.tag, ""), stored == values.end() ? empty : stored->second)) {
                return;
            }
        }
        matches.push_back(response(values));
    }

private:
    [[nodiscard]] bool is_upper_key(core::dicom_tag tag) const {
        const auto above = keys_above(request_.model, request_.level);
        return std::find(above.begin(), above.end(), tag) != above.end();
    }

    [[nodiscard]] core::dicom_dataset response(const row_values& values) const {
        core::dicom_dataset identifier;
        identifier.set_string(tags::query_retrieve_level, vr_type::CS, std::string(to_string(request_.level)));
        identifier.set_string(tags::retrieve_ae_title, vr_type::AE, std::string(request_.called_ae));
        for (const auto tag : keys_above(request_.model, request_.level)) {
            identifier.set_string(tag, tag == tags::patient_id ? vr_type::LO : vr_type::UI, keys_.get_string(tag, ""));
        }
        for (const auto& rule : requested_) {
            const auto stored = values.find(rule.tag);
            identifier.set_string(rule.tag, rule.vr, stored == values.end() ? std::string{} : stored->second);
        }
        // The index holds UTF-8. This attribute is allowed only when the text needs it (C.4.1.1.3.2).
        core::declare_utf8_text(identifier);
        return identifier;
    }

    const find_request& request_;
    core::dicom_dataset keys_;
    std::vector<key_rule> rules_;
    std::vector<key_rule> requested_;
    bool ignored_{false};
};

[[nodiscard]] find_outcome failed(qr_failure failure) {
    find_outcome outcome;
    outcome.failure = failure;
    return outcome;
}

[[nodiscard]] find_outcome find_patients(storage::index_database& database, const find_evaluation& evaluation) {
    find_outcome outcome;
    auto patients = database.search_patients(storage::patient_query{});
    if (patients.is_err()) {
        return failed(qr_failure::unable_to_process);
    }
    for (const auto& patient : patients.value()) {
        evaluation.consider(values_of(patient), outcome.matches);
    }
    return outcome;
}

[[nodiscard]] find_outcome find_studies(storage::index_database& database, const find_evaluation& evaluation, qr_model model,
                                        const core::dicom_dataset& keys, const std::optional<std::string>& patient_id) {
    std::vector<storage::study_record> studies;
    const auto uids = split_values(trimmed_value(keys, tags::study_instance_uid));
    if (model != qr_model::study_root) {
        auto listed = studies_of_patient(database, *patient_id);
        if (listed.is_err()) {
            return failed(qr_failure::unable_to_process);
        }
        studies = std::move(listed.value());
    } else if (uids.empty()) {
        auto listed = database.search_studies(storage::study_query{});
        if (listed.is_err()) {
            return failed(qr_failure::unable_to_process);
        }
        studies = std::move(listed.value());
    } else {
        // A Unique Key selects at most one study each, so skip reading the whole table.
        for (const auto& uid : uids) {
            storage::study_query query;
            query.study_uid = uid;
            auto listed = database.search_studies(query);
            if (listed.is_err()) {
                return failed(qr_failure::unable_to_process);
            }
            std::move(listed.value().begin(), listed.value().end(), std::back_inserter(studies));
        }
    }
    find_outcome outcome;
    std::map<int64_t, row_values> patients;
    for (const auto& study : studies) {
        auto values = values_of(study);
        if (model == qr_model::study_root) {
            auto known = patients.find(study.patient_pk);
            if (known == patients.end()) {
                const auto patient = database.find_patient_by_pk(study.patient_pk);
                if (!patient) {
                    return failed(qr_failure::unable_to_process);
                }
                known = patients.emplace(study.patient_pk, values_of(*patient)).first;
            }
            values.insert(known->second.begin(), known->second.end());
        }
        evaluation.consider(values, outcome.matches);
    }
    return outcome;
}

[[nodiscard]] find_outcome find_series(storage::index_database& database, const find_evaluation& evaluation, qr_model model,
                                       const std::optional<std::string>& patient_id, const std::string& study_uid) {
    find_outcome outcome;
    auto placed = study_under_patient(database, model, patient_id, study_uid);
    if (placed.is_err()) {
        return failed(qr_failure::unable_to_process);
    }
    if (!placed.value()) {
        return outcome;
    }
    auto series = series_of_study(database, study_uid);
    if (series.is_err()) {
        return failed(qr_failure::unable_to_process);
    }
    for (const auto& item : series.value()) {
        evaluation.consider(values_of(item), outcome.matches);
    }
    return outcome;
}

[[nodiscard]] find_outcome find_images(storage::index_database& database, const find_evaluation& evaluation, qr_model model,
                                       const std::optional<std::string>& patient_id, const std::string& study_uid,
                                       const std::string& series_uid) {
    find_outcome outcome;
    auto placed = study_under_patient(database, model, patient_id, study_uid);
    if (placed.is_err()) {
        return failed(qr_failure::unable_to_process);
    }
    auto series = series_of_study(database, study_uid);
    if (series.is_err()) {
        return failed(qr_failure::unable_to_process);
    }
    const auto in_study = std::any_of(series.value().begin(), series.value().end(),
                                      [&series_uid](const storage::series_record& item) { return item.series_uid == series_uid; });
    if (!placed.value() || !in_study) {
        return outcome;
    }
    auto instances = database.list_instances(series_uid);
    if (instances.is_err()) {
        return failed(qr_failure::unable_to_process);
    }
    for (const auto& instance : instances.value()) {
        evaluation.consider(values_of(instance), outcome.matches);
    }
    return outcome;
}

}  // namespace

find_outcome find_in_index(storage::index_database& database, const find_request& request) {
    auto keys = core::utf8_text_copy(request.keys);
    for (const auto tag : keys_above(request.model, request.level)) {
        if (!single_value(keys, tag)) {
            return failed(qr_failure::identifier_mismatch);
        }
    }
    const auto patient_id = request.level != query_level::patient && request.model != qr_model::study_root
                                ? single_value(keys, tags::patient_id)
                                : std::nullopt;
    const auto study_uid = single_value(keys, tags::study_instance_uid).value_or("");
    const auto series_uid = single_value(keys, tags::series_instance_uid).value_or("");
    find_evaluation evaluation(request, keys);
    auto outcome = [&] {
        switch (request.level) {
            case query_level::patient:
                return find_patients(database, evaluation);
            case query_level::study:
                return find_studies(database, evaluation, request.model, keys, patient_id);
            case query_level::series:
                return find_series(database, evaluation, request.model, patient_id, study_uid);
            case query_level::image:
                return find_images(database, evaluation, request.model, patient_id, study_uid, series_uid);
        }
        return failed(qr_failure::identifier_mismatch);
    }();
    outcome.optional_keys_ignored = evaluation.optional_keys_ignored();
    return outcome;
}

retrieve_selection select_from_index(storage::index_database& database, const retrieve_request& request) {
    retrieve_selection selection;
    const auto refuse = [&selection] {
        selection.failure = qr_failure::identifier_mismatch;
        return selection;
    };
    const auto cannot_process = [&selection] {
        selection.failure = qr_failure::unable_to_process;
        return selection;
    };
    const auto keys = core::utf8_text_copy(request.keys);
    const auto level = parse_query_level(trimmed_value(keys, tags::query_retrieve_level));
    if (!level || !level_in_model(request.model, *level)) {
        return refuse();
    }
    // A Unique Key below the level names less than the level would deliver.
    for (const auto tag : {tags::study_instance_uid, tags::series_instance_uid, tags::sop_instance_uid}) {
        if (static_cast<int>(*unique_key_level(tag)) > static_cast<int>(*level) && !trimmed_value(keys, tag).empty()) {
            return refuse();
        }
    }
    for (const auto tag : keys_above(request.model, *level)) {
        if (!single_value(keys, tag)) {
            return refuse();
        }
    }
    const auto patient_id = *level == query_level::patient || request.model != qr_model::study_root
                                ? single_value(keys, tags::patient_id)
                                : std::nullopt;
    if (*level == query_level::patient && !patient_id) {
        return refuse();
    }

    std::vector<std::string> study_uids;
    if (*level == query_level::patient) {
        auto studies = studies_of_patient(database, *patient_id);
        if (studies.is_err()) {
            return cannot_process();
        }
        for (const auto& study : studies.value()) {
            study_uids.push_back(study.study_uid);
        }
    } else {
        study_uids = split_values(trimmed_value(keys, tags::study_instance_uid));
        if (study_uids.empty()) {
            return refuse();
        }
    }

    // UIDs repeated in a list must still store each object once.
    std::set<std::string> selected;
    std::vector<storage::instance_record> instances;
    const auto wanted_series = split_values(trimmed_value(keys, tags::series_instance_uid));
    const auto wanted_images = split_values(trimmed_value(keys, tags::sop_instance_uid));
    if ((*level == query_level::series || *level == query_level::image) && wanted_series.empty()) {
        return refuse();
    }
    if (*level == query_level::image && wanted_images.empty()) {
        return refuse();
    }
    const auto chosen = [](const std::vector<std::string>& wanted, const std::string& uid) {
        return wanted.empty() || std::find(wanted.begin(), wanted.end(), uid) != wanted.end();
    };
    for (const auto& study_uid : study_uids) {
        auto placed = *level == query_level::patient
                          ? kcenon::pacs::Result<bool>::ok(true)
                          : study_under_patient(database, request.model, patient_id, study_uid);
        if (placed.is_err()) {
            return cannot_process();
        }
        if (!placed.value()) {
            continue;
        }
        auto series = series_of_study(database, study_uid);
        if (series.is_err()) {
            return cannot_process();
        }
        for (const auto& item : series.value()) {
            if (!chosen(wanted_series, item.series_uid)) {
                continue;
            }
            auto listed = database.list_instances(item.series_uid);
            if (listed.is_err()) {
                return cannot_process();
            }
            for (auto& instance : listed.value()) {
                if (chosen(wanted_images, instance.sop_uid) && selected.insert(instance.sop_uid).second) {
                    instances.push_back(std::move(instance));
                }
            }
        }
    }
    for (const auto& instance : instances) {
        auto file = core::dicom_file::open(instance.file_path);
        if (file.is_ok()) {
            selection.files.push_back(std::move(file.value()));
        } else {
            selection.unavailable_uids.push_back(instance.sop_uid);
        }
    }
    return selection;
}

}  // namespace kcenon::pacs::services
