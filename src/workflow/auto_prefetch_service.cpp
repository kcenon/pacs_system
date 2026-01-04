/**
 * @file auto_prefetch_service.cpp
 * @brief Implementation of automatic prefetch service for prior studies
 */

#include "pacs/workflow/auto_prefetch_service.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/integration/logger_adapter.hpp"
#include "pacs/integration/monitoring_adapter.hpp"
#include "pacs/integration/thread_adapter.hpp"
#include "pacs/integration/executor_adapter.hpp"

#include <kcenon/common/interfaces/executor_interface.h>

#include <algorithm>
#include <chrono>
#include <ranges>

namespace pacs::workflow {

// =========================================================================
// Construction
// =========================================================================

auto_prefetch_service::auto_prefetch_service(
    storage::index_database& database,
    const prefetch_service_config& config)
    : database_(database)
    , config_(config) {
    if (config_.auto_start && config_.enabled) {
        start();
    }
}

auto_prefetch_service::auto_prefetch_service(
    storage::index_database& database,
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
    const prefetch_service_config& config)
    : database_(database)
    , thread_pool_(std::move(thread_pool))
    , config_(config) {
    if (config_.auto_start && config_.enabled) {
        start();
    }
}

auto_prefetch_service::auto_prefetch_service(
    storage::index_database& database,
    std::shared_ptr<kcenon::common::interfaces::IExecutor> executor,
    const prefetch_service_config& config)
    : database_(database)
    , executor_(std::move(executor))
    , config_(config) {
    if (config_.auto_start && config_.enabled) {
        start();
    }
}

auto_prefetch_service::~auto_prefetch_service() {
    stop(true);
}

// =========================================================================
// Lifecycle Management
// =========================================================================

void auto_prefetch_service::enable() {
    start();
}

void auto_prefetch_service::start() {
    if (enabled_.exchange(true)) {
        return;  // Already enabled
    }

    stop_requested_.store(false);
    next_cycle_time_ = std::chrono::steady_clock::now();

    worker_thread_ = std::thread([this]() {
        run_loop();
    });

    integration::logger_adapter::info(
        "Auto prefetch service started interval_seconds={} max_concurrent={}",
        config_.prefetch_interval.count(),
        config_.max_concurrent_prefetches);
}

void auto_prefetch_service::disable(bool wait_for_completion) {
    stop(wait_for_completion);
}

void auto_prefetch_service::stop(bool wait_for_completion) {
    if (!enabled_.exchange(false)) {
        return;  // Already disabled
    }

    stop_requested_.store(true);

    // Wake up the worker thread
    cv_.notify_all();

    if (wait_for_completion && worker_thread_.joinable()) {
        worker_thread_.join();
    } else if (worker_thread_.joinable()) {
        worker_thread_.detach();
    }

    integration::logger_adapter::info("Auto prefetch service stopped");
}

auto auto_prefetch_service::is_enabled() const noexcept -> bool {
    return enabled_.load();
}

auto auto_prefetch_service::is_running() const noexcept -> bool {
    return is_enabled();
}

// =========================================================================
// Manual Operations
// =========================================================================

auto auto_prefetch_service::prefetch_priors(
    const std::string& patient_id,
    std::chrono::days lookback) -> prefetch_result {

    prefetch_request request;
    request.patient_id = patient_id;
    request.request_time = std::chrono::system_clock::now();

    // Override criteria lookback if specified
    auto saved_lookback = config_.criteria.lookback_period;
    config_.criteria.lookback_period = lookback;

    auto result = process_request(request);

    config_.criteria.lookback_period = saved_lookback;

    return result;
}

void auto_prefetch_service::trigger_for_worklist(
    const std::vector<storage::worklist_item>& worklist_items) {
    on_worklist_query(worklist_items);
}

void auto_prefetch_service::trigger_cycle() {
    std::lock_guard<std::mutex> lock(mutex_);
    next_cycle_time_ = std::chrono::steady_clock::now();
    cv_.notify_one();
}

auto auto_prefetch_service::run_prefetch_cycle() -> prefetch_result {
    return execute_cycle();
}

// =========================================================================
// Worklist Event Handler
// =========================================================================

void auto_prefetch_service::on_worklist_query(
    const std::vector<storage::worklist_item>& worklist_items) {

    for (const auto& item : worklist_items) {
        if (item.patient_id.empty()) {
            continue;
        }

        prefetch_request request;
        request.patient_id = item.patient_id;
        request.patient_name = item.patient_name;
        request.scheduled_modality = item.modality;
        request.scheduled_study_uid = item.study_uid;
        request.request_time = std::chrono::system_clock::now();

        queue_request(request);
    }

    // Wake up worker if there are new requests
    cv_.notify_one();

    integration::logger_adapter::debug(
        "Queued prefetch requests from worklist worklist_items={} queue_size={}",
        worklist_items.size(),
        pending_requests());
}

// =========================================================================
// Statistics and Monitoring
// =========================================================================

auto auto_prefetch_service::get_last_result() const
    -> std::optional<prefetch_result> {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_result_;
}

auto auto_prefetch_service::get_cumulative_stats() const -> prefetch_result {
    std::lock_guard<std::mutex> lock(mutex_);
    return cumulative_stats_;
}

auto auto_prefetch_service::time_until_next_cycle() const
    -> std::optional<std::chrono::seconds> {
    if (!enabled_.load()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    if (next_cycle_time_ <= now) {
        return std::chrono::seconds{0};
    }

    return std::chrono::duration_cast<std::chrono::seconds>(
        next_cycle_time_ - now);
}

auto auto_prefetch_service::cycles_completed() const noexcept -> std::size_t {
    return cycles_count_.load();
}

auto auto_prefetch_service::pending_requests() const noexcept -> std::size_t {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return request_queue_.size();
}

// =========================================================================
// Configuration
// =========================================================================

void auto_prefetch_service::set_prefetch_interval(
    std::chrono::seconds interval) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.prefetch_interval = interval;
}

auto auto_prefetch_service::get_prefetch_interval() const noexcept
    -> std::chrono::seconds {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.prefetch_interval;
}

void auto_prefetch_service::set_prefetch_criteria(
    const prefetch_criteria& criteria) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.criteria = criteria;
}

