execute_process(COMMAND ./init_submodules.sh
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/scripts
                OUTPUT_VARIABLE SUBMODULE_STATUS)

message(STATUS "Submodule status: ${SUBMODULE_STATUS}")