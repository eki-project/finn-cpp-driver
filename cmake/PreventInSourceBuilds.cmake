#
# This function will prevent in-source builds
#
function(myproject_assure_out_of_source_builds)
  # make sure the user doesn't play dirty with symlinks
  get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
  get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)

  # disallow in-source builds
  if("${srcdir}" STREQUAL "${bindir}")
    message("######################################################")
    message("Warning: in-source builds are disabled")
    message("Please create a separate build directory and run cmake from there")
    message("NOTE: Given that you already tried to make an in-source build")
    message("      CMake has already created several files & directories")
    message("      in your source tree. run 'git status' to find them and")
    message("      remove them by doing:")
    message("")
    message("      git clean -n -d")
    message("      git clean -f -d")
    message("      git checkout --")
    message("######################################################")
    message(FATAL_ERROR "Quitting configuration")
  endif()
endfunction()

myproject_assure_out_of_source_builds()