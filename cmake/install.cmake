##################################################
# Installation
##################################################

pacs_register_export_target(pacs_core core)
pacs_register_export_target(pacs_encoding encoding)
pacs_register_export_target(pacs_network network)
pacs_register_export_target(pacs_client client)
pacs_register_export_target(pacs_services services)
pacs_register_export_target(pacs_security security)
pacs_register_export_target(pacs_storage storage)
pacs_register_export_target(pacs_ai ai)
pacs_register_export_target(pacs_monitoring monitoring)
pacs_register_export_target(pacs_workflow workflow)
pacs_register_export_target(pacs_web web)
pacs_register_export_target(pacs_integration integration)

set(PACS_SYSTEM_INSTALL_TARGETS
    pacs_core
    pacs_encoding
    pacs_network
    pacs_client
    pacs_services
    pacs_security
)

foreach(_optional_target
    pacs_storage
    pacs_ai
    pacs_monitoring
    pacs_workflow
    pacs_web
    pacs_integration
)
    if(TARGET ${_optional_target})
        list(APPEND PACS_SYSTEM_INSTALL_TARGETS ${_optional_target})
    endif()
endforeach()

get_property(PACS_EXPORT_BRIDGE_TARGETS GLOBAL PROPERTY PACS_EXPORT_BRIDGE_TARGETS)
if(PACS_EXPORT_BRIDGE_TARGETS)
    list(APPEND PACS_SYSTEM_INSTALL_TARGETS ${PACS_EXPORT_BRIDGE_TARGETS})
endif()
list(REMOVE_DUPLICATES PACS_SYSTEM_INSTALL_TARGETS)

set(PACS_SYSTEM_WITH_SQLITE FALSE)
if(TARGET pacs_storage)
    set(PACS_SYSTEM_WITH_SQLITE TRUE)
endif()

set(PACS_SYSTEM_WITH_DATABASE_SYSTEM FALSE)
if(TARGET database)
    set(PACS_SYSTEM_WITH_DATABASE_SYSTEM TRUE)
endif()

set(PACS_SYSTEM_WITH_MONITORING_SYSTEM FALSE)
if(TARGET monitoring_system OR PACS_MONITORING_SYSTEM_INCLUDE_DIR)
    set(PACS_SYSTEM_WITH_MONITORING_SYSTEM TRUE)
endif()

set(PACS_SYSTEM_WITH_THREAD_SYSTEM FALSE)
if(TARGET ThreadSystem OR TARGET thread_base OR PACS_THREAD_SYSTEM_INCLUDE_DIR)
    set(PACS_SYSTEM_WITH_THREAD_SYSTEM TRUE)
endif()

set(PACS_SYSTEM_WITH_LOGGER_SYSTEM FALSE)
if(TARGET LoggerSystem)
    set(PACS_SYSTEM_WITH_LOGGER_SYSTEM TRUE)
endif()

set(PACS_SYSTEM_WITH_JPEG FALSE)
if(PACS_JPEG_FOUND)
    set(PACS_SYSTEM_WITH_JPEG TRUE)
endif()

set(PACS_SYSTEM_WITH_JPEG2000 FALSE)
if(PACS_JPEG2000_FOUND)
    set(PACS_SYSTEM_WITH_JPEG2000 TRUE)
endif()

set(PACS_SYSTEM_WITH_JPEGLS FALSE)
if(PACS_JPEGLS_FOUND)
    set(PACS_SYSTEM_WITH_JPEGLS TRUE)
endif()

set(PACS_SYSTEM_WITH_HTJ2K FALSE)
if(PACS_HTJ2K_FOUND)
    set(PACS_SYSTEM_WITH_HTJ2K TRUE)
endif()

set(PACS_SYSTEM_WITH_OPENSSL FALSE)
if(PACS_OPENSSL_FOUND)
    set(PACS_SYSTEM_WITH_OPENSSL TRUE)
endif()

set(PACS_SYSTEM_WITH_CROW FALSE)
if(TARGET pacs_web)
    set(PACS_SYSTEM_WITH_CROW TRUE)
endif()

set(PACS_SYSTEM_WITH_AWS_SDK FALSE)
if(TARGET pacs_storage AND PACS_WITH_AWS_SDK AND AWSSDK_FOUND)
    set(PACS_SYSTEM_WITH_AWS_SDK TRUE)
endif()

set(PACS_SYSTEM_WITH_AZURE_SDK FALSE)
if(TARGET pacs_storage AND PACS_WITH_AZURE_SDK AND azure-storage-blobs-cpp_FOUND)
    set(PACS_SYSTEM_WITH_AZURE_SDK TRUE)
endif()

set(PACS_SYSTEM_BRIDGE_TARGET_SETUP "")
get_property(PACS_EXPORT_BRIDGE_MAPPINGS GLOBAL PROPERTY PACS_EXPORT_BRIDGE_MAPPINGS)
foreach(_bridge_mapping IN LISTS PACS_EXPORT_BRIDGE_MAPPINGS)
    string(FIND "${_bridge_mapping}" "=" _bridge_sep)
    if(_bridge_sep LESS 0)
        continue()
    endif()

    string(SUBSTRING "${_bridge_mapping}" 0 ${_bridge_sep} _bridge_target_name)
    math(EXPR _bridge_value_index "${_bridge_sep} + 1")
    string(SUBSTRING "${_bridge_mapping}" ${_bridge_value_index} -1 _bridge_install_target)

    string(APPEND PACS_SYSTEM_BRIDGE_TARGET_SETUP
        "if(TARGET pacs_system::${_bridge_target_name})\n"
        "  set_property(TARGET pacs_system::${_bridge_target_name} APPEND PROPERTY INTERFACE_LINK_LIBRARIES \"${_bridge_install_target}\")\n"
        "endif()\n"
    )
endforeach()

install(DIRECTORY include/pacs
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)

install(TARGETS ${PACS_SYSTEM_INSTALL_TARGETS}
    EXPORT pacs_system-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pacs_system-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pacs_system-config.cmake"
    INSTALL_DESTINATION ${PACS_SYSTEM_CMAKE_INSTALL_DIR}
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/pacs_system-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(EXPORT pacs_system-targets
    FILE pacs_system-targets.cmake
    NAMESPACE pacs_system::
    DESTINATION ${PACS_SYSTEM_CMAKE_INSTALL_DIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/pacs_system-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/pacs_system-config-version.cmake"
    DESTINATION ${PACS_SYSTEM_CMAKE_INSTALL_DIR}
)

set(PACS_SYSTEM_CAN_EXPORT_BUILD_TREE TRUE)
foreach(_pacs_non_exportable_dep
    pacs_common_system_headers
    pacs_thread_system_headers
    pacs_monitoring_system_headers
    NetworkSystem
    thread_base
    LoggerSystem
    monitoring_system
    Crow
)
    if(TARGET ${_pacs_non_exportable_dep})
        set(PACS_SYSTEM_CAN_EXPORT_BUILD_TREE FALSE)
        break()
    endif()
endforeach()

if(PACS_SYSTEM_CAN_EXPORT_BUILD_TREE)
    export(EXPORT pacs_system-targets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/pacs_system-targets.cmake"
        NAMESPACE pacs_system::
    )
else()
    message(STATUS "pacs_system: Skipping build-tree export (local dependency targets cannot be exported)")
endif()