auto auto_prefetch_service::get_prefetch_criteria() const noexcept
    -> const prefetch_criteria& {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.criteria;
}

void auto_prefetch_service::set_cycle_complete_callback(
    prefetch_service_config::cycle_complete_callback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.on_cycle_complete = std::move(callback);
}

void auto_prefetch_service::set_error_callback(
    prefetch_service_config::error_callback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.on_prefetch_error = std::move(callback);
}

// =========================================================================
// Internal Methods
// =========================================================================

void auto_prefetch_service::run_loop() {
    integration::logger_adapter::debug("Prefetch service worker thread started");

    while (!stop_requested_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait until next cycle time or until woken up
        auto wait_until = next_cycle_time_;
        cv_.wait_until(lock, wait_until, [this]() {
            return stop_requested_.load() ||
                   std::chrono::steady_clock::now() >= next_cycle_time_ ||
                   !request_queue_.empty();
        });

        if (stop_requested_.load()) {
            break;
        }

        // Check if we should run a cycle
        auto now = std::chrono::steady_clock::now();
        bool should_run_cycle = (now >= next_cycle_time_) ||
                                (!request_queue_.empty());

        if (should_run_cycle) {
            lock.unlock();

            cycle_in_progress_.store(true);
            auto result = execute_cycle();
            cycle_in_progress_.store(false);

            lock.lock();

            // Update last result
            last_result_ = result;
            update_stats(result);
            ++cycles_count_;

            // Schedule next cycle
            next_cycle_time_ = std::chrono::steady_clock::now() +
                               config_.prefetch_interval;

            // Invoke callback
            if (config_.on_cycle_complete) {
                lock.unlock();
                config_.on_cycle_complete(result);
                lock.lock();
            }
        }
    }

    integration::logger_adapter::debug("Prefetch service worker thread stopped");
}

