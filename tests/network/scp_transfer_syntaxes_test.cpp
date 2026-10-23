/**
 * @file scp_transfer_syntaxes_test.cpp
 * @brief Checks the SCP transfer syntax list against the registry, dicom_server
 *        negotiation, and Section 4 of the DICOM Conformance Statement
 *
 * @see Issue #1275 - Transfer syntax support flags and the Conformance Statement
 */

#include <catch2/catch_test_macros.hpp>

#include "kcenon/pacs/encoding/compression/codec_factory.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/detail/scp_transfer_syntaxes.h"
#include "kcenon/pacs/network/dicom_server.h"
#include "kcenon/pacs/network/server_config.h"
#include "kcenon/pacs/services/verification_scp.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#ifndef PACS_CONFORMANCE_STATEMENT_PATH
#error "PACS_CONFORMANCE_STATEMENT_PATH must name docs/DICOM_CONFORMANCE_STATEMENT.md"
#endif

using namespace kcenon::pacs::network;
using kcenon::pacs::encoding::all_transfer_syntaxes;
using kcenon::pacs::encoding::find_transfer_syntax;
using kcenon::pacs::encoding::compression::codec_factory;

namespace {

/// One row of a Section 4 transfer syntax table.
struct section_4_row {
    std::string negotiation;  ///< "PACS_SCP negotiation" column
    std::string support;      ///< Last column (Part 10 files or pixel data)
};

std::string trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t");
    return std::string(text.substr(first, last - first + 1));
}

/// Splits "| a | b |" into {"a", "b"}.
std::vector<std::string> split_row(std::string_view line) {
    std::vector<std::string> cells;
    line.remove_prefix(1);
    while (!line.empty()) {
        const auto bar = line.find('|');
        if (bar == std::string_view::npos) {
            break;
        }
        cells.push_back(trim(line.substr(0, bar)));
        line.remove_prefix(bar + 1);
    }
    return cells;
}

/// Reads the tables of Section 4 into UID -> row.
std::map<std::string, section_4_row> read_section_4() {
    std::ifstream in(PACS_CONFORMANCE_STATEMENT_PATH);
    REQUIRE(in.is_open());

    std::map<std::string, section_4_row> rows;
    std::vector<std::string> header;
    bool in_section = false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("## ", 0) == 0) {
            in_section = line.rfind("## 4.", 0) == 0;
            header.clear();
            continue;
        }
        if (!in_section) {
            continue;
        }
        if (line.rfind("|", 0) != 0) {
            header.clear();
            continue;
        }

        auto cells = split_row(line);
        if (header.empty()) {
            header = cells;
            continue;
        }
        if (!cells.empty() && cells[0].rfind("---", 0) == 0) {
            continue;  // Separator row
        }
        REQUIRE(cells.size() == header.size());

        std::map<std::string, std::string> by_column;
        for (size_t i = 0; i < cells.size(); ++i) {
            by_column[header[i]] = cells[i];
        }
        REQUIRE(by_column.count("UID") == 1);
        REQUIRE(by_column.count("PACS_SCP negotiation") == 1);

        const auto& uid = by_column["UID"];
        INFO("UID " << uid);
        CHECK(rows.emplace(uid, section_4_row{by_column["PACS_SCP negotiation"],
                                              cells.back()})
                  .second);
    }
    return rows;
}

std::set<std::string> scp_uids() {
    return {detail::scp_transfer_syntaxes.begin(),
            detail::scp_transfer_syntaxes.end()};
}

}  // namespace

TEST_CASE("SCP transfer syntax list uses supported registry entries",
          "[network][negotiation][transfer_syntax]") {
    const auto uids = scp_uids();
    CHECK(uids.size() == detail::scp_transfer_syntaxes.size());  // No duplicates

    // PS3.5 Section 10.1: every SCP accepts the default transfer syntax
    CHECK(uids.count("1.2.840.10008.1.2") == 1);

    for (const auto uid : detail::scp_transfer_syntaxes) {
        INFO("UID " << uid);
        auto ts = find_transfer_syntax(uid);
        REQUIRE(ts.has_value());
        CHECK(ts->is_supported());
        // dimse_message carries datasets only in the little endian,
        // non-deflated syntaxes
        CHECK(ts->endianness() == kcenon::pacs::encoding::byte_order::little_endian);
        CHECK_FALSE(ts->is_deflated());
    }

    const auto list = detail::scp_transfer_syntax_list();
    CHECK(std::vector<std::string>(detail::scp_transfer_syntaxes.begin(),
                                   detail::scp_transfer_syntaxes.end()) == list);
}

TEST_CASE("dicom_server negotiates exactly the SCP transfer syntax list",
          "[dicom_server][negotiation][transfer_syntax]") {
    constexpr uint16_t test_port = 11146;
    constexpr const char* verification_sop_class = "1.2.840.10008.1.1";

    server_config config;
    config.ae_title = "TS_SCP";
    config.port = test_port;

    dicom_server server{config};
    server.register_service(std::make_shared<kcenon::pacs::services::verification_scp>());
    REQUIRE(server.start().is_ok());

    // One presentation context per registered transfer syntax
    association_config scu_config;
    scu_config.calling_ae_title = "TS_SCU";
    scu_config.called_ae_title = "TS_SCP";
    uint8_t context_id = 1;
    for (const auto& ts : all_transfer_syntaxes()) {
        scu_config.proposed_contexts.push_back(
            {context_id, verification_sop_class, {std::string(ts.uid())}});
        context_id = static_cast<uint8_t>(context_id + 2);
    }

    auto connect_result = association::connect("localhost", test_port, scu_config);
    REQUIRE(connect_result.is_ok());
    auto& assoc = connect_result.value();

    std::set<std::string> accepted;
    for (const auto& context : assoc.accepted_contexts()) {
        if (context.is_accepted()) {
            accepted.insert(context.transfer_syntax);
        }
    }
    CHECK(assoc.accepted_contexts().size() == scu_config.proposed_contexts.size());
    CHECK(accepted == scp_uids());

    CHECK(assoc.release().is_ok());
    server.stop();
}

TEST_CASE("Conformance Statement Section 4 matches the SCP list and the registry",
          "[network][negotiation][transfer_syntax][conformance]") {
    const auto rows = read_section_4();
    const auto negotiated = scp_uids();

    // Every registered transfer syntax has exactly one row
    CHECK(rows.size() == all_transfer_syntaxes().size());

    for (const auto& ts : all_transfer_syntaxes()) {
        const std::string uid(ts.uid());
        INFO(ts.name() << " (" << uid << ")");

        auto it = rows.find(uid);
        REQUIRE(it != rows.end());
        const auto& row = it->second;

        // The negotiated part: the column must match the shared SCP list
        if (negotiated.count(uid) == 1) {
            CHECK(row.negotiation == "Accepted");
        } else {
            CHECK(row.negotiation == "Not accepted");
        }

        // The encoding part: "Not supported" or "Not implemented" exactly when
        // the registry flag is false, and "Built in" only for codecs that need
        // no optional library
        const bool marked_unsupported = row.support.rfind("Not ", 0) == 0;
        CHECK(marked_unsupported == !ts.is_supported());
        if (row.support == "Built in") {
            CHECK(codec_factory::is_available(ts));
        }
    }
}
