// BSD 3-Clause License. See LICENSE.
#include "directory_index.h"

#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>

namespace pacs::tools::directory {
namespace {
using namespace kcenon::pacs;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;
constexpr dicom_tag sequence_tag{0x0004, 0x1220};
constexpr uint32_t undefined = 0xffffffffu;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// This scanner indexes the directory's on-disk structure before the general
// dataset parser runs. It never treats matching bytes inside a value as an Item.
class scanner {
public:
    explicit scanner(std::span<const uint8_t> bytes) : bytes_(bytes) {}
    std::vector<uint32_t> run() {
        require(bytes_.size() >= 132 && bytes_.size() <= (std::numeric_limits<uint32_t>::max)(),
                "DICOMDIR size is outside the supported 32-bit offset range");
        require(std::string_view(reinterpret_cast<const char*>(bytes_.data() + 128), 4) == "DICM",
                "Missing DICOMDIR Part 10 prefix");
        position_ = 132;
        while (position_ < bytes_.size()) element(bytes_.size(), 0);
        require(found_, "Missing Directory Record Sequence");
        return offsets_;
    }
private:
    std::span<const uint8_t> bytes_;
    size_t position_ = 0;
    bool found_ = false;
    std::vector<uint32_t> offsets_;
    uint16_t u16(size_t at) const { return static_cast<uint16_t>(bytes_[at] | (bytes_[at + 1] << 8)); }
    uint32_t u32(size_t at) const { return static_cast<uint32_t>(u16(at)) | (static_cast<uint32_t>(u16(at + 2)) << 16); }
    dicom_tag tag() const { return {u16(position_), u16(position_ + 2)}; }
    void need(size_t end, size_t count) const {
        require(position_ <= end && count <= end - position_, "Truncated DICOMDIR structure");
    }
    void delimiter(size_t end, uint16_t expected) {
        need(end, 8);
        require(tag() == dicom_tag{0xfffe, expected} && u32(position_ + 4) == 0,
                "Invalid DICOMDIR delimiter");
        position_ += 8;
    }
    void element(size_t end, unsigned depth) {
        require(depth <= 32, "DICOMDIR sequence nesting limit exceeded");
        need(end, 8);
        const auto current = tag();
        require(current.group() != 0xfffe, "Unexpected DICOMDIR item or delimiter");
        const auto vr = from_string(std::string_view(
            reinterpret_cast<const char*>(bytes_.data() + position_ + 4), 2));
        require(vr.has_value(), "Invalid DICOMDIR explicit VR");
        const bool wide = has_explicit_32bit_length(*vr);
        const size_t header = wide ? 12 : 8;
        need(end, header);
        if (wide) require(u16(position_ + 6) == 0, "Invalid DICOMDIR reserved bytes");
        const uint32_t length = wide ? u32(position_ + 8) : u16(position_ + 6);
        position_ += header;
        const bool capture = depth == 0 && current == sequence_tag;
        if (capture) {
            require(!found_ && *vr == vr_type::SQ, "Invalid or duplicate Directory Record Sequence");
            found_ = true;
        }
        require(length == undefined || length % 2 == 0, "Odd DICOMDIR value length");
        if (length != undefined) need(end, length);
        if (*vr != vr_type::SQ) {
            require(length != undefined, "Unsupported undefined-length DICOMDIR value");
            position_ += length;
            return;
        }
        const size_t sequence_end = length == undefined ? end : position_ + length;
        while (position_ < sequence_end) {
            need(sequence_end, 8);
            if (tag() == dicom_tag{0xfffe, 0xe0dd}) {
                require(length == undefined, "Unexpected DICOMDIR sequence delimiter");
                delimiter(sequence_end, 0xe0dd);
                return;
            }
            require(tag() == dicom_tag{0xfffe, 0xe000}, "Directory sequence does not contain an Item");
            if (capture) offsets_.push_back(static_cast<uint32_t>(position_));
            const auto item_length = u32(position_ + 4);
            position_ += 8;
            if (item_length == undefined) {
                while (true) {
                    need(sequence_end, 8);
                    if (tag() == dicom_tag{0xfffe, 0xe00d}) {
                        delimiter(sequence_end, 0xe00d);
                        break;
                    }
                    element(sequence_end, depth + 1);
                }
            } else {
                require(item_length % 2 == 0, "Odd DICOMDIR Item length");
                need(sequence_end, item_length);
                const auto item_end = position_ + item_length;
                while (position_ < item_end) element(item_end, depth + 1);
            }
        }
        require(length != undefined, "Missing DICOMDIR sequence delimiter");
    }
};

template<typename T>
T number(const dicom_dataset& dataset, dicom_tag tag, vr_type vr) {
    auto* value = dataset.get(tag);
    require(value && value->vr() == vr && value->raw_data().size() == sizeof(T),
            "Missing or malformed DICOMDIR offset/flag");
    auto result = value->as_numeric<T>();
    require(result.is_ok(), "Invalid DICOMDIR offset/flag value");
    return result.value();
}
} // namespace

kcenon::pacs::Result<std::vector<uint32_t>> item_offsets(std::span<const uint8_t> bytes) {
    try { return scanner(bytes).run(); }
    catch (const std::runtime_error& error) {
        return kcenon::pacs::pacs_error<std::vector<uint32_t>>(
            kcenon::pacs::error_codes::invalid_sequence, error.what());
    }
}

kcenon::pacs::Result<index> inspect(std::span<const uint8_t> bytes, const dicom_file& file) {
    try {
        require(file.transfer_syntax() == transfer_syntax::explicit_vr_little_endian,
                "DICOMDIR requires Explicit VR Little Endian");
        const auto& ds = file.dataset();
        const auto& meta = file.meta_information();
        require(meta.get_string(tags::media_storage_sop_class_uid) == "1.2.840.10008.1.3.10" &&
                !meta.get_string(tags::media_storage_sop_instance_uid).empty(),
                "Invalid DICOMDIR File Meta SOP identifiers");
        require(!ds.contains(tags::sop_class_uid) && !ds.contains(tags::sop_instance_uid),
                "Basic Directory dataset must not contain SOP Class/Instance UID");
        require(ds.contains({0x0004, 0x1130}), "Missing File-set ID");
        require(number<uint16_t>(ds, {0x0004, 0x1212}, vr_type::US) == 0,
                "Inconsistent DICOMDIR File-set flag");
        auto offsets = item_offsets(bytes);
        if (offsets.is_err()) return offsets.error();
        index result;
        result.offsets = std::move(offsets.value());
        const auto* sequence = ds.get(sequence_tag);
        require(sequence && sequence->is_sequence(), "Missing Directory Record Sequence");
        const auto& records = sequence->sequence_items();
        require(records.size() == result.offsets.size(), "DICOMDIR decoded record count mismatch");
        result.children.resize(records.size());
        std::map<uint32_t, size_t> positions;
        std::vector<uint32_t> next, lower;
        for (size_t i = 0; i < records.size(); ++i) {
            positions.emplace(result.offsets[i], i);
            next.push_back(number<uint32_t>(records[i], {0x0004, 0x1400}, vr_type::UL));
            lower.push_back(number<uint32_t>(records[i], {0x0004, 0x1420}, vr_type::UL));
            require(number<uint16_t>(records[i], {0x0004, 0x1410}, vr_type::US) != 0,
                    "Inactive DICOMDIR records are unsupported");
        }
        const auto first = number<uint32_t>(ds, {0x0004, 0x1200}, vr_type::UL);
        const auto last = number<uint32_t>(ds, {0x0004, 0x1202}, vr_type::UL);
        std::vector<bool> visited(records.size(), false);
        const std::array<std::string_view, 4> types{"PATIENT", "STUDY", "SERIES", "IMAGE"};
        std::function<void(uint32_t, unsigned, std::vector<size_t>&)> walk;
        walk = [&](uint32_t offset, unsigned level, std::vector<size_t>& output) {
            while (offset != 0) {
                auto entry = positions.find(offset);
                require(entry != positions.end(), "DICOMDIR link does not target a record Item tag");
                const auto i = entry->second;
                require(!visited[i], "Cyclic or multiply-owned DICOMDIR record link");
                visited[i] = true;
                require(level < types.size() && records[i].get_string({0x0004, 0x1430}) == types[level],
                        "Unsupported or structurally inconsistent DICOMDIR record type");
                output.push_back(i);
                walk(lower[i], level + 1, result.children[i]);
                offset = next[i];
            }
        };
        walk(first, 0, result.roots);
        require((result.roots.empty() && last == 0) ||
                (!result.roots.empty() && result.offsets[result.roots.back()] == last),
                "DICOMDIR last root offset does not match the root chain");
        require(std::all_of(visited.begin(), visited.end(), [](bool value) { return value; }),
                "Unreachable DICOMDIR record");
        return result;
    } catch (const std::runtime_error& error) {
        return kcenon::pacs::pacs_error<index>(
            kcenon::pacs::error_codes::invalid_sequence, error.what());
    }
}
} // namespace pacs::tools::directory
