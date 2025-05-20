cmake_minimum_required(VERSION 3.16)

add_library(pacs_config
    config_manager.cpp
)

target_include_directories(pacs_config PUBLIC 
    ${CMAKE_SOURCE_DIR}
)

# Find nlohmann_json dependency
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(pacs_config PUBLIC 
    pacs_common
    nlohmann_json::nlohmann_json
)