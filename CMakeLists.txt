cmake_minimum_required(VERSION 3.16)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
message(STATUS "${CMAKE_MODULE_PATH}")
SET(FINNC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
SET(FINNC_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
SET(CMAKE_COLOR_MAKEFILE ON)


include(cmake/PreventInSourceBuilds.cmake)

project(FinnCPPDriver)

# Custom paths for the unittest configuration and the header for in-use compilation
if(DEFINED FINN_CUSTOM_UNITTEST_CONFIG)
    message(STATUS "Added custom unittest config path (${FINN_CUSTOM_UNITTEST_CONFIG})!")
    add_definitions(-DFINN_CUSTOM_UNITTEST_CONFIG=${FINN_CUSTOM_UNITTEST_CONFIG})
endif()

if(DEFINED FINN_HEADER_LOCATION)
    message(STATUS "Added custom FINN-compiled header path (${FINN_HEADER_LOCATION})!")
    add_definitions(-DFINN_HEADER_LOCATION=${FINN_HEADER_LOCATION})
endif()

# specify the C++ standard
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
message(STATUS "Using C++ Standard ${CMAKE_CXX_STANDARD}")

include(cmake/StandardProjectSettings.cmake)

include(FetchContent)

# Enable building when the device is offline
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)


FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        main
  GIT_SHALLOW     TRUE
)

FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY  https://github.com/google/benchmark.git
  GIT_TAG         main
  GIT_SHALLOW     TRUE

)
set(BENCHMARK_ENABLE_TESTING OFF)

include(cmake/CheckSubmodules.cmake)

# Doxygen