auto auto_prefetch_service::execute_cycle() -> prefetch_result {
    prefetch_result cycle_result;
    cycle_result.timestamp = std::chrono::system_clock::now();
    auto cycle_start = std::chrono::steady_clock::now();

    // Process all pending requests
    while (auto request = dequeue_request()) {
        if (stop_requested_.load()) {
            break;
        }

        auto result = process_request(*request);
        cycle_result += result;
    }

    cycle_result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cycle_start);

    integration::logger_adapter::info(
        "Prefetch cycle completed patients={} studies_prefetched={} studies_failed={} duration_ms={}",
        cycle_result.patients_processed,
        cycle_result.studies_prefetched,
        cycle_result.studies_failed,
        cycle_result.duration.count());

    // Record metrics
    integration::monitoring_adapter::record_histogram(
        "prefetch_cycle_duration_ms",
        static_cast<double>(cycle_result.duration.count()));
    integration::monitoring_adapter::increment_counter(
        "prefetch_studies_total",
        static_cast<int64_t>(cycle_result.studies_prefetched));
    integration::monitoring_adapter::increment_counter(
        "prefetch_failures_total",
        static_cast<int64_t>(cycle_result.studies_failed));

    return cycle_result;
}

auto auto_prefetch_service::process_request(const prefetch_request& request)
    -> prefetch_result {

    prefetch_result result;
    result.patients_processed = 1;
    result.timestamp = std::chrono::system_clock::now();
    auto start_time = std::chrono::steady_clock::now();

    integration::logger_adapter::debug(
        "Processing prefetch request patient_id={} scheduled_modality={}",
        request.patient_id,
        request.scheduled_modality);

    // Query each remote PACS for prior studies
    for (const auto& pacs : config_.remote_pacs) {
        if (!pacs.is_valid()) {
            continue;
        }

        // Query for prior studies
        auto prior_studies = query_prior_studies(
            pacs,
            request.patient_id,
            config_.criteria.lookback_period);

        // Filter based on criteria
        auto filtered_studies = filter_studies(prior_studies, request);

        // Prefetch each study
        for (const auto& study : filtered_studies) {
            // Skip if already present locally
            if (study_exists_locally(study.study_instance_uid)) {
                ++result.studies_already_present;
                continue;
            }

            // Skip the scheduled study itself
            if (study.study_instance_uid == request.scheduled_study_uid) {
                continue;
            }

            // Attempt prefetch
            bool success = prefetch_study(pacs, study);

            if (success) {
                ++result.studies_prefetched;
                result.series_prefetched += study.number_of_series;
                result.instances_prefetched += study.number_of_instances;

                if (config_.on_prefetch_complete) {
                    config_.on_prefetch_complete(
                        request.patient_id, study, true, "");
                }
            } else {
                ++result.studies_failed;

                if (config_.on_prefetch_error) {
                    config_.on_prefetch_error(
                        request.patient_id,
                        study.study_instance_uid,
                        "Failed to prefetch study");
                }
            }

            // Check rate limiting
            if (config_.rate_limit_per_minute > 0) {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto elapsed_minutes =
                    std::chrono::duration_cast<std::chrono::minutes>(elapsed)
                        .count();
                if (elapsed_minutes < 1) {
                    auto prefetched_this_minute =
                        result.studies_prefetched + result.studies_failed;
                    if (prefetched_this_minute >= config_.rate_limit_per_minute) {
                        // Wait until the next minute
                        std::this_thread::sleep_for(
                            std::chrono::seconds(60) - elapsed);
                    }
                }
            }
        }
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);

    return result;
}

