// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file instance_archive.h
 * @brief Keep one received object: its file and its index rows, or neither
 *
 * The one implementation behind every protocol that stores objects, so that an object
 * cannot be found through one and not through another because of how it arrived.
 */

#pragma once

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace kcenon::pacs::storage {

class file_storage;
class index_database;

/**
 * @brief Whether a value is a UID as PS3.5 9.1 defines it: digits and dots, at most 64 characters
 *
 * Every UID of a stored object names files, reaches URLs and comes from the network.
 */
[[nodiscard]] auto is_valid_uid(std::string_view uid) noexcept -> bool;

/**
 * @brief What became of an object offered to the archive
 */
enum class archive_status {
    stored,             ///< The file is written and the index names it
    duplicate_ignored,  ///< The SOP Instance is held already; policy says keep what is held, as a success
    duplicate_refused,  ///< The SOP Instance is held already; policy says refuse the new copy
    rejected,           ///< The object lacks what the archive needs to place it; nothing was written
    failed,             ///< Writing or indexing failed; nothing of this copy is left behind
};

/**
 * @brief The outcome of archive_instance()
 */
struct archive_result {
    archive_status status{archive_status::failed};
    std::string message;
    std::filesystem::path file_path;
};

/**
 * @brief Write an object and record it in the index.
 *
 * The object is refused before anything is written when it has no Patient ID or lacks a valid
 * UID the file path and the index are built from, because an object nothing can find is worse
 * than a refusal the sender can see. If the index cannot take a newly written object, the
 * file is removed again. Offers of the same SOP Instance are handled one at a time, across
 * protocols, and so are writes to the index: its rows are found or created in separate steps.
 *
 * @param files Where the object is written, with its duplicate policy
 * @param database The index the object is recorded in
 * @param dataset The object as received
 * @param syntax The Transfer Syntax the object is encoded in and is kept in
 */
[[nodiscard]] auto archive_instance(file_storage& files, index_database& database,
                                    const core::dicom_dataset& dataset,
                                    const encoding::transfer_syntax& syntax) -> archive_result;

}  // namespace kcenon::pacs::storage
