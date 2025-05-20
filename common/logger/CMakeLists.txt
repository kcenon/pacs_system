cmake_minimum_required(VERSION 3.16)

add_library(pacs_logger
    logger.cpp
    logging_service.cpp
)

target_include_directories(pacs_logger PUBLIC 
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(pacs_logger PUBLIC 
    logger
    pacs_config
)