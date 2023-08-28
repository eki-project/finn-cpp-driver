file(GLOB_RECURSE UTILS_SRC *.cpp)

add_library(finnc_utils ${UTILS_SRC})
target_link_libraries(finnc_utils PUBLIC finnc_options ${Boost_LIBRARIES})
