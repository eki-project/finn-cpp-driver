add_subdirectory(utils)
add_subdirectory(core)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE finnc_options)
target_include_directories(main PRIVATE ${FINNC_SRC_DIR})

add_executable(finn FINNDriver.cpp)
target_include_directories(finn SYSTEM PRIVATE ${XRT_INCLUDE_DIRS} ${FINNC_SRC_DIR})
target_link_directories(finn PRIVATE ${XRT_LIB_CORE_LOCATION} ${XRT_LIB_OCL_LOCATION} ${BOOST_LIBRARYDIR})
target_link_libraries(finn PRIVATE finnc_core finnc_options Threads::Threads OpenCL xrt_coreutil uuid finnc_utils ${Boost_LIBRARIES} nlohmann_json::nlohmann_json OpenMP::OpenMP_CXX)



