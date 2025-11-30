/**
 * @file tag_info.cpp
 * @brief Implementation of tag_info and value_multiplicity
 */

#include "pacs/core/tag_info.hpp"

#include <charconv>
#include <sstream>

namespace pacs::core {

auto value_multiplicity::from_string(std::string_view str)
    -> std::optional<value_multiplicity> {
    if (str.empty()) {
        return std::nullopt;
    }

    value_multiplicity result;

    // Find the hyphen separator
    const auto hyphen_pos = str.find('-');

    if (hyphen_pos == std::string_view::npos) {
        // Single value case: "1", "2", etc.
        uint32_t val{0};
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
        if (ec != std::errc{} || ptr != str.data() + str.size()) {
            return std::nullopt;
        }
        result.min = val;
        result.max = val;
        return result;
    }

    // Range case: "min-max" or "min-n" or "min-2n"
    const auto min_str = str.substr(0, hyphen_pos);
    const auto max_str = str.substr(hyphen_pos + 1);

    if (min_str.empty() || max_str.empty()) {
        return std::nullopt;
    }

    // Parse minimum
    uint32_t min_val{0};
    auto [min_ptr, min_ec] = std::from_chars(
        min_str.data(), min_str.data() + min_str.size(), min_val);
    if (min_ec != std::errc{} || min_ptr != min_str.data() + min_str.size()) {
        return std::nullopt;
    }
    result.min = min_val;

    // Parse maximum
    if (max_str == "n") {
        // Unbounded: "1-n"
        result.max = std::nullopt;
        result.multiplier = 1;
    } else if (max_str.back() == 'n') {
        // Multiplier pattern: "1-2n", "2-3n"
        const auto mult_str = max_str.substr(0, max_str.size() - 1);
        uint32_t mult_val{1};
        auto [mult_ptr, mult_ec] = std::from_chars(
            mult_str.data(), mult_str.data() + mult_str.size(), mult_val);
        if (mult_ec != std::errc{} || mult_ptr != mult_str.data() + mult_str.size()) {
            return std::nullopt;
        }
        result.max = std::nullopt;
        result.multiplier = mult_val;
    } else {
        // Fixed range: "1-2", "1-3"
        uint32_t max_val{0};
        auto [max_ptr, max_ec] = std::from_chars(
            max_str.data(), max_str.data() + max_str.size(), max_val);
        if (max_ec != std::errc{} || max_ptr != max_str.data() + max_str.size()) {
            return std::nullopt;
        }
        result.max = max_val;
    }

    return result;
}

auto value_multiplicity::to_string() const -> std::string {
    std::ostringstream oss;
    oss << min;

    if (!max.has_value()) {
        // Unbounded
        oss << '-';
        if (multiplier > 1) {
            oss << multiplier;
        }
        oss << 'n';
    } else if (max.value() != min) {
        // Range
        oss << '-' << max.value();
    }
    // If max == min, just output the single value

    return oss.str();
}

}  // namespace pacs::core
