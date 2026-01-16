/**
 * @file job_manager.cpp
 * @brief Implementation of the Job Manager
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #553 - Part 2: Job Manager Core Implementation
 */

#include "pacs/client/job_manager.hpp"
#include "pacs/client/remote_node_manager.hpp"
#include "pacs/storage/job_repository.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <queue>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace pacs::client {

// =============================================================================
// UUID Generation
// =============================================================================

namespace {

/**
 * @brief Generate a UUID v4 string
 */
std::string generate_uuid() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    uint64_t ab = dis(gen);
    uint64_t cd = dis(gen);

    // Set version (4) and variant (8, 9, A, or B)
    ab = (ab & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    cd = (cd & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (ab >> 32);
    oss << '-';
    oss << std::setw(4) << ((ab >> 16) & 0xFFFF);
    oss << '-';
    oss << std::setw(4) << (ab & 0xFFFF);
    oss << '-';
    oss << std::setw(4) << (cd >> 48);
    oss << '-';
    oss << std::setw(12) << (cd & 0xFFFFFFFFFFFFULL);

    return oss.str();
}

}  // namespace

// =============================================================================
// Priority Queue Entry
// =============================================================================

struct queue_entry {
    std::string job_id;
    job_priority priority;
    std::chrono::system_clock::time_point queued_at;

    // Higher priority first, then earlier queued time (FIFO within priority)
    bool operator<(const queue_entry& other) const {
        if (static_cast<int>(priority) != static_cast<int>(other.priority)) {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
        return queued_at > other.queued_at;  // Earlier time = higher priority
    }
};

// =============================================================================
// Implementation Structure
// =============================================================================

struct job_manager::impl {
    // Configuration
    job_manager_config config;

    // Dependencies
    std::shared_ptr<storage::job_repository> repo;
    std::shared_ptr<remote_node_manager> node_manager;
    std::shared_ptr<di::ILogger> logger;

    // Job cache (for quick access)
    std::unordered_map<std::string, job_record> job_cache;
    mutable std::shared_mutex cache_mutex;

    // Priority queue for pending jobs
    std::priority_queue<queue_entry> job_queue;
    mutable std::mutex queue_mutex;
    std::condition_variable queue_cv;

    // Active jobs tracking
    std::unordered_set<std::string> active_job_ids;
    mutable std::mutex active_mutex;

    // Paused jobs
    std::unordered_set<std::string> paused_job_ids;
    mutable std::mutex paused_mutex;

    // Cancelled jobs (for checking in worker loop)
    std::unordered_set<std::string> cancelled_job_ids;
    mutable std::mutex cancelled_mutex;

    // Worker threads
    std::vector<std::thread> workers;
    std::atomic<bool> running{false};

    // Callbacks
    job_progress_callback progress_callback;
    job_completion_callback completion_callback;
    mutable std::shared_mutex callbacks_mutex;

    // Completion promises (for wait_for_completion)
    std::unordered_map<std::string, std::shared_ptr<std::promise<job_record>>> completion_promises;
    mutable std::mutex promises_mutex;

    // =========================================================================
    // Helper Methods
    // =========================================================================

    void save_job(const job_record& job) {
        // Save to cache
        {
            std::unique_lock lock(cache_mutex);
            job_cache[job.job_id] = job;
        }

        // Persist to repository
        if (repo) {
            [[maybe_unused]] auto result = repo->save(job);
        }
    }

