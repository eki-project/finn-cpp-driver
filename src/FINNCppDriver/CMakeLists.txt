add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(config) 

add_executable(finnhpc FINNDriver.cpp)
target_include_directories(finnhpc SYSTEM PRIVATE ${XRT_INCLUDE_DIRS})
target_include_directories(finnhpc PRIVATE ${FINN_SRC_DIR})
target_link_directories(finnhpc PRIVATE ${XRT_LIB_CORE_LOCATION} ${XRT_LIB_OCL_LOCATION} ${BOOST_LIBRARYDIR})
target_link_libraries(finnhpc PRIVATE finnc_core finnc_options Threads::Threads OpenCL xrt_coreutil uuid finnc_utils finn_config ${Boost_LIBRARIES} nlohmann_json::nlohmann_json OpenMP::OpenMP_CXX)
set_target_properties(finnhpc
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)




