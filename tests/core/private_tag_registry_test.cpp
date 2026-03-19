/**
 * @file private_tag_registry_test.cpp
 * @brief Unit tests for private_tag_registry class
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/private_tag_registry.hpp>

#include <vector>

using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// ============================================================================
// Registry API Tests
// ============================================================================

TEST_CASE("private_tag_registry singleton", "[core][private_tag_registry]") {
    auto& reg1 = private_tag_registry::instance();
    auto& reg2 = private_tag_registry::instance();

    CHECK(&reg1 == &reg2);
}

TEST_CASE("private_tag_registry register and find",
          "[core][private_tag_registry]") {
    auto& registry = private_tag_registry::instance();
    registry.clear();

    SECTION("register_tag succeeds for new entry") {
        bool ok = registry.register_tag(
            "TEST_VENDOR",
            {0x01, vr_type::OB, "Test Data"});

        CHECK(ok);
        CHECK(registry.size() == 1);
    }

    SECTION("register_tag returns false for duplicate") {
        registry.register_tag(
            "TEST_VENDOR",
            {0x01, vr_type::OB, "Test Data"});
        bool ok = registry.register_tag(
            "TEST_VENDOR",
            {0x01, vr_type::LO, "Different"});

        CHECK_FALSE(ok);
        // Original entry preserved
        auto def = registry.find("TEST_VENDOR", 0x01);
        REQUIRE(def.has_value());
        CHECK(def->vr == vr_type::OB);
    }

    SECTION("find returns matching definition") {
        registry.register_tag(
            "MY_VENDOR",
            {0x10, vr_type::DS, "Calibration"});

        auto def = registry.find("MY_VENDOR", 0x10);

        REQUIRE(def.has_value());
        CHECK(def->element_offset == 0x10);
        CHECK(def->vr == vr_type::DS);
        CHECK(def->name == "Calibration");
    }

    SECTION("find returns nullopt for unknown creator") {
        registry.register_tag(
            "KNOWN_VENDOR",
            {0x01, vr_type::LO, "Data"});

        CHECK_FALSE(registry.find("UNKNOWN_VENDOR", 0x01).has_value());
    }

    SECTION("find returns nullopt for unknown offset") {
        registry.register_tag(
            "MY_VENDOR",
            {0x01, vr_type::LO, "Data"});

        CHECK_FALSE(registry.find("MY_VENDOR", 0x99).has_value());
    }

    SECTION("different creators with same offset are independent") {
        registry.register_tag(
            "VENDOR_A", {0x01, vr_type::OB, "A Data"});
        registry.register_tag(
            "VENDOR_B", {0x01, vr_type::LO, "B Data"});

        auto a = registry.find("VENDOR_A", 0x01);
        auto b = registry.find("VENDOR_B", 0x01);

        REQUIRE(a.has_value());
        REQUIRE(b.has_value());
        CHECK(a->vr == vr_type::OB);
        CHECK(b->vr == vr_type::LO);
    }
}

TEST_CASE("private_tag_registry register_vendor",
          "[core][private_tag_registry]") {
    auto& registry = private_tag_registry::instance();
    registry.clear();

    SECTION("batch registers multiple tags") {
        std::vector<private_tag_definition> defs = {
            {0x01, vr_type::OB, "CSA Data Info"},
            {0x02, vr_type::OB, "CSA Data Type"},
            {0x10, vr_type::LO, "CSA Image Info"},
        };

        auto count = registry.register_vendor("SIEMENS CSA HEADER", defs);

        CHECK(count == 3);
        CHECK(registry.size() == 3);

        CHECK(registry.find("SIEMENS CSA HEADER", 0x01)->vr == vr_type::OB);
        CHECK(registry.find("SIEMENS CSA HEADER", 0x02)->vr == vr_type::OB);
        CHECK(registry.find("SIEMENS CSA HEADER", 0x10)->vr == vr_type::LO);
    }

    SECTION("skips duplicates in batch") {
        registry.register_tag("TEST", {0x01, vr_type::OB, "Existing"});

        std::vector<private_tag_definition> defs = {
            {0x01, vr_type::LO, "Duplicate"},
            {0x02, vr_type::DS, "New"},
        };

        auto count = registry.register_vendor("TEST", defs);

        CHECK(count == 1);  // Only 0x02 was new
        CHECK(registry.find("TEST", 0x01)->vr == vr_type::OB);  // Original
        CHECK(registry.find("TEST", 0x02)->vr == vr_type::DS);
    }
}

TEST_CASE("private_tag_registry clear", "[core][private_tag_registry]") {
    auto& registry = private_tag_registry::instance();
    registry.clear();

    registry.register_tag("VENDOR", {0x01, vr_type::OB, "Data"});
    CHECK(registry.size() == 1);

    registry.clear();

    CHECK(registry.size() == 0);
    CHECK_FALSE(registry.find("VENDOR", 0x01).has_value());
}

// ============================================================================
// Implicit VR LE Decoder Integration Tests
// ============================================================================

TEST_CASE("Implicit VR LE decoding with private creator VR lookup",
          "[core][private_tag_registry][implicit_vr]") {
    auto& registry = private_tag_registry::instance();
    registry.clear();

    SECTION("registered private tag gets correct VR") {
        // Register a vendor tag
        registry.register_tag("MY_APP",
                              {0x01, vr_type::DS, "My Numeric Data"});

        // Create a dataset with private elements, encode as Implicit VR LE
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI,
                      "1.2.3.4.5.6.7.8.9.10");
        ds.set_string(tags::patient_name, vr_type::PN, "TEST^PRIVATE");
        // Private Creator
        ds.set_string({0x0009, 0x0010}, vr_type::LO, "MY_APP");
        // Private data element (block 0x10, offset 0x01)
        ds.set_string({0x0009, 0x1001}, vr_type::DS, "3.14159");

        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::implicit_vr_little_endian);

        auto bytes = file.to_bytes();
        auto restored = dicom_file::from_bytes(bytes);

        REQUIRE(restored.is_ok());
        auto& decoded = restored.value().dataset();

        // Private Creator should be LO
        auto* creator_elem = decoded.get({0x0009, 0x0010});
        REQUIRE(creator_elem != nullptr);
        CHECK(creator_elem->vr() == vr_type::LO);

        // Private data element should have the registered VR (DS)
        auto* data_elem = decoded.get({0x0009, 0x1001});
        REQUIRE(data_elem != nullptr);
        CHECK(data_elem->vr() == vr_type::DS);
        CHECK(data_elem->as_string().unwrap_or("") == "3.14159");
    }

    SECTION("unregistered private tag defaults to UN") {
        registry.clear();

        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI,
                      "1.2.3.4.5.6.7.8.9.11");
        ds.set_string(tags::patient_name, vr_type::PN, "TEST^UNREG");
        ds.set_string({0x0009, 0x0010}, vr_type::LO, "UNKNOWN_VENDOR");
        ds.set_string({0x0009, 0x1001}, vr_type::LO, "some data");

        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::implicit_vr_little_endian);

        auto bytes = file.to_bytes();
        auto restored = dicom_file::from_bytes(bytes);

        REQUIRE(restored.is_ok());
        auto& decoded = restored.value().dataset();

        auto* data_elem = decoded.get({0x0009, 0x1001});
        REQUIRE(data_elem != nullptr);
        CHECK(data_elem->vr() == vr_type::UN);
        // Data is preserved (may have DICOM padding for even length)
        CHECK(data_elem->length() >= 9);
    }

    SECTION("Private Creator element is always decoded as LO") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI,
                      "1.2.3.4.5.6.7.8.9.12");
        ds.set_string({0x0009, 0x0010}, vr_type::LO, "CREATOR_CHECK");

        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::implicit_vr_little_endian);

        auto bytes = file.to_bytes();
        auto restored = dicom_file::from_bytes(bytes);

        REQUIRE(restored.is_ok());
        auto& decoded = restored.value().dataset();

        auto* creator = decoded.get({0x0009, 0x0010});
        REQUIRE(creator != nullptr);
        CHECK(creator->vr() == vr_type::LO);
        CHECK(creator->as_string().unwrap_or("") == "CREATOR_CHECK");
    }

    SECTION("multiple vendors in same group resolved correctly") {
        registry.register_tag("APP_A", {0x01, vr_type::DS, "A Data"});
        registry.register_tag("APP_B", {0x01, vr_type::IS, "B Data"});

        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI,
                      "1.2.3.4.5.6.7.8.9.13");
        ds.set_string({0x0009, 0x0010}, vr_type::LO, "APP_A");
        ds.set_string({0x0009, 0x0011}, vr_type::LO, "APP_B");
        ds.set_string({0x0009, 0x1001}, vr_type::DS, "1.23");
        ds.set_string({0x0009, 0x1101}, vr_type::IS, "42");

        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::implicit_vr_little_endian);

        auto bytes = file.to_bytes();
        auto restored = dicom_file::from_bytes(bytes);

        REQUIRE(restored.is_ok());
        auto& decoded = restored.value().dataset();

        auto* elem_a = decoded.get({0x0009, 0x1001});
        REQUIRE(elem_a != nullptr);
        CHECK(elem_a->vr() == vr_type::DS);

        auto* elem_b = decoded.get({0x0009, 0x1101});
        REQUIRE(elem_b != nullptr);
        CHECK(elem_b->vr() == vr_type::IS);
    }

    // Clean up registry after all tests
    registry.clear();
}
