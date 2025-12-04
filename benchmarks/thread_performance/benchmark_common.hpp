/**
 * @file benchmark_common.hpp
 * @brief Common utilities and fixtures for thread performance benchmarks
 *
 * Provides reusable server fixtures, timing utilities, and DICOM data
 * generators specifically designed for performance benchmarking.
 *
 * @see Issue #154 - Establish performance baseline benchmarks for thread migration
 */

#ifndef PACS_BENCHMARKS_THREAD_PERFORMANCE_BENCHMARK_COMMON_HPP
#define PACS_BENCHMARKS_THREAD_PERFORMANCE_BENCHMARK_COMMON_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_element.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dicom_server.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"
#include "pacs/services/verification_scp.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace pacs::benchmark {

// =============================================================================
// Constants
// =============================================================================

/// Default benchmark port range start
constexpr uint16_t default_benchmark_port = 42104;

/// Default timeout for benchmark operations
constexpr auto default_timeout = std::chrono::milliseconds{10000};

// Note: verification_sop_class_uid is provided by pacs::services::verification_sop_class_uid
// from verification_scp.hpp to avoid symbol conflicts

/// CT Image Storage SOP Class UID
constexpr const char* ct_storage_sop_class_uid = "1.2.840.10008.5.1.4.1.1.2";

/// Implicit VR Little Endian Transfer Syntax
constexpr const char* implicit_vr_le = "1.2.840.10008.1.2";

/// Explicit VR Little Endian Transfer Syntax
constexpr const char* explicit_vr_le = "1.2.840.10008.1.2.1";

// =============================================================================
// Timing Utilities
// =============================================================================

/**
 * @brief High-resolution timer for precise measurements
 */
class high_resolution_timer {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        end_time_ = std::chrono::high_resolution_clock::now();
    }

    [[nodiscard]] std::chrono::nanoseconds elapsed_ns() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time_ - start_time_);
    }

    [[nodiscard]] std::chrono::microseconds elapsed_us() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            end_time_ - start_time_);
    }

    [[nodiscard]] std::chrono::milliseconds elapsed_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time_ - start_time_);
    }

    [[nodiscard]] double elapsed_seconds() const {
        return std::chrono::duration<double>(end_time_ - start_time_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
};

/**
 * @brief Statistics accumulator for benchmark results
 */
struct benchmark_stats {
    size_t count{0};
    double sum_ms{0.0};
    double sum_squared_ms{0.0};
    double min_ms{std::numeric_limits<double>::max()};
    double max_ms{0.0};

    void record(double duration_ms) {
        ++count;
        sum_ms += duration_ms;
        sum_squared_ms += duration_ms * duration_ms;
        min_ms = std::min(min_ms, duration_ms);
        max_ms = std::max(max_ms, duration_ms);
    }

    [[nodiscard]] double mean_ms() const {
        return count > 0 ? sum_ms / static_cast<double>(count) : 0.0;
    }

    [[nodiscard]] double stddev_ms() const {
        if (count < 2) return 0.0;
        double mean = mean_ms();
        double variance = (sum_squared_ms / static_cast<double>(count)) - (mean * mean);
        return std::sqrt(std::max(0.0, variance));
    }

    [[nodiscard]] double throughput_per_second() const {
        return sum_ms > 0.0 ? (static_cast<double>(count) * 1000.0) / sum_ms : 0.0;
    }
};

// =============================================================================
// Port Management
// =============================================================================

/**
 * @brief Find an available port for benchmarking
 * @param start Starting port number
 * @return Available port number
 */
inline uint16_t find_available_port(uint16_t start = default_benchmark_port) {
    static std::atomic<uint16_t> port_offset{0};
    return start + (port_offset++ % 100);
}

// =============================================================================
// UID Generation
// =============================================================================

/**
 * @brief Generate a unique UID for benchmarking
 * @param root Root UID prefix
 * @return Unique UID string
 */
inline std::string generate_uid(const std::string& root = "1.2.826.0.1.3680043.9.8888") {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return root + "." + std::to_string(timestamp) + "." + std::to_string(++counter);
}

// =============================================================================
// DICOM Dataset Generators
// =============================================================================

/**
 * @brief Generate a minimal CT dataset for benchmarking
 * @param study_uid Study Instance UID (generated if empty)
 * @return DICOM dataset
 */
inline core::dicom_dataset generate_benchmark_dataset(const std::string& study_uid = "") {
    core::dicom_dataset ds;

    // Patient module
    ds.set_string(core::tags::patient_name, encoding::vr_type::PN, "BENCHMARK^PATIENT");
    ds.set_string(core::tags::patient_id, encoding::vr_type::LO, "BENCH001");
    ds.set_string(core::tags::patient_birth_date, encoding::vr_type::DA, "19800101");
    ds.set_string(core::tags::patient_sex, encoding::vr_type::CS, "O");

    // Study module
    ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI,
                  study_uid.empty() ? generate_uid() : study_uid);
    ds.set_string(core::tags::study_date, encoding::vr_type::DA, "20240101");
    ds.set_string(core::tags::study_time, encoding::vr_type::TM, "120000");
    ds.set_string(core::tags::accession_number, encoding::vr_type::SH, "BENCH001");
    ds.set_string(core::tags::study_id, encoding::vr_type::SH, "BENCHSTUDY");

    // Series module
    ds.set_string(core::tags::series_instance_uid, encoding::vr_type::UI, generate_uid());
    ds.set_string(core::tags::modality, encoding::vr_type::CS, "CT");
    ds.set_string(core::tags::series_number, encoding::vr_type::IS, "1");

    // SOP Common module
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI, ct_storage_sop_class_uid);
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image module (minimal 64x64 16-bit image)
    ds.set_numeric<uint16_t>(core::tags::rows, encoding::vr_type::US, 64);
    ds.set_numeric<uint16_t>(core::tags::columns, encoding::vr_type::US, 64);
    ds.set_numeric<uint16_t>(core::tags::bits_allocated, encoding::vr_type::US, 16);
    ds.set_numeric<uint16_t>(core::tags::bits_stored, encoding::vr_type::US, 12);
    ds.set_numeric<uint16_t>(core::tags::high_bit, encoding::vr_type::US, 11);
    ds.set_numeric<uint16_t>(core::tags::pixel_representation, encoding::vr_type::US, 0);
    ds.set_numeric<uint16_t>(core::tags::samples_per_pixel, encoding::vr_type::US, 1);
    ds.set_string(core::tags::photometric_interpretation, encoding::vr_type::CS, "MONOCHROME2");

    // Generate minimal pixel data
    std::vector<uint16_t> pixel_data(64 * 64, 512);
    core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OW);
    pixel_elem.set_value(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(pixel_data.data()),
        pixel_data.size() * sizeof(uint16_t)));
    ds.insert(std::move(pixel_elem));

    return ds;
}

