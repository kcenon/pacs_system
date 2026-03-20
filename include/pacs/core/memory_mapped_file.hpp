// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file memory_mapped_file.hpp
 * @brief RAII wrapper for memory-mapped file I/O
 *
 * Provides platform-independent memory-mapped file access using
 * mmap (POSIX) or CreateFileMapping (Windows).
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "result.hpp"

#include <cstdint>
#include <filesystem>
#include <span>

namespace kcenon::pacs::core {

/**
 * @brief RAII memory-mapped file for read-only access
 *
 * Maps a file into the process address space for zero-copy reading.
 * Move-only; unmaps automatically on destruction.
 *
 * @example
 * @code
 * auto result = memory_mapped_file::open("image.dcm");
 * if (result.is_ok()) {
 *     auto& mmf = result.value();
 *     auto span = mmf.as_span();
 *     // Use span data directly without copying
 * }
 * @endcode
 */
class memory_mapped_file {
public:
    /**
     * @brief Open and memory-map a file for reading
     * @param path Path to the file
     * @return Result containing the mapped file or an error
     */
    [[nodiscard]] static auto open(const std::filesystem::path& path)
        -> kcenon::pacs::Result<memory_mapped_file>;

    /**
     * @brief Get pointer to the mapped data
     * @return Pointer to the beginning of the mapped region
     */
    [[nodiscard]] auto data() const noexcept -> const std::uint8_t*;

    /**
     * @brief Get the size of the mapped file
     * @return File size in bytes
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    /**
     * @brief Get a span over the mapped data
     * @return Read-only span of the entire mapped region
     */
    [[nodiscard]] auto as_span() const noexcept -> std::span<const std::uint8_t>;

    // Move-only
    memory_mapped_file(const memory_mapped_file&) = delete;
    auto operator=(const memory_mapped_file&) -> memory_mapped_file& = delete;

    memory_mapped_file(memory_mapped_file&& other) noexcept;
    auto operator=(memory_mapped_file&& other) noexcept -> memory_mapped_file&;

    ~memory_mapped_file();

private:
    memory_mapped_file() = default;

    void unmap() noexcept;

    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;

#if defined(_WIN32)
    void* file_handle_ = nullptr;
    void* mapping_handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};

}  // namespace kcenon::pacs::core
