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
 * @file repository_factory.cpp
 * @brief Implementation of repository factory
 *
 * Provides lazy-initialized repository instances with shared database adapter.
 *
 * @see repository_factory.hpp
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 * @see Issue #610 - Phase 4: Repository Migrations (9 repositories)
 * @see Issue #716 - Complete repository_factory migration for remaining 6
 */

#include "pacs/storage/repository_factory.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/pacs_database_adapter.hpp"

#include "pacs/storage/annotation_repository.hpp"
#include "pacs/storage/audit_repository.hpp"
#include "pacs/storage/instance_repository.hpp"
#include "pacs/storage/mpps_repository.hpp"
#include "pacs/storage/patient_repository.hpp"
#include "pacs/storage/series_repository.hpp"
#include "pacs/storage/study_repository.hpp"
#include "pacs/storage/ups_repository.hpp"
#include "pacs/storage/worklist_repository.hpp"
#include "pacs/storage/commitment_repository.hpp"
#include "pacs/storage/job_repository.hpp"
#include "pacs/storage/key_image_repository.hpp"
#include "pacs/storage/measurement_repository.hpp"
#include "pacs/storage/node_repository.hpp"
#include "pacs/storage/prefetch_history_repository.hpp"
#include "pacs/storage/prefetch_repository.hpp"
#include "pacs/storage/prefetch_rule_repository.hpp"
#include "pacs/storage/routing_repository.hpp"
#include "pacs/storage/recent_study_repository.hpp"
#include "pacs/storage/sync_config_repository.hpp"
#include "pacs/storage/sync_conflict_repository.hpp"
#include "pacs/storage/sync_history_repository.hpp"
#include "pacs/storage/sync_repository.hpp"
#include "pacs/storage/viewer_state_record_repository.hpp"
#include "pacs/storage/viewer_state_repository.hpp"

