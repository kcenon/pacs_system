# move_scu - DICOM C-MOVE SCU utility (dcmtk-compatible)
#
# Provides dcmtk-compatible interface for retrieving DICOM objects
# from a PACS server using C-MOVE service.

add_executable(move_scu
    main.cpp
)

target_link_libraries(move_scu
    PRIVATE
        pacs_network
        pacs_services
        pacs_core
        pacs_encoding
        pacs_integration
)

target_compile_features(move_scu PRIVATE cxx_std_20)

# Install
install(TARGETS move_scu
    RUNTIME DESTINATION bin
)
