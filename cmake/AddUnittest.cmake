function(add_unittest test_name)
  get_filename_component(test ${test_name} NAME_WE)

  list(APPEND CMAKE_MESSAGE_INDENT "  ") #indent +1
  message(STATUS "Set-up unittest: ${test}")
  list(POP_BACK CMAKE_MESSAGE_INDENT)    #indent -1
  
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
    OpenMP::OpenMP_CXX
  )

  target_link_directories(${test} PRIVATE ${BOOST_LIBRARYDIR})

  target_include_directories(${test} PRIVATE ${FINNC_SRC_DIR})

  target_compile_definitions(${test} PRIVATE UNITTEST=1)

  add_test(NAME "${test}"
    COMMAND ${test} ${CATCH_TEST_FILTER}
    WORKING_DIRECTORY ${FINNC_UNITTEST_DIR}
  )
  set_tests_properties("${test}" PROPERTIES LABELS "all")
  set(CTEST_OUTPUT_ON_FAILURE ON)
endfunction()
