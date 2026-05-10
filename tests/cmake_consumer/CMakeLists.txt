# Minimal downstream-consumer test for pacs_system::pacs_system.
#
# This standalone CMake project exists as a regression test for issue #1158:
# v1.0 promises that downstream consumers can write
#
#     find_package(pacs_system 1.0 REQUIRED)
#     target_link_libraries(consumer PRIVATE pacs_system::pacs_system)
#
# and the build must succeed without referencing any deprecated alias.
#
# Usage (after installing pacs_system):
#
#     cmake -S tests/cmake_consumer -B /tmp/pacs_consumer_build \
#           -Dpacs_system_DIR=<prefix>/lib/cmake/pacs_system
#     cmake --build /tmp/pacs_consumer_build
#     /tmp/pacs_consumer_build/pacs_consumer
#
# The expected output is a single line: `pacs_system v1.0 consumer OK`.

cmake_minimum_required(VERSION 3.20)
project(pacs_system_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Acceptance criterion #1: find_package(pacs_system 1.0 REQUIRED) must succeed.
find_package(pacs_system 1.0 REQUIRED)

add_executable(pacs_consumer consumer.cpp)

# Acceptance criterion #2: linking the canonical aggregate target must resolve.
target_link_libraries(pacs_consumer PRIVATE pacs_system::pacs_system)