option(FINNC_BUILD_DOC "Build documentation" OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
set(FINNC_BUILD_DOC ON)
endif()

# Setup the doxygen code documentation
if(FINNC_BUILD_DOC)
  find_package(Doxygen)
  if (DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")
    add_custom_target(doc_doxygen ALL
      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
    )
  else(DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen need to be installed to generate the doxygen documentation.")
  endif()
endif()

#INCLUDES

#set(Boost_USE_STATIC_LIBS   ON)

#Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#XRT
find_package(OpenCL REQUIRED)
set(XRT_DIR "${CMAKE_SOURCE_DIR}/cmake")
find_package(XRT REQUIRED)

#OpenMP
find_package(OpenMP)

#Boost
set(Boost_DEBUG OFF)
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT "${FINNC_BASE_DIR}/deps/finn_boost")
set(BOOST_INCLUDEDIR "${FINNC_BASE_DIR}/deps/finn_boost")
set(BOOST_LIBRARYDIR "${FINNC_BASE_DIR}/deps/finn_boost/stage/lib/boost")
set(Boost_NAMESPACE "finnBoost")
set(FINNC_SAVE_BOOST_ROOT $ENV{BOOST_ROOT})
set(ENV{BOOST_ROOT} "")
find_package(Boost 1.80.0 COMPONENTS system log log_setup program_options filesystem ${BOOST_THREAD} REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
set(ENV{BOOST_ROOT} ${FINNC_SAVE_BOOST_ROOT})
message(STATUS "${Boost_LIBRARIES}")

#mdspan
include_directories(SYSTEM external/mdspan/include)

#xsimd
add_subdirectory(external/xsimd)
include_directories(SYSTEM external/xsimd/include)

#xtl
add_subdirectory(external/xtl)
include_directories(SYSTEM external/xtl/include)

#xtensor
add_subdirectory(external/xtensor)
include_directories(SYSTEM external/xtensor/include)

#Nlohmann json

set(JSON_BuildTests OFF)
add_subdirectory(external/json)
include_directories(SYSTEM external/json/include/)

# Build Unit tests
# Include before clang-tidy inclusion to exclude folder from clang-tidy

set(FINNC_ENABLE_TESTING "Build all tests (default is ON)" ON)
FetchContent_MakeAvailable(googletest googlebenchmark)
include(cmake/AddUnittest.cmake)
include(cmake/AddBenchmark.cmake)
include(cmake/AddIntegrationtest.cmake)

enable_testing()
set(ENV{GTEST_OUTPUT} "json")

if (FINNC_ENABLE_TESTING)
  message(STATUS "Finn C++ unittests:")
  add_subdirectory(unittests)
  message(STATUS "Finn C++ benchmarks:")
  add_subdirectory(benchmarks)
  message(STATUS "Finn C++ integrationtests:")
  add_subdirectory(integrationtest)
endif()

#Compiler Options
add_library(finnc_options INTERFACE)
add_library(finnc::finnc_options ALIAS finnc_options)

OPTION(FINNC_ENABLE_ALLOPT "Enable all optimizations" ON)
if(${FINNC_ENABLE_ALLOPT})
  message(STATUS "All optimizations are enabled")
  target_compile_options(
    finnc_options
    INTERFACE -O3 -march=native -mtune=native -fstack-protector-strong -fopenmp -ffunction-sections -fdata-sections -pipe -funroll-loops)
endif()

### Enable compiler warnings
option(FINNC_ENABLE_WARNINGS "Enable warnings" ON)
if (FINNC_ENABLE_WARNINGS)
  include(cmake/CompilerWarnings.cmake)
  finnc_set_project_warnings(
    finnc_options
    OFF
    ""
    ""
    ""
    "")
endif (FINNC_ENABLE_WARNINGS)

#
# Create options for including cmake files from the cmake folder with a bit of output.
#
macro(check_include)
  if(NOT ${ARGC} EQUAL 3)
    message(FATAL_ERROR "Call to 'check_include' with ${ARGC} arguments instead of 3")
  endif()
  OPTION(${ARGV0} "Enable ${ARGV0}" ON)
  if (${ARGV0})
    message(STATUS "${ARGV1}: enabled")
    include(cmake/${ARGV2})
  else()
    message(STATUS "${ARGV1}: disabled")
  endif()
endmacro()

#Enable all checks in debug mode
#message(STATUS "Build mode ${CMAKE_BUILD_TYPE}")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    #message(WARNING "Analysers are off for Release build!")
    set(CLANG_FORMAT OFF)
    set(CLANG_TIDY OFF)
    set(CPP_CHECK OFF)
    set(IWYU OFF)
endif ()

#Sanitizers & Checks

include(cmake/Sanitizers.cmake)

OPTION(FINNC_ENABLE_SANITIZERS "Enable default analyzers" ON)
OPTION(FINNC_ENABLE_THREAD_SANITIZERS "Enable thread analyzer" OFF)
OPTION(FINNC_ENABLE_MEMORY_SANITIZERS "Enable memory analyzer" OFF)

if (FINNC_ENABLE_SANITIZERS)
finncpp_enable_sanitizers(finnc_options
                         ${FINNC_ENABLE_SANITIZERS}
                         ${FINNC_ENABLE_SANITIZERS}
                         ${FINNC_ENABLE_SANITIZERS}
                         ${FINNC_ENABLE_THREAD_SANITIZERS}
                         ${FINNC_ENABLE_MEMORY_SANITIZERS})
else()
message(STATUS "All Sanitizers disabled.")
endif()

message(STATUS "Checks:")
list(APPEND CMAKE_MESSAGE_INDENT "  ") #indent +1
check_include(IPO          "InterproceduralOptimization" InterproceduralOptimization.cmake)
check_include(CLANG_FORMAT "clang-format" ClangFormat.cmake)
check_include(CLANG_TIDY   "clang-tidy"   ClangTidy.cmake)
check_include(CPP_CHECK    "cppcheck"     CppCheck.cmake)
check_include(IWYU         "iwyu"         IWYU.cmake)
list(POP_BACK CMAKE_MESSAGE_INDENT)    #indent -1

add_subdirectory(src)
