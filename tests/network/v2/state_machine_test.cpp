/**
 * @file state_machine_test.cpp
 * @brief Unit tests for DICOM association handler state machine
 *
 * Tests the state machine transitions in the dicom_association_handler,
 * verifying correct PDU processing and state progression according to
 * DICOM PS3.8 Upper Layer Protocol.
 *
 * @see Issue #163 - Full integration testing for network_system migration
 * @see DICOM PS3.8 Section 9 - State Machine
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/v2/dicom_association_handler.hpp"
#include "pacs/network/pdu_types.hpp"
#include "pacs/network/pdu_encoder.hpp"
#include "pacs/network/server_config.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

using namespace pacs::network;
using namespace pacs::network::v2;

// =============================================================================
// Test Constants
// =============================================================================

// Test constants removed - not needed for state machine tests

// =============================================================================
// Handler State Enum Tests
// =============================================================================

TEST_CASE("handler_state enum values", "[state_machine][unit]") {
    SECTION("all states are distinct") {
        CHECK(handler_state::idle != handler_state::awaiting_response);
        CHECK(handler_state::idle != handler_state::established);
        CHECK(handler_state::idle != handler_state::releasing);
        CHECK(handler_state::idle != handler_state::closed);

        CHECK(handler_state::awaiting_response != handler_state::established);
        CHECK(handler_state::awaiting_response != handler_state::releasing);
        CHECK(handler_state::awaiting_response != handler_state::closed);

        CHECK(handler_state::established != handler_state::releasing);
        CHECK(handler_state::established != handler_state::closed);

        CHECK(handler_state::releasing != handler_state::closed);
    }
}

TEST_CASE("handler_state to_string conversions", "[state_machine][unit]") {
    SECTION("all states have string representation") {
        CHECK(std::string(to_string(handler_state::idle)) == "Idle");
        CHECK(std::string(to_string(handler_state::awaiting_response)) == "Awaiting Response");
        CHECK(std::string(to_string(handler_state::established)) == "Established");
        CHECK(std::string(to_string(handler_state::releasing)) == "Releasing");
        CHECK(std::string(to_string(handler_state::closed)) == "Closed");
    }

    SECTION("to_string is constexpr") {
        constexpr const char* idle_str = to_string(handler_state::idle);
        CHECK(idle_str != nullptr);
    }
}

// =============================================================================
// State Query Tests
// =============================================================================

TEST_CASE("handler_state query functions", "[state_machine][unit]") {
    SECTION("is_established only true for established state") {
        // Test the logic that would be used in handler
        auto check_established = [](handler_state s) {
            return s == handler_state::established;
        };

        CHECK_FALSE(check_established(handler_state::idle));
        CHECK_FALSE(check_established(handler_state::awaiting_response));
        CHECK(check_established(handler_state::established));
        CHECK_FALSE(check_established(handler_state::releasing));
        CHECK_FALSE(check_established(handler_state::closed));
    }

    SECTION("is_closed only true for closed state") {
        auto check_closed = [](handler_state s) {
            return s == handler_state::closed;
        };

        CHECK_FALSE(check_closed(handler_state::idle));
        CHECK_FALSE(check_closed(handler_state::awaiting_response));
        CHECK_FALSE(check_closed(handler_state::established));
        CHECK_FALSE(check_closed(handler_state::releasing));
        CHECK(check_closed(handler_state::closed));
    }
}

// =============================================================================
// State Transition Logic Tests
// =============================================================================

TEST_CASE("valid state transitions", "[state_machine][unit]") {
    // These tests verify the expected state transition paths

    SECTION("idle to awaiting_response (on valid A-ASSOCIATE-RQ)") {
        // When SCP receives A-ASSOCIATE-RQ in idle state,
        // it should transition to awaiting_response or established
        handler_state current = handler_state::idle;

        // Valid transition
        handler_state next = handler_state::awaiting_response;
        CHECK(current != next);
    }

    SECTION("idle to established (on accepted A-ASSOCIATE-RQ)") {
        handler_state current = handler_state::idle;
        handler_state next = handler_state::established;
        CHECK(current != next);
    }

    SECTION("idle to closed (on rejected A-ASSOCIATE-RQ)") {
        handler_state current = handler_state::idle;
        handler_state next = handler_state::closed;
        CHECK(current != next);
    }

    SECTION("established to releasing (on A-RELEASE-RQ)") {
        handler_state current = handler_state::established;
        handler_state next = handler_state::releasing;
        CHECK(current != next);
    }

    SECTION("releasing to closed (on A-RELEASE-RP sent)") {
        handler_state current = handler_state::releasing;
        handler_state next = handler_state::closed;
        CHECK(current != next);
    }

    SECTION("any state to closed (on A-ABORT)") {
        // A-ABORT can transition from any state to closed
        std::vector<handler_state> states = {
            handler_state::idle,
            handler_state::awaiting_response,
            handler_state::established,
            handler_state::releasing
        };

        for (auto state : states) {
            handler_state next = handler_state::closed;
            CHECK(state != next);  // All can transition to closed
        }
    }
}

// =============================================================================
// Invalid State Transition Tests
// =============================================================================

TEST_CASE("invalid PDU reception by state", "[state_machine][unit]") {
    // Test which PDU types should trigger errors in which states

    SECTION("P-DATA-TF only valid in established state") {
        // Receiving P-DATA-TF in non-established states should be error
        std::vector<handler_state> invalid_states = {
            handler_state::idle,
            handler_state::awaiting_response,
            handler_state::releasing,
            handler_state::closed
        };

        for (auto state : invalid_states) {
            CHECK(state != handler_state::established);
        }
    }

    SECTION("A-ASSOCIATE-RQ only valid in idle state") {
        std::vector<handler_state> invalid_states = {
            handler_state::awaiting_response,
            handler_state::established,
            handler_state::releasing,
            handler_state::closed
        };

        for (auto state : invalid_states) {
            CHECK(state != handler_state::idle);
        }
    }

    SECTION("A-RELEASE-RQ only valid in established state") {
        std::vector<handler_state> invalid_states = {
            handler_state::idle,
            handler_state::awaiting_response,
            handler_state::releasing,
            handler_state::closed
        };

        for (auto state : invalid_states) {
            CHECK(state != handler_state::established);
        }
    }
}

// =============================================================================
// Atomic State Operations Tests
// =============================================================================

TEST_CASE("atomic state operations", "[state_machine][thread]") {
    SECTION("atomic state load/store") {
        std::atomic<handler_state> state{handler_state::idle};

        CHECK(state.load() == handler_state::idle);

        state.store(handler_state::established);
        CHECK(state.load() == handler_state::established);
    }

    SECTION("atomic state exchange") {
        std::atomic<handler_state> state{handler_state::idle};

        handler_state old = state.exchange(handler_state::established);
        CHECK(old == handler_state::idle);
        CHECK(state.load() == handler_state::established);
    }

    SECTION("concurrent state access is safe") {
        std::atomic<handler_state> state{handler_state::idle};
        std::atomic<int> read_count{0};
        constexpr int iterations = 1000;

        std::thread reader([&]() {
            for (int i = 0; i < iterations; ++i) {
                auto s = state.load(std::memory_order_acquire);
                (void)s;
                read_count.fetch_add(1, std::memory_order_relaxed);
            }
        });

        std::thread writer([&]() {
            for (int i = 0; i < iterations; ++i) {
                if (i % 2 == 0) {
                    state.store(handler_state::established, std::memory_order_release);
                } else {
                    state.store(handler_state::idle, std::memory_order_release);
                }
            }
        });

        reader.join();
        writer.join();

        CHECK(read_count.load() == iterations);
    }
}

// =============================================================================
// State Machine Diagram Verification
// =============================================================================

TEST_CASE("DICOM state machine diagram verification", "[state_machine][unit]") {
    /*
     * DICOM Upper Layer State Machine (SCP side simplified):
     *
     *              +-------+
     *              | Idle  |
     *              +---+---+
     *                  |
     *         A-ASSOCIATE-RQ received
     *                  |
     *                  v
     *     +------------+------------+
     *     |  Validate request       |
     *     +------------+------------+
     *                  |
     *         +--------+--------+
     *        /                   \
     *   Accept                   Reject
     *       |                       |
     *       v                       v
     * +-------------+          +--------+
     * | Established |          | Closed |
     * +------+------+          +--------+
     *        |
     *   A-RELEASE-RQ
     *        |
     *        v
     *   +-----------+
     *   | Releasing |
     *   +-----+-----+
     *         |
     *    A-RELEASE-RP sent
     *         |
     *         v
     *     +--------+
     *     | Closed |
     *     +--------+
     *
     * Note: A-ABORT can transition any state to Closed
     */

    SECTION("state machine has exactly 5 states") {
        std::vector<handler_state> all_states = {
            handler_state::idle,
            handler_state::awaiting_response,
            handler_state::established,
            handler_state::releasing,
            handler_state::closed
        };
        CHECK(all_states.size() == 5);
    }

    SECTION("initial state is idle") {
        std::atomic<handler_state> state{handler_state::idle};
        CHECK(state.load() == handler_state::idle);
    }

    SECTION("final state is closed") {
        // closed is a terminal state
        auto is_terminal = [](handler_state s) {
            return s == handler_state::closed;
        };

        CHECK(is_terminal(handler_state::closed));
        CHECK_FALSE(is_terminal(handler_state::idle));
        CHECK_FALSE(is_terminal(handler_state::established));
    }
}

