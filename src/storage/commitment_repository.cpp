// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file commitment_repository.cpp
 * @brief Implementation of Storage Commitment transaction repository
 *
 * @see Issue #711 - Storage Commitment database tracking
 */

#include "kcenon/pacs/storage/commitment_repository.h"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <chrono>
#include <iomanip>
#include <sstream>

namespace kcenon::pacs::storage {

using kcenon::common::ok;
using kcenon::common::make_error;

// =============================================================================
// Constructor
// =============================================================================

commitment_repository::commitment_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "storage_commitment", "transaction_uid") {}

// =============================================================================
// Domain-Specific Operations
// =============================================================================

auto commitment_repository::record_request(
    const std::string& transaction_uid,
    const std::string& requesting_ae,
    const std::vector<services::sop_reference>& references) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    return in_transaction([&]() -> VoidResult {
        commitment_record record;
        record.transaction_uid = transaction_uid;
        record.requesting_ae = requesting_ae;
        record.request_time = format_timestamp(
            std::chrono::system_clock::now());
        record.status = commitment_status::pending;
        record.total_instances = static_cast<int>(references.size());

        auto insert_result = insert(record);
        if (insert_result.is_err()) {
            return VoidResult(insert_result.error());
        }

        return insert_references(transaction_uid, references);
    });
}

auto commitment_repository::update_result(
    const std::string& transaction_uid,
    const services::commitment_result& result) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    return in_transaction([&]() -> VoidResult {
        // Determine overall status
        commitment_status new_status;
        if (result.failed_references.empty() &&
            !result.success_references.empty()) {
            new_status = commitment_status::success;
        } else if (result.success_references.empty() &&
                   !result.failed_references.empty()) {
            new_status = commitment_status::failed;
        } else if (!result.success_references.empty() &&
                   !result.failed_references.empty()) {
            new_status = commitment_status::partial;
        } else {
            new_status = commitment_status::failed;
        }

        // Update main record
        auto now_str = format_timestamp(std::chrono::system_clock::now());
        std::string sql =
            "UPDATE storage_commitment SET status = '" +
            std::string(to_string(new_status)) +
            "', completion_time = '" + now_str +
            "', success_count = " +
            std::to_string(result.success_references.size()) +
            ", failure_count = " +
            std::to_string(result.failed_references.size()) +
            " WHERE transaction_uid = '" + transaction_uid + "'";

        auto exec_result = storage_session().execute(sql);
        if (exec_result.is_err()) {
            return VoidResult(exec_result.error());
        }

        // Update successful references
        for (const auto& ref : result.success_references) {
            std::string ref_sql =
                "UPDATE commitment_references SET status = 'success'"
                " WHERE transaction_uid = '" + transaction_uid +
                "' AND sop_instance_uid = '" + ref.sop_instance_uid + "'";
            auto ref_result = storage_session().execute(ref_sql);
            if (ref_result.is_err()) {
                return VoidResult(ref_result.error());
            }
        }

        // Update failed references
        for (const auto& [ref, reason] : result.failed_references) {
            std::string ref_sql =
                "UPDATE commitment_references SET status = 'failed'"
                ", failure_reason = " +
                std::to_string(static_cast<uint16_t>(reason)) +
                " WHERE transaction_uid = '" + transaction_uid +
                "' AND sop_instance_uid = '" + ref.sop_instance_uid + "'";
            auto ref_result = storage_session().execute(ref_sql);
            if (ref_result.is_err()) {
                return VoidResult(ref_result.error());
            }
        }

        return ok();
    });
}

auto commitment_repository::get_status(const std::string& transaction_uid)
    -> Result<commitment_status> {
    auto result = find_by_id(transaction_uid);
    if (result.is_err()) {
        return Result<commitment_status>(result.error());
    }
    return Result<commitment_status>::ok(result.value().status);
}

auto commitment_repository::is_duplicate_transaction(
    const std::string& transaction_uid) -> bool {
    auto result = exists(transaction_uid);
    if (result.is_err()) return false;
    return result.value();
}

