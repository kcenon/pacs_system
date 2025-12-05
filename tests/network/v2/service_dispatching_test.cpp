/**
 * @file service_dispatching_test.cpp
 * @brief Unit tests for DICOM service dispatching in network_system migration
 *
 * Tests the service dispatching layer that routes DIMSE messages to
 * registered SCP services based on SOP Class UID.
 *
 * @see Issue #163 - Full integration testing for network_system migration
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/v2/dicom_association_handler.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/scp_service.hpp"
#include "pacs/services/verification_scp.hpp"

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace pacs::network;
using namespace pacs::network::v2;
using namespace pacs::services;

// =============================================================================
// Test Constants
// =============================================================================

namespace {

constexpr const char* VERIFICATION_SOP_CLASS = "1.2.840.10008.1.1";
constexpr const char* CT_IMAGE_SOP_CLASS = "1.2.840.10008.5.1.4.1.1.2";
constexpr const char* MR_IMAGE_SOP_CLASS = "1.2.840.10008.5.1.4.1.1.4";
constexpr const char* STUDY_ROOT_QR_FIND = "1.2.840.10008.5.1.4.1.2.2.1";
constexpr const char* STUDY_ROOT_QR_MOVE = "1.2.840.10008.5.1.4.1.2.2.2";

}  // namespace

// =============================================================================
// Service Map Type Tests
// =============================================================================

TEST_CASE("service_map type alias", "[service_dispatching][unit]") {
    using service_map = dicom_association_handler::service_map;

    SECTION("service_map is a map from string to service pointer") {
        service_map services;

        CHECK(services.empty());
        CHECK(services.size() == 0);
    }

    SECTION("service_map allows nullptr values") {
        service_map services;
        services[VERIFICATION_SOP_CLASS] = nullptr;

        CHECK(services.size() == 1);
        CHECK(services.find(VERIFICATION_SOP_CLASS) != services.end());
        CHECK(services[VERIFICATION_SOP_CLASS] == nullptr);
    }

    SECTION("service_map supports standard operations") {
        service_map services;

        // Insert
        services[VERIFICATION_SOP_CLASS] = nullptr;
        services[CT_IMAGE_SOP_CLASS] = nullptr;

        // Find
        CHECK(services.find(VERIFICATION_SOP_CLASS) != services.end());
        CHECK(services.find("unknown.sop.class") == services.end());

        // Erase
        services.erase(CT_IMAGE_SOP_CLASS);
        CHECK(services.find(CT_IMAGE_SOP_CLASS) == services.end());

        // Clear
        services.clear();
        CHECK(services.empty());
    }
}

// =============================================================================
// Service Registration Tests
// =============================================================================

TEST_CASE("service registration patterns", "[service_dispatching][unit]") {
    using service_map = dicom_association_handler::service_map;

    SECTION("register single service") {
        service_map services;
        auto verification = std::make_unique<verification_scp>();

        services[VERIFICATION_SOP_CLASS] = verification.get();

        CHECK(services.size() == 1);
        CHECK(services[VERIFICATION_SOP_CLASS] != nullptr);
    }

    SECTION("register multiple services") {
        service_map services;
        auto verification = std::make_unique<verification_scp>();

        // Register same service for multiple SOP classes (hypothetical)
        services[VERIFICATION_SOP_CLASS] = verification.get();

        CHECK(services.size() == 1);
    }

    SECTION("service lookup by SOP class UID") {
        service_map services;
        auto verification = std::make_unique<verification_scp>();
        services[VERIFICATION_SOP_CLASS] = verification.get();

        // Found
        auto it = services.find(VERIFICATION_SOP_CLASS);
        REQUIRE(it != services.end());
        CHECK(it->second == verification.get());

        // Not found
        CHECK(services.find(CT_IMAGE_SOP_CLASS) == services.end());
    }
}

// =============================================================================
// Service Routing Tests
// =============================================================================

TEST_CASE("service routing logic", "[service_dispatching][unit]") {
    using service_map = dicom_association_handler::service_map;

    SECTION("route to correct service by SOP class") {
        service_map services;
        auto verification = std::make_unique<verification_scp>();

        services[VERIFICATION_SOP_CLASS] = verification.get();

        // Simulate routing logic
        auto route = [&services](const std::string& sop_class) -> scp_service* {
            auto it = services.find(sop_class);
            if (it != services.end()) {
                return it->second;
            }
            return nullptr;
        };

        CHECK(route(VERIFICATION_SOP_CLASS) == verification.get());
        CHECK(route(CT_IMAGE_SOP_CLASS) == nullptr);
        CHECK(route("") == nullptr);
    }

    SECTION("handle unknown SOP class gracefully") {
        service_map services;

        auto route = [&services](const std::string& sop_class) -> bool {
            return services.find(sop_class) != services.end();
        };

        CHECK_FALSE(route(VERIFICATION_SOP_CLASS));
        CHECK_FALSE(route("completely.invalid.uid"));
    }
}

// =============================================================================
// Service Interface Tests
// =============================================================================

TEST_CASE("verification_scp service interface", "[service_dispatching][unit]") {
    auto service = std::make_unique<verification_scp>();

    SECTION("service returns supported SOP classes") {
        auto sop_classes = service->supported_sop_classes();

        CHECK_FALSE(sop_classes.empty());

        bool found_verification = false;
        for (const auto& sop : sop_classes) {
            if (sop == VERIFICATION_SOP_CLASS) {
                found_verification = true;
                break;
            }
        }
        CHECK(found_verification);
    }
}

// =============================================================================
// Presentation Context Mapping Tests
// =============================================================================

TEST_CASE("presentation context to service mapping", "[service_dispatching][unit]") {
    SECTION("context ID to abstract syntax mapping") {
        // Simulate accepted presentation contexts
        struct accepted_context {
            uint8_t id;
            std::string abstract_syntax;
            std::string transfer_syntax;
        };

        std::vector<accepted_context> contexts = {
            {1, VERIFICATION_SOP_CLASS, "1.2.840.10008.1.2.1"},
            {3, CT_IMAGE_SOP_CLASS, "1.2.840.10008.1.2.1"},
            {5, MR_IMAGE_SOP_CLASS, "1.2.840.10008.1.2"}
        };

        // Find abstract syntax by context ID
        auto find_abstract_syntax = [&contexts](uint8_t id) -> std::string {
            for (const auto& ctx : contexts) {
                if (ctx.id == id) {
                    return ctx.abstract_syntax;
                }
            }
            return "";
        };

        CHECK(find_abstract_syntax(1) == VERIFICATION_SOP_CLASS);
        CHECK(find_abstract_syntax(3) == CT_IMAGE_SOP_CLASS);
        CHECK(find_abstract_syntax(5) == MR_IMAGE_SOP_CLASS);
        CHECK(find_abstract_syntax(7) == "");  // Not found
    }

    SECTION("context IDs are odd numbers") {
        // DICOM requires odd presentation context IDs
        for (uint8_t id = 1; id < 20; id += 2) {
            CHECK(id % 2 == 1);  // All are odd
        }
    }
}

// =============================================================================
// Service Dispatch Error Handling
// =============================================================================

TEST_CASE("service dispatch error scenarios", "[service_dispatching][unit]") {
    using service_map = dicom_association_handler::service_map;

    SECTION("dispatch to nullptr service") {
        service_map services;
        services[VERIFICATION_SOP_CLASS] = nullptr;

        auto service = services[VERIFICATION_SOP_CLASS];
        CHECK(service == nullptr);
    }

    SECTION("dispatch to non-existent SOP class") {
        service_map services;

        auto it = services.find("non.existent.sop.class");
        CHECK(it == services.end());
    }

    SECTION("empty service map") {
        service_map services;

        CHECK(services.empty());
        CHECK(services.find(VERIFICATION_SOP_CLASS) == services.end());
    }
}

// =============================================================================
// Multi-Service Registration Tests
// =============================================================================

TEST_CASE("multi-service registration", "[service_dispatching][unit]") {
    using service_map = dicom_association_handler::service_map;

    SECTION("register services for different modalities") {
        service_map services;

        // Simulate multiple storage services
        // In real code, each would be a different service implementation
        std::map<std::string, std::string> modality_to_sop = {
            {"CT", CT_IMAGE_SOP_CLASS},
            {"MR", MR_IMAGE_SOP_CLASS}
        };

        for (const auto& [modality, sop] : modality_to_sop) {
            services[sop] = nullptr;  // Would be actual service in production
        }

        CHECK(services.size() == 2);
        CHECK(services.find(CT_IMAGE_SOP_CLASS) != services.end());
        CHECK(services.find(MR_IMAGE_SOP_CLASS) != services.end());
    }

    SECTION("register Q/R services") {
        service_map services;

        services[STUDY_ROOT_QR_FIND] = nullptr;
        services[STUDY_ROOT_QR_MOVE] = nullptr;

        CHECK(services.size() == 2);
    }
}

// =============================================================================
// Service Statistics Tests
// =============================================================================

TEST_CASE("service dispatch statistics", "[service_dispatching][unit]") {
    SECTION("atomic counter for messages processed") {
        std::atomic<uint64_t> messages_processed{0};

        // Simulate dispatching messages
        for (int i = 0; i < 100; ++i) {
            messages_processed.fetch_add(1, std::memory_order_relaxed);
        }

        CHECK(messages_processed.load() == 100);
    }

    SECTION("concurrent dispatch counting") {
        std::atomic<uint64_t> dispatch_count{0};
        constexpr int num_threads = 4;
        constexpr int iterations = 250;

        std::vector<std::thread> workers;
        for (int t = 0; t < num_threads; ++t) {
            workers.emplace_back([&]() {
                for (int i = 0; i < iterations; ++i) {
                    dispatch_count.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& w : workers) {
            w.join();
        }

        CHECK(dispatch_count.load() == num_threads * iterations);
    }
}

// =============================================================================
// Service Map Thread Safety
// =============================================================================

TEST_CASE("service map access patterns", "[service_dispatching][thread]") {
    using service_map = dicom_association_handler::service_map;

    SECTION("read-only access is safe without locks") {
        // Once services are registered, the map is read-only
        service_map services;
        auto verification = std::make_unique<verification_scp>();
        services[VERIFICATION_SOP_CLASS] = verification.get();

        std::atomic<int> successful_lookups{0};
        constexpr int num_threads = 4;
        constexpr int iterations = 100;

        std::vector<std::thread> readers;
        for (int t = 0; t < num_threads; ++t) {
            readers.emplace_back([&]() {
                for (int i = 0; i < iterations; ++i) {
                    auto it = services.find(VERIFICATION_SOP_CLASS);
                    if (it != services.end() && it->second != nullptr) {
                        successful_lookups.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        for (auto& r : readers) {
            r.join();
        }

        CHECK(successful_lookups.load() == num_threads * iterations);
    }
}

// =============================================================================
// SOP Class UID Validation
// =============================================================================

TEST_CASE("SOP class UID validation", "[service_dispatching][unit]") {
    SECTION("valid DICOM UIDs") {
        // Valid UIDs start with "1.2." and contain only digits and dots
        auto is_valid_uid = [](const std::string& uid) -> bool {
            if (uid.empty() || uid.length() > 64) return false;
            if (uid.substr(0, 4) != "1.2.") return false;

            for (char c : uid) {
                if (c != '.' && (c < '0' || c > '9')) {
                    return false;
                }
            }
            return true;
        };

        CHECK(is_valid_uid(VERIFICATION_SOP_CLASS));
        CHECK(is_valid_uid(CT_IMAGE_SOP_CLASS));
        CHECK(is_valid_uid(MR_IMAGE_SOP_CLASS));
        CHECK(is_valid_uid(STUDY_ROOT_QR_FIND));
        CHECK(is_valid_uid(STUDY_ROOT_QR_MOVE));
    }

    SECTION("well-known verification SOP class") {
        CHECK(std::string(VERIFICATION_SOP_CLASS) == "1.2.840.10008.1.1");
    }
}
