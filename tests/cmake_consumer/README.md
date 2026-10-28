# CMake consumer regression test

This directory contains a standalone CMake project that exercises the v1.0
public CMake contract from issue [#1158][1158]:

1. `find_package(pacs_system <version> REQUIRED)` succeeds against an installed
   pacs_system. The requested version is `PACS_CONSUMER_MIN_VERSION`, which
   defaults to `0.1` while the source tree reports the published 0.1.0
   package version; the change that prepares v1.0.0 (#1095) raises it to `1.0`.
2. `target_link_libraries(consumer PRIVATE pacs_system::pacs_system)`
   resolves to the canonical aggregate INTERFACE target.
3. A public `kcenon::pacs::core` header compiles and links without naming
   any individual component target.

The project is intentionally **not** added to the parent `tests/` build
because consumer tests run against an *installed* package, not the in-tree
build, and adding a top-level `add_test` would couple the main build to the
install step.

## Manual verification

Install the dependencies and pacs_system into a scratch prefix as described
in [Source install][source-install], then configure and run the consumer with
the same `CMAKE_PREFIX_PATH` (the prefix and the third-party packages):

```bash
PREFIX=/tmp/pacs_install
THIRD_PARTY=/path/to/vcpkg_installed/<triplet>

cmake -S tests/cmake_consumer -B /tmp/pacs_consumer_build \
      "-DCMAKE_PREFIX_PATH=${PREFIX};${THIRD_PARTY}"
cmake --build /tmp/pacs_consumer_build

/tmp/pacs_consumer_build/pacs_consumer
# Expected output:
# pacs_system v1.0 consumer OK (tag=10,10)
```

`scripts/check-source-install.sh` runs every step, from the dependency
installs to this consumer, and is what the `Source Install` workflow runs.
An install produced by the sibling-directory build (dependencies checked out
next to pacs_system) is not consumable this way.

If `find_package` fails with a version mismatch, the project version in the
top-level `CMakeLists.txt` is below `PACS_CONSUMER_MIN_VERSION`. If the linker complains
about `pacs_system::pacs_system`, the umbrella INTERFACE target was removed
from `cmake/install.cmake`.

[1158]: https://github.com/kcenon/pacs_system/issues/1158
[source-install]: ../../docs/PACKAGE_STATE.md#source-install
