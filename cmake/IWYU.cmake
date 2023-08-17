find_program(IWYU_FOUND "include-what-you-use")
if(NOT IWYU_FOUND)
    message(WARNING "include-what-you-use requested, but not found!")
endif()

set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
  include-what-you-use
  -Xiwyu
  --mapping_file=${PROJECT_SOURCE_DIR}/iwyu.imp
)