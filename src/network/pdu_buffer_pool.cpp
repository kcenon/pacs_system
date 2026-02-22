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
 * @file pdu_buffer_pool.cpp
 * @brief Implementation of PDU buffer pooling
 */

#include <pacs/network/pdu_buffer_pool.hpp>
#include <pacs/network/pdu_decoder.hpp>

namespace pacs::network {

// ============================================================================
// pdu_buffer_pool Implementation
// ============================================================================

pdu_buffer_pool::pdu_buffer_pool()
    : buffer_pool_(DEFAULT_BUFFER_POOL_SIZE)
    , pdv_pool_(DEFAULT_PDV_POOL_SIZE)
    , p_data_pool_(DEFAULT_PDATA_POOL_SIZE) {}

auto pdu_buffer_pool::get() noexcept -> pdu_buffer_pool& {
    static pdu_buffer_pool instance;
    return instance;
}

auto pdu_buffer_pool::acquire_buffer()
    -> tracked_pdu_pool<pooled_buffer>::unique_ptr_type {
    auto buffer = buffer_pool_.acquire();
    buffer->clear();  // Reset buffer state for reuse
    return buffer;
}

auto pdu_buffer_pool::acquire_pdv(uint8_t context_id, bool is_command,
                                   bool is_last)
    -> tracked_pdu_pool<presentation_data_value>::unique_ptr_type {
    auto pdv = pdv_pool_.acquire();
    pdv->context_id = context_id;
    pdv->is_command = is_command;
    pdv->is_last = is_last;
    pdv->data.clear();  // Reset data vector for reuse
    return pdv;
}

auto pdu_buffer_pool::acquire_p_data_tf()
    -> tracked_pdu_pool<p_data_tf_pdu>::unique_ptr_type {
    auto p_data = p_data_pool_.acquire();
    p_data->pdvs.clear();  // Reset PDV vector for reuse
    return p_data;
}

auto pdu_buffer_pool::buffer_statistics() const noexcept
    -> const pdu_pool_statistics& {
    return buffer_pool_.statistics();
}

auto pdu_buffer_pool::pdv_statistics() const noexcept
    -> const pdu_pool_statistics& {
    return pdv_pool_.statistics();
}

auto pdu_buffer_pool::p_data_statistics() const noexcept
    -> const pdu_pool_statistics& {
    return p_data_pool_.statistics();
}

void pdu_buffer_pool::reserve_buffers(std::size_t count) {
    buffer_pool_.reserve(count);
}

void pdu_buffer_pool::reserve_pdvs(std::size_t count) {
    pdv_pool_.reserve(count);
}

void pdu_buffer_pool::clear_all() {
    buffer_pool_.clear();
    pdv_pool_.clear();
    p_data_pool_.clear();
}

void pdu_buffer_pool::reset_statistics() {
    const_cast<pdu_pool_statistics&>(buffer_pool_.statistics()).reset();
    const_cast<pdu_pool_statistics&>(pdv_pool_.statistics()).reset();
    const_cast<pdu_pool_statistics&>(p_data_pool_.statistics()).reset();
}

}  // namespace pacs::network
