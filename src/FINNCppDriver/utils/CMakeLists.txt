file(GLOB_RECURSE UTILS_SRC *.cpp)

add_library(finnc_utils SHARED ${UTILS_SRC})
target_link_libraries(finnc_utils PUBLIC finnc_options ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_directories(finnc_utils PRIVATE ${BOOST_LIBRARYDIR})
target_include_directories(finnc_utils PRIVATE ${FINNC_SRC_DIR})