auto auto_prefetch_service::query_prior_studies(
    const remote_pacs_config& pacs_config,
    const std::string& patient_id,
    std::chrono::days lookback) -> std::vector<prior_study_info> {

    std::vector<prior_study_info> results;

    // Calculate date range
    auto now = std::chrono::system_clock::now();
    auto from_time = now - lookback;

    // Format dates as YYYYMMDD
    auto format_date = [](std::chrono::system_clock::time_point tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = *std::localtime(&time_t);
        char buffer[9];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d", &tm);
        return std::string(buffer);
    };

    std::string from_date = format_date(from_time);
    std::string to_date = format_date(now);

    integration::logger_adapter::debug(
        "Querying prior studies remote_pacs={} patient_id={} from_date={} to_date={}",
        pacs_config.ae_title,
        patient_id,
        from_date,
        to_date);

    // TODO: Implement actual C-FIND query to remote PACS
    // This would use network::association to connect and query
    // For now, return empty results as the actual network implementation
    // depends on the association and DIMSE message handling

    /*
    // Example of what the actual implementation would look like:
    try {
        // Create association to remote PACS
        network::association assoc;
        auto connect_result = assoc.connect(
            pacs_config.host,
            pacs_config.port,
            pacs_config.local_ae_title,
            pacs_config.ae_title);

        if (!connect_result.is_ok()) {
            integration::logger_adapter::error(
                "Failed to connect to remote PACS",
                {{"remote_pacs", pacs_config.ae_title},
                 {"error", connect_result.error()}});
            return results;
        }

        // Build C-FIND query
        core::dicom_dataset query_keys;
        query_keys.set_string(tags::patient_id, patient_id);
        query_keys.set_string(tags::study_date, from_date + "-" + to_date);
        query_keys.set_string(tags::query_retrieve_level, "STUDY");
        // Request return keys
        query_keys.set_string(tags::study_instance_uid, "");
        query_keys.set_string(tags::study_description, "");
        query_keys.set_string(tags::modalities_in_study, "");
        query_keys.set_string(tags::number_of_study_related_series, "");
        query_keys.set_string(tags::number_of_study_related_instances, "");

        // Send C-FIND and collect results
        auto find_result = assoc.find(query_keys);
        if (find_result.is_ok()) {
            for (const auto& dataset : find_result.value()) {
                prior_study_info info;
                info.study_instance_uid =
                    dataset.get_string(tags::study_instance_uid);
                info.patient_id = patient_id;
                info.study_date = dataset.get_string(tags::study_date);
                info.study_description =
                    dataset.get_string(tags::study_description);
                // Parse modalities in study
                // ...
                results.push_back(std::move(info));
            }
        }

        assoc.release();
    } catch (const std::exception& e) {
        integration::logger_adapter::error(
            "Exception querying prior studies",
            {{"error", e.what()}});
    }
    */

    return results;
}

auto auto_prefetch_service::filter_studies(
    const std::vector<prior_study_info>& studies,
    const prefetch_request& request) -> std::vector<prior_study_info> {

    std::vector<prior_study_info> filtered;

    for (const auto& study : studies) {
        // Apply modality filters
        bool modality_match = true;

        // Check include list (if not empty, study must match)
        if (!config_.criteria.include_modalities.empty()) {
            modality_match = false;
            for (const auto& mod : study.modalities) {
                if (config_.criteria.include_modalities.count(mod) > 0) {
                    modality_match = true;
                    break;
                }
            }
        }

        // Check exclude list
        if (modality_match && !config_.criteria.exclude_modalities.empty()) {
            for (const auto& mod : study.modalities) {
                if (config_.criteria.exclude_modalities.count(mod) > 0) {
                    modality_match = false;
                    break;
                }
            }
        }

        if (!modality_match) {
            continue;
        }

        // Apply body part filter
        if (!config_.criteria.include_body_parts.empty()) {
            if (config_.criteria.include_body_parts.count(
                    study.body_part_examined) == 0) {
                continue;
            }
        }

        filtered.push_back(study);
    }

    // Sort by preference if enabled
    if (config_.criteria.prefer_same_modality ||
        config_.criteria.prefer_same_body_part) {
        std::ranges::sort(filtered, [&](const auto& a, const auto& b) {
            int score_a = 0;
            int score_b = 0;

            if (config_.criteria.prefer_same_modality) {
                if (a.modalities.count(request.scheduled_modality) > 0) {
                    score_a += 10;
                }
                if (b.modalities.count(request.scheduled_modality) > 0) {
                    score_b += 10;
                }
            }

            if (config_.criteria.prefer_same_body_part) {
                if (a.body_part_examined == request.scheduled_body_part) {
                    score_a += 5;
                }
                if (b.body_part_examined == request.scheduled_body_part) {
                    score_b += 5;
                }
            }

            // Higher score first, then by date (newer first)
            if (score_a != score_b) {
                return score_a > score_b;
            }
            return a.study_date > b.study_date;
        });
    }

    // Limit results
    if (config_.criteria.max_studies_per_patient > 0 &&
        filtered.size() > config_.criteria.max_studies_per_patient) {
        filtered.resize(config_.criteria.max_studies_per_patient);
    }

    return filtered;
}

