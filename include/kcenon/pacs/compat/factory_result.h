// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/common/error/error_codes.h>
#include <kcenon/common/patterns/result.h>
#include <utility>

namespace kcenon::pacs::compat {

// Released dependencies return smart pointers; current factories return Result.
// Preserve structured errors while keeping both dependency generations usable.
template <typename T>
[[nodiscard]] auto factory_result(kcenon::common::Result<T> result)
    -> kcenon::common::Result<T> {
    return result;
}

template <typename Pointer>
    requires requires { typename Pointer::element_type; }
[[nodiscard]] auto factory_result(Pointer pointer)
    -> kcenon::common::Result<Pointer> {
    if (!pointer) {
        return kcenon::common::make_error<Pointer>(
            kcenon::common::error::codes::common_errors::internal_error,
            "Dependency factory returned null", "pacs");
    }
    return kcenon::common::Result<Pointer>(std::move(pointer));
}

}  // namespace kcenon::pacs::compat
