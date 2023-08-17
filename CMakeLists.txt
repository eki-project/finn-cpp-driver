cmake_minimum_required(VERSION 3.16)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/PreventInSourceBuilds.cmake)

project(FinnCPPDriver)

# specify the C++ standard
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
message(STATUS "Using C++ Standard ${CMAKE_CXX_STANDARD}")

include(cmake/StandardProjectSettings.cmake)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        main
)

# Build Unit tests
# Include before clang-tidy inclusion to exclude folder from clang-tidy

set(FINNC_ENABLE_TESTING "Build all tests (default is ON)" ON)
FetchContent_MakeAvailable(googletest)
include(cmake/AddUnittest.cmake)
enable_testing()

if (FINNC_ENABLE_TESTING)
  message(STATUS "Finn C++ unittests:")
  add_subdirectory(unittests)
endif()

#INCLUDES

#XRT
find_package(OpenCL REQUIRED)
find_package(XRT REQUIRED)

#OpenMP
find_package(OpenMP)

#mdspan
include_directories(SYSTEM external/mdspan/include)

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

if (CMAKE_BUILD_TYPE EQUAL "Release")
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

finncpp_enable_sanitizers(finnc_options
                          ${FINNC_ENABLE_SANITIZERS}
                          ${FINNC_ENABLE_SANITIZERS}
                          ${FINNC_ENABLE_SANITIZERS}
                          ${FINNC_ENABLE_THREAD_SANITIZERS}
                          ${FINNC_ENABLE_MEMORY_SANITIZERS})

message(STATUS "Checks:")
list(APPEND CMAKE_MESSAGE_INDENT "  ") #indent +1
check_include(IPO          "InterproceduralOptimization" InterproceduralOptimization.cmake)
check_include(CLANG_FORMAT "clang-format" ClangFormat.cmake)
check_include(CLANG_TIDY   "clang-tidy"   ClangTidy.cmake)
check_include(CPP_CHECK    "cppcheck"     CppCheck.cmake)
check_include(IWYU         "iwyu"         IWYU.cmake)
list(POP_BACK CMAKE_MESSAGE_INDENT)    #indent -1


add_subdirectory(src)
