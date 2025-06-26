file(GLOB_RECURSE CORE_SRC *.cpp)

add_library(finnc_core SHARED ${CORE_SRC})
target_include_directories(finnc_core SYSTEM PUBLIC ${XRT_INCLUDE_DIRS} ${FINN_SRC_DIR})
target_link_directories(finnc_core PUBLIC ${XRT_LIB_CORE_LOCATION})
target_link_directories(finnc_core PUBLIC ${XRT_LIB_OCL_LOCATION})
target_link_libraries(finnc_core PUBLIC finnc_options finn_config Threads::Threads OpenCL xrt_coreutil rt uuid)

add_library(finnc_core_test SHARED ${CORE_SRC})
target_include_directories(finnc_core_test SYSTEM PUBLIC ${XRT_MOCK_INCLUDE} ${FINN_SRC_DIR})
target_link_libraries(finnc_core_test PUBLIC finnc_options finn_config xrt_mock)
target_compile_definitions(finnc_core_test PRIVATE UNITTEST=1)

# target_include_directories(finnc_core_test SYSTEM PUBLIC ${XRT_INCLUDE_DIRS})
# target_link_directories(finnc_core_test PUBLIC ${XRT_LIB_CORE_LOCATION})
# target_link_directories(finnc_core_test PUBLIC ${XRT_LIB_OCL_LOCATION})
# target_link_libraries(finnc_core_test PUBLIC ${Boost_LIBRARIES} finnc_options finnc_utils Threads::Threads OpenCL xrt_coreutil uuid)

set_target_properties(finnc_core
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(finnc_core_test
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
