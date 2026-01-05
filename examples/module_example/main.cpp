/**
 * @file main.cpp
 * @brief C++20 module usage example for pacs_system
 *
 * This example demonstrates how to use pacs_system with C++20 modules.
 * Build with -DPACS_BUILD_MODULES=ON to enable module support.
 *
 * Usage:
 *   ./module_example [dicom_file]
 *
 * @note Requires CMake 3.28+ and a module-capable compiler
 *       (Clang 16+, GCC 14+, or MSVC 2022 17.4+)
 */

#ifdef KCENON_USE_MODULES

// Import the pacs module (replaces all header includes)
import kcenon.pacs;

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "=== PACS System C++20 Module Example ===\n\n";

    // Demonstrate core module usage
    std::cout << "1. Core Module Demo\n";
    std::cout << "-------------------\n";

    // Create DICOM tags using module-exported types
    pacs::core::dicom_tag patient_name_tag(0x0010, 0x0010);
    pacs::core::dicom_tag patient_id_tag(0x0010, 0x0020);
    pacs::core::dicom_tag study_uid_tag(0x0020, 0x000D);

    std::cout << "Patient Name Tag: (" << std::hex
              << patient_name_tag.group() << ","
              << patient_name_tag.element() << ")\n";

    // Create a DICOM dataset
    pacs::core::dicom_dataset dataset;
    std::cout << "Created empty dataset (size: " << std::dec
              << dataset.size() << " elements)\n\n";

    // Demonstrate encoding module usage
    std::cout << "2. Encoding Module Demo\n";
    std::cout << "-----------------------\n";

    auto implicit_le = pacs::encoding::transfer_syntax::implicit_vr_little_endian();
    auto explicit_le = pacs::encoding::transfer_syntax::explicit_vr_little_endian();

    std::cout << "Implicit VR LE - Little Endian: "
              << (implicit_le.is_little_endian() ? "Yes" : "No") << "\n";
    std::cout << "Explicit VR LE - Implicit VR: "
              << (explicit_le.is_implicit_vr() ? "Yes" : "No") << "\n\n";

    // If a DICOM file is provided, read and display it
    if (argc > 1) {
        std::cout << "3. File Operations Demo\n";
        std::cout << "-----------------------\n";

        std::string filepath = argv[1];
        std::cout << "Reading DICOM file: " << filepath << "\n";

        pacs::core::dicom_file file;
        auto result = file.read(filepath);

        if (result) {
            std::cout << "Successfully read DICOM file\n";
            std::cout << "Dataset elements: " << file.dataset().size() << "\n";
        } else {
            std::cout << "Failed to read DICOM file\n";
        }
    }

    std::cout << "\n=== Module Example Complete ===\n";
    return 0;
}

#else

#include <iostream>

int main() {
    std::cout << "C++20 modules are not enabled.\n";
    std::cout << "Build with -DPACS_BUILD_MODULES=ON to use this example.\n";
    std::cout << "\nExample:\n";
    std::cout << "  cmake -DPACS_BUILD_MODULES=ON -DPACS_BUILD_EXAMPLES=ON ..\n";
    std::cout << "  cmake --build .\n";
    return 1;
}

#endif  // KCENON_USE_MODULES
