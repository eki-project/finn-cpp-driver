add_subdirectory(xrtMock)

add_custom_target(UnitTests)
set_target_properties(UnitTests PROPERTIES FOLDER "Unittests")

set(FINNC_UNITTEST_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(check-unittests
  COMMAND ${CMAKE_CTEST_COMMAND} --progress --output-on-failure -j 8
  WORKING_DIRECTORY ${FINNC_UNITTEST_DIR}
  DEPENDS UnitTests
)

add_subdirectory(core)
add_subdirectory(config)
add_subdirectory(utils)
