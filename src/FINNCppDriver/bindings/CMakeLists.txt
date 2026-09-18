if(pybind11_FOUND)
    # TODO(bwintermann): For now the module is built into lib/, but it should maybe be exported somwhere else?
    set(BINDINGS_DIR "${CMAKE_BINARY_DIR}/lib/finnhpcpy")
    message(STATUS "Found pybind11 - generating Python bindings to: ${BINDINGS_DIR}")
    
    # Compile the pybind module
    pybind11_add_module(finnhpcpy WITHOUT_SOABI PythonBindings.cpp)
    target_include_directories(finnhpcpy SYSTEM PRIVATE ${XRT_INCLUDE_DIRS})
    target_include_directories(finnhpcpy PRIVATE ${FINN_SRC_DIR})
    target_link_directories(finnhpcpy PRIVATE ${XRT_LIB_CORE_LOCATION} ${XRT_LIB_OCL_LOCATION})
    target_link_libraries(finnhpcpy PRIVATE finnc_core finnc_options Threads::Threads xrt_coreutil uuid finn_config nlohmann_json::nlohmann_json OpenMP::OpenMP_CXX)

    # Write into the specified directory
    set_target_properties(finnhpcpy PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${BINDINGS_DIR}")
    
    # Make it a python package
    file(MAKE_DIRECTORY "${BINDINGS_DIR}")
    file(TOUCH "${BINDINGS_DIR}/__init__.py")

    # Optionally generate typing stubs
    find_program(PYBIND11_STUBGEN pybind11-stubgen)
    if(PYBIND11_STUBGEN)
        message(STATUS "Found pybind11-stubgen: ${PYBIND11_STUBGEN}. Creating Python package in ${BINDINGS_DIR}")
        configure_file(generate_stubs.sh "${BINDINGS_DIR}/../generate_stubs.configured.sh" USE_SOURCE_PERMISSIONS)
        # Generate the stubs
        add_custom_command(
            TARGET finnhpcpy
            POST_BUILD
            COMMAND "./generate_stubs.configured.sh"
            WORKING_DIRECTORY "${BINDINGS_DIR}/.."
            COMMENT "Generating stubs..."
        )
    else()
        message(WARNING "Did not find pybind11-stubgen")
    endif()
else()
    message(WARNING "Did not find pybind11 - cannot generate Python bindings.")
endif()