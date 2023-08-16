function(add_unittest test_name)
  message("Set-up unittest: ${test_name}")
  get_filename_component(test ${test_name} NAME_WE)
  add_executable(${test}
    ${test_name}
  )
  add_dependencies(UnitTests ${test})

  target_link_libraries(${test}
    LINK_PUBLIC
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    gtest
     -Wl,--no-as-needed -lm -ldl
  )
  add_test(NAME "${test}"
    COMMAND ${test} ${CATCH_TEST_FILTER}
    WORKING_DIRECTORY ${FINNC_UNITTEST_DIR}
  )
  set_tests_properties("${test}" PROPERTIES LABELS "all")
  set(CTEST_OUTPUT_ON_FAILURE ON)
endfunction()