auto commitment_repository::get_references(
    const std::string& transaction_uid)
    -> Result<std::vector<commitment_reference_record>> {
    if (!db() || !db()->is_connected()) {
        return Result<std::vector<commitment_reference_record>>(
            kcenon::common::error_info{-1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select({"transaction_uid", "sop_class_uid", "sop_instance_uid",
                     "status", "failure_reason"})
        .from("commitment_references")
        .where("transaction_uid", "=", transaction_uid);

    auto result = storage_session().select(builder.build());
    if (result.is_err()) {
        return Result<std::vector<commitment_reference_record>>(result.error());
    }

    std::vector<commitment_reference_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        commitment_reference_record ref;
        ref.transaction_uid = row.at("transaction_uid");
        ref.sop_class_uid = row.at("sop_class_uid");
        ref.sop_instance_uid = row.at("sop_instance_uid");
        ref.status = row.at("status");

        auto reason_it = row.find("failure_reason");
        if (reason_it != row.end() && !reason_it->second.empty()) {
            ref.failure_reason = std::stoi(reason_it->second);
        }

        records.push_back(std::move(ref));
    }

    return Result<std::vector<commitment_reference_record>>::ok(
        std::move(records));
}

auto commitment_repository::find_by_status(commitment_status status,
                                            size_t /*limit*/)
    -> list_result_type {
    return find_where("status", "=",
                      std::string(to_string(status)));
}

auto commitment_repository::cleanup_old_transactions(
    std::chrono::hours max_age) -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto cutoff = std::chrono::system_clock::now() - max_age;
    auto cutoff_str = format_timestamp(cutoff);

    // Delete references first (FK constraint)
    std::string ref_sql =
        "DELETE FROM commitment_references WHERE transaction_uid IN "
        "(SELECT transaction_uid FROM storage_commitment "
        "WHERE status IN ('success', 'failed', 'partial') "
        "AND completion_time < '" + cutoff_str + "')";

    auto ref_result = storage_session().execute(ref_sql);
    if (ref_result.is_err()) {
        return Result<size_t>(ref_result.error());
    }

    // Count records to be deleted
    std::string count_sql =
        "SELECT count(*) as cnt FROM storage_commitment "
        "WHERE status IN ('success', 'failed', 'partial') "
        "AND completion_time < '" + cutoff_str + "'";

    auto count_result = storage_session().select(count_sql);
    size_t deleted_count = 0;
    if (count_result.is_ok() && !count_result.value().empty()) {
        deleted_count = std::stoull(count_result.value()[0].at("cnt"));
    }

    // Delete main records (CASCADE handles references)
    std::string sql =
        "DELETE FROM storage_commitment "
        "WHERE status IN ('success', 'failed', 'partial') "
        "AND completion_time < '" + cutoff_str + "'";

    auto result = storage_session().execute(sql);
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    return kcenon::common::ok(deleted_count);
}

// =============================================================================
// base_repository Overrides
// =============================================================================

auto commitment_repository::map_row_to_entity(const database_row& row) const
    -> commitment_record {
    commitment_record record;
    record.transaction_uid = row.at("transaction_uid");
    record.requesting_ae = row.at("requesting_ae");
    record.request_time = row.at("request_time");

    auto completion_it = row.find("completion_time");
    if (completion_it != row.end()) {
        record.completion_time = completion_it->second;
    }

    record.status = commitment_status_from_string(row.at("status"));

    auto total_it = row.find("total_instances");
    if (total_it != row.end() && !total_it->second.empty()) {
        record.total_instances = std::stoi(total_it->second);
    }

    auto success_it = row.find("success_count");
    if (success_it != row.end() && !success_it->second.empty()) {
        record.success_count = std::stoi(success_it->second);
    }

    auto failure_it = row.find("failure_count");
    if (failure_it != row.end() && !failure_it->second.empty()) {
        record.failure_count = std::stoi(failure_it->second);
    }

    return record;
}

auto commitment_repository::entity_to_row(
    const commitment_record& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["transaction_uid"] = entity.transaction_uid;
    row["requesting_ae"] = entity.requesting_ae;
    row["request_time"] = entity.request_time;
    row["completion_time"] = entity.completion_time;
    row["status"] = std::string(to_string(entity.status));
    row["total_instances"] = static_cast<int64_t>(entity.total_instances);
    row["success_count"] = static_cast<int64_t>(entity.success_count);
    row["failure_count"] = static_cast<int64_t>(entity.failure_count);

    return row;
}

auto commitment_repository::get_pk(const commitment_record& entity) const
    -> std::string {
    return entity.transaction_uid;
}

auto commitment_repository::has_pk(const commitment_record& entity) const
    -> bool {
    return !entity.transaction_uid.empty();
}

// =============================================================================
// Private Helpers
// =============================================================================

auto commitment_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

auto commitment_repository::insert_references(
    const std::string& transaction_uid,
    const std::vector<services::sop_reference>& references) -> VoidResult {
    for (const auto& ref : references) {
        std::string sql =
            "INSERT INTO commitment_references "
            "(transaction_uid, sop_class_uid, sop_instance_uid, status) "
            "VALUES ('" + transaction_uid + "', '" +
            ref.sop_class_uid + "', '" +
            ref.sop_instance_uid + "', 'pending')";

        auto result = storage_session().execute(sql);
        if (result.is_err()) {
            return VoidResult(result.error());
        }
    }
    return ok();
}

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