// =============================================================================
// Benchmark Server Fixture
// =============================================================================

/**
 * @brief DICOM server fixture for benchmarking
 *
 * Provides a lightweight server with configurable services
 * and counters for measuring performance.
 */
class benchmark_server {
public:
    explicit benchmark_server(uint16_t port, const std::string& ae_title = "BENCH_SCP")
        : port_(port)
        , ae_title_(ae_title) {

        network::server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 100;
        config.idle_timeout = std::chrono::seconds{60};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.1";
        config.implementation_version_name = "BENCH_SCP";

        server_ = std::make_unique<network::dicom_server>(config);
    }

    /**
     * @brief Initialize server with verification service only (for echo benchmarks)
     */
    bool initialize_echo_only() {
        server_->register_service(std::make_shared<services::verification_scp>());
        return true;
    }

    /**
     * @brief Initialize server with storage service (for store benchmarks)
     */
    bool initialize_with_storage() {
        server_->register_service(std::make_shared<services::verification_scp>());

        auto storage_scp = std::make_shared<services::storage_scp>();
        storage_scp->set_handler([this](
            const core::dicom_dataset& /* dataset */,
            const std::string& /* calling_ae */,
            const std::string& /* sop_class_uid */,
            const std::string& /* sop_instance_uid */) {
            ++store_count_;
            return services::storage_status::success;
        });
        server_->register_service(storage_scp);
        return true;
    }

    bool start() {
        auto result = server_->start();
        if (result.is_ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            return true;
        }
        return false;
    }

    void stop() {
        server_->stop();
    }

    [[nodiscard]] uint16_t port() const { return port_; }
    [[nodiscard]] const std::string& ae_title() const { return ae_title_; }
    [[nodiscard]] size_t store_count() const { return store_count_.load(); }
    [[nodiscard]] size_t active_associations() const { return server_->active_associations(); }

private:
    uint16_t port_;
    std::string ae_title_;
    std::unique_ptr<network::dicom_server> server_;
    std::atomic<size_t> store_count_{0};
};

// =============================================================================
// Association Configuration Helpers
// =============================================================================

/**
 * @brief Create association config for verification (C-ECHO)
 */
inline network::association_config make_echo_config(
    const std::string& calling_ae,
    const std::string& called_ae) {

    network::association_config config;
    config.calling_ae_title = calling_ae;
    config.called_ae_title = called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.2";
    config.proposed_contexts.push_back({
        1,
        std::string(services::verification_sop_class_uid),
        {explicit_vr_le, implicit_vr_le}
    });
    return config;
}

/**
 * @brief Create association config for storage (C-STORE)
 */
inline network::association_config make_store_config(
    const std::string& calling_ae,
    const std::string& called_ae) {

    network::association_config config;
    config.calling_ae_title = calling_ae;
    config.called_ae_title = called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.3";
    config.proposed_contexts.push_back({
        1,
        std::string(ct_storage_sop_class_uid),
        {explicit_vr_le, implicit_vr_le}
    });
    return config;
}

}  // namespace pacs::benchmark

#endif  // PACS_BENCHMARKS_THREAD_PERFORMANCE_BENCHMARK_COMMON_HPP
