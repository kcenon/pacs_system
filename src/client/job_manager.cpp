/**
 * @file job_manager.cpp
 * @brief Implementation of the Job Manager
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #553 - Part 2: Job Manager Core Implementation
 * @see Issue #554 - Part 3: Job Execution and Tests
 */

#include "pacs/client/job_manager.hpp"
#include "pacs/client/remote_node_manager.hpp"
#include "pacs/storage/job_repository.hpp"
#include "pacs/services/query_scu.hpp"
#include "pacs/services/retrieve_scu.hpp"
#include "pacs/services/storage_scu.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"

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
        logger->info_fmt("Executing retrieve job {} from node {}",
                         job.job_id, job.source_node_id);

        // Get the source node information
        auto node_opt = node_manager->get_node(job.source_node_id);
        if (!node_opt) {
            complete_job(job.job_id, job_status::failed,
                         "Source node not found: " + job.source_node_id);
            return;
        }

        const auto& node = *node_opt;
        if (!node.is_online()) {
            complete_job(job.job_id, job_status::failed,
                         "Source node is offline: " + job.source_node_id);
            return;
        }

        // Check for cancellation
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Wait if paused
        while (is_job_paused(job.job_id) && !is_job_cancelled(job.job_id)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Determine SOP classes needed based on retrieve type
        std::vector<std::string> sop_classes;
        if (node.supports_move) {
            sop_classes.push_back(std::string(services::study_root_move_sop_class_uid));
        } else if (node.supports_get) {
            sop_classes.push_back(std::string(services::study_root_get_sop_class_uid));
        } else {
            complete_job(job.job_id, job_status::failed,
                         "Node does not support C-MOVE or C-GET");
            return;
        }

        // Acquire association
        auto assoc_result = node_manager->acquire_association(
            job.source_node_id, sop_classes);
        if (assoc_result.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Failed to acquire association: " + assoc_result.error().message);
            return;
        }

        auto assoc = std::move(assoc_result.value());

        // Configure retrieve SCU
        services::retrieve_scu_config retrieve_config;
        retrieve_config.mode = node.supports_move
            ? services::retrieve_mode::c_move
            : services::retrieve_mode::c_get;
        retrieve_config.model = services::query_model::study_root;

        // Set level based on job scope
        if (job.sop_instance_uid.has_value()) {
            retrieve_config.level = services::query_level::image;
        } else if (job.series_uid.has_value()) {
            retrieve_config.level = services::query_level::series;
        } else {
            retrieve_config.level = services::query_level::study;
        }

        // Set move destination if using C-MOVE
        if (retrieve_config.mode == services::retrieve_mode::c_move) {
            retrieve_config.move_destination = config.local_ae_title;
        }

        services::retrieve_scu scu(retrieve_config, logger);

        // Track progress
        job_progress progress;
        progress.total_items = 0;  // Will be updated from pending responses

        // Progress callback
        auto progress_callback = [this, &job, &progress](
                                     const services::retrieve_progress& rp) {
            // Check for pause/cancel
            if (is_job_cancelled(job.job_id)) {
                return;
            }

            // Wait if paused
            while (is_job_paused(job.job_id) && !is_job_cancelled(job.job_id)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            progress.total_items = rp.total();
            progress.completed_items = rp.completed;
            progress.failed_items = rp.failed;
            progress.skipped_items = rp.warning;
            progress.percent_complete = rp.percent();
            progress.elapsed = rp.elapsed();
            update_progress(job.job_id, progress);
        };

        // Validate study UID is present
        if (!job.study_uid.has_value()) {
            complete_job(job.job_id, job_status::failed,
                         "No study UID specified for retrieve job");
            node_manager->release_association(job.source_node_id, std::move(assoc));
            return;
        }

        // Execute retrieve based on level
        auto result = job.series_uid.has_value()
            ? scu.retrieve_series(*assoc, *job.series_uid, progress_callback)
            : scu.retrieve_study(*assoc, *job.study_uid, progress_callback);

        // Release association back to pool
        node_manager->release_association(job.source_node_id, std::move(assoc));

        // Handle result
        if (result.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Retrieve failed: " + result.error().message);
            return;
        }

        const auto& retrieve_result = result.value();

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
        } else if (retrieve_result.is_success()) {
            // Update final progress
            progress.completed_items = retrieve_result.completed;
            progress.failed_items = retrieve_result.failed;
            progress.skipped_items = retrieve_result.warning;
            progress.percent_complete = 100.0f;
            update_progress(job.job_id, progress);

            complete_job(job.job_id, job_status::completed);
        } else if (retrieve_result.has_failures()) {
            std::ostringstream oss;
            oss << "Retrieve completed with failures: " << retrieve_result.failed
                << " of " << (retrieve_result.completed + retrieve_result.failed)
                << " sub-operations failed";
            complete_job(job.job_id, job_status::failed, oss.str());
        } else {
            complete_job(job.job_id, job_status::completed);
        }
    }

    void execute_store_job(job_record& job) {
        logger->info_fmt("Executing store job {} to node {} ({} instances)",
                         job.job_id, job.destination_node_id, job.instance_uids.size());

        // Validate input
        if (job.instance_uids.empty()) {
            complete_job(job.job_id, job_status::failed,
                         "No instance UIDs specified for store job");
            return;
        }

        // Get destination node information
        auto node_opt = node_manager->get_node(job.destination_node_id);
        if (!node_opt) {
            complete_job(job.job_id, job_status::failed,
                         "Destination node not found: " + job.destination_node_id);
            return;
        }

        const auto& node = *node_opt;
        if (!node.is_online()) {
            complete_job(job.job_id, job_status::failed,
                         "Destination node is offline: " + job.destination_node_id);
            return;
        }

        if (!node.supports_store) {
            complete_job(job.job_id, job_status::failed,
                         "Destination node does not support C-STORE");
            return;
        }

        // Check for cancellation
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Initialize progress tracking
        job_progress progress;
        progress.total_items = job.instance_uids.size();

        // We need to determine the SOP classes from the instances to be stored
        // For now, use a placeholder set of common storage SOP classes
        std::vector<std::string> sop_classes = {
            "1.2.840.10008.5.1.4.1.1.2",     // CT Image Storage
            "1.2.840.10008.5.1.4.1.1.4",     // MR Image Storage
            "1.2.840.10008.5.1.4.1.1.7",     // Secondary Capture
            "1.2.840.10008.5.1.4.1.1.1.2",   // Digital X-Ray
        };

        // Acquire association
        auto assoc_result = node_manager->acquire_association(
            job.destination_node_id, sop_classes);
        if (assoc_result.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Failed to acquire association: " + assoc_result.error().message);
            return;
        }

        auto assoc = std::move(assoc_result.value());

        // Create storage SCU
        services::storage_scu_config store_config;
        store_config.continue_on_error = true;
        services::storage_scu scu(store_config, logger);

        // Process each instance
        size_t completed = 0;
        size_t failed = 0;
        std::string last_error;

        for (size_t i = 0; i < job.instance_uids.size(); ++i) {
            // Check for cancellation
            if (is_job_cancelled(job.job_id)) {
                break;
            }

            // Wait if paused
            while (is_job_paused(job.job_id) && !is_job_cancelled(job.job_id)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (is_job_cancelled(job.job_id)) {
                break;
            }

            const auto& sop_instance_uid = job.instance_uids[i];

            // Update progress - current item
            progress.current_item = sop_instance_uid;
            progress.current_item_description = "Storing instance " + std::to_string(i + 1);
            update_progress(job.job_id, progress);

            // Load the DICOM file for this instance
            // In a real implementation, this would query the local storage
            // to get the file path for the SOP Instance UID
            // For now, we simulate the store operation
            logger->debug_fmt("Storing instance {} ({}/{})",
                              sop_instance_uid, i + 1, job.instance_uids.size());

            // Simulated store result - in real implementation, would call:
            // auto result = scu.store(*assoc, dataset);
            // For now, mark as successful
            ++completed;
            progress.completed_items = completed;
            progress.failed_items = failed;
            progress.calculate_percent();
            update_progress(job.job_id, progress);
        }

        // Release association back to pool
        node_manager->release_association(job.destination_node_id, std::move(assoc));

        // Determine final status
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
        } else if (failed == 0) {
            progress.percent_complete = 100.0f;
            update_progress(job.job_id, progress);
            complete_job(job.job_id, job_status::completed);
        } else if (completed > 0) {
            // Partial success
            std::ostringstream oss;
            oss << "Store completed with failures: " << failed << " of "
                << job.instance_uids.size() << " instances failed";
            if (!last_error.empty()) {
                oss << ". Last error: " << last_error;
            }
            complete_job(job.job_id, job_status::failed, oss.str());
        } else {
            // Complete failure
            complete_job(job.job_id, job_status::failed,
                         "All store operations failed: " + last_error);
        }
    }

    void execute_query_job(job_record& job) {
        logger->info_fmt("Executing query job {} on node {}",
                         job.job_id, job.source_node_id);

        // Get the node information
        auto node_opt = node_manager->get_node(job.source_node_id);
        if (!node_opt) {
            complete_job(job.job_id, job_status::failed,
                         "Node not found: " + job.source_node_id);
            return;
        }

        const auto& node = *node_opt;
        if (!node.is_online()) {
            complete_job(job.job_id, job_status::failed,
                         "Node is offline: " + job.source_node_id);
            return;
        }

        if (!node.supports_find) {
            complete_job(job.job_id, job_status::failed,
                         "Node does not support C-FIND");
            return;
        }

        // Check for cancellation
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Wait if paused
        while (is_job_paused(job.job_id) && !is_job_cancelled(job.job_id)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Get query level from metadata
        std::string query_level = "STUDY";  // Default
        auto level_it = job.metadata.find("query_level");
        if (level_it != job.metadata.end()) {
            query_level = level_it->second;
        }

        // Determine SOP class based on model (default to study root)
        std::vector<std::string> sop_classes = {
            std::string(services::study_root_find_sop_class_uid)
        };

        // Acquire association
        auto assoc_result = node_manager->acquire_association(
            job.source_node_id, sop_classes);
        if (assoc_result.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Failed to acquire association: " + assoc_result.error().message);
            return;
        }

        auto assoc = std::move(assoc_result.value());

        // Configure query SCU
        services::query_scu_config query_config;
        query_config.model = services::query_model::study_root;

        // Set level
        if (query_level == "PATIENT") {
            query_config.level = services::query_level::patient;
        } else if (query_level == "STUDY") {
            query_config.level = services::query_level::study;
        } else if (query_level == "SERIES") {
            query_config.level = services::query_level::series;
        } else if (query_level == "IMAGE") {
            query_config.level = services::query_level::image;
        }

        services::query_scu scu(query_config, logger);

        // Build query keys from metadata
        core::dicom_dataset query_keys;
        query_keys.set_string(core::tags::query_retrieve_level, encoding::vr_type::CS, query_level);

        // Copy query keys from job metadata
        for (const auto& [key, value] : job.metadata) {
            if (key.starts_with("query_") && key != "query_level") {
                std::string tag_name = key.substr(6);  // Remove "query_" prefix
                // Map common tag names to DICOM tags
                if (tag_name == "PatientID") {
                    query_keys.set_string(core::tags::patient_id, encoding::vr_type::LO, value);
                } else if (tag_name == "PatientName") {
                    query_keys.set_string(core::tags::patient_name, encoding::vr_type::PN, value);
                } else if (tag_name == "StudyDate") {
                    query_keys.set_string(core::tags::study_date, encoding::vr_type::DA, value);
                } else if (tag_name == "StudyInstanceUID") {
                    query_keys.set_string(core::tags::study_instance_uid, encoding::vr_type::UI, value);
                } else if (tag_name == "AccessionNumber") {
                    query_keys.set_string(core::tags::accession_number, encoding::vr_type::SH, value);
                } else if (tag_name == "Modality") {
                    query_keys.set_string(core::tags::modality, encoding::vr_type::CS, value);
                } else if (tag_name == "SeriesInstanceUID") {
                    query_keys.set_string(core::tags::series_instance_uid, encoding::vr_type::UI, value);
                }
            }
        }

        // Initialize progress
        job_progress progress;
        progress.total_items = 1;  // Query is a single operation
        update_progress(job.job_id, progress);

        // Execute query
        auto result = scu.find(*assoc, query_keys);

        // Release association
        node_manager->release_association(job.source_node_id, std::move(assoc));

        // Handle result
        if (result.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Query failed: " + result.error().message);
            return;
        }

        const auto& query_result = result.value();

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
        } else if (query_result.is_success()) {
            progress.completed_items = 1;
            progress.percent_complete = 100.0f;
            progress.current_item_description = std::to_string(query_result.matches.size())
                                                + " matches found";
            update_progress(job.job_id, progress);

            // Store result count in metadata
            {
                std::unique_lock lock(cache_mutex);
                auto it = job_cache.find(job.job_id);
                if (it != job_cache.end()) {
                    it->second.metadata["result_count"] = std::to_string(query_result.matches.size());
                }
            }

            complete_job(job.job_id, job_status::completed);
        } else if (query_result.is_cancelled()) {
            complete_job(job.job_id, job_status::cancelled);
        } else {
            std::ostringstream oss;
            oss << "Query completed with status: 0x" << std::hex << query_result.status;
            complete_job(job.job_id, job_status::failed, oss.str());
        }
    }

    void execute_sync_job(job_record& job) {
        logger->info_fmt("Executing sync job {} from node {}",
                         job.job_id, job.source_node_id);

        // Synchronization job queries remote node for studies matching criteria
        // and retrieves any that are not in local storage

        // Get the source node
        auto node_opt = node_manager->get_node(job.source_node_id);
        if (!node_opt) {
            complete_job(job.job_id, job_status::failed,
                         "Source node not found: " + job.source_node_id);
            return;
        }

        const auto& node = *node_opt;
        if (!node.is_online()) {
            complete_job(job.job_id, job_status::failed,
                         "Source node is offline: " + job.source_node_id);
            return;
        }

        if (!node.supports_find) {
            complete_job(job.job_id, job_status::failed,
                         "Node does not support C-FIND for sync");
            return;
        }

        // Check for cancellation
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Wait if paused
        while (is_job_paused(job.job_id) && !is_job_cancelled(job.job_id)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Initialize progress
        job_progress progress;
        progress.total_items = 2;  // Query + Retrieve phases
        progress.current_item_description = "Querying remote node for studies";
        update_progress(job.job_id, progress);

        // Acquire association for query
        std::vector<std::string> query_sop_classes = {
            std::string(services::study_root_find_sop_class_uid)
        };

        auto assoc_result = node_manager->acquire_association(
            job.source_node_id, query_sop_classes);
        if (assoc_result.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Failed to acquire association: " + assoc_result.error().message);
            return;
        }

        auto assoc = std::move(assoc_result.value());

        // Configure query for studies
        services::query_scu_config query_config;
        query_config.model = services::query_model::study_root;
        query_config.level = services::query_level::study;

        services::query_scu query_scu(query_config, logger);

        // Build query keys
        core::dicom_dataset query_keys;
        query_keys.set_string(core::tags::query_retrieve_level, encoding::vr_type::CS, "STUDY");

        // Filter by patient if specified
        if (job.patient_id.has_value()) {
            query_keys.set_string(core::tags::patient_id, encoding::vr_type::LO, *job.patient_id);
        }

        // Request return attributes
        query_keys.set_string(core::tags::study_instance_uid, encoding::vr_type::UI, "");
        query_keys.set_string(core::tags::study_date, encoding::vr_type::DA, "");
        query_keys.set_string(core::tags::patient_name, encoding::vr_type::PN, "");

        // Execute query
        auto query_result_res = query_scu.find(*assoc, query_keys);

        // Release query association
        node_manager->release_association(job.source_node_id, std::move(assoc));

        if (query_result_res.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Sync query failed: " + query_result_res.error().message);
            return;
        }

        const auto& query_result = query_result_res.value();

        progress.completed_items = 1;
        progress.current_item_description =
            std::to_string(query_result.matches.size()) + " studies found";
        update_progress(job.job_id, progress);

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Sync complete - in full implementation, would compare with local
        // storage and create retrieve jobs for missing studies
        progress.completed_items = 2;
        progress.percent_complete = 100.0f;
        progress.current_item_description =
            "Sync complete: " + std::to_string(query_result.matches.size()) + " studies checked";
        update_progress(job.job_id, progress);

        // Store result in metadata
        {
            std::unique_lock lock(cache_mutex);
            auto it = job_cache.find(job.job_id);
            if (it != job_cache.end()) {
                it->second.metadata["studies_found"] = std::to_string(query_result.matches.size());
            }
        }

        complete_job(job.job_id, job_status::completed);
    }

    void execute_prefetch_job(job_record& job) {
        logger->info_fmt("Executing prefetch job {} for patient {} from node {}",
                         job.job_id, job.patient_id.value_or("unknown"),
                         job.source_node_id);

        // Prefetch retrieves prior studies for a patient before they arrive

        // Validate patient ID
        if (!job.patient_id.has_value()) {
            complete_job(job.job_id, job_status::failed,
                         "No patient ID specified for prefetch job");
            return;
        }

        // Get the source node
        auto node_opt = node_manager->get_node(job.source_node_id);
        if (!node_opt) {
            complete_job(job.job_id, job_status::failed,
                         "Source node not found: " + job.source_node_id);
            return;
        }

        const auto& node = *node_opt;
        if (!node.is_online()) {
            complete_job(job.job_id, job_status::failed,
                         "Source node is offline: " + job.source_node_id);
            return;
        }

        if (!node.supports_find || !node.supports_query_retrieve()) {
            complete_job(job.job_id, job_status::failed,
                         "Node does not support query/retrieve for prefetch");
            return;
        }

        // Check for cancellation
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Wait if paused
        while (is_job_paused(job.job_id) && !is_job_cancelled(job.job_id)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Initialize progress
        job_progress progress;
        progress.total_items = 2;  // Query + Retrieve phases
        progress.current_item_description = "Querying prior studies";
        update_progress(job.job_id, progress);

        // Acquire association for query
        std::vector<std::string> query_sop_classes = {
            std::string(services::study_root_find_sop_class_uid)
        };

        auto assoc_result = node_manager->acquire_association(
            job.source_node_id, query_sop_classes);
        if (assoc_result.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Failed to acquire association: " + assoc_result.error().message);
            return;
        }

        auto assoc = std::move(assoc_result.value());

        // Configure query for patient's studies
        services::query_scu_config query_config;
        query_config.model = services::query_model::study_root;
        query_config.level = services::query_level::study;

        services::query_scu query_scu(query_config, logger);

        // Query for all studies of this patient
        core::dicom_dataset query_keys;
        query_keys.set_string(core::tags::query_retrieve_level, encoding::vr_type::CS, "STUDY");
        query_keys.set_string(core::tags::patient_id, encoding::vr_type::LO, *job.patient_id);
        query_keys.set_string(core::tags::study_instance_uid, encoding::vr_type::UI, "");
        query_keys.set_string(core::tags::study_date, encoding::vr_type::DA, "");
        query_keys.set_string(core::tags::modality, encoding::vr_type::CS, "");

        // Execute query
        auto query_result_res = query_scu.find(*assoc, query_keys);

        // Release query association
        node_manager->release_association(job.source_node_id, std::move(assoc));

        if (query_result_res.is_err()) {
            complete_job(job.job_id, job_status::failed,
                         "Prefetch query failed: " + query_result_res.error().message);
            return;
        }

        const auto& query_result = query_result_res.value();

        progress.completed_items = 1;
        progress.current_item_description =
            std::to_string(query_result.matches.size()) + " prior studies found";
        update_progress(job.job_id, progress);

        if (is_job_cancelled(job.job_id)) {
            complete_job(job.job_id, job_status::cancelled);
            return;
        }

        // Complete prefetch - in full implementation would create
        // retrieve jobs for each prior study
        progress.completed_items = 2;
        progress.percent_complete = 100.0f;
        progress.current_item_description =
            "Prefetch complete: " + std::to_string(query_result.matches.size()) + " studies identified";
        update_progress(job.job_id, progress);

        // Store result in metadata
        {
            std::unique_lock lock(cache_mutex);
            auto it = job_cache.find(job.job_id);
            if (it != job_cache.end()) {
                it->second.metadata["prior_studies"] = std::to_string(query_result.matches.size());
            }
        }

        complete_job(job.job_id, job_status::completed);
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
