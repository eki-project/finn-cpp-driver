file(GLOB_RECURSE UTILS_SRC *.cpp)

add_library(finnc_utils SHARED ${UTILS_SRC})
target_link_libraries(finnc_utils PUBLIC finnc_options ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_directories(finnc_utils PRIVATE ${BOOST_LIBRARYDIR})
target_include_directories(finnc_utils PRIVATE ${FINN_SRC_DIR})
set_target_properties(finnc_utils
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
