
if(FINN_USE_HOST_MEMORY)
    set(FINN_USE_HOST_MEMORY_BOOL true)
else()
    set(FINN_USE_HOST_MEMORY_BOOL false)
endif()

configure_file("${PROJECT_SOURCE_DIR}/src/FINNCppDriver/config/CompilationOptions.h.in" "${PROJECT_BINARY_DIR}/src/FINNCppDriver/config/CompilationOptions.h")


add_library(finn_config INTERFACE)
target_include_directories(finn_config INTERFACE "${PROJECT_BINARY_DIR}/src")