    std::optional<job_record> get_job_from_cache(std::string_view job_id) const {
        std::shared_lock lock(cache_mutex);
        auto it = job_cache.find(std::string(job_id));
        if (it != job_cache.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void update_job_status(const std::string& job_id, job_status status,
                           const std::string& error_msg = "",
                           const std::string& error_details = "") {
        {
            std::unique_lock lock(cache_mutex);
            auto it = job_cache.find(job_id);
            if (it != job_cache.end()) {
                it->second.status = status;
                if (!error_msg.empty()) {
                    it->second.error_message = error_msg;
                    it->second.error_details = error_details;
                }
                if (status == job_status::running && !it->second.started_at.has_value()) {
                    it->second.started_at = std::chrono::system_clock::now();
                }
                if (is_terminal_status(status) && !it->second.completed_at.has_value()) {
                    it->second.completed_at = std::chrono::system_clock::now();
                }
            }
        }

        // Update repository
        if (repo) {
            if (is_terminal_status(status)) {
                if (status == job_status::completed) {
                    [[maybe_unused]] auto result = repo->mark_completed(job_id);
                } else if (status == job_status::failed) {
                    [[maybe_unused]] auto result = repo->mark_failed(job_id, error_msg, error_details);
                } else {
                    [[maybe_unused]] auto result = repo->update_status(job_id, status, error_msg, error_details);
                }
            } else if (status == job_status::running) {
                [[maybe_unused]] auto result = repo->mark_started(job_id);
            } else {
                [[maybe_unused]] auto result = repo->update_status(job_id, status, error_msg, error_details);
            }
        }
    }

    void notify_progress(const std::string& job_id, const job_progress& progress) {
        std::shared_lock lock(callbacks_mutex);
        if (progress_callback) {
            progress_callback(job_id, progress);
        }
    }

    void notify_completion(const std::string& job_id, const job_record& record) {
        // Invoke callback
        {
            std::shared_lock lock(callbacks_mutex);
            if (completion_callback) {
                completion_callback(job_id, record);
            }
        }

        // Fulfill promise if exists
        {
            std::lock_guard lock(promises_mutex);
            auto it = completion_promises.find(job_id);
            if (it != completion_promises.end()) {
                it->second->set_value(record);
                completion_promises.erase(it);
            }
        }
    }

    bool is_job_cancelled(const std::string& job_id) {
        std::lock_guard lock(cancelled_mutex);
        return cancelled_job_ids.count(job_id) > 0;
    }

    bool is_job_paused(const std::string& job_id) {
        std::lock_guard lock(paused_mutex);
        return paused_job_ids.count(job_id) > 0;
    }

    void mark_job_active(const std::string& job_id) {
        std::lock_guard lock(active_mutex);
        active_job_ids.insert(job_id);
    }

    void mark_job_inactive(const std::string& job_id) {
        std::lock_guard lock(active_mutex);
        active_job_ids.erase(job_id);
    }

    void enqueue_job(const std::string& job_id, job_priority priority) {
        {
            std::lock_guard lock(queue_mutex);
            job_queue.push({job_id, priority, std::chrono::system_clock::now()});
        }
        queue_cv.notify_one();
    }

    // =========================================================================
    // Worker Loop
    // =========================================================================

    void worker_loop() {
        while (running.load()) {
            std::string job_id;
            job_priority priority{};

            // Wait for a job
            {
                std::unique_lock lock(queue_mutex);
                queue_cv.wait(lock, [this] {
                    return !running.load() || !job_queue.empty();
                });

                if (!running.load()) {
                    break;
                }

                if (job_queue.empty()) {
                    continue;
                }

                auto entry = job_queue.top();
                job_queue.pop();
                job_id = entry.job_id;
                priority = entry.priority;
            }

            // Check if cancelled or paused
            if (is_job_cancelled(job_id)) {
                logger->debug_fmt("Job {} was cancelled before execution", job_id);
                continue;
            }

            if (is_job_paused(job_id)) {
                logger->debug_fmt("Job {} is paused, re-queueing", job_id);
                // Re-queue with same priority for later
                enqueue_job(job_id, priority);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Get job record
            auto job_opt = get_job_from_cache(job_id);
            if (!job_opt) {
                logger->warn_fmt("Job {} not found in cache", job_id);
                continue;
            }

            auto job = *job_opt;

            // Mark as running
            mark_job_active(job_id);
            update_job_status(job_id, job_status::running);
            logger->info_fmt("Starting job {}: type={}", job_id, to_string(job.type));

            // Execute job
            try {
                execute_job(job);
            } catch (const std::exception& e) {
                logger->error_fmt("Job {} failed with exception: {}", job_id, e.what());
                complete_job(job_id, job_status::failed, e.what());
            }

            mark_job_inactive(job_id);
        }
    }

    void execute_job(job_record& job) {
        switch (job.type) {
            case job_type::retrieve:
                execute_retrieve_job(job);
                break;
            case job_type::store:
                execute_store_job(job);
                break;
            case job_type::query:
                execute_query_job(job);
                break;
            case job_type::sync:
                execute_sync_job(job);
                break;
            case job_type::prefetch:
                execute_prefetch_job(job);
                break;
            default:
                complete_job(job.job_id, job_status::failed, "Unsupported job type");
                break;
        }
    }

    void execute_retrieve_job(job_record& job) {
        // Placeholder implementation - actual C-MOVE/C-GET will be in Part 3
        logger->info_fmt("Executing retrieve job {} from node {}",
                         job.job_id, job.source_node_id);

        // Simulate progress updates
        job_progress progress;
        progress.total_items = 10;  // Placeholder

        for (size_t i = 0; i < progress.total_items && !is_job_cancelled(job.job_id); ++i) {
            if (is_job_paused(job.job_id)) {
                // Wait while paused
                while (is_job_paused(job.job_id) && !is_job_cancelled(job.job_id)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (is_job_cancelled(job.job_id)) {
                    break;
                }
            }

            progress.completed_items = i + 1;
            progress.calculate_percent();
            progress.current_item = "instance_" + std::to_string(i + 1);
            update_progress(job.job_id, progress);

            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
        } else {
            complete_job(job.job_id, job_status::completed);
        }
    }

    void execute_store_job(job_record& job) {
        // Placeholder implementation - actual C-STORE will be in Part 3
        logger->info_fmt("Executing store job {} to node {}",
                         job.job_id, job.destination_node_id);

        job_progress progress;
        progress.total_items = job.instance_uids.size();

        for (size_t i = 0; i < progress.total_items && !is_job_cancelled(job.job_id); ++i) {
            if (is_job_paused(job.job_id)) {
                while (is_job_paused(job.job_id) && !is_job_cancelled(job.job_id)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (is_job_cancelled(job.job_id)) {
                    break;
                }
            }

            progress.completed_items = i + 1;
            progress.calculate_percent();
            progress.current_item = job.instance_uids[i];
            update_progress(job.job_id, progress);

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
        } else {
            complete_job(job.job_id, job_status::completed);
        }
    }

    void execute_query_job(job_record& job) {
        // Placeholder implementation - actual C-FIND will be in Part 3
        logger->info_fmt("Executing query job {} on node {}",
                         job.job_id, job.source_node_id);

        job_progress progress;
        progress.total_items = 1;
        progress.completed_items = 1;
        progress.calculate_percent();
        update_progress(job.job_id, progress);

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
        } else {
            complete_job(job.job_id, job_status::completed);
        }
    }

    void execute_sync_job(job_record& job) {
        // Placeholder implementation
        logger->info_fmt("Executing sync job {} from node {}",
                         job.job_id, job.source_node_id);

        job_progress progress;
        progress.total_items = 1;
        progress.completed_items = 1;
        progress.calculate_percent();
        update_progress(job.job_id, progress);

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
        } else {
            complete_job(job.job_id, job_status::completed);
        }
    }

    void execute_prefetch_job(job_record& job) {
        // Placeholder implementation
        logger->info_fmt("Executing prefetch job {} from node {}",
                         job.job_id, job.source_node_id);

        job_progress progress;
        progress.total_items = 1;
        progress.completed_items = 1;
        progress.calculate_percent();
        update_progress(job.job_id, progress);

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
        } else {
            complete_job(job.job_id, job_status::completed);
        }
    }

    void update_progress(const std::string& job_id, const job_progress& progress) {
        // Update cache
        {
            std::unique_lock lock(cache_mutex);
            auto it = job_cache.find(job_id);
            if (it != job_cache.end()) {
                it->second.progress = progress;
            }
        }

        // Update repository
        if (repo) {
            [[maybe_unused]] auto result = repo->update_progress(job_id, progress);
        }

        // Notify callback
        notify_progress(job_id, progress);
    }

    void complete_job(const std::string& job_id, job_status status,
                      const std::string& error_msg = "") {
        update_job_status(job_id, status, error_msg);

        // Get final job record for notification
        auto job_opt = get_job_from_cache(job_id);
        if (job_opt) {
            notify_completion(job_id, *job_opt);
        }

        // Clean up cancelled set
        if (status == job_status::cancelled) {
            std::lock_guard lock(cancelled_mutex);
            cancelled_job_ids.erase(job_id);
        }

        logger->info_fmt("Job {} completed with status: {}", job_id, to_string(status));
    }

    void load_pending_jobs_from_repo() {
        if (!repo) return;

        auto pending = repo->find_pending_jobs(config.max_queue_size);
        for (auto& job : pending) {
            // Add to cache
            {
                std::unique_lock lock(cache_mutex);
                job_cache[job.job_id] = job;
            }
            // Enqueue for execution
            enqueue_job(job.job_id, job.priority);
        }

        if (!pending.empty()) {
            logger->info_fmt("Loaded {} pending jobs from repository", pending.size());
        }
    }
};

// =============================================================================
// Construction / Destruction
// =============================================================================

job_manager::job_manager(
    std::shared_ptr<storage::job_repository> repo,
    std::shared_ptr<remote_node_manager> node_manager,
    std::shared_ptr<di::ILogger> logger)
    : job_manager(job_manager_config{}, std::move(repo), std::move(node_manager), std::move(logger)) {
}

job_manager::job_manager(
    const job_manager_config& config,
    std::shared_ptr<storage::job_repository> repo,
    std::shared_ptr<remote_node_manager> node_manager,
    std::shared_ptr<di::ILogger> logger)
    : impl_(std::make_unique<impl>()) {

    impl_->config = config;
    impl_->repo = std::move(repo);
    impl_->node_manager = std::move(node_manager);
    impl_->logger = logger ? std::move(logger) : di::null_logger();

    // Load any pending jobs from repository
    impl_->load_pending_jobs_from_repo();
}

job_manager::~job_manager() {
    stop_workers();
}

// =============================================================================
// Job Creation
// =============================================================================

std::string job_manager::create_retrieve_job(
    std::string_view source_node_id,
    std::string_view study_uid,
    std::optional<std::string_view> series_uid,
    job_priority priority) {

    job_record job;
    job.job_id = generate_uuid();
    job.type = job_type::retrieve;
    job.status = job_status::pending;
    job.priority = priority;
    job.source_node_id = std::string(source_node_id);
    job.study_uid = std::string(study_uid);
    if (series_uid) {
        job.series_uid = std::string(*series_uid);
    }
    job.created_at = std::chrono::system_clock::now();

    impl_->save_job(job);
    impl_->logger->info_fmt("Created retrieve job {}: study={}", job.job_id, study_uid);

    return job.job_id;
}

std::string job_manager::create_store_job(
    std::string_view destination_node_id,
    const std::vector<std::string>& instance_uids,
    job_priority priority) {

    job_record job;
    job.job_id = generate_uuid();
    job.type = job_type::store;
    job.status = job_status::pending;
    job.priority = priority;
    job.destination_node_id = std::string(destination_node_id);
    job.instance_uids = instance_uids;
    job.progress.total_items = instance_uids.size();
    job.created_at = std::chrono::system_clock::now();

    impl_->save_job(job);
    impl_->logger->info_fmt("Created store job {}: {} instances to {}",
                             job.job_id, instance_uids.size(), destination_node_id);

    return job.job_id;
}

std::string job_manager::create_query_job(
    std::string_view node_id,
    std::string_view query_level,
    const std::unordered_map<std::string, std::string>& query_keys,
    job_priority priority) {

    job_record job;
    job.job_id = generate_uuid();
    job.type = job_type::query;
    job.status = job_status::pending;
    job.priority = priority;
    job.source_node_id = std::string(node_id);
    job.metadata["query_level"] = std::string(query_level);
    for (const auto& [key, value] : query_keys) {
        job.metadata["query_" + key] = value;
    }
    job.created_at = std::chrono::system_clock::now();

    impl_->save_job(job);
    impl_->logger->info_fmt("Created query job {}: level={}", job.job_id, query_level);

    return job.job_id;
}

std::string job_manager::create_sync_job(
    std::string_view source_node_id,
    std::optional<std::string_view> patient_id,
    job_priority priority) {

    job_record job;
    job.job_id = generate_uuid();
    job.type = job_type::sync;
    job.status = job_status::pending;
    job.priority = priority;
    job.source_node_id = std::string(source_node_id);
    if (patient_id) {
        job.patient_id = std::string(*patient_id);
    }
    job.created_at = std::chrono::system_clock::now();

    impl_->save_job(job);
    impl_->logger->info_fmt("Created sync job {}: from {}", job.job_id, source_node_id);

    return job.job_id;
}

std::string job_manager::create_prefetch_job(
    std::string_view source_node_id,
    std::string_view patient_id,
    job_priority priority) {

    job_record job;
    job.job_id = generate_uuid();
    job.type = job_type::prefetch;
    job.status = job_status::pending;
    job.priority = priority;
    job.source_node_id = std::string(source_node_id);
    job.patient_id = std::string(patient_id);
    job.created_at = std::chrono::system_clock::now();

    impl_->save_job(job);
    impl_->logger->info_fmt("Created prefetch job {}: patient={}", job.job_id, patient_id);

    return job.job_id;
}

// =============================================================================
// Job Control
// =============================================================================

pacs::VoidResult job_manager::start_job(std::string_view job_id) {
    auto job_opt = impl_->get_job_from_cache(job_id);
    if (!job_opt) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Job not found: " + std::string(job_id));
    }

    if (!job_opt->can_start()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Job cannot be started in current state: " + std::string(to_string(job_opt->status)));
    }

    // Update status and enqueue
    impl_->update_job_status(std::string(job_id), job_status::queued);
    {
        std::unique_lock lock(impl_->cache_mutex);
        auto it = impl_->job_cache.find(std::string(job_id));
        if (it != impl_->job_cache.end()) {
            it->second.queued_at = std::chrono::system_clock::now();
        }
    }
    impl_->enqueue_job(std::string(job_id), job_opt->priority);

    impl_->logger->info_fmt("Started job {}", job_id);
    return pacs::ok();
}

pacs::VoidResult job_manager::pause_job(std::string_view job_id) {
    auto job_opt = impl_->get_job_from_cache(job_id);
    if (!job_opt) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Job not found: " + std::string(job_id));
    }

