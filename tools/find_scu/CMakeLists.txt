# find_scu - DICOM C-FIND SCU utility (dcmtk-compatible)
#
# Provides dcmtk-compatible interface for querying PACS servers.
# Supports Patient Root and Study Root query models with flexible
# query key specification and multiple output formats.

add_executable(find_scu
    main.cpp
)

target_link_libraries(find_scu
    PRIVATE
        pacs_network
        pacs_services
        pacs_core
        pacs_encoding
        pacs_integration
)

target_compile_features(find_scu PRIVATE cxx_std_20)

# Install
install(TARGETS find_scu
    RUNTIME DESTINATION bin
)
