// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file sql_literal.h
 * @brief Quote untrusted text for the repositories that build SQL strings
 */

#pragma once

#include <string>
#include <string_view>

namespace kcenon::pacs::storage {

/**
 * @brief Quote a value as one SQL string literal.
 *
 * Query keys reach these statements from the network, so a value must never be able
 * to end its literal. Doubling the single quote is the only escape SQL defines; a
 * backslash stays an ordinary character unless the statement names it with ESCAPE.
 */
[[nodiscard]] inline std::string sql_literal(std::string_view value) {
    std::string quoted;
    quoted.reserve(value.size()+2);
    quoted += '\'';
    for (const char character : value) {
        if (character == '\'') {
            quoted += '\'';
        }
        quoted += character;
    }
    quoted += '\'';
    return quoted;
}

}  // namespace kcenon::pacs::storage
