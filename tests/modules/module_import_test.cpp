/**
 * @file module_import_test.cpp
 * @brief C++20 module import tests for pacs_system
 *
 * Tests that all module partitions can be imported correctly and
 * that exported types are accessible.
 *
 * @note This file is only compiled when PACS_BUILD_MODULES=ON
 */

#ifdef KCENON_USE_MODULES

import kcenon.pacs;

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

TEST_CASE("Module import - core partition", "[modules][core]") {
    SECTION("dicom_tag is accessible") {
        pacs::core::dicom_tag tag(0x0010, 0x0010);
        REQUIRE(tag.group() == 0x0010);
        REQUIRE(tag.element() == 0x0010);
    }

    SECTION("dicom_element is accessible") {
        pacs::core::dicom_tag tag(0x0010, 0x0010);
        pacs::core::dicom_element elem(tag, "Test^Patient");
        REQUIRE(elem.tag() == tag);
    }

    SECTION("dicom_dataset is accessible") {
        pacs::core::dicom_dataset ds;
        REQUIRE(ds.empty());
    }
}

TEST_CASE("Module import - encoding partition", "[modules][encoding]") {
    SECTION("transfer_syntax is accessible") {
        auto ts = pacs::encoding::transfer_syntax::implicit_vr_little_endian();
        REQUIRE(ts.is_little_endian());
        REQUIRE(ts.is_implicit_vr());
    }

    SECTION("vr_type is accessible") {
        auto vr = pacs::encoding::vr_type::PN;
        REQUIRE(vr == pacs::encoding::vr_type::PN);
    }
}

TEST_CASE("Module import - network partition", "[modules][network]") {
    SECTION("presentation_context is accessible") {
        // Verify type is accessible (compile-time check)
        using pc_type = pacs::network::presentation_context;
        REQUIRE(sizeof(pc_type) > 0);
    }
}

TEST_CASE("Module import - services partition", "[modules][services]") {
    SECTION("service types are accessible") {
        // Verify types are accessible (compile-time check)
        using verification_scp = pacs::services::verification_scp;
        REQUIRE(sizeof(verification_scp) > 0);
    }
}

#ifdef PACS_BUILD_STORAGE
TEST_CASE("Module import - storage partition", "[modules][storage]") {
    SECTION("storage interface is accessible") {
        // Verify type is accessible (compile-time check)
        using storage_interface = pacs::storage::storage_interface;
        REQUIRE(sizeof(storage_interface) > 0);
    }
}
#endif

#ifdef PACS_BUILD_AI
TEST_CASE("Module import - ai partition", "[modules][ai]") {
    SECTION("ai types are accessible") {
        // Verify types are accessible (compile-time check)
        using ai_result_handler = pacs::ai::ai_result_handler;
        REQUIRE(sizeof(ai_result_handler) > 0);
    }
}
#endif

TEST_CASE("Module import - all partitions compile together", "[modules][integration]") {
    // This test verifies that all module partitions can be imported
    // together without conflicts
    SECTION("multiple partitions work together") {
        pacs::core::dicom_dataset ds;
        auto ts = pacs::encoding::transfer_syntax::implicit_vr_little_endian();
        REQUIRE(ds.empty());
        REQUIRE(ts.is_little_endian());
    }
}

#else

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Module tests skipped - modules not enabled", "[modules][skip]") {
    WARN("C++20 modules are not enabled. Build with -DPACS_BUILD_MODULES=ON to run module tests.");
    REQUIRE(true);
}

#endif  // KCENON_USE_MODULES
