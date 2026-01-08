/**
 * @file dimse_process_job.hpp
 * @brief DIMSE processing job for Stage 3 of the pipeline
 *
 * This job handles DIMSE message processing and routing requests
 * to the appropriate service handlers.
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #520 - Phase 3: Protocol Handling Jobs
 */

#pragma once

#include <pacs/network/pipeline/pipeline_coordinator.hpp>
#include <pacs/network/pipeline/pipeline_job_types.hpp>
#include <pacs/network/pipeline/jobs/pdu_decode_job.hpp>
#include <pacs/core/result.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pacs::network::pipeline {

/**
 * @enum dimse_command_type
 * @brief DICOM DIMSE command types
 */
enum class dimse_command_type : uint16_t {
    c_store_rq = 0x0001,
    c_store_rsp = 0x8001,
    c_get_rq = 0x0010,
    c_get_rsp = 0x8010,
    c_find_rq = 0x0020,
    c_find_rsp = 0x8020,
    c_move_rq = 0x0021,
    c_move_rsp = 0x8021,
    c_echo_rq = 0x0030,
    c_echo_rsp = 0x8030,
    n_event_report_rq = 0x0100,
    n_event_report_rsp = 0x8100,
    n_get_rq = 0x0110,
    n_get_rsp = 0x8110,
    n_set_rq = 0x0120,
    n_set_rsp = 0x8120,
    n_action_rq = 0x0130,
    n_action_rsp = 0x8130,
    n_create_rq = 0x0140,
    n_create_rsp = 0x8140,
    n_delete_rq = 0x0150,
    n_delete_rsp = 0x8150,
    c_cancel_rq = 0x0FFF,
};

/**
 * @struct dimse_request
 * @brief Parsed DIMSE request for service execution
 */
struct dimse_request {
    /// The DIMSE command type
    dimse_command_type command_type;

    /// Session ID
    uint64_t session_id;

    /// Message ID for correlation
    uint16_t message_id;

    /// Presentation context ID
    uint8_t presentation_context_id;

    /// Affected/Requested SOP Class UID
    std::string sop_class_uid;

    /// Affected/Requested SOP Instance UID
    std::string sop_instance_uid;

    /// Command data set (serialized)
    std::vector<uint8_t> command_data;

    /// Data set (if present)
    std::vector<uint8_t> data_set;

    /// Priority (0=medium, 1=high, 2=low)
    uint16_t priority{0};
};

/**
 * @class dimse_process_job
 * @brief Job for processing DIMSE messages
 *
 * Stage 3 of the pipeline. Processes DIMSE messages and routes
 * them to the storage/query execution stage.
 *
 * @example
 * @code
 * auto job = std::make_unique<dimse_process_job>(decoded_pdu, on_request);
 * coordinator.submit_to_stage(pipeline_stage::dimse_process, std::move(job));
 * @endcode
 */
class dimse_process_job : public pipeline_job_base {
public:
    /// Callback type for processed request
    using request_callback = std::function<void(const dimse_request& request)>;

    /// Callback type for association handling
    using association_callback = std::function<void(uint64_t session_id,
                                                    pacs::network::pdu_type type,
                                                    const std::vector<uint8_t>& data)>;

    /// Callback type for processing errors
    using error_callback = std::function<void(uint64_t session_id,
                                              const std::string& error)>;

    /**
     * @brief Construct a DIMSE process job from decoded PDU
     *
     * @param pdu The decoded PDU to process
     * @param on_request Callback for DIMSE requests
     * @param on_association Callback for association messages
     * @param on_error Callback for errors
     */
    dimse_process_job(decoded_pdu pdu,
                      request_callback on_request = nullptr,
                      association_callback on_association = nullptr,
                      error_callback on_error = nullptr);

    ~dimse_process_job() override = default;

    // Non-copyable, movable
    dimse_process_job(const dimse_process_job&) = delete;
    dimse_process_job& operator=(const dimse_process_job&) = delete;
    dimse_process_job(dimse_process_job&&) = default;
    dimse_process_job& operator=(dimse_process_job&&) = default;

    /**
     * @brief Execute the DIMSE process job
     *
     * Processes the PDU and routes to appropriate handler.
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
     * @brief Get the decoded PDU
     */
    [[nodiscard]] auto get_pdu() const noexcept -> const decoded_pdu&;

private:
    job_context context_;
    decoded_pdu pdu_;
    request_callback on_request_;
    association_callback on_association_;
    error_callback on_error_;

    /// Process P-DATA-TF PDU
    [[nodiscard]] auto process_p_data() -> Result<dimse_request>;

    /// Process association PDUs (A-ASSOCIATE, A-RELEASE, A-ABORT)
    [[nodiscard]] auto process_association_pdu() -> VoidResult;
};

}  // namespace pacs::network::pipeline
