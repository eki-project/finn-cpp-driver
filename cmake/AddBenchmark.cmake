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
    finnc_core
    xrt_coreutil
    benchmark::benchmark
    OpenMP::OpenMP_CXX
  )

endfunction()
