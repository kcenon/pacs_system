// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file qr_matching.cpp
 * @brief Attribute matching for Query/Retrieve (DICOM PS3.4 Section C.2.2.2)
 */

#include "kcenon/pacs/services/qr_matching.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::pacs::services {

namespace {

constexpr char kValueDelimiter = '\\';

[[nodiscard]] std::string_view trimmed(std::string_view value) noexcept {
    const auto padding = [](char character) { return character == ' ' || character == '\0'; };
    while (!value.empty() && padding(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && padding(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

[[nodiscard]] std::vector<std::string_view> values_of(std::string_view text) {
    std::vector<std::string_view> values;
    for (;;) {
        const auto delimiter = text.find(kValueDelimiter);
        values.push_back(trimmed(text.substr(0, delimiter)));
        if (delimiter == std::string_view::npos) {
            return values;
        }
        text.remove_prefix(delimiter+1);
    }
}

[[nodiscard]] std::size_t character_length(std::string_view text, std::size_t at) noexcept {
    const auto lead = static_cast<unsigned char>(text[at]);
    const std::size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    return (std::min)(length, text.size()-at);
}

[[nodiscard]] char ascii_lower(char character) noexcept {
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character-'A'+'a') : character;
}

[[nodiscard]] bool text_value_matches(std::string_view key, std::string_view stored, bool ignore_ascii_case) noexcept {
    // Without a wild card the same walk is Single Value Matching.
    return wildcard_matches(key, stored, ignore_ascii_case);
}

[[nodiscard]] bool digits_only(std::string_view text) noexcept {
    return std::all_of(text.begin(), text.end(), [](char character) { return character >= '0' && character <= '9'; });
}

/// PS3.5 allows the retired YYYY.MM.DD form in queries and in existing objects.
[[nodiscard]] std::optional<std::string> normalized_date(std::string_view value) {
    std::string date;
    std::copy_if(value.begin(), value.end(), std::back_inserter(date), [](char character) { return character != '.'; });
    if (date.size() != 8 || !digits_only(date)) {
        return std::nullopt;
    }
    return date;
}

/// A TM value as HHMMSS.FFFFFF. Omitted precision becomes the start or the end of what it names.
[[nodiscard]] std::optional<std::string> normalized_time(std::string_view value, bool upper_bound) {
    std::string digits;
    std::copy_if(value.begin(), value.end(), std::back_inserter(digits), [](char character) { return character != ':'; });
    std::string fraction;
    if (const auto point = digits.find('.'); point != std::string::npos) {
        fraction = digits.substr(point+1);
        digits.resize(point);
    }
    if (digits.empty() || digits.size() % 2 != 0 || digits.size() > 6 || fraction.size() > 6 ||
        !digits_only(digits) || !digits_only(fraction)) {
        return std::nullopt;
    }
    if (upper_bound) {
        digits += std::string("595959").substr(digits.size());
        fraction.append(6-fraction.size(), '9');
    } else {
        digits.append(6-digits.size(), '0');
        fraction.append(6-fraction.size(), '0');
    }
    return digits+"."+fraction;
}

template <typename Lower, typename Upper>
[[nodiscard]] bool range_matches(std::string_view key, Lower lower_of, Upper upper_of, const std::optional<std::string>& stored) {
    if (!stored) {
        return false;
    }
    const auto dash = key.find('-');
    // Range Matching is by meaning, and a single value is the range of itself (C.2.2.2.1.3).
    const auto first = key.substr(0, dash);
    const auto last = dash == std::string_view::npos ? key : key.substr(dash+1);
    if (first.empty() && last.empty()) {
        return false;
    }
    if (!first.empty()) {
        const auto lower = lower_of(first);
        if (!lower || *stored < *lower) {
            return false;
        }
    }
    if (!last.empty()) {
        const auto upper = upper_of(last);
        if (!upper || *stored > *upper) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<int> integer_of(std::string_view text) noexcept {
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
    }
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data()+text.size(), value);
    if (error != std::errc{} || end != text.data()+text.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] bool one_value_matches(key_matching matching, std::string_view key, std::string_view stored) {
    switch (matching) {
        case key_matching::uid:
            return key == stored;
        case key_matching::text:
            return text_value_matches(key, stored, false);
        case key_matching::person_name:
            return text_value_matches(key, stored, true);
        case key_matching::date:
            return range_matches(key, normalized_date, normalized_date, normalized_date(stored));
        case key_matching::time:
            return range_matches(key, [](std::string_view value) { return normalized_time(value, false); },
                                 [](std::string_view value) { return normalized_time(value, true); },
                                 normalized_time(stored, false));
        case key_matching::number: {
            const auto wanted = integer_of(key);
            return wanted && wanted == integer_of(stored);
        }
    }
    return false;
}

}  // namespace

bool wildcard_matches(std::string_view pattern, std::string_view text, bool ignore_ascii_case) noexcept {
    const auto same = [ignore_ascii_case](char left, char right) {
        return ignore_ascii_case ? ascii_lower(left) == ascii_lower(right) : left == right;
    };
    constexpr auto none = std::string_view::npos;
    std::size_t at_pattern = 0, at_text = 0, after_star = none, resume = 0;
    while (at_text < text.size()) {
        if (at_pattern < pattern.size() && pattern[at_pattern] == '*') {
            after_star = ++at_pattern;
            resume = at_text;
        } else if (at_pattern < pattern.size() && pattern[at_pattern] == '?') {
            ++at_pattern;
            at_text += character_length(text, at_text);
        } else if (at_pattern < pattern.size() && same(pattern[at_pattern], text[at_text])) {
            ++at_pattern;
            ++at_text;
        } else if (after_star != none) {
            // Let the last star take one more character, then try again from there.
            at_pattern = after_star;
            resume += character_length(text, resume);
            at_text = resume;
        } else {
            return false;
        }
    }
    while (at_pattern < pattern.size() && pattern[at_pattern] == '*') {
        ++at_pattern;
    }
    return at_pattern == pattern.size();
}

bool key_matches(key_matching matching, std::string_view key, std::string_view stored) {
    key = trimmed(key);
    stored = trimmed(stored);
    if (key.empty() || stored.empty()) {
        return true;
    }
    const auto stored_values = values_of(stored);
    for (const auto key_value : values_of(key)) {
        for (const auto stored_value : stored_values) {
            if (!key_value.empty() && one_value_matches(matching, key_value, stored_value)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace kcenon::pacs::services
