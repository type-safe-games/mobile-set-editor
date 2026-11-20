# Run `magicseteditor --cli` on a set with commands from a file, and fail on any warning or error.
# Usage: cmake -DMSE=<exe> -DSET=<set> -DCOMMANDS=<file> [-DWORKDIR=<dir>] -P run_cli.cmake
if(NOT WORKDIR)
  set(WORKDIR ${CMAKE_CURRENT_BINARY_DIR})
endif()
file(MAKE_DIRECTORY ${WORKDIR})
execute_process(
  COMMAND ${MSE} --cli -q ${SET}
  INPUT_FILE ${COMMANDS}
  WORKING_DIRECTORY ${WORKDIR}
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out
  RESULT_VARIABLE rc
  TIMEOUT 120
)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "magicseteditor exited with ${rc}")
endif()
if(out MATCHES "WARNING|ERROR")
  message(FATAL_ERROR "magicseteditor reported warnings or errors")
endif()
