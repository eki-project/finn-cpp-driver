function(add_unittest test_name)
  list(APPEND CMAKE_MESSAGE_INDENT "  ") #indent +1
  message(STATUS "Set-up unittest: ${test_name}")
  list(POP_BACK CMAKE_MESSAGE_INDENT)    #indent -1
  get_filename_component(test ${test_name} NAME_WE)
  add_executable(${test}
    ${test_name}
  )
  add_dependencies(UnitTests ${test})

  target_link_libraries(${test}
    PUBLIC
    gtest
    finnc_options
    ${Boost_LIBRARIES}
    finnc_utils
    finnc_core_test
    xrt_mock
    # -Wl,--no-as-needed -lm -ldl
  )
  add_test(NAME "${test}"
    COMMAND ${test} ${CATCH_TEST_FILTER}
    WORKING_DIRECTORY ${FINNC_UNITTEST_DIR}
  )
  set_tests_properties("${test}" PROPERTIES LABELS "all")
  set(CTEST_OUTPUT_ON_FAILURE ON)
endfunction()
