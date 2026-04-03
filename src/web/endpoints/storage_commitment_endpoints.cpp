// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_commitment_endpoints.cpp
 * @brief DICOMweb Storage Commitment REST endpoint implementation
 *
 * @see DICOM Supplement 234 -- DICOMweb Storage Commitment
 * @see DICOM PS3.18 -- Web Services
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include "pacs/storage/index_database.hpp"
#include "pacs/web/auth/oauth2_middleware.hpp"
#include "pacs/web/endpoints/storage_commitment_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <chrono>
#include <random>
#include <sstream>
#include <string>

namespace kcenon::pacs::web {

namespace storage_commitment {

// ============================================================================
// transaction_state helpers
// ============================================================================

std::string_view to_string(transaction_state state) noexcept {
    switch (state) {
        case transaction_state::pending: return "PENDING";
        case transaction_state::success: return "SUCCESS";
        case transaction_state::partial: return "PARTIAL";
        case transaction_state::failure: return "FAILURE";
    }
    return "UNKNOWN";
}

std::optional<transaction_state> parse_state(std::string_view str) noexcept {
    if (str == "PENDING") return transaction_state::pending;
    if (str == "SUCCESS") return transaction_state::success;
    if (str == "PARTIAL") return transaction_state::partial;
    if (str == "FAILURE") return transaction_state::failure;
    return std::nullopt;
}

// ============================================================================
// transaction_store
// ============================================================================

void transaction_store::add(commitment_transaction txn) {
    std::lock_guard lock(mutex_);
    auto uid = txn.transaction_uid;
    transactions_.emplace(std::move(uid), std::move(txn));
}

std::optional<commitment_transaction> transaction_store::find(
    std::string_view transaction_uid) const {
    std::lock_guard lock(mutex_);
    auto it = transactions_.find(transaction_uid);
    if (it != transactions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool transaction_store::update(const commitment_transaction& txn) {
    std::lock_guard lock(mutex_);
    auto it = transactions_.find(txn.transaction_uid);
    if (it != transactions_.end()) {
        it->second = txn;
        return true;
    }
    return false;
}

std::vector<commitment_transaction> transaction_store::list(size_t limit) const {
    std::lock_guard lock(mutex_);
    std::vector<commitment_transaction> result;
    result.reserve(limit == 0 ? transactions_.size()
                              : std::min(limit, transactions_.size()));

    for (auto it = transactions_.rbegin(); it != transactions_.rend(); ++it) {
        if (limit > 0 && result.size() >= limit) break;
        result.push_back(it->second);
    }
    return result;
}

size_t transaction_store::size() const {
    std::lock_guard lock(mutex_);
    return transactions_.size();
}

// ============================================================================
// JSON serialization
// ============================================================================

namespace {

std::string format_timestamp(std::chrono::system_clock::time_point tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string sop_reference_to_json(const services::sop_reference& ref) {
    return R"({"sopClassUID":")" + json_escape(ref.sop_class_uid) +
           R"(","sopInstanceUID":")" + json_escape(ref.sop_instance_uid) + R"("})";
}

std::string failed_reference_to_json(
    const std::pair<services::sop_reference,
                    services::commitment_failure_reason>& entry) {
    return R"({"sopClassUID":")" + json_escape(entry.first.sop_class_uid) +
           R"(","sopInstanceUID":")" + json_escape(entry.first.sop_instance_uid) +
           R"(","failureReason":")" +
           std::string(services::to_string(entry.second)) + R"("})";
}

}  // namespace

