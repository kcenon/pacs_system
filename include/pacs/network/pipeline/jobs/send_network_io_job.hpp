/**
 * @file send_network_io_job.hpp
 * @brief Network I/O send job for Stage 6 of the pipeline
 *
 * This job handles sending encoded PDU bytes over the network.
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
 * @class send_network_io_job
 * @brief Job for sending PDU data over network connections
 *
 * Stage 6 of the pipeline. Sends encoded PDU bytes to the network.
 *
 * @example
 * @code
 * auto job = std::make_unique<send_network_io_job>(session_id, pdu_data, on_complete);
 * coordinator.submit_to_stage(pipeline_stage::network_send, std::move(job));
 * @endcode
 */
class send_network_io_job : public pipeline_job_base {
public:
    /// Callback type for send completion
    using completion_callback = std::function<void(uint64_t session_id,
                                                   bool success,
                                                   size_t bytes_sent)>;

    /// Callback type for send errors
    using error_callback = std::function<void(uint64_t session_id,
                                              const std::string& error)>;

    /// Function type for actual network send operation
    using send_function = std::function<VoidResult(uint64_t session_id,
                                                   const std::vector<uint8_t>& data)>;

    /**
     * @brief Construct a send job
     *
     * @param session_id The session/connection identifier
     * @param data The PDU data to send
     * @param send_fn Function to perform actual network send
     * @param on_complete Callback to invoke on completion
     * @param on_error Callback to invoke on errors
     */
    send_network_io_job(uint64_t session_id,
                        std::vector<uint8_t> data,
                        send_function send_fn,
                        completion_callback on_complete = nullptr,
                        error_callback on_error = nullptr);

    ~send_network_io_job() override = default;

    // Non-copyable, movable
    send_network_io_job(const send_network_io_job&) = delete;
    send_network_io_job& operator=(const send_network_io_job&) = delete;
    send_network_io_job(send_network_io_job&&) = default;
    send_network_io_job& operator=(send_network_io_job&&) = default;

    /**
     * @brief Execute the send job
     *
     * Sends the PDU data over the network.
     *
     * @param coordinator Pipeline coordinator (not used for final stage)
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
     * @brief Get the data to send
     */
    [[nodiscard]] auto get_data() const noexcept -> const std::vector<uint8_t>&;

    /**
     * @brief Get the session ID
     */
    [[nodiscard]] auto get_session_id() const noexcept -> uint64_t;

private:
    job_context context_;
    std::vector<uint8_t> data_;
    send_function send_fn_;
    completion_callback on_complete_;
    error_callback on_error_;
};

}  // namespace pacs::network::pipeline
