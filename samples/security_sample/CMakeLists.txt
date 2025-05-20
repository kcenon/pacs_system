cmake_minimum_required(VERSION 3.16)

add_executable(security_sample
    security_sample.cpp
)

target_link_libraries(security_sample 
    pacs_common
    pacs_config
    pacs_logger
    pacs_security
)

target_include_directories(security_sample PRIVATE
    ${CMAKE_SOURCE_DIR}
)