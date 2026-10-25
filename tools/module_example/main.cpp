/**
 * @file main.cpp
 * @brief C++20 module usage example for pacs_system
 * @example module_example/main.cpp
 *
 * This example demonstrates how to use pacs_system with C++20 modules.
 * Build with -DPACS_BUILD_MODULES=ON to enable module support.
 *
 * Usage:
 *   ./module_example [dicom_file]
 *
 * The program checks the values it prints and exits with 1 if one of them
 * is wrong, so CI can run it as a smoke test of the module interface.
 *
 * @note Requires CMake 3.28+ and a module-capable compiler
 *       (Clang 16+, GCC 14+, or MSVC 2022 17.4+)
 */

#ifdef KCENON_USE_MODULES

#include <iomanip>
#include <iostream>
#include <string>

// Import the pacs module (replaces the pacs_system header includes)
import kcenon.pacs;

namespace core = kcenon::pacs::core;
namespace encoding = kcenon::pacs::encoding;

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::cout << "  CHECK FAILED: " << what << "\n";
        ++failures;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << "=== PACS System C++20 Module Example ===\n";
    std::cout << "Module: " << kcenon::pacs::module_version::module_name
              << " " << kcenon::pacs::module_version::string << "\n\n";

    // Demonstrate core module usage
    std::cout << "1. Core Module Demo\n";
    std::cout << "-------------------\n";

    // Create DICOM tags using module-exported types
    core::dicom_tag patient_name_tag(0x0010, 0x0010);
    check(patient_name_tag == core::tags::patient_name,
          "dicom_tag(0x0010, 0x0010) == tags::patient_name");

    std::cout << "Patient Name Tag: (" << std::hex << std::setfill('0')
              << std::setw(4) << patient_name_tag.group() << ","
              << std::setw(4) << patient_name_tag.element() << std::dec
              << std::setfill(' ') << ")\n";

    const auto info = core::dicom_dictionary::instance().find(patient_name_tag);
    check(info.has_value() && info->keyword == "PatientName",
          "dictionary keyword for (0010,0010) is PatientName");
    if (info) {
        std::cout << "Dictionary keyword: " << info->keyword << "\n";
    }

    // Create a DICOM dataset
    core::dicom_dataset dataset;
    dataset.set_string(core::tags::patient_name, encoding::vr_type::PN,
                       "DOE^JOHN");
    dataset.set_string(core::tags::patient_id, encoding::vr_type::LO, "12345");
    check(dataset.size() == 2, "dataset holds two elements");
    check(dataset.get_string(core::tags::patient_name) == "DOE^JOHN",
          "PatientName reads back as DOE^JOHN");
    std::cout << "Dataset elements: " << dataset.size()
              << ", PatientName: " << dataset.get_string(core::tags::patient_name)
              << "\n\n";

    // Demonstrate encoding module usage
    std::cout << "2. Encoding Module Demo\n";
    std::cout << "-----------------------\n";

    // Standard transfer syntaxes are static data members
    const auto& implicit_le = encoding::transfer_syntax::implicit_vr_little_endian;
    const auto& explicit_le = encoding::transfer_syntax::explicit_vr_little_endian;

    const bool implicit_is_le =
        implicit_le.endianness() == encoding::byte_order::little_endian;
    const bool explicit_is_implicit_vr =
        explicit_le.vr_type() == encoding::vr_encoding::implicit;
    check(implicit_le.uid() == "1.2.840.10008.1.2",
          "Implicit VR Little Endian UID");
    check(implicit_is_le, "Implicit VR Little Endian is little endian");
    check(!explicit_is_implicit_vr, "Explicit VR Little Endian uses explicit VR");

    std::cout << implicit_le.name() << " (" << implicit_le.uid() << ")"
              << " - Little Endian: " << (implicit_is_le ? "Yes" : "No") << "\n";
    std::cout << explicit_le.name() << " (" << explicit_le.uid() << ")"
              << " - Implicit VR: " << (explicit_is_implicit_vr ? "Yes" : "No")
              << "\n";

    const auto found = encoding::find_transfer_syntax("1.2.840.10008.1.2.1");
    check(found.has_value() && *found == explicit_le,
          "find_transfer_syntax finds Explicit VR Little Endian");
    std::cout << "Supported transfer syntaxes: "
              << encoding::supported_transfer_syntaxes().size() << "\n";

    // If a DICOM file is provided, read and display it
    if (argc > 1) {
        std::cout << "\n3. File Operations Demo\n";
        std::cout << "-----------------------\n";

        std::string filepath = argv[1];
        std::cout << "Reading DICOM file: " << filepath << "\n";

        auto result = core::dicom_file::open(filepath);
        if (result.is_ok()) {
            std::cout << "Successfully read DICOM file\n";
            std::cout << "Dataset elements: " << result.value().dataset().size()
                      << "\n";
        } else {
            std::cout << "Failed to read DICOM file: " << result.error().message
                      << "\n";
            ++failures;
        }
    }

    if (failures != 0) {
        std::cout << "\n=== Module Example FAILED (" << failures
                  << " check(s)) ===\n";
        return 1;
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
