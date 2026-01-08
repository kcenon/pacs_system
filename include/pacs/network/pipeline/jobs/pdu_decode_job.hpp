/**
 * @file pdu_decode_job.hpp
 * @brief PDU decoding job for Stage 2 of the pipeline
 *
 * This job handles decoding raw PDU bytes into structured PDU objects
 * and submitting them to the DIMSE processing stage.
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #520 - Phase 3: Protocol Handling Jobs
 */

#pragma once

#include <pacs/network/pipeline/pipeline_coordinator.hpp>
#include <pacs/network/pipeline/pipeline_job_types.hpp>
#include <pacs/network/pdu_types.hpp>
#include <pacs/core/result.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pacs::network::pipeline {

/**
 * @struct decoded_pdu
 * @brief Result of PDU decoding containing the PDU type and data
 */
struct decoded_pdu {
    /// The type of PDU that was decoded
    pacs::network::pdu_type type;

    /// Session this PDU belongs to
    uint64_t session_id;

    /// Raw PDU data for further processing
    std::vector<uint8_t> data;

    /// Presentation context ID (for P-DATA-TF)
    uint8_t presentation_context_id{0};

    /// Whether this is the last fragment (for P-DATA-TF)
    bool is_last_fragment{true};
};

/**
 * @class pdu_decode_job
 * @brief Job for decoding raw PDU bytes
 *
 * Stage 2 of the pipeline. Decodes PDU bytes and submits
 * the decoded result to the DIMSE processing stage.
 *
 * @example
 * @code
 * auto job = std::make_unique<pdu_decode_job>(session_id, pdu_bytes);
 * coordinator.submit_to_stage(pipeline_stage::pdu_decode, std::move(job));
 * @endcode
 */
class pdu_decode_job : public pipeline_job_base {
public:
    /// Callback type for decoded PDU
    using decode_callback = std::function<void(const decoded_pdu& pdu)>;

    /// Callback type for decode errors
    using error_callback = std::function<void(uint64_t session_id,
                                              const std::string& error)>;

    /**
     * @brief Construct a decode job
     *
     * @param session_id The session/connection identifier
     * @param raw_data The raw PDU bytes to decode
     * @param on_decoded Callback to invoke when PDU is decoded
     * @param on_error Callback to invoke on errors
     */
    pdu_decode_job(uint64_t session_id,
                   std::vector<uint8_t> raw_data,
                   decode_callback on_decoded = nullptr,
                   error_callback on_error = nullptr);

    ~pdu_decode_job() override = default;

    // Non-copyable, movable
    pdu_decode_job(const pdu_decode_job&) = delete;
    pdu_decode_job& operator=(const pdu_decode_job&) = delete;
    pdu_decode_job(pdu_decode_job&&) = default;
    pdu_decode_job& operator=(pdu_decode_job&&) = default;

    /**
     * @brief Execute the decode job
     *
     * Decodes the PDU and submits to the DIMSE processing stage.
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
     * @brief Get the raw PDU data
     */
    [[nodiscard]] auto get_raw_data() const noexcept -> const std::vector<uint8_t>&;

private:
    job_context context_;
    std::vector<uint8_t> raw_data_;
    decode_callback on_decoded_;
    error_callback on_error_;

    /// Internal decode method
    [[nodiscard]] auto decode_pdu() -> Result<decoded_pdu>;
};

}  // namespace pacs::network::pipeline
