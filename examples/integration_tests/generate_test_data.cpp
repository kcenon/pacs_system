/**
 * @file generate_test_data.cpp
 * @brief Generate minimal DICOM test data files for binary integration tests
 *
 * Creates minimal DICOM files that can be used for testing storage and
 * query operations without requiring external test data.
 *
 * @see Issue #136 - Binary Integration Test Scripts
 *
 * Usage:
 *   generate_test_data [output_dir]
 *
 * Output files:
 *   ct_minimal.dcm  - Minimal CT image
 *   mr_minimal.dcm  - Minimal MR image
 *   xa_minimal.dcm  - Minimal XA image
 */

#include "test_fixtures.hpp"
#include "pacs/core/dicom_file.hpp"

#include <filesystem>
#include <iostream>

namespace {

using namespace pacs::integration_test;
using namespace pacs::core;

/**
 * @brief Save a dataset to a DICOM file
 */
bool save_dataset(
    const dicom_dataset& ds,
    const std::filesystem::path& path) {

    // Create DICOM file with Explicit VR Little Endian transfer syntax
    auto file = dicom_file::create(
        dicom_dataset{ds},  // Copy the dataset
        pacs::encoding::transfer_syntax::explicit_vr_little_endian);

    auto result = file.save(path);
    if (result.is_err()) {
        std::cerr << "Failed to save " << path << ": "
                  << result.error().message << "\n";
        return false;
    }

    std::cout << "  Created: " << path.filename() << " ("
              << std::filesystem::file_size(path) << " bytes)\n";
    return true;
}

/**
 * @brief Generate all test data files
 */
bool generate_all(const std::filesystem::path& output_dir) {
    std::cout << "\nGenerating test DICOM files in: " << output_dir << "\n\n";

    // Create output directory if it doesn't exist
    if (!std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    bool success = true;

    // Generate CT dataset
    std::cout << "Generating CT image...\n";
    auto ct_ds = generate_ct_dataset();
    if (!save_dataset(ct_ds, output_dir / "ct_minimal.dcm")) {
        success = false;
    }

    // Generate MR dataset
    std::cout << "Generating MR image...\n";
    auto mr_ds = generate_mr_dataset();
    if (!save_dataset(mr_ds, output_dir / "mr_minimal.dcm")) {
        success = false;
    }

    // Generate XA dataset
    std::cout << "Generating XA image...\n";
    auto xa_ds = generate_xa_dataset();
    if (!save_dataset(xa_ds, output_dir / "xa_minimal.dcm")) {
        success = false;
    }

    return success;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____  ___ ____ ___  __  __   _____         _
 |  _ \|_ _/ ___/ _ \|  \/  | |_   _|__  ___| |_
 | | | || | |  | | | | |\/| |   | |/ _ \/ __| __|
 | |_| || | |__| |_| | |  | |   | |  __/\__ \ |_
 |____/|___\____\___/|_|  |_|   |_|\___||___/\__|

        Data Generator for Integration Tests
)" << "\n";

    // Determine output directory
    std::filesystem::path output_dir;

    if (argc > 1) {
        output_dir = argv[1];
    } else {
        // Default: test_data directory relative to executable
        output_dir = std::filesystem::path(argv[0]).parent_path() / ".." / ".." /
                     "examples" / "integration_tests" / "test_data";
    }

    if (!generate_all(output_dir)) {
        std::cerr << "\nSome files failed to generate\n";
        return 1;
    }

    std::cout << "\nAll test data files generated successfully!\n";
    std::cout << "\nFiles can be used with binary integration tests:\n";
    std::cout << "  ./test_store_retrieve.sh\n";
    std::cout << "  store_scu localhost 11112 PACS_SCP " << output_dir / "ct_minimal.dcm" << "\n";

    return 0;
}
