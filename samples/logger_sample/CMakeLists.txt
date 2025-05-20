cmake_minimum_required(VERSION 3.16)

add_executable(logger_sample
    logger_sample.cpp
)

target_link_libraries(logger_sample 
    pacs_common
    pacs_logger
    pacs_config
)

target_include_directories(logger_sample PRIVATE
    ${CMAKE_SOURCE_DIR}
)