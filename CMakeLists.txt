cmake_minimum_required(VERSION 3.16)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
message(STATUS "${CMAKE_MODULE_PATH}")
SET(FINN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
SET(FINN_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
SET(CMAKE_COLOR_MAKEFILE ON)


include(cmake/PreventInSourceBuilds.cmake)

project(FinnCPPDriver)

# specify the C++ standard
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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

option(FINN_BUILD_DOC "Build documentation" OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
set(FINN_BUILD_DOC ON)
endif()

# Setup the doxygen code documentation
if(FINN_BUILD_DOC)
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

#Set Boost policies for compatibility
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

#Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#XRT
find_package(OpenCL REQUIRED)
set(XRT_DIR "${CMAKE_SOURCE_DIR}/cmake")
find_package(XRT REQUIRED)

#OpenMP
find_package(OpenMP REQUIRED)

#Boost
set(Boost_DEBUG OFF)
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT "${FINN_BASE_DIR}/deps/finn_boost")
set(BOOST_INCLUDEDIR "${FINN_BASE_DIR}/deps/finn_boost")
set(BOOST_LIBRARYDIR "${FINN_BASE_DIR}/deps/finn_boost/stage/lib/boost")
set(Boost_NAMESPACE "finnBoost")
set(FINN_SAVE_BOOST_ROOT $ENV{BOOST_ROOT})
set(ENV{BOOST_ROOT} "")
find_package(Boost 1.80.0 COMPONENTS system log log_setup program_options filesystem ${BOOST_THREAD} REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
set(ENV{BOOST_ROOT} ${FINN_SAVE_BOOST_ROOT})
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

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(external/json)
include_directories(SYSTEM external/json/include/)

# Custom paths for the unittest configuration and the header for in-use compilation
if(DEFINED FINN_CUSTOM_UNITTEST_CONFIG)
    message(STATUS "Added custom unittest config path (${FINN_CUSTOM_UNITTEST_CONFIG})!")
    add_definitions(-DFINN_CUSTOM_UNITTEST_CONFIG=${FINN_CUSTOM_UNITTEST_CONFIG})
    get_filename_component(FINN_CUSTOM_UNITTEST_CONFIG_FOLDER ${FINN_CUSTOM_UNITTEST_CONFIG} DIRECTORY)
    get_filename_component(FINN_CUSTOM_UNITTEST_CONFIG_NAME ${FINN_CUSTOM_UNITTEST_CONFIG} NAME)
    if (EXISTS "${FINN_CUSTOM_UNITTEST_CONFIG_FOLDER}/${FINN_CUSTOM_UNITTEST_CONFIG_NAME}" AND NOT IS_DIRECTORY "${FINN_CUSTOM_UNITTEST_CONFIG_FOLDER}/${FINN_CUSTOM_UNITTEST_CONFIG_NAME}")
      message(STATUS "Custom FINN header: found")
    else()
      message(FATAL_ERROR "Could not find FINN header at ${FINN_CUSTOM_UNITTEST_CONFIG_FOLDER}/${FINN_CUSTOM_UNITTEST_CONFIG_NAME}!")
    endif()
endif()

if(DEFINED FINN_HEADER_LOCATION)
    message(STATUS "Added custom FINN-compiled header path (${FINN_HEADER_LOCATION})!")
    add_definitions(-DFINN_HEADER_LOCATION=${FINN_HEADER_LOCATION})
    get_filename_component(FINN_HEADER_LOCATION_FOLDER ${FINN_HEADER_LOCATION} DIRECTORY)
    get_filename_component(FINN_HEADER_LOCATION_NAME ${FINN_HEADER_LOCATION} NAME)
    if (EXISTS "${FINN_HEADER_LOCATION_FOLDER}/${FINN_HEADER_LOCATION_NAME}" AND NOT IS_DIRECTORY "${FINN_HEADER_LOCATION_FOLDER}/${FINN_HEADER_LOCATION_NAME}")
      message(STATUS "Custom FINN header: found")
    else()
      message(FATAL_ERROR "Could not find FINN header at ${FINN_HEADER_LOCATION_FOLDER}/${FINN_HEADER_LOCATION_NAME}!")
    endif()
else()
    message(WARNING "No custom FINN-compiled header set. This should only be the case for testing purposes!")
endif()

# Build Unit tests
# Include before clang-tidy inclusion to exclude folder from clang-tidy

set(FINN_ENABLE_TESTING "Build all tests (default is ON)" ON)
FetchContent_MakeAvailable(googletest googlebenchmark)
include(cmake/AddUnittest.cmake)
include(cmake/AddBenchmark.cmake)
include(cmake/AddIntegrationtest.cmake)

enable_testing()
set(ENV{GTEST_OUTPUT} "json")

if (FINN_ENABLE_TESTING)
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

OPTION(FINN_ENABLE_ALLOPT "Enable all optimizations" ON)
if(${FINN_ENABLE_ALLOPT})
  message(STATUS "All optimizations are enabled")
  target_compile_options(
    finnc_options
    INTERFACE -Ofast -ffast-math -march=native -mtune=native -fstack-protector-strong -fopenmp -ffunction-sections -fdata-sections -pipe -funroll-loops)
endif()

### Enable compiler warnings
option(FINN_ENABLE_WARNINGS "Enable warnings" ON)
if (FINN_ENABLE_WARNINGS)
  include(cmake/CompilerWarnings.cmake)
  finnc_set_project_warnings(
    finnc_options
    OFF
    ""
    ""
    ""
    "")
endif (FINN_ENABLE_WARNINGS)

#Feature Options
OPTION(FINN_USE_HOST_MEM "Enable the use of host memory access" ON)
if (FINN_USE_HOST_MEM)
  message(STATUS "Host Memory Access: enabled")
else()
message(STATUS "Host Memory Access: disabled")
endif (FINN_USE_HOST_MEM)

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

#Sanitizers & Checks

include(cmake/Sanitizers.cmake)

#Disable sanitizers by default in release mode
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  OPTION(FINN_ENABLE_SANITIZERS "Enable default analyzers" OFF)
  OPTION(FINN_ENABLE_THREAD_SANITIZERS "Enable thread analyzer" OFF)
  OPTION(FINN_ENABLE_MEMORY_SANITIZERS "Enable memory analyzer" OFF)
else()
  OPTION(FINN_ENABLE_SANITIZERS "Enable default analyzers" ON)
  OPTION(FINN_ENABLE_THREAD_SANITIZERS "Enable thread analyzer" OFF)
  OPTION(FINN_ENABLE_MEMORY_SANITIZERS "Enable memory analyzer" OFF)
endif()

if (FINN_ENABLE_SANITIZERS)
finncpp_enable_sanitizers(finnc_options
                         ${FINN_ENABLE_SANITIZERS}
                         ${FINN_ENABLE_SANITIZERS}
                         ${FINN_ENABLE_SANITIZERS}
                         ${FINN_ENABLE_THREAD_SANITIZERS}
                         ${FINN_ENABLE_MEMORY_SANITIZERS})
else()
message(STATUS "All Sanitizers disabled.")
endif()

#Enable all checks in debug mode
#message(STATUS "Build mode ${CMAKE_BUILD_TYPE}")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    #message(WARNING "Analysers are off for Release build!")
    set(FINN_CLANG_FORMAT OFF)
    set(FINN_CLANG_TIDY OFF)
    set(FINN_CPP_CHECK OFF)
    set(FINN_IWYU OFF)
endif ()

# Disable Clang Tidy because of bug with constexpr ifs and constexpr constructors
set(FINN_CLANG_TIDY OFF)

message(STATUS "Checks:")
list(APPEND CMAKE_MESSAGE_INDENT "  ") #indent +1
check_include(FINN_IPO          "InterproceduralOptimization" InterproceduralOptimization.cmake)
check_include(FINN_CLANG_FORMAT "clang-format" ClangFormat.cmake)
check_include(FINN_CLANG_TIDY   "clang-tidy"   ClangTidy.cmake)
check_include(FINN_CPP_CHECK    "cppcheck"     CppCheck.cmake)
check_include(FINN_IWYU         "iwyu"         IWYU.cmake)
list(POP_BACK CMAKE_MESSAGE_INDENT)    #indent -1

add_subdirectory(src)