    if (!job_opt->can_pause()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Job cannot be paused in current state: " + std::string(to_string(job_opt->status)));
    }

    {
        std::lock_guard lock(impl_->paused_mutex);
        impl_->paused_job_ids.insert(std::string(job_id));
    }
    impl_->update_job_status(std::string(job_id), job_status::paused);

    impl_->logger->info_fmt("Paused job {}", job_id);
    return pacs::ok();
}

pacs::VoidResult job_manager::resume_job(std::string_view job_id) {
    auto job_opt = impl_->get_job_from_cache(job_id);
    if (!job_opt) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Job not found: " + std::string(job_id));
    }

    if (job_opt->status != job_status::paused) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Job is not paused: " + std::string(to_string(job_opt->status)));
    }

    {
        std::lock_guard lock(impl_->paused_mutex);
        impl_->paused_job_ids.erase(std::string(job_id));
    }
    impl_->update_job_status(std::string(job_id), job_status::queued);

    impl_->logger->info_fmt("Resumed job {}", job_id);
    return pacs::ok();
}

pacs::VoidResult job_manager::cancel_job(std::string_view job_id) {
    auto job_opt = impl_->get_job_from_cache(job_id);
    if (!job_opt) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Job not found: " + std::string(job_id));
    }

    if (!job_opt->can_cancel()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Job cannot be cancelled in current state: " + std::string(to_string(job_opt->status)));
    }

    {
        std::lock_guard lock(impl_->cancelled_mutex);
        impl_->cancelled_job_ids.insert(std::string(job_id));
    }

    // If not actively running, update status immediately
    {
        std::lock_guard lock(impl_->active_mutex);
        if (impl_->active_job_ids.count(std::string(job_id)) == 0) {
            impl_->update_job_status(std::string(job_id), job_status::cancelled);
            auto final_job = impl_->get_job_from_cache(job_id);
            if (final_job) {
                impl_->notify_completion(std::string(job_id), *final_job);
            }
        }
    }

    impl_->logger->info_fmt("Cancelled job {}", job_id);
    return pacs::ok();
}