std::string transaction_to_json(const commitment_transaction& txn) {
    std::ostringstream ss;
    ss << R"({"transactionUID":")" << json_escape(txn.transaction_uid) << R"(",)";
    ss << R"("state":")" << to_string(txn.state) << R"(",)";

    if (!txn.study_instance_uid.empty()) {
        ss << R"("studyInstanceUID":")" << json_escape(txn.study_instance_uid) << R"(",)";
    }

    ss << R"("createdAt":")" << format_timestamp(txn.created_at) << R"(",)";

    if (txn.completed_at.has_value()) {
        ss << R"("completedAt":")" << format_timestamp(*txn.completed_at) << R"(",)";
    }

    // Requested references
    ss << R"("requestedReferences":[)";
    for (size_t i = 0; i < txn.requested_references.size(); ++i) {
        if (i > 0) ss << ",";
        ss << sop_reference_to_json(txn.requested_references[i]);
    }
    ss << "],";

    // Success references
    ss << R"("successReferences":[)";
    for (size_t i = 0; i < txn.success_references.size(); ++i) {
        if (i > 0) ss << ",";
        ss << sop_reference_to_json(txn.success_references[i]);
    }
    ss << "],";

    // Failed references
    ss << R"("failedReferences":[)";
    for (size_t i = 0; i < txn.failed_references.size(); ++i) {
        if (i > 0) ss << ",";
        ss << failed_reference_to_json(txn.failed_references[i]);
    }
    ss << "]}";

    return ss.str();
}

std::string transactions_to_json(
    const std::vector<commitment_transaction>& transactions) {
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < transactions.size(); ++i) {
        if (i > 0) ss << ",";
        ss << transaction_to_json(transactions[i]);
    }
    ss << "]";
    return ss.str();
}

// ============================================================================
// Request parsing
// ============================================================================

namespace {

// Simple JSON string extraction (finds "key":"value" pairs)
std::string extract_json_string(std::string_view json, std::string_view key) {
    auto key_pattern = std::string("\"") + std::string(key) + "\"";
    auto pos = json.find(key_pattern);
    if (pos == std::string_view::npos) return {};

    pos = json.find('"', pos + key_pattern.size());
    if (pos == std::string_view::npos) return {};
    pos++; // skip opening quote

    auto end = json.find('"', pos);
    if (end == std::string_view::npos) return {};

    return std::string(json.substr(pos, end - pos));
}

}  // namespace

parse_result parse_commitment_request(
    std::string_view json_body,
    std::string_view study_uid) {

    parse_result result;

    if (json_body.empty()) {
        result.error_message = "Request body is empty";
        return result;
    }

    // Look for referencedSOPSequence array
    auto seq_pos = json_body.find("\"referencedSOPSequence\"");
    if (seq_pos == std::string_view::npos) {
        result.error_message = "Missing referencedSOPSequence in request body";
        return result;
    }

    // Find the array start
    auto arr_start = json_body.find('[', seq_pos);
    if (arr_start == std::string_view::npos) {
        result.error_message = "Invalid referencedSOPSequence format";
        return result;
    }

    // Find matching array end
    size_t depth = 0;
    size_t arr_end = std::string_view::npos;
    for (size_t i = arr_start; i < json_body.size(); ++i) {
        if (json_body[i] == '[') ++depth;
        else if (json_body[i] == ']') {
            --depth;
            if (depth == 0) {
                arr_end = i;
                break;
            }
        }
    }

    if (arr_end == std::string_view::npos) {
        result.error_message = "Unterminated referencedSOPSequence array";
        return result;
    }

    // Parse individual items in the array
    auto array_content = json_body.substr(arr_start + 1, arr_end - arr_start - 1);

    // Find each object in the array
    size_t pos = 0;
    while (pos < array_content.size()) {
        auto obj_start = array_content.find('{', pos);
        if (obj_start == std::string_view::npos) break;

        size_t obj_depth = 0;
        size_t obj_end = std::string_view::npos;
        for (size_t i = obj_start; i < array_content.size(); ++i) {
            if (array_content[i] == '{') ++obj_depth;
            else if (array_content[i] == '}') {
                --obj_depth;
                if (obj_depth == 0) {
                    obj_end = i;
                    break;
                }
            }
        }

        if (obj_end == std::string_view::npos) break;

        auto obj = array_content.substr(obj_start, obj_end - obj_start + 1);
        auto sop_class = extract_json_string(obj, "sopClassUID");
        auto sop_instance = extract_json_string(obj, "sopInstanceUID");

        if (sop_instance.empty()) {
            result.error_message = "Missing sopInstanceUID in reference";
            return result;
        }

        result.references.push_back({std::move(sop_class), std::move(sop_instance)});
        pos = obj_end + 1;
    }

    if (result.references.empty()) {
        result.error_message = "No valid SOP references found in request";
        return result;
    }

    // Ignore study_uid parameter (used for context only)
    (void)study_uid;

    result.valid = true;
    return result;
}

}  // namespace storage_commitment

