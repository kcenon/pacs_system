##################################################
# Compiler Warnings (Target-Scoped)
#
# IMPORTANT: Use target_compile_options() instead of add_compile_options()
# to prevent warning flags from propagating to dependencies (container_system,
# network_system, etc.) via add_subdirectory().
#
# The pacs_apply_warnings() function is called after all pacs_* targets
# are defined to apply warnings ONLY to pacs_system's own targets.
##################################################

# Define warning flags for later application to pacs targets only
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(PACS_WARNING_FLAGS -Wall -Wextra -Wpedantic)
    if(PACS_WARNINGS_AS_ERRORS)
        list(APPEND PACS_WARNING_FLAGS -Werror)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(PACS_WARNING_FLAGS /W4)
    if(PACS_WARNINGS_AS_ERRORS)
        list(APPEND PACS_WARNING_FLAGS /WX)
    endif()
endif()

# Function to apply warnings to pacs targets only
function(pacs_apply_warnings target_name)
    if(TARGET ${target_name} AND PACS_WARNING_FLAGS)
        target_compile_options(${target_name} PRIVATE ${PACS_WARNING_FLAGS})
    endif()
endfunction()
