cmake_minimum_required(VERSION 3.16)

add_executable(config_sample
    config_sample.cpp
)

target_link_libraries(config_sample 
    pacs_common
    pacs_config
)

target_include_directories(config_sample PRIVATE
    ${CMAKE_SOURCE_DIR}
)