pacs::VoidResult job_manager::retry_job(std::string_view job_id) {
    auto job_opt = impl_->get_job_from_cache(job_id);
    if (!job_opt) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Job not found: " + std::string(job_id));
    }

    if (!job_opt->can_retry()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Job cannot be retried: " + std::string(to_string(job_opt->status)));
    }

    // Reset job state
    {
        std::unique_lock lock(impl_->cache_mutex);
        auto it = impl_->job_cache.find(std::string(job_id));
        if (it != impl_->job_cache.end()) {
            it->second.status = job_status::pending;
            it->second.retry_count++;
            it->second.error_message.clear();
            it->second.error_details.clear();
            it->second.started_at = std::nullopt;
            it->second.completed_at = std::nullopt;
            it->second.progress = job_progress{};
        }
    }

    if (impl_->repo) {
        [[maybe_unused]] auto result = impl_->repo->increment_retry(job_id);
        [[maybe_unused]] auto status_result = impl_->repo->update_status(job_id, job_status::pending);
    }

    impl_->logger->info_fmt("Retrying job {} (attempt {})", job_id, job_opt->retry_count + 1);
    return pacs::ok();
}

pacs::VoidResult job_manager::delete_job(std::string_view job_id) {
    auto job_opt = impl_->get_job_from_cache(job_id);
    if (!job_opt) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Job not found: " + std::string(job_id));
    }

    // Cancel if not terminal
    if (!job_opt->is_finished()) {
        auto cancel_result = cancel_job(job_id);
        if (cancel_result.is_err()) {
            return cancel_result;
        }
    }

    // Remove from cache
    {
        std::unique_lock lock(impl_->cache_mutex);
        impl_->job_cache.erase(std::string(job_id));
    }

    // Remove from repository
    if (impl_->repo) {
        [[maybe_unused]] auto result = impl_->repo->remove(job_id);
    }

    impl_->logger->info_fmt("Deleted job {}", job_id);
    return pacs::ok();
}

