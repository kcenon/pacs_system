# samples/common/CMakeLists.txt
# Common utilities library for developer samples

add_library(pacs_samples_common STATIC
    signal_handler.cpp
    test_data_generator.cpp
    console_utils.cpp
)

target_include_directories(pacs_samples_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pacs_samples_common PUBLIC
    pacs_core
    pacs_encoding
)

# Require C++20
target_compile_features(pacs_samples_common PUBLIC cxx_std_20)

# Add alias for cleaner target names
add_library(pacs::samples::common ALIAS pacs_samples_common)
