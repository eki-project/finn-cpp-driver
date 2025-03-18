find_program(CPP_CHECK_FOUND cppcheck)
if(NOT CPP_CHECK_FOUND)
    message(WARNING "Cppcheck requested, but not found!")
endif()

set(CMAKE_CXX_CPPCHECK cppcheck
--suppress=unmatchedSuppression;
  --suppress=unmatchedSuppression:{};
  --suppress=missingInclude;
  --suppress=missingIncludeSystem;
  --suppress=checkersReport;
  --enable=all;
  --inline-suppr;
  --inconclusive;
  #-i <some/path2/ignore>;
)