auto auto_prefetch_service::study_exists_locally(const std::string& study_uid)
    -> bool {
    // Query local database to check if study exists
    auto study_record = database_.find_study(study_uid);
    return study_record.has_value();
}

auto auto_prefetch_service::prefetch_study(
    const remote_pacs_config& pacs_config,
    const prior_study_info& study) -> bool {

    integration::logger_adapter::debug(
        "Prefetching study study_uid={} patient_id={} remote_pacs={}",
        study.study_instance_uid,
        study.patient_id,
        pacs_config.ae_title);

    // TODO: Implement actual C-MOVE to prefetch study
    // This would use network::association to connect and send C-MOVE

    /*
    // Example of what the actual implementation would look like:
    try {
        // Create association to remote PACS
        network::association assoc;
        auto connect_result = assoc.connect(
            pacs_config.host,
            pacs_config.port,
            pacs_config.local_ae_title,
            pacs_config.ae_title);

        if (!connect_result.is_ok()) {
            integration::logger_adapter::error(
                "Failed to connect for C-MOVE",
                {{"remote_pacs", pacs_config.ae_title},
                 {"error", connect_result.error()}});
            return false;
        }

        // Build C-MOVE query
        core::dicom_dataset move_keys;
        move_keys.set_string(tags::query_retrieve_level, "STUDY");
        move_keys.set_string(tags::study_instance_uid, study.study_instance_uid);

        // Send C-MOVE (destination is our storage SCP)
        auto move_result = assoc.move(move_keys, our_storage_ae_title);

        assoc.release();

        if (move_result.is_ok()) {
            integration::logger_adapter::info(
                "Successfully prefetched study",
                {{"study_uid", study.study_instance_uid}});
            return true;
        } else {
            integration::logger_adapter::error(
                "C-MOVE failed",
                {{"study_uid", study.study_instance_uid},
                 {"error", move_result.error()}});
            return false;
        }
    } catch (const std::exception& e) {
        integration::logger_adapter::error(
            "Exception during prefetch",
            {{"study_uid", study.study_instance_uid},
             {"error", e.what()}});
        return false;
    }
    */

    // For now, return false as actual implementation requires network module
    return false;
}

void auto_prefetch_service::update_stats(const prefetch_result& result) {
    cumulative_stats_ += result;
}

void auto_prefetch_service::queue_request(const prefetch_request& request) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // Deduplicate by patient ID
    if (queued_patients_.count(request.patient_id) > 0) {
        return;
    }

    request_queue_.push(request);
    queued_patients_.insert(request.patient_id);
}

auto auto_prefetch_service::dequeue_request()
    -> std::optional<prefetch_request> {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (request_queue_.empty()) {
        return std::nullopt;
    }

    auto request = std::move(request_queue_.front());
    request_queue_.pop();
    queued_patients_.erase(request.patient_id);

    return request;
}

}  // namespace pacs::workflow
