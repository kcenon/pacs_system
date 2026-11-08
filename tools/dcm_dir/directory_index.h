// BSD 3-Clause License. See LICENSE.
#pragma once

#include <kcenon/pacs/core/dicom_file.h>
#include <cstdint>
#include <span>
#include <vector>

namespace pacs::tools::directory {
struct index {
    std::vector<uint32_t> offsets;
    std::vector<size_t> roots;
    std::vector<std::vector<size_t>> children;
};

// Absolute file offsets (including the preamble and DICM prefix), never a byte
// search: only actual Items immediately inside Directory Record Sequence count.
// Supports Explicit VR Little Endian, defined/undefined sequences and items.
kcenon::pacs::Result<std::vector<uint32_t>> item_offsets(std::span<const uint8_t> bytes);

// Bounded traversal of the supported PATIENT -> STUDY -> SERIES -> IMAGE graph.
// Rejects missing, invalid, cyclic, multiply-owned and unreachable record links.
kcenon::pacs::Result<index> inspect(std::span<const uint8_t> bytes,
                                  const kcenon::pacs::core::dicom_file& file);
} // namespace pacs::tools::directory