// =============================================================================
// Job Queries
// =============================================================================

std::optional<job_record> job_manager::get_job(std::string_view job_id) const {
    return impl_->get_job_from_cache(job_id);
}

std::vector<job_record> job_manager::list_jobs(
    std::optional<job_status> status,
    std::optional<job_type> type,
    size_t limit,
    size_t offset) const {

    if (impl_->repo) {
        storage::job_query_options options;
        options.status = status;
        options.type = type;
        options.limit = limit;
        options.offset = offset;
        return impl_->repo->find_jobs(options);
    }

    // Fallback to cache
    std::vector<job_record> result;
    std::shared_lock lock(impl_->cache_mutex);
    for (const auto& [_, job] : impl_->job_cache) {
        if (status && job.status != *status) continue;
        if (type && job.type != *type) continue;
        if (offset > 0) {
            --offset;
            continue;
        }
        result.push_back(job);
        if (result.size() >= limit) break;
    }
    return result;
}

std::vector<job_record> job_manager::list_jobs_by_node(std::string_view node_id) const {
    if (impl_->repo) {
        return impl_->repo->find_by_node(node_id);
    }

    // Fallback to cache
    std::vector<job_record> result;
    std::shared_lock lock(impl_->cache_mutex);
    std::string node_str(node_id);
    for (const auto& [_, job] : impl_->job_cache) {
        if (job.source_node_id == node_str || job.destination_node_id == node_str) {
            result.push_back(job);
        }
    }
    return result;
}

