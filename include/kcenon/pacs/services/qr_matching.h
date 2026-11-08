// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file qr_matching.h
 * @brief Attribute matching for Query/Retrieve (DICOM PS3.4 Section C.2.2.2)
 *
 * Pure functions over UTF-8 text, so the rules can be tested without a database or
 * a network. The index holds UTF-8, and Key Attributes are decoded before matching.
 */

#pragma once

#include <string_view>

namespace kcenon::pacs::services {

/// How a Key Attribute compares with a stored value.
enum class key_matching {
    uid,          ///< Single Value or List of UID Matching. No wild cards.
    text,         ///< Single Value or Wild Card Matching, case-sensitive.
    person_name,  ///< Single Value or Wild Card Matching, ignoring ASCII case.
    date,         ///< DA: Single Value or Range Matching, by meaning.
    time,         ///< TM: Single Value or Range Matching, by meaning.
    number        ///< IS: Single Value Matching, by integer value.
};

/**
 * @brief Wild Card Matching (C.2.2.2.4).
 *
 * "*" matches any sequence, including none, and "?" matches any single character.
 * A character is a whole UTF-8 sequence here, however many bytes it has.
 */
[[nodiscard]] bool wildcard_matches(std::string_view pattern, std::string_view text,
                                    bool ignore_ascii_case) noexcept;

/**
 * @brief Decide whether a stored value satisfies a Key Attribute.
 *
 * - An empty key is Universal Matching.
 * - An empty stored value matches any key: the value is unknown (C.2.2.1.2).
 * - A stored value with several values matches when any of them does (C.2.2.3).
 * - A key with several values matches when any of them does. This is List of UID
 *   Matching for UIDs. For other VRs C.2.2.2.8 leaves it undefined without Extended
 *   Negotiation, and the Conformance Statement declares this choice.
 */
[[nodiscard]] bool key_matches(key_matching matching, std::string_view key, std::string_view stored);

}  // namespace kcenon::pacs::services
