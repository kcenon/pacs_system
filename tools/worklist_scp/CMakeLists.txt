# Modality Worklist SCP Sample
# DICOM Worklist Server for providing scheduled procedure information

add_executable(worklist_scp
    main.cpp
)

target_include_directories(worklist_scp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(worklist_scp
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(worklist_scp PRIVATE cxx_std_20)

# Install target
install(TARGETS worklist_scp
    RUNTIME DESTINATION bin
)
