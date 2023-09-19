add_subdirectory(utils)
add_subdirectory(core)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE finnc_options)

add_executable(finn FINNDriver.cpp)
target_include_directories(finn SYSTEM PRIVATE ${XRT_INCLUDE_DIRS})
target_link_directories(finn PRIVATE ${XRT_LIB_CORE_LOCATION})
target_link_directories(finn PRIVATE ${XRT_LIB_OCL_LOCATION})
target_link_libraries(finn PRIVATE finnc_options Threads::Threads OpenCL xrt_coreutil uuid finnc_utils ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)
#target_link_libraries(finn PUBLIC finnc_options ${Boost_LIBRARIES} finnc_utils finnc_core)

