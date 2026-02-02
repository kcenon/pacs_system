/**
 * @file streaming_query_handler.cpp
 * @brief Implementation of streaming query handler
 */

#include "pacs/services/cache/streaming_query_handler.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/index_database.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

streaming_query_handler::streaming_query_handler(storage::index_database* db)
    : db_(db) {}

// =============================================================================
// Configuration
// =============================================================================

void streaming_query_handler::set_page_size(size_t size) noexcept {
    page_size_ = size;
}

auto streaming_query_handler::page_size() const noexcept -> size_t {
    return page_size_;
}

void streaming_query_handler::set_max_results(size_t max) noexcept {
    max_results_ = max;
}

auto streaming_query_handler::max_results() const noexcept -> size_t {
    return max_results_;
}

// =============================================================================
// Stream Operations
// =============================================================================

auto streaming_query_handler::create_stream(query_level level,
                                            const core::dicom_dataset& query_keys,
                                            [[maybe_unused]] const std::string& calling_ae)
    -> StreamResult {
    stream_config config;
    config.page_size = page_size_;

    return query_result_stream::create(db_, level, query_keys, config);
}

auto streaming_query_handler::resume_stream(const std::string& cursor_state,
                                            query_level level,
                                            const core::dicom_dataset& query_keys)
    -> StreamResult {
    stream_config config;
    config.page_size = page_size_;

    return query_result_stream::from_cursor(db_, cursor_state, level, query_keys, config);
}

// =============================================================================
// Compatibility Adapter
// =============================================================================

auto streaming_query_handler::as_query_handler() -> query_handler {
    // Capture necessary state for the lambda
    auto* db = db_;
    auto page_size = page_size_;
    auto max_results = max_results_;

    return [db, page_size, max_results](query_level level,
                                        const core::dicom_dataset& query_keys,
                                        [[maybe_unused]] const std::string& calling_ae)
               -> std::vector<core::dicom_dataset> {
        stream_config config;
        config.page_size = page_size;

        auto stream_result = query_result_stream::create(db, level, query_keys, config);
        if (stream_result.is_err()) {
            // Return empty results on error
            return {};
        }

        auto stream = std::move(stream_result.value());
        std::vector<core::dicom_dataset> results;

        // Collect all results from the stream
        while (stream->has_more()) {
            auto batch = stream->next_batch();
            if (!batch.has_value()) {
                break;
            }

            for (auto& ds : batch.value()) {
                results.push_back(std::move(ds));

                // Check max results limit
                if (max_results > 0 && results.size() >= max_results) {
                    return results;
                }
            }
        }

        return results;
    };
}

}  // namespace pacs::services

#endif  // PACS_WITH_DATABASE_SYSTEM
