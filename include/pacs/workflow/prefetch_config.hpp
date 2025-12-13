/**
 * @file prefetch_config.hpp
 * @brief Configuration for automatic prefetch service
 *
 * This file provides configuration structures for the auto_prefetch_service
 * which automatically prefetches prior studies when patients appear in
 * the modality worklist.
 *
 * @see SRS-WKF-001 - Auto Prefetch Service Specification
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace pacs::workflow {

/**
 * @brief Remote PACS connection configuration
 *
 * Defines connection parameters for a remote PACS server
 * from which prior studies will be prefetched.
 */
struct remote_pacs_config {
    /// Remote PACS AE title
    std::string ae_title;

    /// Remote PACS hostname or IP address
    std::string host;

    /// Remote PACS port (default: 11112)
    uint16_t port{11112};

    /// Our local AE title for association
    std::string local_ae_title{"PACS_PREFETCH"};

    /// Connection timeout
    std::chrono::seconds connection_timeout{30};

    /// Association timeout
    std::chrono::seconds association_timeout{60};

    /// Enable TLS for secure connections
    bool use_tls{false};

    /**
     * @brief Check if configuration is valid
     * @return true if required fields are set
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !ae_title.empty() && !host.empty() && port > 0;
    }
};

/**
 * @brief Prefetch selection criteria
 *
 * Defines which prior studies should be prefetched based on
 * various filtering criteria.
 */
struct prefetch_criteria {
    /// Lookback period for prior studies (default: 365 days)
    std::chrono::days lookback_period{365};

    /// Maximum number of prior studies to prefetch per patient
    std::size_t max_studies_per_patient{10};

    /// Maximum number of prior series to prefetch per study
    std::size_t max_series_per_study{0};  // 0 = unlimited

    /// Modalities to include (empty = all modalities)
    std::set<std::string> include_modalities;

    /// Modalities to exclude
    std::set<std::string> exclude_modalities;

    /// Only prefetch studies with specific body parts
    std::set<std::string> include_body_parts;

    /// Prefer same modality as scheduled procedure
    bool prefer_same_modality{true};

    /// Prefer same body part as scheduled procedure
    bool prefer_same_body_part{true};
};

/**
 * @brief Prefetch result statistics
 *
 * Tracks the outcome of a prefetch operation or cycle.
 */
struct prefetch_result {
    /// Number of patients processed
    std::size_t patients_processed{0};

    /// Number of studies prefetched successfully
    std::size_t studies_prefetched{0};

    /// Number of series prefetched successfully
    std::size_t series_prefetched{0};

    /// Number of instances (images) prefetched
    std::size_t instances_prefetched{0};

    /// Number of studies that failed to prefetch
    std::size_t studies_failed{0};

    /// Number of studies already present (skipped)
    std::size_t studies_already_present{0};

    /// Total bytes downloaded
    std::size_t bytes_downloaded{0};

    /// Duration of the prefetch operation
    std::chrono::milliseconds duration{0};

    /// Time when this result was recorded
    std::chrono::system_clock::time_point timestamp;

    /**
     * @brief Check if the result indicates success (no failures)
     * @return true if no failures occurred
     */
    [[nodiscard]] auto is_successful() const noexcept -> bool {
        return studies_failed == 0;
    }

    /**
     * @brief Combine results from another prefetch operation
     * @param other The result to add
     * @return Reference to this result
     */
    auto operator+=(const prefetch_result& other) -> prefetch_result& {
        patients_processed += other.patients_processed;
        studies_prefetched += other.studies_prefetched;
        series_prefetched += other.series_prefetched;
        instances_prefetched += other.instances_prefetched;
        studies_failed += other.studies_failed;
        studies_already_present += other.studies_already_present;
        bytes_downloaded += other.bytes_downloaded;
        duration += other.duration;
        return *this;
    }
};

/**
 * @brief Prior study information
 *
 * Represents a prior study found on a remote PACS that may be
 * a candidate for prefetching.
 */
struct prior_study_info {
    /// Study Instance UID
    std::string study_instance_uid;

    /// Patient ID
    std::string patient_id;

    /// Patient Name
    std::string patient_name;

    /// Study Date (YYYYMMDD format)
    std::string study_date;

    /// Study Description
    std::string study_description;

    /// Modalities in Study
    std::set<std::string> modalities;

    /// Body Part Examined
    std::string body_part_examined;

    /// Accession Number
    std::string accession_number;

    /// Number of Series in Study
    std::size_t number_of_series{0};

    /// Number of Instances in Study
    std::size_t number_of_instances{0};
};

/**
 * @brief Configuration for the auto prefetch service
 *
 * Comprehensive configuration for controlling automatic
 * prefetching of prior studies based on worklist queries.
 */
struct prefetch_service_config {
    /// Enable/disable the prefetch service
    bool enabled{true};

    /// Interval between prefetch cycles (default: 5 minutes)
    std::chrono::seconds prefetch_interval{300};

    /// Maximum concurrent prefetch operations
    std::size_t max_concurrent_prefetches{4};

    /// Whether to start automatically on construction
    bool auto_start{false};

    /// Remote PACS configurations (can prefetch from multiple sources)
    std::vector<remote_pacs_config> remote_pacs;

    /// Selection criteria for prior studies
    prefetch_criteria criteria;

    /// Rate limit: maximum prefetches per minute (0 = unlimited)
    std::size_t rate_limit_per_minute{0};

    /// Retry failed prefetches
    bool retry_on_failure{true};

    /// Maximum retry attempts
    std::size_t max_retry_attempts{3};

    /// Delay between retries
    std::chrono::seconds retry_delay{60};

    /// Callback for prefetch cycle completion
    using cycle_complete_callback =
        std::function<void(const prefetch_result& result)>;
    cycle_complete_callback on_cycle_complete;

    /// Callback for individual prefetch completion
    using prefetch_complete_callback =
        std::function<void(const std::string& patient_id,
                          const prior_study_info& study,
                          bool success,
                          const std::string& error_message)>;
    prefetch_complete_callback on_prefetch_complete;

    /// Callback for prefetch errors
    using error_callback =
        std::function<void(const std::string& patient_id,
                          const std::string& study_uid,
                          const std::string& error)>;
    error_callback on_prefetch_error;

    /**
     * @brief Check if configuration is valid
     * @return true if configuration is valid for operation
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        if (!enabled) {
            return true;  // Disabled config is always valid
        }
        // Must have at least one valid remote PACS
        for (const auto& pacs : remote_pacs) {
            if (pacs.is_valid()) {
                return true;
            }
        }
        return false;
    }
};

}  // namespace pacs::workflow
