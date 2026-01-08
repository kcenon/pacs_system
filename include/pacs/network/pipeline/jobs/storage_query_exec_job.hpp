/**
 * @file storage_query_exec_job.hpp
 * @brief Storage and query execution job for Stage 4 of the pipeline
 *
 * This job handles the actual execution of DICOM operations including
 * C-STORE, C-FIND, C-GET, and C-MOVE. Blocking I/O is allowed in this stage.
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #521 - Phase 4: Execution Jobs
 */

#pragma once

#include <pacs/network/pipeline/pipeline_coordinator.hpp>
#include <pacs/network/pipeline/pipeline_job_types.hpp>
#include <pacs/network/pipeline/jobs/dimse_process_job.hpp>
#include <pacs/core/result.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pacs::network::pipeline {

/**
 * @enum dimse_status
 * @brief DICOM DIMSE status codes
 */
enum class dimse_status : uint16_t {
    success = 0x0000,
    pending = 0xFF00,
    pending_warning = 0xFF01,
    cancel = 0xFE00,
    warning_attribute_list_error = 0xB000,
    warning_attribute_value_out_of_range = 0xB006,
    failure_refused_out_of_resources = 0xA700,
    failure_refused_sop_class_not_supported = 0x0122,
    failure_invalid_sop_instance = 0x0117,
    failure_unable_to_process = 0xC000,
    failure_more_than_one_match = 0xC100,
    failure_unable_to_perform = 0xC200,
};

/**
 * @struct service_result
 * @brief Result from service execution
 */
struct service_result {
    /// DIMSE status code
    dimse_status status{dimse_status::success};

    /// Session ID for routing response
    uint64_t session_id;

    /// Message ID for correlation
    uint16_t message_id;

    /// Presentation context ID
    uint8_t presentation_context_id;

    /// Response command type
    dimse_command_type response_type;

    /// SOP Class UID (echoed back)
    std::string sop_class_uid;

    /// SOP Instance UID (echoed back)
    std::string sop_instance_uid;

    /// Response data set (if any)
    std::vector<uint8_t> data_set;

    /// Number of remaining sub-operations (for C-GET/C-MOVE)
    uint16_t remaining_sub_ops{0};

    /// Number of completed sub-operations
    uint16_t completed_sub_ops{0};

    /// Number of failed sub-operations
    uint16_t failed_sub_ops{0};

    /// Number of warning sub-operations
    uint16_t warning_sub_ops{0};

    /// Error comment (if any)
    std::string error_comment;
};

/**
 * @class storage_query_exec_job
 * @brief Job for executing storage and query operations
 *
 * Stage 4 of the pipeline. Executes the actual DICOM operations
 * with blocking I/O allowed for database and file system access.
 *
 * @example
 * @code
 * auto job = std::make_unique<storage_query_exec_job>(request, store_handler);
 * coordinator.submit_to_stage(pipeline_stage::storage_query_exec, std::move(job));
 * @endcode
 */
class storage_query_exec_job : public pipeline_job_base {
public:
    /// Service handler function type
    using service_handler = std::function<Result<service_result>(const dimse_request&)>;

    /// Callback type for execution completion
    using completion_callback = std::function<void(const service_result& result)>;

    /// Callback type for execution errors
    using error_callback = std::function<void(uint64_t session_id,
                                              const std::string& error)>;

    /**
     * @brief Construct an execution job
     *
     * @param request The DIMSE request to execute
     * @param handler Service handler for the operation
     * @param on_complete Callback for completion
     * @param on_error Callback for errors
     */
    storage_query_exec_job(dimse_request request,
                           service_handler handler,
                           completion_callback on_complete = nullptr,
                           error_callback on_error = nullptr);

    ~storage_query_exec_job() override = default;

    // Non-copyable, movable
    storage_query_exec_job(const storage_query_exec_job&) = delete;
    storage_query_exec_job& operator=(const storage_query_exec_job&) = delete;
    storage_query_exec_job(storage_query_exec_job&&) = default;
    storage_query_exec_job& operator=(storage_query_exec_job&&) = default;

    /**
     * @brief Execute the storage/query operation
     *
     * Executes the DIMSE operation and submits result to encode stage.
     *
     * @param coordinator Pipeline coordinator for stage submission
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto execute(pipeline_coordinator& coordinator) -> VoidResult override;

    /**
     * @brief Get the job context
     */
    [[nodiscard]] auto get_context() const noexcept -> const job_context& override;
    [[nodiscard]] auto get_context() noexcept -> job_context& override;

    /**
     * @brief Get the job name
     */
    [[nodiscard]] auto get_name() const -> std::string override;

    /**
     * @brief Get the DIMSE request
     */
    [[nodiscard]] auto get_request() const noexcept -> const dimse_request&;

private:
    job_context context_;
    dimse_request request_;
    service_handler handler_;
    completion_callback on_complete_;
    error_callback on_error_;

    /// Determine job category from command type
    static auto get_category_for_command(dimse_command_type type) -> job_category;
};

}  // namespace pacs::network::pipeline
