# get_scu - DICOM C-GET SCU utility (dcmtk-compatible)
#
# Provides dcmtk-compatible interface for retrieving DICOM objects
# directly from a PACS server using C-GET service.
# Unlike C-MOVE, C-GET doesn't require a separate storage SCP,
# making it more firewall-friendly for NAT environments.

add_executable(get_scu
    main.cpp
)

target_link_libraries(get_scu
    PRIVATE
        pacs_network
        pacs_services
        pacs_core
        pacs_encoding
        pacs_integration
)

target_compile_features(get_scu PRIVATE cxx_std_20)

# Install
install(TARGETS get_scu
    RUNTIME DESTINATION bin
)
