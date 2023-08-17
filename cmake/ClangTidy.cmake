find_program(CLANG_TIDY_FOUND "clang-tidy")
if(NOT CLANG_TIDY_FOUND)
    message(WARNING "clang-tidy requested, but not found!")
endif()

set(CMAKE_CXX_CLANG_TIDY
  clang-tidy;
  -extra-arg=-Wno-unknown-warning-option;
)
