add_executable(main main.cpp)
target_link_libraries(main PRIVATE finnc_options)

add_executable(finn template_driver.cpp utils/device_handler.hpp)
target_include_directories(finn SYSTEM PUBLIC ${XRT_INCLUDE_DIRS})
target_link_directories(finn PUBLIC ${XRT_LIB_CORE_LOCATION})
target_link_directories(finn PUBLIC ${XRT_LIB_OCL_LOCATION})
target_link_libraries(finn PUBLIC finnc_options Threads::Threads OpenCL xrt_coreutil uuid)

