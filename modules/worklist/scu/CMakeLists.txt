# BSD 3-Clause License
# Copyright (c) 2024, kcenon
# SPDX-License-Identifier: BSD-3-Clause

# Create worklist SCU library
add_library(pacs_worklist_scu
    worklist_scu.cpp
)

# Add alias
add_library(pacs::worklist::scu ALIAS pacs_worklist_scu)

# Configure include directories
target_include_directories(pacs_worklist_scu
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
        $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(pacs_worklist_scu
    PUBLIC
        pacs::dicom
        pacs::common
        pacs::interfaces
)

# Set compile features
target_compile_features(pacs_worklist_scu PUBLIC cxx_std_20)