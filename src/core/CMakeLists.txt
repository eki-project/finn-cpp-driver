file(GLOB_RECURSE CORE_SRC *.cpp)

add_library(finnc_core SHARED ${CORE_SRC})
target_include_directories(finnc_core SYSTEM PUBLIC ${XRT_INCLUDE_DIRS})
target_link_directories(finnc_core PUBLIC ${XRT_LIB_CORE_LOCATION})
target_link_directories(finnc_core PUBLIC ${XRT_LIB_OCL_LOCATION})
target_link_libraries(finnc_core PUBLIC finnc_options finnc_utils Threads::Threads OpenCL xrt_coreutil rt uuid ${Boost_LIBRARIES})

add_library(finnc_core_test SHARED ${CORE_SRC})
target_include_directories(finnc_core_test SYSTEM PUBLIC ${XRT_MOCK_INCLUDE})
target_link_libraries(finnc_core_test PUBLIC ${Boost_LIBRARIES} finnc_options finnc_utils xrt_mock)
target_compile_definitions(finnc_core_test PRIVATE UNITTEST=1)

