add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(config) 

add_executable(finn FINNDriver.cpp)
target_include_directories(finn SYSTEM PRIVATE ${XRT_INCLUDE_DIRS})
target_include_directories(finn PRIVATE ${FINN_SRC_DIR})
target_link_directories(finn PRIVATE ${XRT_LIB_CORE_LOCATION} ${XRT_LIB_OCL_LOCATION} ${BOOST_LIBRARYDIR})
target_link_libraries(finn PRIVATE finnc_core finnc_options Threads::Threads OpenCL xrt_coreutil uuid finnc_utils finn_config ${Boost_LIBRARIES} nlohmann_json::nlohmann_json OpenMP::OpenMP_CXX)