namespace pacs::storage {

repository_factory::repository_factory(
    std::shared_ptr<pacs_database_adapter> db)
    : db_(std::move(db)) {}

auto repository_factory::patients() -> std::shared_ptr<patient_repository> {
    if (!patients_) {
        patients_ = std::make_shared<patient_repository>(db_);
    }
    return patients_;
}

auto repository_factory::studies() -> std::shared_ptr<study_repository> {
    if (!studies_) {
        studies_ = std::make_shared<study_repository>(db_);
    }
    return studies_;
}

auto repository_factory::series() -> std::shared_ptr<series_repository> {
    if (!series_) {
        series_ = std::make_shared<series_repository>(db_);
    }
    return series_;
}

auto repository_factory::instances() -> std::shared_ptr<instance_repository> {
    if (!instances_) {
        instances_ = std::make_shared<instance_repository>(db_);
    }
    return instances_;
}

auto repository_factory::mpps() -> std::shared_ptr<mpps_repository> {
    if (!mpps_) {
        mpps_ = std::make_shared<mpps_repository>(db_);
    }
    return mpps_;
}

auto repository_factory::worklist() -> std::shared_ptr<worklist_repository> {
    if (!worklist_) {
        worklist_ = std::make_shared<worklist_repository>(db_);
    }
    return worklist_;
}

auto repository_factory::ups() -> std::shared_ptr<ups_repository> {
    if (!ups_) {
        ups_ = std::make_shared<ups_repository>(db_);
    }
    return ups_;
}

auto repository_factory::audit() -> std::shared_ptr<audit_repository> {
    if (!audit_) {
        audit_ = std::make_shared<audit_repository>(db_);
    }
    return audit_;
}

auto repository_factory::jobs() -> std::shared_ptr<job_repository> {
    if (!jobs_) {
        jobs_ = std::make_shared<job_repository>(db_);
    }
    return jobs_;
}

auto repository_factory::annotations()
    -> std::shared_ptr<annotation_repository> {
    if (!annotations_) {
        annotations_ = std::make_shared<annotation_repository>(db_);
    }
    return annotations_;
}

auto repository_factory::routing_rules()
    -> std::shared_ptr<routing_repository> {
    if (!routing_rules_) {
        routing_rules_ = std::make_shared<routing_repository>(db_);
    }
    return routing_rules_;
}

auto repository_factory::nodes() -> std::shared_ptr<node_repository> {
    if (!nodes_) {
        nodes_ = std::make_shared<node_repository>(db_);
    }
    return nodes_;
}

auto repository_factory::sync_states() -> std::shared_ptr<sync_repository> {
    if (!sync_states_) {
        sync_states_ = std::make_shared<sync_repository>(db_);
    }
    return sync_states_;
}

auto repository_factory::sync_configs()
    -> std::shared_ptr<sync_config_repository> {
    if (!sync_configs_) {
        sync_configs_ = std::make_shared<sync_config_repository>(db_);
    }
    return sync_configs_;
}

auto repository_factory::sync_conflicts()
    -> std::shared_ptr<sync_conflict_repository> {
    if (!sync_conflicts_) {
        sync_conflicts_ = std::make_shared<sync_conflict_repository>(db_);
    }
    return sync_conflicts_;
}

auto repository_factory::sync_history()
    -> std::shared_ptr<sync_history_repository> {
    if (!sync_history_) {
        sync_history_ = std::make_shared<sync_history_repository>(db_);
    }
    return sync_history_;
}

auto repository_factory::key_images()
    -> std::shared_ptr<key_image_repository> {
    if (!key_images_) {
        key_images_ = std::make_shared<key_image_repository>(db_);
    }
    return key_images_;
}

auto repository_factory::measurements()
    -> std::shared_ptr<measurement_repository> {
    if (!measurements_) {
        measurements_ = std::make_shared<measurement_repository>(db_);
    }
    return measurements_;
}

auto repository_factory::viewer_states()
    -> std::shared_ptr<viewer_state_repository> {
    if (!viewer_states_) {
        viewer_states_ = std::make_shared<viewer_state_repository>(db_);
    }
    return viewer_states_;
}

auto repository_factory::viewer_state_records()
    -> std::shared_ptr<viewer_state_record_repository> {
    if (!viewer_state_records_) {
        viewer_state_records_ =
            std::make_shared<viewer_state_record_repository>(db_);
    }
    return viewer_state_records_;
}

auto repository_factory::recent_studies()
    -> std::shared_ptr<recent_study_repository> {
    if (!recent_studies_) {
        recent_studies_ = std::make_shared<recent_study_repository>(db_);
    }
    return recent_studies_;
}

auto repository_factory::prefetch_queue()
    -> std::shared_ptr<prefetch_repository> {
    if (!prefetch_queue_) {
        prefetch_queue_ = std::make_shared<prefetch_repository>(db_);
    }
    return prefetch_queue_;
}

auto repository_factory::prefetch_rules()
    -> std::shared_ptr<prefetch_rule_repository> {
    if (!prefetch_rules_) {
        prefetch_rules_ = std::make_shared<prefetch_rule_repository>(db_);
    }
    return prefetch_rules_;
}

auto repository_factory::prefetch_history()
    -> std::shared_ptr<prefetch_history_repository> {
    if (!prefetch_history_) {
        prefetch_history_ = std::make_shared<prefetch_history_repository>(db_);
    }
    return prefetch_history_;
}

auto repository_factory::commitments()
    -> std::shared_ptr<commitment_repository> {
    if (!commitments_) {
        commitments_ = std::make_shared<commitment_repository>(db_);
    }
    return commitments_;
}

auto repository_factory::canonical_repositories() -> canonical_repository_set {
    return {
        .patients = patients(),
        .studies = studies(),
        .series = series(),
        .instances = instances(),
        .mpps = mpps(),
        .worklist = worklist(),
        .ups = ups(),
        .audit = audit(),
        .jobs = jobs(),
        .annotations = annotations(),
        .routing_rules = routing_rules(),
        .nodes = nodes(),
        .sync =
            {
                .configs = sync_configs(),
                .conflicts = sync_conflicts(),
                .history = sync_history(),
            },
        .key_images = key_images(),
        .measurements = measurements(),
        .viewer_state =
            {
                .records = viewer_state_records(),
                .recent_studies = recent_studies(),
            },
        .prefetch =
            {
                .rules = prefetch_rules(),
                .history = prefetch_history(),
            },
        .commitments = commitments(),
    };
}

auto repository_factory::compatibility_repositories()
    -> compatibility_repository_set {
    return {
        .sync_states = sync_states(),
        .viewer_states = viewer_states(),
        .prefetch_queue = prefetch_queue(),
    };
}

auto repository_factory::db() const
    -> std::shared_ptr<pacs_database_adapter> {
    return db_;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
