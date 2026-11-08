# DIMSE-N SCU: a command-line host for the Storage Commitment, UPS and N-GET SCU classes

add_executable(dimse_n_scu
    main.cpp
)

target_include_directories(dimse_n_scu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(dimse_n_scu
    PRIVATE
        pacs_core
        pacs_encoding
        pacs_network
        pacs_services
        pacs_integration
)

target_compile_features(dimse_n_scu PRIVATE cxx_std_20)

install(TARGETS dimse_n_scu
    RUNTIME DESTINATION bin
)
