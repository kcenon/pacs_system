/**
 * @file response_encode_job.hpp
 * @brief Response encoding job for Stage 5 of the pipeline
 *
 * This job handles encoding DIMSE responses into PDU bytes
 * for transmission over the network.
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #522 - Phase 5: Response Jobs
 */

#pragma once

#include <pacs/network/pipeline/pipeline_coordinator.hpp>
#include <pacs/network/pipeline/pipeline_job_types.hpp>
#include <pacs/network/pipeline/jobs/storage_query_exec_job.hpp>
#include <pacs/core/result.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace pacs::network::pipeline {

/**
 * @struct encoded_response
 * @brief Encoded PDU ready for network transmission
 */
struct encoded_response {
    /// Session ID for routing
    uint64_t session_id;

    /// Encoded PDU bytes
    std::vector<uint8_t> pdu_data;

    /// Whether this is the last response in a sequence
    bool is_final{true};

    /// Original message ID for correlation
    uint16_t message_id;
};

/**
 * @class response_encode_job
 * @brief Job for encoding DIMSE responses into PDU bytes
 *
 * Stage 5 of the pipeline. Encodes service results into PDU format
 * and submits to the network send stage.
 *
 * @example
 * @code
 * auto job = std::make_unique<response_encode_job>(result, max_pdu_size);
 * coordinator.submit_to_stage(pipeline_stage::response_encode, std::move(job));
 * @endcode
 */
class response_encode_job : public pipeline_job_base {
public:
    /// Callback type for encoded response
    using encode_callback = std::function<void(const encoded_response& response)>;

    /// Callback type for encoding errors
    using error_callback = std::function<void(uint64_t session_id,
                                              const std::string& error)>;

    /**
     * @brief Construct an encode job
     *
     * @param result The service result to encode
     * @param max_pdu_size Maximum PDU size for fragmentation
     * @param on_encoded Callback for encoded response
     * @param on_error Callback for errors
     */
    response_encode_job(service_result result,
                        uint32_t max_pdu_size = 16384,
                        encode_callback on_encoded = nullptr,
                        error_callback on_error = nullptr);

    ~response_encode_job() override = default;

    // Non-copyable, movable
    response_encode_job(const response_encode_job&) = delete;
    response_encode_job& operator=(const response_encode_job&) = delete;
    response_encode_job(response_encode_job&&) = default;
    response_encode_job& operator=(response_encode_job&&) = default;

    /**
     * @brief Execute the encode job
     *
     * Encodes the result and submits to the network send stage.
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
     * @brief Get the service result
     */
    [[nodiscard]] auto get_result() const noexcept -> const service_result&;

private:
    job_context context_;
    service_result result_;
    uint32_t max_pdu_size_;
    encode_callback on_encoded_;
    error_callback on_error_;

    /// Encode the response into PDU bytes
    [[nodiscard]] auto encode_response() -> Result<std::vector<encoded_response>>;

    /// Encode DIMSE command
    [[nodiscard]] auto encode_dimse_command() -> Result<std::vector<uint8_t>>;

    /// Fragment large data if needed
    [[nodiscard]] auto fragment_data(const std::vector<uint8_t>& data)
        -> std::vector<std::vector<uint8_t>>;
};

}  // namespace pacs::network::pipeline
