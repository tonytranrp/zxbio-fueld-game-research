# Runs `pb_cli list`, verifies it exits 0, and confirms its stdout contains
# every substring listed in EXPECTED_TOKENS.  Used to assert that pipelines
# registered through pb::tooling::pipeline_registry surface in the CLI's
# `list` output (modeled on run_pb_cli_describe.cmake, but checks stdout
# instead of an --out file since `list` is a stdout-only command).
#
# Inputs (all via -D<NAME>=<VALUE>):
#   CLI_PATH         absolute path to the pb_cli executable
#   EXPECTED_TOKENS          semicolon-separated substrings that must appear
#                            in `pb_cli list` stdout
#   EXPECTED_ORDERED_TOKENS  optional semicolon-separated substrings that must
#                            appear in stdout in exactly that order
#
# stderr must remain empty: `list` is a successful stdout-only command.

if(NOT DEFINED CLI_PATH)
  message(FATAL_ERROR "CLI_PATH is required")
endif()
if(NOT DEFINED EXPECTED_TOKENS)
  message(FATAL_ERROR "EXPECTED_TOKENS is required")
endif()

execute_process(
  COMMAND "${CLI_PATH}" list
  RESULT_VARIABLE cli_result
  OUTPUT_VARIABLE cli_stdout
  ERROR_VARIABLE cli_stderr
)

if(NOT cli_result EQUAL 0)
  message(FATAL_ERROR
    "pb_cli list failed with status ${cli_result}.\n"
    "stdout: ${cli_stdout}\nstderr: ${cli_stderr}")
endif()

if(NOT cli_stderr STREQUAL "")
  message(FATAL_ERROR
    "pb_cli list wrote to stderr on a successful run.\n"
    "stderr:\n${cli_stderr}")
endif()

foreach(token IN LISTS EXPECTED_TOKENS)
  string(FIND "${cli_stdout}" "${token}" found_at)
  if(found_at EQUAL -1)
    message(FATAL_ERROR
      "pb_cli list is missing expected token '${token}'.\n"
      "Output:\n${cli_stdout}")
  endif()
endforeach()

if(DEFINED EXPECTED_ORDERED_TOKENS)
  set(remaining_output "${cli_stdout}")
  foreach(token IN LISTS EXPECTED_ORDERED_TOKENS)
    string(FIND "${remaining_output}" "${token}" found_at)
    if(found_at EQUAL -1)
      message(FATAL_ERROR
        "pb_cli list is missing expected ordered token '${token}' after the previous token.\n"
        "Output:\n${cli_stdout}")
    endif()
    string(LENGTH "${remaining_output}" remaining_length)
    string(LENGTH "${token}" token_length)
    math(EXPR next_at "${found_at} + ${token_length}")
    math(EXPR next_length "${remaining_length} - ${next_at}")
    string(SUBSTRING "${remaining_output}" ${next_at} ${next_length} remaining_output)
  endforeach()
endif()
