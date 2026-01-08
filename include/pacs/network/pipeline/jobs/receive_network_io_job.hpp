/**
 * @file receive_network_io_job.hpp
 * @brief Network I/O receive job for Stage 1 of the pipeline
 *
 * This job handles receiving raw PDU bytes from the network and
 * submitting them to the next stage for decoding.
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #519 - Phase 2: Network I/O Jobs
 */

#pragma once

#include <pacs/network/pipeline/pipeline_coordinator.hpp>
#include <pacs/network/pipeline/pipeline_job_types.hpp>
#include <pacs/core/result.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pacs::network::pipeline {

/**
 * @class receive_network_io_job
 * @brief Job for receiving PDU data from network connections
 *
 * Stage 1 of the pipeline. Receives raw bytes from the network
 * and forwards them to the PDU decode stage.
 *
 * @example
 * @code
 * auto job = std::make_unique<receive_network_io_job>(session_id, on_data_received);
 * coordinator.submit_to_stage(pipeline_stage::network_receive, std::move(job));
 * @endcode
 */
class receive_network_io_job : public pipeline_job_base {
public:
    /// Callback type for received data
    using data_callback = std::function<void(uint64_t session_id,
                                             std::vector<uint8_t> data)>;

    /// Callback type for connection errors
    using error_callback = std::function<void(uint64_t session_id,
                                              const std::string& error)>;

    /**
     * @brief Construct a receive job
     *
     * @param session_id The session/connection identifier
     * @param data The received PDU data
     * @param on_data Callback to invoke when data is processed
     * @param on_error Callback to invoke on errors
     */
    receive_network_io_job(uint64_t session_id,
                           std::vector<uint8_t> data,
                           data_callback on_data = nullptr,
                           error_callback on_error = nullptr);

    ~receive_network_io_job() override = default;

    // Non-copyable, movable
    receive_network_io_job(const receive_network_io_job&) = delete;
    receive_network_io_job& operator=(const receive_network_io_job&) = delete;
    receive_network_io_job(receive_network_io_job&&) = default;
    receive_network_io_job& operator=(receive_network_io_job&&) = default;

    /**
     * @brief Execute the receive job
     *
     * Processes the received data and submits it to the decode stage.
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
     * @brief Get the received data
     */
    [[nodiscard]] auto get_data() const noexcept -> const std::vector<uint8_t>&;

    /**
     * @brief Get the session ID
     */
    [[nodiscard]] auto get_session_id() const noexcept -> uint64_t;

private:
    job_context context_;
    std::vector<uint8_t> data_;
    data_callback on_data_;
    error_callback on_error_;
};

}  // namespace pacs::network::pipeline
