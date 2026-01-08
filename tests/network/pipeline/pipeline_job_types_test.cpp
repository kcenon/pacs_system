/**
 * @file pipeline_job_types_test.cpp
 * @brief Unit tests for pipeline job types and helper functions
 *
 * @see Issue #524 - Phase 7: Testing & Benchmarks
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/pipeline/pipeline_job_types.hpp"

using namespace pacs::network::pipeline;

TEST_CASE("pipeline_stage enum values", "[network][pipeline][job_types]") {
    SECTION("stage values are sequential from 0") {
        CHECK(static_cast<uint8_t>(pipeline_stage::network_receive) == 0);
        CHECK(static_cast<uint8_t>(pipeline_stage::pdu_decode) == 1);
        CHECK(static_cast<uint8_t>(pipeline_stage::dimse_process) == 2);
        CHECK(static_cast<uint8_t>(pipeline_stage::storage_query_exec) == 3);
        CHECK(static_cast<uint8_t>(pipeline_stage::response_encode) == 4);
        CHECK(static_cast<uint8_t>(pipeline_stage::network_send) == 5);
        CHECK(static_cast<uint8_t>(pipeline_stage::stage_count) == 6);
    }
}

TEST_CASE("get_stage_name returns correct names", "[network][pipeline][job_types]") {
    SECTION("network_receive stage") {
        CHECK(get_stage_name(pipeline_stage::network_receive) == "network_receive");
    }

    SECTION("pdu_decode stage") {
        CHECK(get_stage_name(pipeline_stage::pdu_decode) == "pdu_decode");
    }

    SECTION("dimse_process stage") {
        CHECK(get_stage_name(pipeline_stage::dimse_process) == "dimse_process");
    }

    SECTION("storage_query_exec stage") {
        CHECK(get_stage_name(pipeline_stage::storage_query_exec) == "storage_query_exec");
    }

    SECTION("response_encode stage") {
        CHECK(get_stage_name(pipeline_stage::response_encode) == "response_encode");
    }

    SECTION("network_send stage") {
        CHECK(get_stage_name(pipeline_stage::network_send) == "network_send");
    }

    SECTION("unknown stage returns unknown") {
        auto invalid_stage = static_cast<pipeline_stage>(255);
        CHECK(get_stage_name(invalid_stage) == "unknown");
    }
}

TEST_CASE("is_blocking_stage correctly identifies blocking stages",
          "[network][pipeline][job_types]") {
    SECTION("storage_query_exec is blocking") {
        CHECK(is_blocking_stage(pipeline_stage::storage_query_exec) == true);
    }

    SECTION("non-blocking stages") {
        CHECK(is_blocking_stage(pipeline_stage::network_receive) == false);
        CHECK(is_blocking_stage(pipeline_stage::pdu_decode) == false);
        CHECK(is_blocking_stage(pipeline_stage::dimse_process) == false);
        CHECK(is_blocking_stage(pipeline_stage::response_encode) == false);
        CHECK(is_blocking_stage(pipeline_stage::network_send) == false);
    }
}

TEST_CASE("is_network_io_stage correctly identifies network I/O stages",
          "[network][pipeline][job_types]") {
    SECTION("network stages") {
        CHECK(is_network_io_stage(pipeline_stage::network_receive) == true);
        CHECK(is_network_io_stage(pipeline_stage::network_send) == true);
    }

    SECTION("non-network stages") {
        CHECK(is_network_io_stage(pipeline_stage::pdu_decode) == false);
        CHECK(is_network_io_stage(pipeline_stage::dimse_process) == false);
        CHECK(is_network_io_stage(pipeline_stage::storage_query_exec) == false);
        CHECK(is_network_io_stage(pipeline_stage::response_encode) == false);
    }
}

TEST_CASE("job_category enum values", "[network][pipeline][job_types]") {
    SECTION("category values are sequential") {
        CHECK(static_cast<uint8_t>(job_category::echo) == 0);
        CHECK(static_cast<uint8_t>(job_category::store) == 1);
        CHECK(static_cast<uint8_t>(job_category::find) == 2);
        CHECK(static_cast<uint8_t>(job_category::get) == 3);
        CHECK(static_cast<uint8_t>(job_category::move) == 4);
        CHECK(static_cast<uint8_t>(job_category::association) == 5);
        CHECK(static_cast<uint8_t>(job_category::control) == 6);
        CHECK(static_cast<uint8_t>(job_category::other) == 7);
    }
}

TEST_CASE("get_category_name returns correct names", "[network][pipeline][job_types]") {
    CHECK(get_category_name(job_category::echo) == "echo");
    CHECK(get_category_name(job_category::store) == "store");
    CHECK(get_category_name(job_category::find) == "find");
    CHECK(get_category_name(job_category::get) == "get");
    CHECK(get_category_name(job_category::move) == "move");
    CHECK(get_category_name(job_category::association) == "association");
    CHECK(get_category_name(job_category::control) == "control");
    CHECK(get_category_name(job_category::other) == "other");

    SECTION("unknown category returns other") {
        auto invalid_category = static_cast<job_category>(255);
        CHECK(get_category_name(invalid_category) == "other");
    }
}

TEST_CASE("job_context default initialization", "[network][pipeline][job_types]") {
    job_context ctx;

    CHECK(ctx.job_id == 0);
    CHECK(ctx.session_id == 0);
    CHECK(ctx.message_id == 0);
    CHECK(ctx.stage == pipeline_stage::network_receive);
    CHECK(ctx.category == job_category::other);
    CHECK(ctx.enqueue_time_ns == 0);
    CHECK(ctx.sequence_number == 0);
    CHECK(ctx.priority == 128);
}

TEST_CASE("job_context custom initialization", "[network][pipeline][job_types]") {
    job_context ctx;
    ctx.job_id = 12345;
    ctx.session_id = 100;
    ctx.message_id = 1;
    ctx.stage = pipeline_stage::dimse_process;
    ctx.category = job_category::store;
    ctx.enqueue_time_ns = 1000000;
    ctx.sequence_number = 5;
    ctx.priority = 10;

    CHECK(ctx.job_id == 12345);
    CHECK(ctx.session_id == 100);
    CHECK(ctx.message_id == 1);
    CHECK(ctx.stage == pipeline_stage::dimse_process);
    CHECK(ctx.category == job_category::store);
    CHECK(ctx.enqueue_time_ns == 1000000);
    CHECK(ctx.sequence_number == 5);
    CHECK(ctx.priority == 10);
}

TEST_CASE("constexpr functions are compile-time evaluable", "[network][pipeline][job_types]") {
    // These should compile as constexpr
    constexpr auto name = get_stage_name(pipeline_stage::network_receive);
    constexpr auto is_blocking = is_blocking_stage(pipeline_stage::storage_query_exec);
    constexpr auto is_network = is_network_io_stage(pipeline_stage::network_send);
    constexpr auto cat_name = get_category_name(job_category::echo);

    CHECK(name == "network_receive");
    CHECK(is_blocking == true);
    CHECK(is_network == true);
    CHECK(cat_name == "echo");
}
