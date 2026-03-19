// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file streaming_query_handler.cpp
 * @brief Implementation of streaming query handler
 */

#include "pacs/services/cache/streaming_query_handler.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/index_database.hpp"

namespace kcenon::pacs::services {

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

}  // namespace kcenon::pacs::services

#endif  // PACS_WITH_DATABASE_SYSTEM
