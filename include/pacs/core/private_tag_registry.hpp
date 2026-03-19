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
 * @file private_tag_registry.hpp
 * @brief Registry of vendor-specific private tag definitions
 *
 * Provides VR lookup for private data elements keyed by Private Creator
 * identification string and element offset, enabling correct VR resolution
 * in Implicit VR Little Endian decoding.
 *
 * @see DICOM PS3.5 Section 7.8.1 - Private Data Elements
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <pacs/encoding/vr_type.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::core {

/**
 * @brief Definition of a vendor-specific private tag
 *
 * Describes a single private data element within a vendor's private block.
 * The element_offset identifies the element within the block (0x00-0xFF),
 * corresponding to the low byte of the full element number.
 */
struct private_tag_definition {
    uint8_t element_offset;          ///< Offset within the private block (0x00-0xFF)
    encoding::vr_type vr;            ///< Value Representation
    std::string name;                ///< Human-readable name
};

/**
 * @brief Registry of vendor-specific private tag definitions
 *
 * A thread-safe singleton registry that maps (creator_id, element_offset)
 * pairs to VR information. Used by the Implicit VR Little Endian decoder
 * to resolve the correct VR for private data elements.
 *
 * Thread Safety:
 * - Read operations (find) are thread-safe and can be concurrent
 * - Write operations (register_tag, register_vendor) are serialized
 *
 * @example
 * @code
 * auto& registry = private_tag_registry::instance();
 *
 * // Register a vendor tag
 * registry.register_tag("SIEMENS CSA HEADER", {0x01, vr_type::OB, "CSA Data"});
 *
 * // Look up VR for a private element
 * if (auto def = registry.find("SIEMENS CSA HEADER", 0x01)) {
 *     // def->vr == vr_type::OB
 * }
 * @endcode
 */
class private_tag_registry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the global private_tag_registry instance
     */
    [[nodiscard]] static auto instance() -> private_tag_registry&;

    // Non-copyable and non-movable (singleton)
    private_tag_registry(const private_tag_registry&) = delete;
    private_tag_registry(private_tag_registry&&) = delete;
    auto operator=(const private_tag_registry&) -> private_tag_registry& = delete;
    auto operator=(private_tag_registry&&) -> private_tag_registry& = delete;

    /**
     * @brief Register a single vendor-specific private tag definition
     * @param creator_id Private Creator identification string
     * @param definition The tag definition (offset, VR, name)
     * @return true if registration succeeded, false if already exists
     */
    auto register_tag(std::string_view creator_id,
                      const private_tag_definition& definition) -> bool;

    /**
     * @brief Register a complete vendor dictionary (batch registration)
     * @param creator_id Private Creator identification string
     * @param definitions Span of tag definitions to register
     * @return Number of tags successfully registered
     */
    auto register_vendor(std::string_view creator_id,
                         std::span<const private_tag_definition> definitions)
        -> size_t;

    /**
     * @brief Look up a private tag definition by creator ID and element offset
     * @param creator_id Private Creator identification string
     * @param element_offset Offset within the private block (0x00-0xFF)
     * @return The tag definition if found, nullopt otherwise
     */
    [[nodiscard]] auto find(std::string_view creator_id,
                            uint8_t element_offset) const
        -> std::optional<private_tag_definition>;

    /**
     * @brief Get the number of registered definitions
     * @return Total count of registered private tag definitions
     */
    [[nodiscard]] auto size() const -> size_t;

    /**
     * @brief Clear all registered definitions
     *
     * Primarily for testing purposes.
     */
    void clear();

private:
    private_tag_registry() = default;

    /// Key type: (creator_id, element_offset)
    using key_type = std::pair<std::string, uint8_t>;

    /// Storage for registered definitions
    std::map<key_type, private_tag_definition> definitions_;

    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;
};

}  // namespace kcenon::pacs::core

