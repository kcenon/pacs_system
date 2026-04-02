// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file aira_assessment_manager.cpp
 * @brief Implementation of IHE AIRA Assessment Manager Actor
 */

#include "pacs/ai/aira_assessment_manager.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

#include <algorithm>

namespace kcenon::pacs::ai {

using namespace kcenon::pacs::core;

// =============================================================================
// DICOM Tags for parsing assessment SR
// =============================================================================

namespace manager_tags {

constexpr dicom_tag content_sequence{0x0040, 0xA730};
constexpr dicom_tag value_type{0x0040, 0xA040};
constexpr dicom_tag concept_name_code_sequence{0x0040, 0xA043};
constexpr dicom_tag code_value{0x0008, 0x0100};
constexpr dicom_tag concept_code_sequence{0x0040, 0xA168};
constexpr dicom_tag person_name{0x0040, 0xA123};
constexpr dicom_tag completion_flag{0x0040, 0xA491};
constexpr dicom_tag referenced_sop_sequence{0x0008, 0x1199};
constexpr dicom_tag referenced_sop_instance_uid{0x0008, 0x1155};

}  // namespace manager_tags

// =============================================================================
// assessment_manager Implementation
// =============================================================================

bool assessment_manager::store_assessment(
    const core::dicom_dataset& assessment_sr) {

    auto uid = assessment_sr.get_string(tags::sop_instance_uid);
    if (uid.empty()) {
        return false;
    }

    auto info = parse_assessment_info(assessment_sr);
    datasets_[uid] = assessment_sr;
    metadata_[uid] = std::move(info);

    return true;
}

std::optional<core::dicom_dataset> assessment_manager::retrieve_assessment(
    const std::string& assessment_uid) const {

    auto it = datasets_.find(assessment_uid);
    if (it == datasets_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<assessment_info> assessment_manager::find_by_ai_result(
    const std::string& ai_result_uid) const {

    std::vector<assessment_info> results;
    for (const auto& [uid, info] : metadata_) {
        if (info.ai_result_uid == ai_result_uid) {
            results.push_back(info);
        }
    }
    return results;
}

std::vector<assessment_info> assessment_manager::find_by_assessor(
    const std::string& assessor_name) const {

    std::vector<assessment_info> results;
    for (const auto& [uid, info] : metadata_) {
        if (info.assessor_name == assessor_name) {
            results.push_back(info);
        }
    }
    return results;
}

std::vector<assessment_info> assessment_manager::find_by_type(
    assessment_type type) const {

    std::vector<assessment_info> results;
    for (const auto& [uid, info] : metadata_) {
        if (info.type == type) {
            results.push_back(info);
        }
    }
    return results;
}

std::optional<assessment_info> assessment_manager::get_info(
    const std::string& assessment_uid) const {

    auto it = metadata_.find(assessment_uid);
    if (it == metadata_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool assessment_manager::exists(const std::string& assessment_uid) const {
    return datasets_.find(assessment_uid) != datasets_.end();
}

bool assessment_manager::remove(const std::string& assessment_uid) {
    auto ds_it = datasets_.find(assessment_uid);
    if (ds_it == datasets_.end()) {
        return false;
    }
    datasets_.erase(ds_it);
    metadata_.erase(assessment_uid);
    return true;
}

size_t assessment_manager::count() const noexcept {
    return datasets_.size();
}

std::map<assessment_type, size_t> assessment_manager::get_statistics() const {
    std::map<assessment_type, size_t> stats;
    stats[assessment_type::accept] = 0;
    stats[assessment_type::modify] = 0;
    stats[assessment_type::reject] = 0;

    for (const auto& [uid, info] : metadata_) {
        stats[info.type]++;
    }
    return stats;
}

// =============================================================================
// Private Methods
// =============================================================================

assessment_info assessment_manager::parse_assessment_info(
    const core::dicom_dataset& sr) const {

    assessment_info info;
    info.assessment_uid = sr.get_string(tags::sop_instance_uid);
    info.assessment_time = std::chrono::system_clock::now();

    // Parse completion flag to determine status
    auto completion = sr.get_string(manager_tags::completion_flag);
    if (completion == "COMPLETE") {
        info.status = assessment_status::final_;
    } else {
        info.status = assessment_status::draft;
    }

    // Parse Content Sequence for assessment type and assessor
    const auto* content_elem = sr.get(manager_tags::content_sequence);
    if (content_elem && content_elem->is_sequence()) {
        for (const auto& item : content_elem->sequence_items()) {
            auto vt = item.get_string(manager_tags::value_type);

            // Extract assessment type from CODE items
            if (vt == "CODE") {
                const auto* concept_name = item.get(
                    manager_tags::concept_name_code_sequence);
                if (concept_name && concept_name->is_sequence() &&
                    !concept_name->sequence_items().empty()) {
                    auto code = concept_name->sequence_items()[0].get_string(
                        manager_tags::code_value);
                    if (code == "AIRA-010") {
                        // Assessment Decision item
                        const auto* concept_code = item.get(
                            manager_tags::concept_code_sequence);
                        if (concept_code && concept_code->is_sequence() &&
                            !concept_code->sequence_items().empty()) {
                            auto type_code =
                                concept_code->sequence_items()[0].get_string(
                                    manager_tags::code_value);
                            if (type_code == "AIRA-ACCEPT") {
                                info.type = assessment_type::accept;
                            } else if (type_code == "AIRA-MODIFY") {
                                info.type = assessment_type::modify;
                            } else if (type_code == "AIRA-REJECT") {
                                info.type = assessment_type::reject;
                            }
                        }
                    }
                }
            }

            // Extract assessor name from PNAME items
            if (vt == "PNAME") {
                auto name = item.get_string(manager_tags::person_name);
                if (!name.empty()) {
                    info.assessor_name = name;
                }
            }
        }
    }

    // Parse Referenced SOP Sequence for AI result UID
    const auto* ref_elem = sr.get(manager_tags::referenced_sop_sequence);
    if (ref_elem && ref_elem->is_sequence() &&
        !ref_elem->sequence_items().empty()) {
        info.ai_result_uid = ref_elem->sequence_items()[0].get_string(
            manager_tags::referenced_sop_instance_uid);
    }

    return info;
}

}  // namespace kcenon::pacs::ai
