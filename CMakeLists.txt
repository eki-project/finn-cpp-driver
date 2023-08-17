cmake_minimum_required(VERSION 3.16)

include(cmake/PreventInSourceBuilds.cmake)

project(FinnCPPDriver)

# specify the C++ standard
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED True)
message(STATUS "Using C++ Standard ${CMAKE_CXX_STANDARD}")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "DEBUG")
  message(STATUS "Build type not specified: Use debug by default")
endif(NOT CMAKE_BUILD_TYPE)

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
  message("Finn C++ unittests")
  add_subdirectory(unittests)
endif()

#INCLUDES

#mdspan
include_directories(SYSTEM external/mdspan/include)

### Enable compiler warnings
option(FINNC_ENABLE_WARNINGS "Enable warnings" ON)
if (FINNC_ENABLE_WARNINGS)
  include(cmake/CompilerWarnings.cmake)
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

if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)
  set(SUPPORTS_UBSAN ON)
else()
  set(SUPPORTS_UBSAN OFF)
endif()


#Sanitizers & Checks

message(STATUS "Checks:")
list(APPEND CMAKE_MESSAGE_INDENT "  ") #indent +1
check_include(IPO          "InterproceduralOptimization" InterproceduralOptimization.cmake)
check_include(CLANG_FORMAT "clang-format" ClangFormat.cmake)
check_include(CLANG_TIDY   "clang-tidy"   ClangTidy.cmake)
check_include(CPP_CHECK    "cppcheck"     CppCheck.cmake)
check_include(IWYU         "iwyu"         IWYU.cmake)
list(POP_BACK CMAKE_MESSAGE_INDENT)    #indent -1


add_subdirectory(src)
