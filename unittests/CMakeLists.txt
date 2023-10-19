add_subdirectory(xrtMock)

add_custom_target(UnitTests)
set_target_properties(UnitTests PROPERTIES FOLDER "Unittests")

set(FINNC_UNITTEST_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(check-unittests
  COMMAND ${CMAKE_CTEST_COMMAND} --progress --output-on-failure -j 8
  WORKING_DIRECTORY ${FINNC_UNITTEST_DIR}
  DEPENDS UnitTests
)

add_subdirectory(core)
add_subdirectory(config)
add_subdirectory(utils)



# add_executable(benchmark DeviceBufferBenchmark.cpp)

# message(STATUS "XRT LIB CORE LOCATION: ${XRT_LIB_CORE_LOCATION}")
# message(STATUS "XRT INCLUDE DIRS: ${XRT_INCLUDE_DIRS}")
# #target_include_directories(benchmark SYSTEM PRIVATE ${XRT_INCLUDE_DIRS})
# #target_link_directories(benchmark PRIVATE ${XRT_LIB_CORE_LOCATION})
# #target_link_directories(benchmark PRIVATE ${XRT_LIB_OCL_LOCATION})
# target_link_libraries(benchmark PRIVATE finnc_core_test finnc_options Threads::Threads OpenCL xrt_mock uuid finnc_utils ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

# message(STATUS "Benchmark required boost libs: ${Boost_LIBRARIES}")

# set(BENCH_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
# add_custom_target(execute-benchmark
#   COMMAND ./benchmark
#   WORKING_DIRECTORY ${BENCH_BUILD_DIR}
#   DEPENDS benchmark
# )
