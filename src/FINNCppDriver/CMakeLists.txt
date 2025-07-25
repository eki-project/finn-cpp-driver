#add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(config) 

add_executable(finnhpc FINNDriver.cpp)

add_custom_target(
 genHeader ALL
 COMMAND ${PYTHON_EXECUTABLE} ${FINN_BASE_DIR}/scripts/generateQuantHeader.py
 BYPRODUCTS hostParameters.h
 COMMENT "Generating enums"
)

add_dependencies(finnhpc genHeader)

target_include_directories(finnhpc SYSTEM PRIVATE ${XRT_INCLUDE_DIRS})
target_include_directories(finnhpc PRIVATE ${FINN_SRC_DIR})
target_link_directories(finnhpc PRIVATE ${XRT_LIB_CORE_LOCATION} ${XRT_LIB_OCL_LOCATION})
target_link_libraries(finnhpc PRIVATE finnc_core finnc_options Threads::Threads OpenCL xrt_coreutil uuid finn_config nlohmann_json::nlohmann_json OpenMP::OpenMP_CXX)
set_target_properties(finnhpc
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)




