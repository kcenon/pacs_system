/**
 * @file pdu_decode_job.cpp
 * @brief Implementation of PDU decode job
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #520 - Phase 3: Protocol Handling Jobs
 */

#include <pacs/network/pipeline/jobs/pdu_decode_job.hpp>
#include <pacs/network/pipeline/jobs/dimse_process_job.hpp>

#include <chrono>

namespace pacs::network::pipeline {

pdu_decode_job::pdu_decode_job(uint64_t session_id,
                               std::vector<uint8_t> raw_data,
                               decode_callback on_decoded,
                               error_callback on_error)
    : raw_data_(std::move(raw_data))
    , on_decoded_(std::move(on_decoded))
    , on_error_(std::move(on_error)) {

    context_.session_id = session_id;
    context_.stage = pipeline_stage::pdu_decode;
    context_.category = job_category::other;
    context_.enqueue_time_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

auto pdu_decode_job::execute(pipeline_coordinator& coordinator) -> VoidResult {
    // Validate we have data
    if (raw_data_.size() < 6) {  // Minimum PDU header size
        if (on_error_) {
            on_error_(context_.session_id, "PDU data too short");
        }
        return pacs_void_error(error_codes::insufficient_data,
                               "PDU data too short for header");
    }

    // Decode the PDU
    auto decode_result = decode_pdu();
    if (!decode_result.is_ok()) {
        if (on_error_) {
            auto err = decode_result.error();
            on_error_(context_.session_id, err.message);
        }
        return VoidResult(decode_result.error());
    }

    auto pdu = decode_result.value();

    // Invoke callback if set
    if (on_decoded_) {
        on_decoded_(pdu);
    }

    // Update context based on PDU type
    if (pdu.type == pacs::network::pdu_type::associate_rq ||
        pdu.type == pacs::network::pdu_type::associate_ac ||
        pdu.type == pacs::network::pdu_type::associate_rj ||
        pdu.type == pacs::network::pdu_type::release_rq ||
        pdu.type == pacs::network::pdu_type::release_rp ||
        pdu.type == pacs::network::pdu_type::abort) {
        context_.category = job_category::association;
    }

    // Create DIMSE process job and submit to next stage
    auto dimse_job = std::make_unique<dimse_process_job>(std::move(pdu));

    // Copy context to DIMSE job
    dimse_job->get_context() = context_;
    dimse_job->get_context().stage = pipeline_stage::dimse_process;

    return coordinator.submit_to_stage(
        pipeline_stage::dimse_process,
        std::move(dimse_job)
    );
}

auto pdu_decode_job::get_context() const noexcept -> const job_context& {
    return context_;
}

auto pdu_decode_job::get_context() noexcept -> job_context& {
    return context_;
}

auto pdu_decode_job::get_name() const -> std::string {
    return "pdu_decode_job[session=" +
           std::to_string(context_.session_id) +
           ", bytes=" + std::to_string(raw_data_.size()) + "]";
}

auto pdu_decode_job::get_raw_data() const noexcept
    -> const std::vector<uint8_t>& {
    return raw_data_;
}

auto pdu_decode_job::decode_pdu() -> Result<decoded_pdu> {
    decoded_pdu result;
    result.session_id = context_.session_id;

    // PDU Header: Type (1 byte) + Reserved (1 byte) + Length (4 bytes)
    if (raw_data_.size() < 6) {
        return pacs_error<decoded_pdu>(error_codes::insufficient_data,
                                       "Insufficient data for PDU header");
    }

    // Extract PDU type
    result.type = static_cast<pacs::network::pdu_type>(raw_data_[0]);

    // Extract length (big-endian, 4 bytes starting at offset 2)
    uint32_t length = (static_cast<uint32_t>(raw_data_[2]) << 24) |
                      (static_cast<uint32_t>(raw_data_[3]) << 16) |
                      (static_cast<uint32_t>(raw_data_[4]) << 8) |
                      static_cast<uint32_t>(raw_data_[5]);

    // Validate we have complete PDU
    if (raw_data_.size() < 6 + length) {
        return pacs_error<decoded_pdu>(error_codes::incomplete_pdu,
                                       "Incomplete PDU data");
    }

    // Copy PDU data (excluding header)
    result.data.assign(raw_data_.begin() + 6,
                       raw_data_.begin() + 6 + length);

    // For P-DATA-TF, extract presentation context ID and flags
    if (result.type == pacs::network::pdu_type::p_data_tf && !result.data.empty()) {
        // P-DATA-TF contains PDV items
        // PDV Item: Length (4) + Presentation Context ID (1) + Message Control Header (1) + Data
        if (result.data.size() >= 6) {
            result.presentation_context_id = result.data[4];
            uint8_t control_header = result.data[5];
            result.is_last_fragment = (control_header & 0x02) != 0;
        }
    }

    return ok(std::move(result));
}

}  // namespace pacs::network::pipeline
