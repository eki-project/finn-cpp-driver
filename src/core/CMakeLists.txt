file(GLOB_RECURSE UTILS_SRC *.cpp)

add_library(finnc_core ${UTILS_SRC})
target_include_directories(finnc_core SYSTEM PUBLIC ${XRT_INCLUDE_DIRS})
target_link_directories(finnc_core PUBLIC ${XRT_LIB_CORE_LOCATION})
target_link_directories(finnc_core PUBLIC ${XRT_LIB_OCL_LOCATION})
target_link_libraries(finnc_core PUBLIC ${Boost_LIBRARIES} finnc_utils Threads::Threads OpenCL xrt_coreutil uuid)
