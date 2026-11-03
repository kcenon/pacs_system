// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file instance_archive.cpp
 * @brief Keep one received object: its file and its index rows, or neither
 */

#include "kcenon/pacs/storage/instance_archive.h"

#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/utf8_text.h"
#include "kcenon/pacs/storage/file_storage.h"
#include "kcenon/pacs/storage/index_database.h"
#include "kcenon/pacs/storage/instance_record.h"

#include <array>
#include <charconv>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace kcenon::pacs::storage {

namespace {

namespace tags = core::tags;

constexpr core::dicom_tag body_part_examined_tag{0x0018, 0x0015};
constexpr core::dicom_tag number_of_frames_tag{0x0028, 0x0008};

/// Finding a row and creating it are separate statements, so two writers would both create it.
[[nodiscard]] std::mutex& index_writer() {
    static std::mutex lock;
    return lock;
}

/// Offers of one SOP Instance run one at a time; unrelated ones rarely share a lock.
[[nodiscard]] std::mutex& lock_for(const std::string& sop_uid) {
    static std::array<std::mutex, 64> locks;
    return locks[std::hash<std::string>{}(sop_uid) % locks.size()];
}

/// The value of an IS or US attribute, whichever way the object encodes it.
[[nodiscard]] std::optional<int> number_of(const core::dicom_dataset& dataset, core::dicom_tag tag) {
    const auto text = dataset.get_string(tag);
    const auto first = text.find_first_not_of(' ');
    if (first == std::string::npos) {
        return std::nullopt;
    }
    int value = 0;
    const auto* end = text.data()+text.size();
    const auto start = text[first] == '+' ? first+1 : first;
    const auto parsed = std::from_chars(text.data()+start, end, value);
    return parsed.ec == std::errc{} ? std::optional<int>{value} : std::nullopt;
}

/// The index takes only the enumerated values of Patient's Sex; others are not worth losing an object over.
[[nodiscard]] std::string indexed_sex(std::string value) {
    return value == "M" || value == "F" || value == "O" ? value : std::string{};
}

/// A failure message when the index would refuse a row, or no way to find the object afterwards.
[[nodiscard]] std::optional<std::string> what_is_missing(const core::dicom_dataset& text) {
    if (text.get_string(tags::patient_id).empty()) {
        return "Patient ID is empty: no query could reach this object";
    }
    for (const auto& [tag, name] : {std::pair{tags::sop_class_uid, "SOP Class UID"},
                                    std::pair{tags::sop_instance_uid, "SOP Instance UID"},
                                    std::pair{tags::study_instance_uid, "Study Instance UID"},
                                    std::pair{tags::series_instance_uid, "Series Instance UID"}}) {
        const auto uid = text.get_string(tag);
        if (uid.empty()) {
            return std::string(name) + " is missing";
        }
        if (!is_valid_uid(uid)) {
            return std::string(name) + " is not a UID: " + uid;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> index_rows(index_database& database, const core::dicom_dataset& text,
                                                    const std::filesystem::path& file_path, std::string_view syntax_uid) {
    auto patient = database.upsert_patient(text.get_string(tags::patient_id), text.get_string(tags::patient_name),
                                           text.get_string(tags::patient_birth_date), indexed_sex(text.get_string(tags::patient_sex)));
    if (patient.is_err()) {
        return "patient row: "+patient.error().message;
    }
    auto study = database.upsert_study(patient.value(), text.get_string(tags::study_instance_uid), text.get_string(tags::study_id),
                                       text.get_string(tags::study_date), text.get_string(tags::study_time),
                                       text.get_string(tags::accession_number), text.get_string(tags::referring_physician_name),
                                       text.get_string(tags::study_description));
    if (study.is_err()) {
        return "study row: "+study.error().message;
    }
    auto series = database.upsert_series(study.value(), text.get_string(tags::series_instance_uid), text.get_string(tags::modality),
                                         number_of(text, tags::series_number), text.get_string(tags::series_description),
                                         text.get_string(body_part_examined_tag), text.get_string(tags::station_name));
    if (series.is_err()) {
        return "series row: "+series.error().message;
    }
    instance_record record;
    record.series_pk = series.value();
    record.sop_uid = text.get_string(tags::sop_instance_uid);
    record.sop_class_uid = text.get_string(tags::sop_class_uid);
    record.instance_number = number_of(text, tags::instance_number);
    record.transfer_syntax = std::string(syntax_uid);
    record.content_date = text.get_string(tags::content_date);
    record.content_time = text.get_string(tags::content_time);
    if (text.contains(tags::rows)) {
        record.rows = number_of(text, tags::rows);
        record.columns = number_of(text, tags::columns);
        record.bits_allocated = number_of(text, tags::bits_allocated);
        // Number of Frames is Type 1 only in multi-frame IODs, and every other image has one.
        record.number_of_frames = number_of(text, number_of_frames_tag).value_or(1);
    }
    record.file_path = file_path.string();
    std::error_code ignored;
    const auto size = std::filesystem::file_size(file_path, ignored);
    record.file_size = ignored ? 0 : static_cast<int64_t>(size);
    auto instance = database.upsert_instance(record);
    if (instance.is_err()) {
        return "instance row: "+instance.error().message;
    }
    return std::nullopt;
}

}  // namespace

auto is_valid_uid(std::string_view uid) noexcept -> bool {
    if (uid.empty() || uid.size() > 64 || uid.front() == '.' || uid.back() == '.') {
        return false;
    }
    char previous = '\0';
    for (const char c : uid) {
        if ((c < '0' || c > '9') && c != '.') {
            return false;
        }
        if (c == '.' && previous == '.') {
            return false;
        }
        previous = c;
    }
    return true;
}

auto archive_instance(file_storage& files, index_database& database, const core::dicom_dataset& dataset,
                      const encoding::transfer_syntax& syntax) -> archive_result {
    // The index holds UTF-8, whatever character set each object uses.
    const auto text = core::utf8_text_copy(dataset);
    if (const auto missing = what_is_missing(text)) {
        return {archive_status::rejected, *missing, {}};
    }
    const auto sop_uid = text.get_string(tags::sop_instance_uid);

    const std::scoped_lock lock(lock_for(sop_uid));
    const bool held = files.exists(sop_uid);
    if (held && files.duplicate_handling() == duplicate_policy::reject) {
        return {archive_status::duplicate_refused, "SOP Instance "+sop_uid+" is already held", files.get_file_path(sop_uid)};
    }
    if (held && files.duplicate_handling() == duplicate_policy::ignore) {
        return {archive_status::duplicate_ignored, "SOP Instance "+sop_uid+" is already held", files.get_file_path(sop_uid)};
    }

    if (auto written = files.store(dataset, syntax); written.is_err()) {
        return {archive_status::failed, written.error().message, {}};
    }
    const auto file_path = files.get_file_path(sop_uid);
    const auto problem = [&] {
        const std::scoped_lock writing(index_writer());
        return index_rows(database, text, file_path, syntax.uid());
    }();
    if (problem) {
        if (!held) {
            // Nobody could find it, and a later copy would be refused as a duplicate of it.
            (void)files.remove(sop_uid);
        }
        return {archive_status::failed, "Cannot index SOP Instance "+sop_uid+": "+*problem, {}};
    }
    return {archive_status::stored, {}, file_path};
}

}  // namespace kcenon::pacs::storage
