add_executable(main main.cpp)
target_link_libraries(main PRIVATE finnc_options)

add_executable(finn template_driver.cpp)
#target_compile_options(finn PUBLIC "-pthread")
target_include_directories(finn SYSTEM PUBLIC ${XRT_INCLUDE_DIRS})
target_link_libraries(finn PUBLIC finnc_options ${XRT_LIBS} Threads::Threads "-L$ENV{XILINX_XRT}/lib -lrt -lOpenCL -luuid -lxrt_coreutil")

