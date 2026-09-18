// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#include <kcenon/pacs/compat/factory_result.h>
#include <memory>

int main() {
    using kcenon::pacs::compat::factory_result;
    using kcenon::common::Result;

    auto legacy_shared = factory_result(std::make_shared<int>(42));
    if (legacy_shared.is_err() || *legacy_shared.value() != 42) return 1;
    auto legacy_unique = factory_result(std::make_unique<int>(43));
    if (legacy_unique.is_err() || *legacy_unique.value() != 43) return 2;
    if (!factory_result(std::shared_ptr<int>{}).is_err()) return 3;
    if (!factory_result(std::unique_ptr<int>{}).is_err()) return 4;

    auto modern_shared = factory_result(Result<std::shared_ptr<int>>(std::make_shared<int>(44)));
    if (modern_shared.is_err() || *modern_shared.value() != 44) return 5;
    auto modern_unique = factory_result(Result<std::unique_ptr<int>>(std::make_unique<int>(45)));
    if (modern_unique.is_err() || *modern_unique.value() != 45) return 6;
    auto failed = factory_result(kcenon::common::make_error<std::unique_ptr<int>>(
        -10, "original factory error", "dependency"));
    if (!failed.is_err() || failed.error().code != -10
        || failed.error().message != "original factory error"
        || failed.error().module != "dependency") return 7;
    return 0;
}