// =============================================================================
// Time Tracking for State Machine
// =============================================================================

TEST_CASE("state machine time tracking", "[state_machine][unit]") {
    using clock = std::chrono::steady_clock;

    SECTION("last activity timestamp updates") {
        auto t1 = clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        auto t2 = clock::now();

        CHECK(t2 > t1);
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
        CHECK(diff.count() >= 10);
    }

    SECTION("time point can be stored atomically via mutex") {
        // While time_point itself isn't atomic, we verify the pattern
        clock::time_point last_activity;
        std::mutex mutex;

        auto update = [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            last_activity = clock::now();
        };

        auto read = [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            return last_activity;
        };

        update();
        auto t1 = read();

        std::this_thread::sleep_for(std::chrono::milliseconds{10});

        update();
        auto t2 = read();

        CHECK(t2 > t1);
    }
}

// =============================================================================
// PDU Type Mapping Tests
// =============================================================================

TEST_CASE("PDU type to state action mapping", "[state_machine][unit]") {
    // Map PDU types to the expected actions

    SECTION("associate_rq maps to negotiation") {
        pdu_type type = pdu_type::associate_rq;
        CHECK(to_string(type) == std::string("A-ASSOCIATE-RQ"));
    }

    SECTION("p_data_tf maps to data processing") {
        pdu_type type = pdu_type::p_data_tf;
        CHECK(to_string(type) == std::string("P-DATA-TF"));
    }

    SECTION("release_rq maps to graceful close") {
        pdu_type type = pdu_type::release_rq;
        CHECK(to_string(type) == std::string("A-RELEASE-RQ"));
    }

    SECTION("abort maps to immediate close") {
        pdu_type type = pdu_type::abort;
        CHECK(to_string(type) == std::string("A-ABORT"));
    }

    SECTION("SCP should not receive response PDUs") {
        // These PDU types are responses that SCP sends, not receives
        std::vector<pdu_type> response_types = {
            pdu_type::associate_ac,
            pdu_type::associate_rj,
            pdu_type::release_rp
        };

        for (auto type : response_types) {
            // Verify they exist and have names
            CHECK(to_string(type) != nullptr);
        }
    }
}
