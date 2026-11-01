// BSD 3-Clause License. See LICENSE.
#pragma once
#include <string>

namespace worklist_cli {
inline bool matches_range(const std::string& value, const std::string& pattern) {
    if (pattern.empty()) { return true; }
    const auto dash = pattern.find('-');
    if (dash == std::string::npos) { return value == pattern; }
    const auto lower = pattern.substr(0, dash);
    const auto upper = pattern.substr(dash + 1);
    return (lower.empty() || value >= lower) && (upper.empty() || value <= upper);
}

inline bool matches_schedule(const std::string& date, const std::string& time,
                             const std::string& date_key, const std::string& time_key) {
    const auto date_dash = date_key.find('-');
    const auto time_dash = time_key.find('-');
    if (date_dash == std::string::npos || time_dash == std::string::npos) {
        return matches_range(date, date_key) && matches_range(time, time_key);
    }
    // PS3.4 K.6.1.2.2: paired ranges describe one continuous date/time interval,
    // not the same time window repeated on every matching day.
    const auto first_date = date_key.substr(0, date_dash);
    const auto last_date = date_key.substr(date_dash + 1);
    const auto first_time = time_key.substr(0, time_dash);
    const auto last_time = time_key.substr(time_dash + 1);
    return (first_date.empty() || date > first_date ||
            (date == first_date && (first_time.empty() || time >= first_time))) &&
           (last_date.empty() || date < last_date ||
            (date == last_date && (last_time.empty() || time <= last_time)));
}
} // namespace worklist_cli
