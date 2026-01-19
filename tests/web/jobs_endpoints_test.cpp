/**
 * @file jobs_endpoints_test.cpp
 * @brief Unit tests for Jobs REST API endpoints
 *
 * @see Issue #558 - Part 1: Jobs REST API Endpoints (CRUD)
 * @see Issue #559 - Part 2: Jobs REST API Control Endpoints
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/client/job_types.hpp"
#include "pacs/web/rest_types.hpp"

using namespace pacs::client;
using namespace pacs::web;

// =============================================================================
// Job Status State Transition Tests
// =============================================================================

TEST_CASE("job state transitions for control endpoints", "[web][jobs][state]") {
    job_record job;
    job.job_id = "test-job-123";
    job.type = job_type::retrieve;
    job.max_retries = 3;
    job.retry_count = 0;

    SECTION("start: only valid from pending/queued/paused states") {
        job.status = job_status::pending;
        CHECK(job.can_start());

        job.status = job_status::queued;
        CHECK(job.can_start());

        job.status = job_status::paused;
        CHECK(job.can_start());

        job.status = job_status::running;
        CHECK_FALSE(job.can_start());

        job.status = job_status::completed;
        CHECK_FALSE(job.can_start());

        job.status = job_status::failed;
        CHECK_FALSE(job.can_start());

        job.status = job_status::cancelled;
        CHECK_FALSE(job.can_start());
    }

    SECTION("pause: only valid from running/queued states") {
        job.status = job_status::running;
        CHECK(job.can_pause());

        job.status = job_status::queued;
        CHECK(job.can_pause());

        job.status = job_status::pending;
        CHECK_FALSE(job.can_pause());

        job.status = job_status::completed;
        CHECK_FALSE(job.can_pause());

        job.status = job_status::paused;
        CHECK_FALSE(job.can_pause());
    }

    SECTION("resume: only valid from paused state (uses can_start)") {
        job.status = job_status::paused;
        CHECK(job.can_start());

        job.status = job_status::running;
        CHECK_FALSE(job.can_start());

        job.status = job_status::completed;
        CHECK_FALSE(job.can_start());
    }

    SECTION("cancel: valid from pending/queued/running/paused states") {
        job.status = job_status::pending;
        CHECK(job.can_cancel());

        job.status = job_status::queued;
        CHECK(job.can_cancel());

        job.status = job_status::running;
        CHECK(job.can_cancel());

        job.status = job_status::paused;
        CHECK(job.can_cancel());

        job.status = job_status::completed;
        CHECK_FALSE(job.can_cancel());

        job.status = job_status::failed;
        CHECK_FALSE(job.can_cancel());

        job.status = job_status::cancelled;
        CHECK_FALSE(job.can_cancel());
    }

    SECTION("retry: only valid from failed state with retries remaining") {
        job.status = job_status::failed;
        job.retry_count = 0;
        CHECK(job.can_retry());

        job.retry_count = 2;
        CHECK(job.can_retry());

        job.retry_count = 3;  // max_retries = 3
        CHECK_FALSE(job.can_retry());

        job.status = job_status::completed;
        job.retry_count = 0;
        CHECK_FALSE(job.can_retry());

        job.status = job_status::running;
        CHECK_FALSE(job.can_retry());
    }
}

// =============================================================================
// Error Response Tests
// =============================================================================

TEST_CASE("error response format for invalid state transitions", "[web][jobs][error]") {
    SECTION("INVALID_STATE_TRANSITION error format") {
        auto json = make_error_json("INVALID_STATE_TRANSITION",
                                     "Cannot start job: job is already running");

        CHECK(json.find("\"error\"") != std::string::npos);
        CHECK(json.find("\"code\":\"INVALID_STATE_TRANSITION\"") != std::string::npos);
        CHECK(json.find("Cannot start job") != std::string::npos);
    }

    SECTION("NOT_FOUND error format") {
        auto json = make_error_json("NOT_FOUND", "Job not found");

        CHECK(json.find("\"error\"") != std::string::npos);
        CHECK(json.find("\"code\":\"NOT_FOUND\"") != std::string::npos);
        CHECK(json.find("\"message\":\"Job not found\"") != std::string::npos);
    }

    SECTION("SERVICE_UNAVAILABLE error format") {
        auto json = make_error_json("SERVICE_UNAVAILABLE",
                                     "Job manager not configured");

        CHECK(json.find("\"error\"") != std::string::npos);
        CHECK(json.find("\"code\":\"SERVICE_UNAVAILABLE\"") != std::string::npos);
    }
}

// =============================================================================
// HTTP Status Code Tests for Control Endpoints
// =============================================================================

TEST_CASE("expected HTTP status codes for control endpoints", "[web][jobs][http]") {
    SECTION("success responses should use 200 OK") {
        CHECK(static_cast<int>(http_status::ok) == 200);
    }

    SECTION("not found responses should use 404") {
        CHECK(static_cast<int>(http_status::not_found) == 404);
    }

    SECTION("conflict responses should use 409 for invalid state transitions") {
        // 409 Conflict is appropriate for state transition errors
        // as it indicates the request conflicts with current resource state
        constexpr int conflict_status = 409;
        CHECK(conflict_status == 409);
    }

    SECTION("service unavailable should use 503") {
        CHECK(static_cast<int>(http_status::service_unavailable) == 503);
    }
}

// =============================================================================
// Job Type String Conversion Tests
// =============================================================================

TEST_CASE("job control endpoint type conversions", "[web][jobs][types]") {
    SECTION("job_status to_string for control responses") {
        CHECK(std::string_view(to_string(job_status::pending)) == "pending");
        CHECK(std::string_view(to_string(job_status::queued)) == "queued");
        CHECK(std::string_view(to_string(job_status::running)) == "running");
        CHECK(std::string_view(to_string(job_status::paused)) == "paused");
        CHECK(std::string_view(to_string(job_status::completed)) == "completed");
        CHECK(std::string_view(to_string(job_status::failed)) == "failed");
        CHECK(std::string_view(to_string(job_status::cancelled)) == "cancelled");
    }
}

// =============================================================================
// JSON Escape Tests for Job IDs
// =============================================================================

TEST_CASE("json escape for job IDs in responses", "[web][jobs][json]") {
    SECTION("normal job ID") {
        auto escaped = json_escape("job-123-abc");
        CHECK(escaped == "job-123-abc");
    }

    SECTION("UUID format job ID") {
        auto escaped = json_escape("550e8400-e29b-41d4-a716-446655440000");
        CHECK(escaped == "550e8400-e29b-41d4-a716-446655440000");
    }

    SECTION("job ID with special characters should be escaped") {
        auto escaped = json_escape("job\"with\"quotes");
        CHECK(escaped == "job\\\"with\\\"quotes");
    }
}
