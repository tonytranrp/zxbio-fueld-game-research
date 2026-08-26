# Runs `pb_cli <COMMAND>` (or caller-supplied COMMAND_ARGS) and checks stdout contains EXPECTED_TOKENS.
# Inputs via -D:
#   CLI_PATH         absolute path to the pb_cli executable
#   COMMAND          command argument passed to pb_cli (legacy single-arg form)
#   COMMAND_ARGS     optional semicolon-separated argument list passed to pb_cli
#   EXPECTED_TOKENS          semicolon-separated substrings expected in stdout
#   EXPECTED_ORDERED_TOKENS  optional semicolon-separated substrings expected
#                            in stdout in exactly that order
#   EXPECT_FAIL              optional: if ON, expect non-zero status and check stderr tokens
#   EXPECTED_STDERR_TOKENS   optional: semicolon-separated substrings expected in stderr on failure
#
# Successful commands must keep stderr empty; expected failures must keep stdout empty.

if(NOT DEFINED CLI_PATH)
  message(FATAL_ERROR "CLI_PATH is required")
endif()
if(NOT DEFINED COMMAND_ARGS)
  if(NOT DEFINED COMMAND)
    message(FATAL_ERROR "COMMAND or COMMAND_ARGS is required")
  endif()
  set(COMMAND_ARGS "${COMMAND}")
endif()
if(NOT DEFINED EXPECT_FAIL)
  set(EXPECT_FAIL "OFF")
endif()
if(NOT DEFINED EXPECTED_TOKENS AND NOT EXPECT_FAIL)
  message(FATAL_ERROR "EXPECTED_TOKENS is required")
endif()

execute_process(
  COMMAND "${CLI_PATH}" ${COMMAND_ARGS}
  RESULT_VARIABLE cli_result
  OUTPUT_VARIABLE cli_stdout
  ERROR_VARIABLE cli_stderr
)

if(EXPECT_FAIL)
  if(cli_result EQUAL 0)
    message(FATAL_ERROR
      "Expected pb_cli ${COMMAND_ARGS} to fail, but it succeeded.
"
      "stdout: ${cli_stdout}
stderr: ${cli_stderr}")
  endif()
  if(NOT cli_stdout STREQUAL "")
    message(FATAL_ERROR
      "pb_cli ${COMMAND_ARGS} wrote to stdout on an expected failure.
"
      "stdout:
${cli_stdout}")
  endif()
  if(DEFINED EXPECTED_STDERR_TOKENS)
    foreach(token IN LISTS EXPECTED_STDERR_TOKENS)
      string(FIND "${cli_stderr}" "${token}" found_at)
      if(found_at EQUAL -1)
        message(FATAL_ERROR
          "pb_cli ${COMMAND_ARGS} failure is missing expected stderr token '${token}'.
"
          "stderr:
${cli_stderr}")
      endif()
    endforeach()
  endif()
  return()
endif()

if(NOT cli_result EQUAL 0)
  message(FATAL_ERROR
    "pb_cli ${COMMAND_ARGS} failed with status ${cli_result}.
"
    "stdout: ${cli_stdout}
stderr: ${cli_stderr}")
endif()

if(NOT cli_stderr STREQUAL "")
  message(FATAL_ERROR
    "pb_cli ${COMMAND_ARGS} wrote to stderr on a successful run.
"
    "stderr:
${cli_stderr}")
endif()

foreach(token IN LISTS EXPECTED_TOKENS)
  string(FIND "${cli_stdout}" "${token}" found_at)
  if(found_at EQUAL -1)
    message(FATAL_ERROR
      "pb_cli ${COMMAND_ARGS} is missing expected token '${token}'.
"
      "Output:
${cli_stdout}")
  endif()
endforeach()

if(DEFINED EXPECTED_ORDERED_TOKENS)
  set(remaining_output "${cli_stdout}")
  foreach(token IN LISTS EXPECTED_ORDERED_TOKENS)
    string(FIND "${remaining_output}" "${token}" found_at)
    if(found_at EQUAL -1)
      message(FATAL_ERROR
        "pb_cli ${COMMAND_ARGS} is missing expected ordered token '${token}' after the previous token.
"
        "Output:
${cli_stdout}")
    endif()
    string(LENGTH "${remaining_output}" remaining_length)
    string(LENGTH "${token}" token_length)
    math(EXPR next_at "${found_at} + ${token_length}")
    math(EXPR next_length "${remaining_length} - ${next_at}")
    string(SUBSTRING "${remaining_output}" ${next_at} ${next_length} remaining_output)
  endforeach()
endif()