// ============================================================================
// Endpoint Registration
// ============================================================================

namespace endpoints {

namespace {

void add_cors_headers(crow::response& res, const rest_server_context& ctx) {
    if (ctx.config != nullptr && !ctx.config->cors_allowed_origins.empty()) {
        res.add_header("Access-Control-Allow-Origin",
                       ctx.config->cors_allowed_origins);
    }
}

bool check_storage_commitment_auth(
    const std::shared_ptr<rest_server_context>& ctx,
    const crow::request& req,
    crow::response& res,
    const std::vector<std::string>& required_scopes) {
    if (ctx->oauth2 && ctx->oauth2->enabled()) {
        auto auth = ctx->oauth2->authenticate(req, res);
        if (!auth) return false;

        if (!required_scopes.empty()) {
            return ctx->oauth2->require_any_scope(
                auth->claims, res, required_scopes);
        }
        return true;
    }
    return true;
}

std::string generate_transaction_uid() {
    // Generate a unique transaction UID using timestamp + random
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count();

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(1, 999999);

    std::ostringstream ss;
    ss << "2.25." << ms << "." << dist(gen);
    return ss.str();
}

void verify_instances_async(
    std::shared_ptr<rest_server_context> ctx,
    std::shared_ptr<storage_commitment::transaction_store> store,
    std::string transaction_uid) {

    auto txn_opt = store->find(transaction_uid);
    if (!txn_opt) return;

    auto txn = *txn_opt;

    for (const auto& ref : txn.requested_references) {
        if (!ctx->database) {
            txn.failed_references.emplace_back(
                ref, services::commitment_failure_reason::processing_failure);
            continue;
        }

        auto instance = ctx->database->find_instance(ref.sop_instance_uid);
        if (instance.has_value()) {
            txn.success_references.push_back(ref);
        } else {
            txn.failed_references.emplace_back(
                ref, services::commitment_failure_reason::no_such_object_instance);
        }
    }

    txn.completed_at = std::chrono::system_clock::now();

    if (txn.failed_references.empty()) {
        txn.state = storage_commitment::transaction_state::success;
    } else if (txn.success_references.empty()) {
        txn.state = storage_commitment::transaction_state::failure;
    } else {
        txn.state = storage_commitment::transaction_state::partial;
    }

    store->update(txn);
}

}  // namespace

void register_storage_commitment_endpoints_impl(
    crow::SimpleApp& app,
    std::shared_ptr<rest_server_context> ctx) {

    // Shared transaction store for all requests
    auto store = std::make_shared<storage_commitment::transaction_store>();

    // POST /dicomweb/storage-commitments
    // Request storage commitment for a list of SOP Instances
    CROW_ROUTE(app, "/dicomweb/storage-commitments")
        .methods(crow::HTTPMethod::POST)(
            [ctx, store](const crow::request& req) {
                crow::response res;
                res.add_header("Content-Type", "application/dicom+json");
                add_cors_headers(res, *ctx);

                if (!check_storage_commitment_auth(
                        ctx, req, res, {"dicomweb.write"})) {
                    return res;
                }

                if (!ctx->database) {
                    res.code = 503;
                    res.body = make_error_json(
                        "DATABASE_UNAVAILABLE",
                        "Storage service is not available");
                    return res;
                }

                auto parsed = storage_commitment::parse_commitment_request(
                    req.body);

                if (!parsed.valid) {
                    res.code = 400;
                    res.body = make_error_json(
                        "INVALID_REQUEST", parsed.error_message);
                    return res;
                }

                // Create transaction
                storage_commitment::commitment_transaction txn;
                txn.transaction_uid = generate_transaction_uid();
                txn.state = storage_commitment::transaction_state::pending;
                txn.requested_references = std::move(parsed.references);
                txn.created_at = std::chrono::system_clock::now();

                store->add(txn);

                // Verify instances synchronously for simplicity
                // (async with thread pool in production)
                verify_instances_async(ctx, store, txn.transaction_uid);

                auto result = store->find(txn.transaction_uid);
                res.code = 202;  // Accepted
                res.add_header("Content-Location",
                               "/dicomweb/storage-commitments/" +
                                   txn.transaction_uid);
                res.body = storage_commitment::transaction_to_json(*result);
                return res;
            });

    // POST /dicomweb/studies/{studyUID}/commitment
    // Request storage commitment for a study
    CROW_ROUTE(app, "/dicomweb/studies/<string>/commitment")
        .methods(crow::HTTPMethod::POST)(
            [ctx, store](const crow::request& req,
                         const std::string& study_uid) {
                crow::response res;
                res.add_header("Content-Type", "application/dicom+json");
                add_cors_headers(res, *ctx);

                if (!check_storage_commitment_auth(
                        ctx, req, res, {"dicomweb.write"})) {
                    return res;
                }

                if (!ctx->database) {
                    res.code = 503;
                    res.body = make_error_json(
                        "DATABASE_UNAVAILABLE",
                        "Storage service is not available");
                    return res;
                }

                auto parsed = storage_commitment::parse_commitment_request(
                    req.body, study_uid);

                if (!parsed.valid) {
                    res.code = 400;
                    res.body = make_error_json(
                        "INVALID_REQUEST", parsed.error_message);
                    return res;
                }

                storage_commitment::commitment_transaction txn;
                txn.transaction_uid = generate_transaction_uid();
                txn.state = storage_commitment::transaction_state::pending;
                txn.requested_references = std::move(parsed.references);
                txn.study_instance_uid = study_uid;
                txn.created_at = std::chrono::system_clock::now();

                store->add(txn);
                verify_instances_async(ctx, store, txn.transaction_uid);

                auto result = store->find(txn.transaction_uid);
                res.code = 202;
                res.add_header("Content-Location",
                               "/dicomweb/storage-commitments/" +
                                   txn.transaction_uid);
                res.body = storage_commitment::transaction_to_json(*result);
                return res;
            });

    // GET /dicomweb/storage-commitments/{transactionUID}
    // Check commitment status
    CROW_ROUTE(app, "/dicomweb/storage-commitments/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx, store](const crow::request& req,
                         const std::string& transaction_uid) {
                crow::response res;
                res.add_header("Content-Type", "application/dicom+json");
                add_cors_headers(res, *ctx);

                if (!check_storage_commitment_auth(
                        ctx, req, res, {"dicomweb.read"})) {
                    return res;
                }

                auto txn = store->find(transaction_uid);
                if (!txn) {
                    res.code = 404;
                    res.body = make_error_json(
                        "NOT_FOUND",
                        "Transaction not found: " + transaction_uid);
                    return res;
                }

                res.code = 200;
                res.body = storage_commitment::transaction_to_json(*txn);
                return res;
            });

    // GET /dicomweb/storage-commitments
    // List all commitment transactions
    CROW_ROUTE(app, "/dicomweb/storage-commitments")
        .methods(crow::HTTPMethod::GET)(
            [ctx, store](const crow::request& req) {
                crow::response res;
                res.add_header("Content-Type", "application/dicom+json");
                add_cors_headers(res, *ctx);

                if (!check_storage_commitment_auth(
                        ctx, req, res, {"dicomweb.read"})) {
                    return res;
                }

                size_t limit = 100;
                auto limit_param = req.url_params.get("limit");
                if (limit_param != nullptr) {
                    try {
                        int val = std::stoi(limit_param);
                        if (val > 0) {
                            limit = static_cast<size_t>(val);
                        }
                    } catch (...) {
                        // Use default
                    }
                }

                auto transactions = store->list(limit);
                res.code = 200;
                res.body = storage_commitment::transactions_to_json(transactions);
                return res;
            });
}

}  // namespace endpoints

}  // namespace kcenon::pacs::web
