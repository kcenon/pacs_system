/**
 * @file job_contract_test.cpp
 * @brief What a job keeps across a restart and a retry, and that routing starts its jobs
 *
 * @see Issue #1323 - validate workflows, AI integration, and observability
 */

#include <kcenon/pacs/client/job_manager.h>
#include <kcenon/pacs/client/remote_node_manager.h>
#include <kcenon/pacs/client/routing_manager.h>
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/vr_type.h>
#include <kcenon/pacs/storage/index_database.h>
#include <kcenon/pacs/storage/job_repository.h>
#include <kcenon/pacs/storage/node_repository.h>
#include <kcenon/pacs/storage/routing_repository.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using namespace kcenon::pacs;

namespace {

/// An index database in a file of its own, so a second manager reads what the first one left.
class job_store {
public:
    job_store() : path_(unique_path()) {
        auto opened = storage::index_database::open(path_.string());
        REQUIRE(opened.is_ok());
        database_ = std::move(opened.value());
#ifdef PACS_WITH_DATABASE_SYSTEM
        auto handle = database_->db_adapter();
#else
        auto handle = database_->native_handle();
#endif
        jobs = std::make_shared<storage::job_repository>(handle);
        rules = std::make_shared<storage::routing_repository>(handle);
        node_rows = std::make_shared<storage::node_repository>(handle);
        nodes = std::make_shared<client::remote_node_manager>(node_rows, quiet_nodes());
    }

    static client::node_manager_config quiet_nodes() {
        client::node_manager_config config;
        config.auto_start_health_check = false;
        return config;
    }

