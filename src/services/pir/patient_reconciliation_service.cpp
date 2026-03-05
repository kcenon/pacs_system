// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file patient_reconciliation_service.cpp
 * @brief Implementation of IHE PIR Patient Information Reconciliation Service
 */

#include "pacs/services/pir/patient_reconciliation_service.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <set>

namespace pacs::services::pir {

using namespace pacs::core;
using namespace pacs::encoding;

// =============================================================================
// patient_reconciliation_service Implementation
// =============================================================================

bool patient_reconciliation_service::add_instance(
    const core::dicom_dataset& dataset) {

    auto uid = dataset.get_string(tags::sop_instance_uid);
    if (uid.empty()) {
        return false;
    }

    instances_.push_back(dataset);
    return true;
}

reconciliation_result patient_reconciliation_service::update_demographics(
    const demographics_update_request& request) {

    reconciliation_result result;
    result.type = reconciliation_type::demographics_update;

    if (request.target_patient_id.empty()) {
        result.error_message = "Target patient ID is required";
        return result;
    }

    size_t updated = 0;
    std::set<std::string> affected_studies;

    for (auto& instance : instances_) {
        auto pid = instance.get_string(tags::patient_id);
        if (pid == request.target_patient_id) {
            apply_demographics(instance, request.updated_demographics);
            updated++;

            auto study_uid = instance.get_string(tags::study_instance_uid);
            if (!study_uid.empty()) {
                affected_studies.insert(study_uid);
            }
        }
    }

    if (updated == 0) {
        result.error_message = "No instances found for patient ID: " +
                               request.target_patient_id;
        return result;
    }

    result.success = true;
    result.instances_updated = updated;
    result.studies_affected = affected_studies.size();

    // Create audit record
    reconciliation_audit_record audit;
    audit.record_id = generate_record_id();
    audit.type = reconciliation_type::demographics_update;
    audit.primary_patient_id = request.target_patient_id;
    audit.operator_name = request.operator_name.value_or("SYSTEM");
    audit.instances_updated = updated;
    audit.timestamp = std::chrono::system_clock::now();
    audit.success = true;
    audit_records_.push_back(std::move(audit));

    return result;
}

reconciliation_result patient_reconciliation_service::merge_patients(
    const patient_merge_request& request) {

    reconciliation_result result;
    result.type = reconciliation_type::patient_merge;

    if (request.source_patient_id.empty()) {
        result.error_message = "Source patient ID is required";
        return result;
    }

    if (request.target_patient_id.empty()) {
        result.error_message = "Target patient ID is required";
        return result;
    }

    if (request.source_patient_id == request.target_patient_id) {
        result.error_message = "Source and target patient IDs must be different";
        return result;
    }

    size_t updated = 0;
    std::set<std::string> affected_studies;

    for (auto& instance : instances_) {
        auto pid = instance.get_string(tags::patient_id);
        if (pid == request.source_patient_id) {
            // Reassign to target patient
            instance.set_string(tags::patient_id, vr_type::LO,
                                request.target_patient_id);

            // Apply target demographics if provided
            if (request.target_demographics) {
                apply_demographics(instance, request.target_demographics.value());
            }

            updated++;

            auto study_uid = instance.get_string(tags::study_instance_uid);
            if (!study_uid.empty()) {
                affected_studies.insert(study_uid);
            }
        }
    }

    if (updated == 0) {
        result.error_message = "No instances found for source patient ID: " +
                               request.source_patient_id;
        return result;
    }

    result.success = true;
    result.instances_updated = updated;
    result.studies_affected = affected_studies.size();

    // Create audit record
    reconciliation_audit_record audit;
    audit.record_id = generate_record_id();
    audit.type = reconciliation_type::patient_merge;
    audit.primary_patient_id = request.target_patient_id;
    audit.secondary_patient_id = request.source_patient_id;
    audit.operator_name = request.operator_name.value_or("SYSTEM");
    audit.instances_updated = updated;
    audit.timestamp = std::chrono::system_clock::now();
    audit.success = true;
    audit_records_.push_back(std::move(audit));

    return result;
}

std::vector<core::dicom_dataset> patient_reconciliation_service::find_instances(
    const std::string& patient_id) const {

    std::vector<core::dicom_dataset> results;
    for (const auto& instance : instances_) {
        if (instance.get_string(tags::patient_id) == patient_id) {
            results.push_back(instance);
        }
    }
    return results;
}

size_t patient_reconciliation_service::instance_count() const noexcept {
    return instances_.size();
}

std::vector<std::string> patient_reconciliation_service::get_patient_ids() const {
    std::set<std::string> ids;
    for (const auto& instance : instances_) {
        auto pid = instance.get_string(tags::patient_id);
        if (!pid.empty()) {
            ids.insert(pid);
        }
    }
    return {ids.begin(), ids.end()};
}

const std::vector<reconciliation_audit_record>&
patient_reconciliation_service::audit_trail() const noexcept {
    return audit_records_;
}

std::vector<reconciliation_audit_record>
patient_reconciliation_service::audit_trail_for_patient(
    const std::string& patient_id) const {

    std::vector<reconciliation_audit_record> results;
    for (const auto& record : audit_records_) {
        if (record.primary_patient_id == patient_id ||
            (record.secondary_patient_id &&
             record.secondary_patient_id.value() == patient_id)) {
            results.push_back(record);
        }
    }
    return results;
}

// =============================================================================
// Private Methods
// =============================================================================

void patient_reconciliation_service::apply_demographics(
    core::dicom_dataset& dataset,
    const patient_demographics& demographics) const {

    if (demographics.patient_name) {
        dataset.set_string(tags::patient_name, vr_type::PN,
                           demographics.patient_name.value());
    }
    if (demographics.patient_id) {
        dataset.set_string(tags::patient_id, vr_type::LO,
                           demographics.patient_id.value());
    }
    if (demographics.patient_birth_date) {
        dataset.set_string(tags::patient_birth_date, vr_type::DA,
                           demographics.patient_birth_date.value());
    }
    if (demographics.patient_sex) {
        dataset.set_string(tags::patient_sex, vr_type::CS,
                           demographics.patient_sex.value());
    }
    if (demographics.issuer_of_patient_id) {
        dataset.set_string(tags::issuer_of_patient_id, vr_type::LO,
                           demographics.issuer_of_patient_id.value());
    }
}

std::string patient_reconciliation_service::generate_record_id() const {
    static std::mt19937_64 gen{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return "PIR-" + std::to_string(timestamp) + "-" +
           std::to_string(dist(gen) % 100000);
}

// =============================================================================
// Free Functions
// =============================================================================

std::string to_string(reconciliation_type type) {
    switch (type) {
    case reconciliation_type::demographics_update: return "demographics_update";
    case reconciliation_type::patient_merge:       return "patient_merge";
    case reconciliation_type::patient_link:        return "patient_link";
    }
    return "demographics_update";
}

}  // namespace pacs::services::pir
