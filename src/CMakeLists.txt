add_subdirectory(utils)
add_subdirectory(core)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE finnc_options)

add_executable(finn FINNDriver.cpp)
target_include_directories(finn SYSTEM PRIVATE ${XRT_INCLUDE_DIRS})
target_link_directories(finn PRIVATE ${XRT_LIB_CORE_LOCATION})
target_link_directories(finn PRIVATE ${XRT_LIB_OCL_LOCATION})
target_link_libraries(finn PRIVATE finnc_core finnc_options Threads::Threads OpenCL xrt_coreutil uuid finnc_utils ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)
#target_link_libraries(finn PUBLIC finnc_options ${Boost_LIBRARIES} finnc_utils finnc_core)




add_executable(benchmark DeviceBufferBenchmark.cpp)
target_include_directories(benchmark SYSTEM PRIVATE ${XRT_INCLUDE_DIRS})
target_link_directories(benchmark PRIVATE ${XRT_LIB_CORE_LOCATION})
target_link_directories(benchmark PRIVATE ${XRT_LIB_OCL_LOCATION})
target_link_libraries(benchmark PRIVATE finnc_core finnc_options Threads::Threads OpenCL xrt_mock uuid finnc_utils ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

message(STATUS "Benchmark required boost libs: ${Boost_LIBRARIES}")

set(BENCH_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
add_custom_target(execute-benchmark
  COMMAND ./benchmark
  WORKING_DIRECTORY ${BENCH_BUILD_DIR}
  DEPENDS benchmark
)