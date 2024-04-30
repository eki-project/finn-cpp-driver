file(GLOB_RECURSE XRT_MOCK_SRC *.cpp)

add_library(xrt_mock SHARED ${XRT_MOCK_SRC})
target_link_libraries(xrt_mock PRIVATE ${Boost_LIBRARIES})
target_include_directories(xrt_mock PRIVATE ${FINN_SRC_DIR})
SET(XRT_MOCK_INCLUDE  "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "XRT_MOCK_INCLUDE")

