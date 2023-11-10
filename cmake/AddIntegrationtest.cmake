function(add_integrationtest integrationtest_name)
  get_filename_component(integrationtest ${integrationtest_name} NAME_WE)

  list(APPEND CMAKE_MESSAGE_INDENT "  ") #indent +1
  message(STATUS "Set-up integrationtest: ${integrationtest}")
  list(POP_BACK CMAKE_MESSAGE_INDENT)    #indent -1
  
  add_executable(${integrationtest} EXCLUDE_FROM_ALL
    ${integrationtest_name}
  )
  add_dependencies(Integrationtests ${integrationtest})

  target_compile_definitions(${integrationtest} PRIVATE UNITTEST=1)

  target_include_directories(${integrationtest} SYSTEM PRIVATE ${XRT_INCLUDE_DIRS} ${FINNC_SRC_DIR})
  target_link_directories(${integrationtest} PRIVATE ${XRT_LIB_CORE_LOCATION} ${XRT_LIB_OCL_LOCATION} ${BOOST_LIBRARYDIR})
  target_link_libraries(${integrationtest} PRIVATE gtest finnc_core finnc_options Threads::Threads OpenCL xrt_coreutil uuid finnc_utils ${Boost_LIBRARIES} nlohmann_json::nlohmann_json OpenMP::OpenMP_CXX)


endfunction()
