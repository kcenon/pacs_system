# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create storage library
add_library(pacs_storage
    encrypted_storage.cpp
)

# Add alias
add_library(pacs::storage ALIAS pacs_storage)

# Configure include directories
target_include_directories(pacs_storage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find Boost
find_package(Boost QUIET COMPONENTS filesystem)

# Link libraries
target_link_libraries(pacs_storage
    PUBLIC
        pacs::logger
        pacs::security
)

if(Boost_FOUND)
    target_link_libraries(pacs_storage PUBLIC Boost::filesystem)
    target_compile_definitions(pacs_storage PRIVATE HAS_BOOST_FILESYSTEM)
else()
    # Use std::filesystem if Boost is not available
    target_compile_definitions(pacs_storage PRIVATE USE_STD_FILESYSTEM)
endif()

# Set compile features
target_compile_features(pacs_storage PUBLIC cxx_std_20)

# Install library
install(TARGETS pacs_storage
    EXPORT pacs_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    encrypted_storage.h
    DESTINATION include/pacs/common/storage
)