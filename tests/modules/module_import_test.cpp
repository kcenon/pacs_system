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

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

import kcenon.pacs;

TEST_CASE("Module import - core partition", "[modules][core]") {
    SECTION("dicom_tag is accessible") {
        kcenon::pacs::core::dicom_tag tag(0x0010, 0x0010);
        REQUIRE(tag.group() == 0x0010);
        REQUIRE(tag.element() == 0x0010);
        REQUIRE(tag == kcenon::pacs::core::tags::patient_name);
    }

    SECTION("dicom_element is accessible") {
        kcenon::pacs::core::dicom_tag tag(0x0010, 0x0010);
        auto elem = kcenon::pacs::core::dicom_element::from_string(
            tag, kcenon::pacs::encoding::vr_type::PN, "Test^Patient");
        REQUIRE(elem.tag() == tag);
    }

    SECTION("dicom_dataset is accessible") {
        kcenon::pacs::core::dicom_dataset ds;
        REQUIRE(ds.empty());
        ds.set_string(kcenon::pacs::core::tags::patient_name,
                      kcenon::pacs::encoding::vr_type::PN, "DOE^JOHN");
        REQUIRE(ds.get_string(kcenon::pacs::core::tags::patient_name) == "DOE^JOHN");
    }
}

TEST_CASE("Module import - encoding partition", "[modules][encoding]") {
    SECTION("transfer_syntax is accessible") {
        const auto& ts = kcenon::pacs::encoding::transfer_syntax::implicit_vr_little_endian;
        REQUIRE(ts.endianness() == kcenon::pacs::encoding::byte_order::little_endian);
        REQUIRE(ts.vr_type() == kcenon::pacs::encoding::vr_encoding::implicit);
    }

    SECTION("vr_type is accessible") {
        auto vr = kcenon::pacs::encoding::vr_type::PN;
        REQUIRE(vr == kcenon::pacs::encoding::vr_type::PN);
        REQUIRE(kcenon::pacs::encoding::to_string(vr) == "PN");
    }
}

TEST_CASE("Module import - network partition", "[modules][network]") {
    SECTION("presentation context types are accessible") {
        // Verify types are accessible (compile-time check)
        using pc_type = kcenon::pacs::network::proposed_presentation_context;
        REQUIRE(sizeof(pc_type) > 0);
    }

    SECTION("DIMSE status checks are accessible") {
        REQUIRE(kcenon::pacs::network::dimse::is_success(0x0000));
        REQUIRE(kcenon::pacs::network::dimse::is_pending(0xFF00));
    }
}

TEST_CASE("Module import - services partition", "[modules][services]") {
    SECTION("service types are accessible") {
        // Verify types are accessible (compile-time check)
        using verification_scp = kcenon::pacs::services::verification_scp;
        REQUIRE(sizeof(verification_scp) > 0);
        REQUIRE(kcenon::pacs::services::verification_sop_class_uid == "1.2.840.10008.1.1");
    }

    SECTION("validation and cache types are accessible") {
        using validator = kcenon::pacs::services::validation::us_iod_validator;
        using cache = kcenon::pacs::services::cache::query_cache;
        REQUIRE(sizeof(validator) > 0);
        REQUIRE(sizeof(cache) > 0);
    }
}

#ifdef PACS_MODULES_WITH_STORAGE
TEST_CASE("Module import - storage partition", "[modules][storage]") {
    SECTION("storage interface is accessible") {
        // Verify type is accessible (compile-time check)
        using storage_interface = kcenon::pacs::storage::storage_interface;
        REQUIRE(sizeof(storage_interface) > 0);
    }
}
#endif

#ifdef PACS_MODULES_WITH_AI
TEST_CASE("Module import - ai partition", "[modules][ai]") {
    SECTION("ai types are accessible") {
        // Verify types are accessible (compile-time check)
        using ai_result_handler = kcenon::pacs::ai::ai_result_handler;
        REQUIRE(sizeof(ai_result_handler) > 0);
    }
}
#endif

TEST_CASE("Module import - all partitions compile together", "[modules][integration]") {
    // This test verifies that all module partitions can be imported
    // together without conflicts
    SECTION("multiple partitions work together") {
        kcenon::pacs::core::dicom_dataset ds;
        const auto& ts = kcenon::pacs::encoding::transfer_syntax::implicit_vr_little_endian;
        REQUIRE(ds.empty());
        REQUIRE(ts.is_valid());
    }
}

#else

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Module tests skipped - modules not enabled", "[modules][skip]") {
    WARN("C++20 modules are not enabled. Build with -DPACS_BUILD_MODULES=ON to run module tests.");
    REQUIRE(true);
}

#endif  // KCENON_USE_MODULES
