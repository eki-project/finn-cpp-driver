add_library(finnc_log SHARED Logger.cpp)
target_include_directories(finnc_log PUBLIC ${FINN_SRC_DIR})
set_target_properties(finnc_log
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
