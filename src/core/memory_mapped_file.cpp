// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file memory_mapped_file.cpp
 * @brief Implementation of RAII memory-mapped file I/O
 */

#include "kcenon/pacs/core/memory_mapped_file.h"

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

namespace kcenon::pacs::core {

auto memory_mapped_file::open(const std::filesystem::path& path)
    -> kcenon::pacs::Result<memory_mapped_file> {

    memory_mapped_file result;

#if defined(_WIN32)
    // Open the file
    result.file_handle_ = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (result.file_handle_ == INVALID_HANDLE_VALUE) {
        result.file_handle_ = nullptr;
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::file_not_found,
            "Failed to open file: " + path.string());
    }

    // Get file size
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(result.file_handle_, &file_size)) {
        CloseHandle(result.file_handle_);
        result.file_handle_ = nullptr;
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::file_read_error,
            "Failed to get file size: " + path.string());
    }

    result.size_ = static_cast<std::size_t>(file_size.QuadPart);

    if (result.size_ == 0) {
        CloseHandle(result.file_handle_);
        result.file_handle_ = nullptr;
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::invalid_dicom_file,
            "File is empty: " + path.string());
    }

    // Create file mapping
    result.mapping_handle_ = CreateFileMappingW(
        result.file_handle_,
        nullptr,
        PAGE_READONLY,
        0, 0,
        nullptr);

    if (result.mapping_handle_ == nullptr) {
        CloseHandle(result.file_handle_);
        result.file_handle_ = nullptr;
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::file_read_error,
            "Failed to create file mapping: " + path.string());
    }

    // Map view of file
    auto* mapped = MapViewOfFile(
        result.mapping_handle_,
        FILE_MAP_READ,
        0, 0,
        0);

    if (mapped == nullptr) {
        CloseHandle(result.mapping_handle_);
        CloseHandle(result.file_handle_);
        result.mapping_handle_ = nullptr;
        result.file_handle_ = nullptr;
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::file_read_error,
            "Failed to map file into memory: " + path.string());
    }

    result.data_ = static_cast<const std::uint8_t*>(mapped);

#else  // POSIX
    // Open the file
    result.fd_ = ::open(path.c_str(), O_RDONLY);
    if (result.fd_ < 0) {
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::file_not_found,
            "Failed to open file: " + path.string());
    }

    // Get file size
    struct stat st {};
    if (::fstat(result.fd_, &st) != 0) {
        ::close(result.fd_);
        result.fd_ = -1;
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::file_read_error,
            "Failed to stat file: " + path.string());
    }

    result.size_ = static_cast<std::size_t>(st.st_size);

    if (result.size_ == 0) {
        ::close(result.fd_);
        result.fd_ = -1;
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::invalid_dicom_file,
            "File is empty: " + path.string());
    }

    // Memory-map the file
    auto* mapped = ::mmap(nullptr, result.size_, PROT_READ, MAP_PRIVATE,
                          result.fd_, 0);
    if (mapped == MAP_FAILED) {
        ::close(result.fd_);
        result.fd_ = -1;
        return kcenon::pacs::pacs_error<memory_mapped_file>(
            kcenon::pacs::error_codes::file_read_error,
            "Failed to mmap file: " + path.string());
    }

    result.data_ = static_cast<const std::uint8_t*>(mapped);
#endif

    return kcenon::pacs::Result<memory_mapped_file>::ok(std::move(result));
}

auto memory_mapped_file::data() const noexcept -> const std::uint8_t* {
    return data_;
}

auto memory_mapped_file::size() const noexcept -> std::size_t {
    return size_;
}

auto memory_mapped_file::as_span() const noexcept -> std::span<const std::uint8_t> {
    return {data_, size_};
}

memory_mapped_file::memory_mapped_file(memory_mapped_file&& other) noexcept
    : data_{other.data_}
    , size_{other.size_}
#if defined(_WIN32)
    , file_handle_{other.file_handle_}
    , mapping_handle_{other.mapping_handle_}
#else
    , fd_{other.fd_}
#endif
{
    other.data_ = nullptr;
    other.size_ = 0;
#if defined(_WIN32)
    other.file_handle_ = nullptr;
    other.mapping_handle_ = nullptr;
#else
    other.fd_ = -1;
#endif
}

auto memory_mapped_file::operator=(memory_mapped_file&& other) noexcept
    -> memory_mapped_file& {
    if (this != &other) {
        unmap();

        data_ = other.data_;
        size_ = other.size_;
#if defined(_WIN32)
        file_handle_ = other.file_handle_;
        mapping_handle_ = other.mapping_handle_;
        other.file_handle_ = nullptr;
        other.mapping_handle_ = nullptr;
#else
        fd_ = other.fd_;
        other.fd_ = -1;
#endif
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

memory_mapped_file::~memory_mapped_file() {
    unmap();
}

void memory_mapped_file::unmap() noexcept {
    if (data_ == nullptr) {
        return;
    }

#if defined(_WIN32)
    UnmapViewOfFile(data_);
    if (mapping_handle_ != nullptr) {
        CloseHandle(mapping_handle_);
    }
    if (file_handle_ != nullptr) {
        CloseHandle(file_handle_);
    }
    mapping_handle_ = nullptr;
    file_handle_ = nullptr;
#else
    ::munmap(const_cast<std::uint8_t*>(data_), size_);
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = -1;
#endif

    data_ = nullptr;
    size_ = 0;
}

}  // namespace kcenon::pacs::core
