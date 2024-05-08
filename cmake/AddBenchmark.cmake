function(add_benchmark benchmark_name)
  get_filename_component(benchmark ${benchmark_name} NAME_WE)

  list(APPEND CMAKE_MESSAGE_INDENT "  ") #indent +1
  message(STATUS "Set-up benchmark: ${benchmark}")
  list(POP_BACK CMAKE_MESSAGE_INDENT)    #indent -1
  
  add_executable(${benchmark}
    ${benchmark_name}
  )
  add_dependencies(Benchmarks ${benchmark})
  target_include_directories(${benchmark} PRIVATE ${FINN_SRC_DIR})

  target_link_libraries(${benchmark}
    PUBLIC
    finnc_options
    ${Boost_LIBRARIES}
    finnc_utils
    finnc_core_test
    xrt_mock
    benchmark::benchmark
    OpenMP::OpenMP_CXX
  )

  target_link_directories(${benchmark} PRIVATE ${BOOST_LIBRARYDIR})

endfunction()
