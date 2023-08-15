find_program(CPP_CHECK_FOUND cppcheck)
if(NOT CPP_CHECK_FOUND)
    message(FATAL_ERROR "cppcheck not found")
endif()

set(CMAKE_CXX_CPPCHECK cppcheck
  --suppress=missingInclude;
  --enable=all;
  --inline-suppr;
  --inconclusive;
  #-i <some/path2/ignore>;
)