// =============================================================================
// Progress Monitoring
// =============================================================================

job_progress job_manager::get_progress(std::string_view job_id) const {
    auto job_opt = impl_->get_job_from_cache(job_id);
    if (job_opt) {
        return job_opt->progress;
    }
    return {};
}

void job_manager::set_progress_callback(job_progress_callback callback) {
    std::unique_lock lock(impl_->callbacks_mutex);
    impl_->progress_callback = std::move(callback);
}

void job_manager::set_completion_callback(job_completion_callback callback) {
    std::unique_lock lock(impl_->callbacks_mutex);
    impl_->completion_callback = std::move(callback);
}

// =============================================================================
// Wait for Completion
// =============================================================================

std::future<job_record> job_manager::wait_for_completion(std::string_view job_id) {
    auto promise = std::make_shared<std::promise<job_record>>();
    auto future = promise->get_future();

    // Check if already completed
    auto job_opt = impl_->get_job_from_cache(job_id);
    if (job_opt && job_opt->is_finished()) {
        promise->set_value(*job_opt);
        return future;
    }

    // Store promise for later fulfillment
    {
        std::lock_guard lock(impl_->promises_mutex);
        impl_->completion_promises[std::string(job_id)] = promise;
    }

    return future;
}

// =============================================================================
// Worker Management
// =============================================================================

