# Print SCU Sample
# Print Management Client demonstrating Film Session/Box workflow

add_executable(print_scu
    main.cpp
)

target_include_directories(print_scu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link required PACS libraries
target_link_libraries(print_scu
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

# Set C++20 standard
target_compile_features(print_scu PRIVATE cxx_std_20)

# Install target
install(TARGETS print_scu
    RUNTIME DESTINATION bin
)
