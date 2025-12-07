/**
 * @file pacs_metrics.cpp
 * @brief Implementation of operation metrics collection for PACS DICOM services
 *
 * @see Issue #210 - [Quick Win] feat(monitoring): Implement operation metrics collection
 */

#include "pacs/monitoring/pacs_metrics.hpp"

#include <iomanip>
#include <sstream>

namespace pacs::monitoring {

namespace {

/**
 * @brief Helper to format a single operation counter as JSON object
 * @param counter The operation counter to format
 * @return JSON object string for the counter
 */
std::string counter_to_json(const operation_counter& counter) {
    std::ostringstream oss;
    const auto total = counter.total_count();
    const auto success = counter.success_count.load(std::memory_order_relaxed);
    const auto failure = counter.failure_count.load(std::memory_order_relaxed);
    const auto avg_us = counter.average_duration_us();
    const auto min_us = counter.min_duration_us.load(std::memory_order_relaxed);
    const auto max_us = counter.max_duration_us.load(std::memory_order_relaxed);

    oss << "{"
        << R"("total":)" << total
        << R"(,"success":)" << success
        << R"(,"failure":)" << failure
        << R"(,"avg_duration_us":)" << avg_us;

    // Only include min/max if there were operations
    if (total > 0) {
        oss << R"(,"min_duration_us":)" << min_us
            << R"(,"max_duration_us":)" << max_us;
    }

    oss << "}";
    return oss.str();
}

/**
 * @brief Helper to format Prometheus metrics for an operation counter
 * @param oss Output stream
 * @param prefix Metric prefix
 * @param op_name Operation name
 * @param counter The operation counter
 */
void counter_to_prometheus(std::ostringstream& oss,
                            std::string_view prefix,
                            std::string_view op_name,
                            const operation_counter& counter) {
    const auto success = counter.success_count.load(std::memory_order_relaxed);
    const auto failure = counter.failure_count.load(std::memory_order_relaxed);
    const auto total = counter.total_count();
    const auto total_duration = counter.total_duration_us.load(std::memory_order_relaxed);

    // Total operations counter
    oss << "# HELP " << prefix << "_dimse_" << op_name << "_total "
        << "Total " << op_name << " operations\n"
        << "# TYPE " << prefix << "_dimse_" << op_name << "_total counter\n"
        << prefix << "_dimse_" << op_name << "_total " << total << "\n";

    // Success counter
    oss << "# HELP " << prefix << "_dimse_" << op_name << "_success_total "
        << "Successful " << op_name << " operations\n"
        << "# TYPE " << prefix << "_dimse_" << op_name << "_success_total counter\n"
        << prefix << "_dimse_" << op_name << "_success_total " << success << "\n";

    // Failure counter
    oss << "# HELP " << prefix << "_dimse_" << op_name << "_failure_total "
        << "Failed " << op_name << " operations\n"
        << "# TYPE " << prefix << "_dimse_" << op_name << "_failure_total counter\n"
        << prefix << "_dimse_" << op_name << "_failure_total " << failure << "\n";

    // Duration sum (for calculating averages)
    oss << "# HELP " << prefix << "_dimse_" << op_name << "_duration_microseconds_sum "
        << "Total duration of " << op_name << " operations in microseconds\n"
        << "# TYPE " << prefix << "_dimse_" << op_name << "_duration_microseconds_sum counter\n"
        << prefix << "_dimse_" << op_name << "_duration_microseconds_sum " << total_duration << "\n";

    // Duration min/max (only if there were operations)
    if (total > 0) {
        const auto min_us = counter.min_duration_us.load(std::memory_order_relaxed);
        const auto max_us = counter.max_duration_us.load(std::memory_order_relaxed);

        oss << "# HELP " << prefix << "_dimse_" << op_name << "_duration_microseconds_min "
            << "Minimum duration of " << op_name << " operations in microseconds\n"
            << "# TYPE " << prefix << "_dimse_" << op_name << "_duration_microseconds_min gauge\n"
            << prefix << "_dimse_" << op_name << "_duration_microseconds_min " << min_us << "\n";

        oss << "# HELP " << prefix << "_dimse_" << op_name << "_duration_microseconds_max "
            << "Maximum duration of " << op_name << " operations in microseconds\n"
            << "# TYPE " << prefix << "_dimse_" << op_name << "_duration_microseconds_max gauge\n"
            << prefix << "_dimse_" << op_name << "_duration_microseconds_max " << max_us << "\n";
    }
}

}  // namespace

std::string pacs_metrics::to_json() const {
    std::ostringstream oss;
    oss << "{";

    // DIMSE operations section
    oss << R"("dimse_operations":{)"
        << R"("c_echo":)" << counter_to_json(c_echo_)
        << R"(,"c_store":)" << counter_to_json(c_store_)
        << R"(,"c_find":)" << counter_to_json(c_find_)
        << R"(,"c_move":)" << counter_to_json(c_move_)
        << R"(,"c_get":)" << counter_to_json(c_get_)
        << R"(,"n_create":)" << counter_to_json(n_create_)
        << R"(,"n_set":)" << counter_to_json(n_set_)
        << R"(,"n_get":)" << counter_to_json(n_get_)
        << R"(,"n_action":)" << counter_to_json(n_action_)
        << R"(,"n_event":)" << counter_to_json(n_event_)
        << R"(,"n_delete":)" << counter_to_json(n_delete_)
        << "}";

    // Data transfer section
    oss << R"(,"data_transfer":{)"
        << R"("bytes_sent":)" << transfer_.bytes_sent.load(std::memory_order_relaxed)
        << R"(,"bytes_received":)" << transfer_.bytes_received.load(std::memory_order_relaxed)
        << R"(,"images_stored":)" << transfer_.images_stored.load(std::memory_order_relaxed)
        << R"(,"images_retrieved":)" << transfer_.images_retrieved.load(std::memory_order_relaxed)
        << "}";

    // Associations section
    oss << R"(,"associations":{)"
        << R"("total_established":)" << associations_.total_established.load(std::memory_order_relaxed)
        << R"(,"total_rejected":)" << associations_.total_rejected.load(std::memory_order_relaxed)
        << R"(,"total_aborted":)" << associations_.total_aborted.load(std::memory_order_relaxed)
        << R"(,"current_active":)" << associations_.current_active.load(std::memory_order_relaxed)
        << R"(,"peak_active":)" << associations_.peak_active.load(std::memory_order_relaxed)
        << "}";

    oss << "}";
    return oss.str();
}

std::string pacs_metrics::to_prometheus(std::string_view prefix) const {
    std::ostringstream oss;

    // DIMSE operation metrics
    counter_to_prometheus(oss, prefix, "c_echo", c_echo_);
    counter_to_prometheus(oss, prefix, "c_store", c_store_);
    counter_to_prometheus(oss, prefix, "c_find", c_find_);
    counter_to_prometheus(oss, prefix, "c_move", c_move_);
    counter_to_prometheus(oss, prefix, "c_get", c_get_);
    counter_to_prometheus(oss, prefix, "n_create", n_create_);
    counter_to_prometheus(oss, prefix, "n_set", n_set_);
    counter_to_prometheus(oss, prefix, "n_get", n_get_);
    counter_to_prometheus(oss, prefix, "n_action", n_action_);
    counter_to_prometheus(oss, prefix, "n_event", n_event_);
    counter_to_prometheus(oss, prefix, "n_delete", n_delete_);

    // Data transfer metrics
    oss << "# HELP " << prefix << "_bytes_sent_total Total bytes sent over network\n"
        << "# TYPE " << prefix << "_bytes_sent_total counter\n"
        << prefix << "_bytes_sent_total " << transfer_.bytes_sent.load(std::memory_order_relaxed) << "\n";

    oss << "# HELP " << prefix << "_bytes_received_total Total bytes received from network\n"
        << "# TYPE " << prefix << "_bytes_received_total counter\n"
        << prefix << "_bytes_received_total " << transfer_.bytes_received.load(std::memory_order_relaxed) << "\n";

    oss << "# HELP " << prefix << "_images_stored_total Total DICOM images stored\n"
        << "# TYPE " << prefix << "_images_stored_total counter\n"
        << prefix << "_images_stored_total " << transfer_.images_stored.load(std::memory_order_relaxed) << "\n";

    oss << "# HELP " << prefix << "_images_retrieved_total Total DICOM images retrieved\n"
        << "# TYPE " << prefix << "_images_retrieved_total counter\n"
        << prefix << "_images_retrieved_total " << transfer_.images_retrieved.load(std::memory_order_relaxed) << "\n";

    // Association metrics
    oss << "# HELP " << prefix << "_associations_established_total Total associations established\n"
        << "# TYPE " << prefix << "_associations_established_total counter\n"
        << prefix << "_associations_established_total " << associations_.total_established.load(std::memory_order_relaxed) << "\n";

    oss << "# HELP " << prefix << "_associations_rejected_total Total associations rejected\n"
        << "# TYPE " << prefix << "_associations_rejected_total counter\n"
        << prefix << "_associations_rejected_total " << associations_.total_rejected.load(std::memory_order_relaxed) << "\n";

    oss << "# HELP " << prefix << "_associations_aborted_total Total associations aborted\n"
        << "# TYPE " << prefix << "_associations_aborted_total counter\n"
        << prefix << "_associations_aborted_total " << associations_.total_aborted.load(std::memory_order_relaxed) << "\n";

    oss << "# HELP " << prefix << "_associations_active Current active associations\n"
        << "# TYPE " << prefix << "_associations_active gauge\n"
        << prefix << "_associations_active " << associations_.current_active.load(std::memory_order_relaxed) << "\n";

    oss << "# HELP " << prefix << "_associations_peak_active Peak concurrent associations\n"
        << "# TYPE " << prefix << "_associations_peak_active gauge\n"
        << prefix << "_associations_peak_active " << associations_.peak_active.load(std::memory_order_relaxed) << "\n";

    return oss.str();
}

}  // namespace pacs::monitoring
