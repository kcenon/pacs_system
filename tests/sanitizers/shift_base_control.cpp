// BSD 3-Clause License
// Copyright (c) 2026, PACS System Contributors
// See the LICENSE file in the project root for full license information.

// Deliberate UBSan controls for the OpenJPH RCT sanitizer job (#1299).
//
// Without arguments this left-shifts a negative value. That is undefined before
// C++20, so a C++14 build must report it even with the OpenJPH-only suppression
// loaded. C++20 defines the shift, so a C++20 build of this same source must not.
//
// With "overflow" it overflows a signed addition, which is undefined in every
// standard. A report here shows that a silent C++20 shift comes from the language
// rule and not from a sanitizer that stopped instrumenting.

#include <climits>
#include <cstring>

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "overflow") == 0) {
        volatile int maximum = INT_MAX;
        volatile int one = 1;
        return maximum + one == 0 ? 0 : 1;
    }
    volatile int negative = -128;
    volatile int shift = 1;
    return (negative << shift) == -256 ? 0 : 1;
}
