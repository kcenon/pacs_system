// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file qr_index.h
 * @brief Answer Query/Retrieve requests from the index database
 *
 * The one implementation behind every SCP that serves the index, so that
 * what a key means cannot differ between tools. Implements the Hierarchical Search
 * Method (DICOM PS3.4 Section C.4.1.3.1) and the Unique Key rules for retrieval.
 */

#pragma once

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/services/query_scp.h"
#include "kcenon/pacs/services/retrieve_scp.h"

namespace kcenon::pacs::storage {
class index_database;
}

namespace kcenon::pacs::services {

/**
 * @brief Evaluate a C-FIND Identifier against the index.
 *
 * Text keys are decoded from the Identifier's character set first, so they compare
 * with the UTF-8 the index holds. Each match is one response Identifier holding what
 * the request asked for, Query/Retrieve Level, Retrieve AE Title and, when the text
 * needs it, Specific Character Set. A key the index does not hold is ignored, left out
 * of the response and reported through find_outcome::optional_keys_ignored (C.2.2.1.3).
 *
 * An Identifier without the single Unique Key value each level above the query level
 * requires is refused rather than answered as a search with no matches, because the
 * requester would read that as a true statement about the archive.
 */
[[nodiscard]] find_outcome find_in_index(storage::index_database& database, const find_request& request);

/**
 * @brief Resolve a C-MOVE or C-GET Identifier to the objects it names.
 *
 * Only Unique Keys select objects. Other keys cannot narrow or widen a retrieve, so they
 * are ignored, except that a Unique Key below the Query/Retrieve Level is refused: the
 * requester clearly meant less than the level would deliver. An indexed object whose
 * file cannot be read is returned by UID, to be reported as a failed sub-operation.
 */
[[nodiscard]] retrieve_selection select_from_index(storage::index_database& database, const retrieve_request& request);

}  // namespace kcenon::pacs::services