void job_manager::start_workers() {
    if (impl_->running.load()) {
        return;
    }

    impl_->running.store(true);
    impl_->workers.reserve(impl_->config.worker_count);

    for (size_t i = 0; i < impl_->config.worker_count; ++i) {
        impl_->workers.emplace_back([this]() {
            impl_->worker_loop();
        });
    }

    impl_->logger->info_fmt("Started {} worker threads", impl_->config.worker_count);
}

void job_manager::stop_workers() {
    if (!impl_->running.load()) {
        return;
    }

    impl_->running.store(false);
    impl_->queue_cv.notify_all();

    for (auto& worker : impl_->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    impl_->workers.clear();

    impl_->logger->info("Stopped worker threads");
}

bool job_manager::is_running() const noexcept {
    return impl_->running.load();
}

// =============================================================================
// Statistics
// =============================================================================

size_t job_manager::active_jobs() const {
    std::lock_guard lock(impl_->active_mutex);
    return impl_->active_job_ids.size();
}

size_t job_manager::pending_jobs() const {
    std::lock_guard lock(impl_->queue_mutex);
    return impl_->job_queue.size();
}

size_t job_manager::completed_jobs_today() const {
    if (impl_->repo) {
        return impl_->repo->count_completed_today();
    }
    return 0;
}

size_t job_manager::failed_jobs_today() const {
    if (impl_->repo) {
        return impl_->repo->count_failed_today();
    }
    return 0;
}

// =============================================================================
// Configuration
// =============================================================================

const job_manager_config& job_manager::config() const noexcept {
    return impl_->config;
}

}  // namespace pacs::client
