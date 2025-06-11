
if(FINN_USE_HOST_MEM)
    set(FINN_USE_HOST_MEMORY_BOOL true)
else()
    set(FINN_USE_HOST_MEMORY_BOOL false)
endif()

configure_file("${PROJECT_SOURCE_DIR}/src/FINNCppDriver/config/CompilationOptions.h.in" "${PROJECT_BINARY_DIR}/src/FINNCppDriver/config/CompilationOptions.h")


add_library(finn_config INTERFACE)
target_include_directories(finn_config INTERFACE "${PROJECT_BINARY_DIR}/src")

set_target_properties(finn_config
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

