// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_sop_contract_test.cpp
 * @brief Keeps the conformance statement, the registry and the SCP's default in step
 *
 * Section 3.2 once claimed classes the SCP refused, and named UIDs that PS3.6 assigns
 * to other classes. Independent evidence for each class is in the DCMTK corpus.
 *
 * @see Issue #1318 - Cover all supported SOPs, codecs, and file formats
 */

#include <catch2/catch_test_macros.hpp>

#include "kcenon/pacs/services/sop_class_registry.h"
#include "kcenon/pacs/services/sop_classes/ct_storage.h"
#include "kcenon/pacs/services/sop_classes/ophthalmic_storage.h"
#include "kcenon/pacs/services/sop_classes/seg_storage.h"
#include "kcenon/pacs/services/sop_classes/us_storage.h"
#include "kcenon/pacs/services/sop_classes/waveform_storage.h"
#include "kcenon/pacs/services/storage_scp.h"

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef PACS_CONFORMANCE_STATEMENT_PATH
#error "PACS_CONFORMANCE_STATEMENT_PATH must name docs/DICOM_CONFORMANCE_STATEMENT.md"
#endif

using namespace kcenon::pacs::services;

namespace {

std::vector<std::string> split_row(const std::string& line) {
    std::vector<std::string> cells;
    std::stringstream row(line.substr(1));
    std::string cell;
    while (std::getline(row, cell, '|')) {
        const auto first = cell.find_first_not_of(' ');
        const auto last = cell.find_last_not_of(' ');
        cells.push_back(first == std::string::npos ? "" : cell.substr(first, last - first + 1));
    }
    return cells;
}

/// UID to name for every row of Section 3.2 of the conformance statement.
std::map<std::string, std::string> declared_storage_classes() {
    std::ifstream in(PACS_CONFORMANCE_STATEMENT_PATH);
    REQUIRE(in.good());
    std::map<std::string, std::string> rows;
    bool inside = false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("### 3.2 ", 0) == 0) {
            inside = true;
        } else if (line.rfind("### 3.3 ", 0) == 0) {
            break;
        } else if (inside && line.rfind("| ", 0) == 0) {
            const auto cells = split_row(line);
            if (cells.size() == 4 && cells[1].rfind("1.2.840.", 0) == 0) {
                CHECK(cells[2] == "Yes");
                CHECK(cells[3] == "Yes");
                rows.emplace(cells[1], cells[0]);
            }
        }
    }
    return rows;
}

}  // namespace

TEST_CASE("storage SOP contract: statement, registry and SCP default agree",
          "[services][sop][contract]") {
    const auto declared = declared_storage_classes();
    REQUIRE_FALSE(declared.empty());

    const auto& registry = sop_class_registry::instance();
    const auto registered = registry.get_all_storage_classes(true);
    const std::set<std::string> registered_set(registered.begin(), registered.end());

    for (const auto& [uid, name] : declared) {
        INFO("declared " << uid << " " << name);
        REQUIRE(registered_set.count(uid) == 1);
        CHECK(std::string(registry.get_info(uid)->name) == name);
    }
    for (const auto& uid : registered) {
        INFO("registered " << uid);
        CHECK(declared.count(uid) == 1);
    }

    const auto accepted = get_standard_storage_sop_classes();
    CHECK(std::set<std::string>(accepted.begin(), accepted.end()) == registered_set);
}

TEST_CASE("storage SOP contract: UIDs follow PS3.6", "[services][sop][contract]") {
    using namespace sop_classes;
    CHECK(legacy_converted_enhanced_ct_image_storage_uid == "1.2.840.10008.5.1.4.1.1.2.2");
    CHECK(ct_for_processing_image_storage_uid == legacy_converted_enhanced_ct_image_storage_uid);
    CHECK(us_multiframe_image_storage_uid == "1.2.840.10008.5.1.4.1.1.3.1");
    CHECK(us_multiframe_image_storage_retired_uid == "1.2.840.10008.5.1.4.1.1.3");
    CHECK(waveform_presentation_state_storage_uid == "1.2.840.10008.5.1.4.1.1.9.100.1");
    CHECK(waveform_annotation_storage_uid == "1.2.840.10008.5.1.4.1.1.88.77");
    CHECK(label_map_segmentation_storage_uid == "1.2.840.10008.5.1.4.1.1.66.7");
    CHECK(heightmap_segmentation_storage_uid == "1.2.840.10008.5.1.4.1.1.66.8");
    CHECK(wide_field_ophthalmic_photo_storage_uid == "1.2.840.10008.5.1.4.1.1.77.1.5.5");
}
