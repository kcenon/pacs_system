# Worklist SCU Sample
# Modality Worklist Query Client demonstrating MWL C-FIND SCU

add_executable(worklist_scu
    main.cpp
)

target_include_directories(worklist_scu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(worklist_scu
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(worklist_scu PRIVATE cxx_std_20)

# Install target
install(TARGETS worklist_scu
    RUNTIME DESTINATION bin
)
