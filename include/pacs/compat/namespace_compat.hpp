// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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
 * @file namespace_compat.hpp
 * @brief Backward-compatible namespace alias for migration from pacs:: to kcenon::pacs::
 *
 * Include this header in downstream code that still uses the old pacs:: namespace
 * to transparently redirect to kcenon::pacs::.
 *
 * IMPORTANT: Do NOT include this header from any pacs internal header, as it
 * would prevent reopening the kcenon::pacs namespace via the alias.
 *
 * @code
 * #include <pacs/compat/namespace_compat.hpp>
 * // Now pacs::core::dicom_tag resolves to kcenon::pacs::core::dicom_tag
 * @endcode
 *
 * @author kcenon
 * @since 2.0.0
 */

#pragma once

// Establish kcenon::pacs namespace (must exist before aliasing)
namespace kcenon::pacs {}

/// Backward-compatible alias: allows existing code using pacs:: to compile
/// without modification after the namespace migration to kcenon::pacs::.
namespace pacs = kcenon::pacs;