    ~job_store() {
        nodes.reset();
        node_rows.reset();
        rules.reset();
        jobs.reset();
        database_.reset();
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    job_store(const job_store&) = delete;
    job_store& operator=(const job_store&) = delete;

    std::shared_ptr<storage::job_repository> jobs;
    std::shared_ptr<storage::routing_repository> rules;
    std::shared_ptr<storage::node_repository> node_rows;
    std::shared_ptr<client::remote_node_manager> nodes;

private:
    static std::filesystem::path unique_path() {
        static std::atomic<int> sequence{0};
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() /
               ("job_contract_" + std::to_string(ticks) + "_" + std::to_string(sequence++) + ".db");
    }

    std::filesystem::path path_;
    std::unique_ptr<storage::index_database> database_;
};

std::optional<client::job_status> status_of(const client::job_manager& manager, const std::string& job_id) {
    const auto job = manager.get_job(job_id);
    if (!job) {
        return std::nullopt;
    }
    return job->status;
}

}  // namespace

TEST_CASE("a restart keeps each job's state", "[job_manager][contract]") {
    job_store store;
    std::string pending;
    std::string queued;
    std::string interrupted;
    std::string failed;
    {
        // No workers: nothing runs, so each job stays where this test puts it.
        client::job_manager first(store.jobs, store.nodes);
        pending = first.create_store_job("node-b", {"1.2.826.0.1.1"});
        queued = first.create_store_job("node-b", {"1.2.826.0.1.2"});
        REQUIRE(first.start_job(queued).is_ok());
        interrupted = first.create_store_job("node-b", {"1.2.826.0.1.3"});
        failed = first.create_store_job("node-b", {"1.2.826.0.1.4"});
    }
    // What a stop that could not finish them leaves behind: one still running, part done, and one
    // failed. Each of these writes several columns at once.
    REQUIRE(store.jobs->mark_started(interrupted).is_ok());
    client::job_progress part;
    part.total_items = 3;
    part.completed_items = 2;
    part.current_item = "1.2.826.0.1.3";
    REQUIRE(store.jobs->update_progress(interrupted, part).is_ok());
    REQUIRE(store.jobs->mark_failed(failed, "the node refused it").is_ok());

    client::job_manager second(store.jobs, store.nodes);

    SECTION("a pending job still waits for a start") {
        CHECK(status_of(second, pending) == client::job_status::pending);
    }

    SECTION("queued and interrupted jobs are queued to run again") {
        CHECK(status_of(second, queued) == client::job_status::queued);
        const auto resumed = second.get_job(interrupted);
        REQUIRE(resumed.has_value());
        CHECK(resumed->status == client::job_status::queued);
        CHECK(resumed->started_at.has_value());
        CHECK(resumed->progress.total_items == 3);
        CHECK(resumed->progress.completed_items == 2);
        CHECK(resumed->progress.current_item == "1.2.826.0.1.3");
    }

    SECTION("a job that ended before the restart is readable and retryable") {
        const auto before = second.get_job(failed);
        REQUIRE(before.has_value());
        CHECK(before->status == client::job_status::failed);
        CHECK(before->error_message == "the node refused it");

        REQUIRE(second.retry_job(failed).is_ok());
        const auto after = second.get_job(failed);
        REQUIRE(after.has_value());
        CHECK(after->status == client::job_status::queued);
        CHECK(after->retry_count == 1);
    }
}

TEST_CASE("a job started twice runs once", "[job_manager][contract]") {
    job_store store;
    client::job_manager jobs(store.jobs, store.nodes);
    std::atomic<int> completions{0};
    jobs.set_completion_callback(
        [&completions](const std::string&, const client::job_record&) { ++completions; });
    // The node does not exist, so the job ends at once, before the second entry comes up.
    const auto job_id = jobs.create_store_job("no-such-node", {"1.2.826.0.1.20"});
    REQUIRE(jobs.start_job(job_id).is_ok());
    // Still queued, so a second start is accepted and queues the job again.
    REQUIRE(jobs.start_job(job_id).is_ok());
    REQUIRE(jobs.pending_jobs() == 2);

    jobs.start_workers();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (jobs.pending_jobs() > 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(jobs.pending_jobs() == 0);
    // The queue is empty once the second entry is taken; let its worker finish with it.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    jobs.stop_workers();
    CHECK(completions == 1);
    CHECK(status_of(jobs, job_id) == client::job_status::failed);
}

TEST_CASE("a remote node is stored", "[remote_node_manager][contract]") {
    job_store store;
    client::remote_node node;
    node.node_id = "node-b";
    node.name = "B";
    node.ae_title = "NODE_B";
    node.host = "127.0.0.1";
    node.port = 11112;
    const auto added = store.nodes->add_node(node);
    INFO((added.is_err() ? added.error().message : std::string{}));
    REQUIRE(added.is_ok());

    // A manager made afterwards reads the repository, as one does after a restart.
    client::remote_node_manager again(store.node_rows, job_store::quiet_nodes());
    const auto found = again.get_node("node-b");
    REQUIRE(found.has_value());
    CHECK(found->ae_title == "NODE_B");
    CHECK(found->port == 11112);
}

TEST_CASE("routing starts the forward job a rule makes", "[routing_manager][contract]") {
    job_store store;
    auto jobs = std::make_shared<client::job_manager>(store.jobs, store.nodes);
    client::routing_manager routing(store.rules, jobs);

    client::routing_rule rule;
    rule.rule_id = "ct-to-b";
    rule.name = "CT to B";
    rule.conditions.emplace_back(client::routing_field::modality, "CT");
    rule.actions.emplace_back("node-b");
    const auto added = routing.add_rule(rule);
    INFO((added.is_err() ? added.error().message : std::string{}));
    REQUIRE(added.is_ok());
    routing.enable();

    core::dicom_dataset ct;
    ct.set_string(core::tags::modality, encoding::vr_type::CS, "CT");
    ct.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, "1.2.826.0.1.9");
    routing.route(ct);

    core::dicom_dataset mr;
    mr.set_string(core::tags::modality, encoding::vr_type::CS, "MR");
    mr.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, "1.2.826.0.1.10");
    routing.route(mr);

    const auto made = jobs->list_jobs();
    REQUIRE(made.size() == 1);
    CHECK(made.front().destination_node_id == "node-b");
    CHECK(made.front().instance_uids == std::vector<std::string>{"1.2.826.0.1.9"});
    // It used to stay pending: nothing ever started a forward job, so nothing was forwarded.
    CHECK(made.front().status == client::job_status::queued);
}
