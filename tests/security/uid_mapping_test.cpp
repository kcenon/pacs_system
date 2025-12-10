/**
 * @file uid_mapping_test.cpp
 * @brief Unit tests for UID mapping functionality
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/security/uid_mapping.hpp"

#include <thread>
#include <vector>

using namespace pacs::security;

TEST_CASE("UidMapping: Basic Operations", "[security][anonymization]") {
    uid_mapping mapping;

    SECTION("Initial state is empty") {
        REQUIRE(mapping.empty());
        REQUIRE(mapping.size() == 0);
    }

    SECTION("get_or_create generates new mapping") {
        const std::string original = "1.2.3.4.5.6.7.8.9";
        auto result = mapping.get_or_create(original);

        REQUIRE(result.is_ok());
        REQUIRE_FALSE(result.value().empty());
        REQUIRE(result.value() != original);
        REQUIRE(mapping.size() == 1);
        REQUIRE_FALSE(mapping.empty());
    }

    SECTION("get_or_create returns same mapping for same UID") {
        const std::string original = "1.2.3.4.5.6.7.8.9";

        auto result1 = mapping.get_or_create(original);
        auto result2 = mapping.get_or_create(original);

        REQUIRE(result1.is_ok());
        REQUIRE(result2.is_ok());
        REQUIRE(result1.value() == result2.value());
        REQUIRE(mapping.size() == 1);
    }

    SECTION("Different UIDs get different mappings") {
        const std::string uid1 = "1.2.3.4.5.6.7.8.9";
        const std::string uid2 = "1.2.3.4.5.6.7.8.10";

        auto result1 = mapping.get_or_create(uid1);
        auto result2 = mapping.get_or_create(uid2);

        REQUIRE(result1.is_ok());
        REQUIRE(result2.is_ok());
        REQUIRE(result1.value() != result2.value());
        REQUIRE(mapping.size() == 2);
    }
}

TEST_CASE("UidMapping: Lookup Operations", "[security][anonymization]") {
    uid_mapping mapping;
    const std::string original = "1.2.3.4.5.6.7.8.9";
    auto anon_result = mapping.get_or_create(original);
    REQUIRE(anon_result.is_ok());
    auto anonymized = anon_result.value();

    SECTION("get_anonymized returns mapping") {
        auto result = mapping.get_anonymized(original);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == anonymized);
    }

    SECTION("get_anonymized returns nullopt for unknown UID") {
        auto result = mapping.get_anonymized("unknown.uid");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("get_original performs reverse lookup") {
        auto result = mapping.get_original(anonymized);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == original);
    }

    SECTION("get_original returns nullopt for unknown anonymized UID") {
        auto result = mapping.get_original("unknown.anon.uid");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("has_mapping returns correct status") {
        REQUIRE(mapping.has_mapping(original));
        REQUIRE_FALSE(mapping.has_mapping("unknown.uid"));
    }
}

TEST_CASE("UidMapping: Manual Mapping", "[security][anonymization]") {
    uid_mapping mapping;

    SECTION("add_mapping adds new mapping") {
        auto result = mapping.add_mapping("original.uid", "anon.uid");
        REQUIRE(result.is_ok());
        REQUIRE(mapping.has_mapping("original.uid"));
        REQUIRE(mapping.get_anonymized("original.uid").value() == "anon.uid");
    }

    SECTION("add_mapping fails for conflicting mapping") {
        mapping.add_mapping("original.uid", "anon.uid.1");
        auto result = mapping.add_mapping("original.uid", "anon.uid.2");
        REQUIRE_FALSE(result.is_ok());
    }

    SECTION("add_mapping succeeds for same mapping") {
        mapping.add_mapping("original.uid", "anon.uid");
        auto result = mapping.add_mapping("original.uid", "anon.uid");
        REQUIRE(result.is_ok());
    }
}

TEST_CASE("UidMapping: Clear and Remove", "[security][anonymization]") {
    uid_mapping mapping;
    mapping.get_or_create("uid.1");
    mapping.get_or_create("uid.2");
    mapping.get_or_create("uid.3");

    SECTION("remove deletes specific mapping") {
        REQUIRE(mapping.size() == 3);
        REQUIRE(mapping.remove("uid.2"));
        REQUIRE(mapping.size() == 2);
        REQUIRE_FALSE(mapping.has_mapping("uid.2"));
        REQUIRE(mapping.has_mapping("uid.1"));
        REQUIRE(mapping.has_mapping("uid.3"));
    }

    SECTION("remove returns false for unknown UID") {
        REQUIRE_FALSE(mapping.remove("unknown.uid"));
        REQUIRE(mapping.size() == 3);
    }

    SECTION("clear removes all mappings") {
        mapping.clear();
        REQUIRE(mapping.empty());
        REQUIRE(mapping.size() == 0);
    }
}

TEST_CASE("UidMapping: UID Root Configuration", "[security][anonymization]") {
    SECTION("Default UID root is used") {
        uid_mapping mapping;
        auto uid = mapping.generate_uid();
        REQUIRE(uid.find("1.2.826.0.1.3680043.8.498.1") == 0);
    }

    SECTION("Custom UID root is used") {
        uid_mapping mapping("1.2.3.4.5");
        auto uid = mapping.generate_uid();
        REQUIRE(uid.find("1.2.3.4.5") == 0);
    }

    SECTION("set_uid_root changes the root") {
        uid_mapping mapping;
        mapping.set_uid_root("9.8.7.6.5");
        REQUIRE(mapping.get_uid_root() == "9.8.7.6.5");
        auto uid = mapping.generate_uid();
        REQUIRE(uid.find("9.8.7.6.5") == 0);
    }
}

TEST_CASE("UidMapping: Merge Operation", "[security][anonymization]") {
    uid_mapping mapping1;
    uid_mapping mapping2;

    mapping1.add_mapping("uid.1", "anon.1");
    mapping1.add_mapping("uid.2", "anon.2");

    mapping2.add_mapping("uid.3", "anon.3");
    mapping2.add_mapping("uid.4", "anon.4");

    SECTION("merge adds non-conflicting mappings") {
        auto added = mapping1.merge(mapping2);
        REQUIRE(added == 2);
        REQUIRE(mapping1.size() == 4);
        REQUIRE(mapping1.has_mapping("uid.3"));
        REQUIRE(mapping1.has_mapping("uid.4"));
    }

    SECTION("merge skips conflicting mappings") {
        mapping2.add_mapping("uid.1", "different.anon");
        auto added = mapping1.merge(mapping2);
        REQUIRE(added == 2);  // uid.3 and uid.4 added
        REQUIRE(mapping1.get_anonymized("uid.1").value() == "anon.1");  // Original preserved
    }
}

TEST_CASE("UidMapping: Copy and Move", "[security][anonymization]") {
    uid_mapping original;
    original.add_mapping("uid.1", "anon.1");
    original.add_mapping("uid.2", "anon.2");

    SECTION("Copy constructor creates independent copy") {
        uid_mapping copy(original);
        REQUIRE(copy.size() == 2);
        REQUIRE(copy.get_anonymized("uid.1").value() == "anon.1");

        // Modifying copy doesn't affect original
        copy.add_mapping("uid.3", "anon.3");
        REQUIRE(copy.size() == 3);
        REQUIRE(original.size() == 2);
    }

    SECTION("Move constructor transfers ownership") {
        uid_mapping moved(std::move(original));
        REQUIRE(moved.size() == 2);
        REQUIRE(moved.get_anonymized("uid.1").value() == "anon.1");
    }

    SECTION("Copy assignment creates independent copy") {
        uid_mapping copy;
        copy = original;
        REQUIRE(copy.size() == 2);
        copy.clear();
        REQUIRE(original.size() == 2);
    }
}

TEST_CASE("UidMapping: JSON Export", "[security][anonymization]") {
    uid_mapping mapping;
    mapping.add_mapping("uid.1", "anon.1");
    mapping.add_mapping("uid.2", "anon.2");

    auto json = mapping.to_json();

    REQUIRE_FALSE(json.empty());
    REQUIRE(json.find("uid_root") != std::string::npos);
    REQUIRE(json.find("mappings") != std::string::npos);
    REQUIRE(json.find("uid.1") != std::string::npos);
    REQUIRE(json.find("anon.1") != std::string::npos);
}

TEST_CASE("UidMapping: Thread Safety", "[security][anonymization]") {
    uid_mapping mapping;
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&mapping, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string uid = "uid." + std::to_string(t) + "." + std::to_string(i);
                auto result = mapping.get_or_create(uid);
                REQUIRE(result.is_ok());

                // Verify consistency
                auto lookup = mapping.get_anonymized(uid);
                REQUIRE(lookup.has_value());
                REQUIRE(lookup.value() == result.value());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    REQUIRE(mapping.size() == num_threads * ops_per_thread);
}
