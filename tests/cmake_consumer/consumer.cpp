// Minimal downstream-consumer source for the pacs_system v1.0 CMake export
// regression test. Includes a public pacs_core header so the test exercises
// both the include search path published by pacs_system::pacs_system and the
// link line for at least one component of the aggregate target.

#include <kcenon/pacs/core/dicom_tag.h>

#include <iostream>

int main() {
    // Constructing a DICOM tag exercises one of the simplest pacs_core
    // entry points without dragging in any optional component. If the
    // include path or link order from pacs_system::pacs_system is wrong,
    // this translation unit will fail to compile or link.
    kcenon::pacs::core::dicom_tag tag(0x0010, 0x0010);
    std::cout << "pacs_system v1.0 consumer OK ("
              << "tag=" << std::hex << tag.group()
              << "," << tag.element() << ")"
              << std::endl;
    return 0;
}
