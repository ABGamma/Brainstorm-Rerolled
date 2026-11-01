# Runs the golden program and fails if its output differs from the expected
# file. Written as a CMake script so the test works identically on Windows,
# macOS and Linux (no dependency on `diff`).
# EMULATOR is set when cross-compiling (e.g. "wine" for MinGW builds).
execute_process(COMMAND ${EMULATOR} ${GOLDEN} OUTPUT_FILE ${OUT} RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "golden exited with code ${rc}")
endif()
file(READ ${EXPECTED} expected)
file(READ ${OUT} actual)
# Normalise Windows line endings so a CRLF checkout still compares equal.
string(REPLACE "\r\n" "\n" expected "${expected}")
string(REPLACE "\r\n" "\n" actual "${actual}")
if(NOT expected STREQUAL actual)
  message(FATAL_ERROR
    "Golden output differs from ${EXPECTED}.\n"
    "Actual output saved to ${OUT}; compare the two files to see which "
    "seeds or filters changed.")
endif()
message(STATUS "golden output matches")
