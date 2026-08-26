# Runs `pb_cli describe <PIPELINE_NAME> --format=<FORMAT> --out=<OUT_FILE>`,
# verifies the executable exits with the expected status, and confirms the
# output file contains every substring listed in EXPECTED_TOKENS.
# Successful --out runs must keep stdout and stderr empty.
#
# Inputs:
#   CLI_PATH         absolute path to the pb_cli executable
#   PIPELINE_NAME    pipeline name argument passed to `describe`
#   FORMAT           "dot" | "json"
#   OUT_FILE         absolute path where the CLI writes its output
#   EXPECTED_TOKENS          semicolon-separated substrings that must appear
#                            in the generated output file
#   EXPECTED_ORDERED_TOKENS  optional semicolon-separated substrings that must
#                            appear in generated output in exactly that order
#   EXPECT_FAIL              (optional) if "ON", expect a non-zero exit code
#                            and skip content checks
#   EXPECTED_STDERR_TOKENS   optional semicolon-separated substrings expected
#                            in stderr when EXPECT_FAIL is ON
#
# All inputs must be passed via -D<NAME>=<VALUE>.

if(NOT DEFINED CLI_PATH)
  message(FATAL_ERROR "CLI_PATH is required")
endif()
if(NOT DEFINED PIPELINE_NAME)
  message(FATAL_ERROR "PIPELINE_NAME is required")
endif()
if(NOT DEFINED FORMAT)
  set(FORMAT "dot")
endif()
if(NOT DEFINED OUT_FILE)
  message(FATAL_ERROR "OUT_FILE is required")
endif()
if(NOT DEFINED EXPECT_FAIL)
  set(EXPECT_FAIL "OFF")
endif()

if(EXISTS "${OUT_FILE}")
  file(REMOVE "${OUT_FILE}")
endif()

execute_process(
  COMMAND "${CLI_PATH}" describe "${PIPELINE_NAME}" "--format=${FORMAT}" "--out=${OUT_FILE}"
  RESULT_VARIABLE cli_result
  OUTPUT_VARIABLE cli_stdout
  ERROR_VARIABLE cli_stderr
)

if(EXPECT_FAIL)
  if(cli_result EQUAL 0)
    message(FATAL_ERROR
      "Expected pb_cli describe '${PIPELINE_NAME}' to fail, but it succeeded.\n"
      "stdout: ${cli_stdout}\nstderr: ${cli_stderr}")
  endif()
  if(NOT cli_stdout STREQUAL "")
    message(FATAL_ERROR
      "pb_cli describe failure wrote to stdout.
"
      "stdout:
${cli_stdout}")
  endif()
  if(EXISTS "${OUT_FILE}")
    message(FATAL_ERROR
      "pb_cli describe failure unexpectedly produced '${OUT_FILE}'.\n"
      "stdout: ${cli_stdout}\nstderr: ${cli_stderr}")
  endif()
  if(DEFINED EXPECTED_STDERR_TOKENS)
    foreach(token IN LISTS EXPECTED_STDERR_TOKENS)
      string(FIND "${cli_stderr}" "${token}" found_at)
      if(found_at EQUAL -1)
        message(FATAL_ERROR
          "pb_cli describe failure is missing expected stderr token '${token}'.\n"
          "stderr:\n${cli_stderr}")
      endif()
    endforeach()
  endif()
  return()
endif()

if(NOT cli_result EQUAL 0)
  message(FATAL_ERROR
    "pb_cli describe '${PIPELINE_NAME}' failed with status ${cli_result}.\n"
    "stdout: ${cli_stdout}\nstderr: ${cli_stderr}")
endif()

if(NOT cli_stdout STREQUAL "")
  message(FATAL_ERROR
    "pb_cli describe '${PIPELINE_NAME}' wrote to stdout while --out was used.
"
    "stdout:
${cli_stdout}")
endif()

if(NOT cli_stderr STREQUAL "")
  message(FATAL_ERROR
    "pb_cli describe '${PIPELINE_NAME}' wrote to stderr on a successful --out run.
"
    "stderr:
${cli_stderr}")
endif()

if(NOT EXISTS "${OUT_FILE}")
  message(FATAL_ERROR "pb_cli describe did not produce '${OUT_FILE}'.")
endif()

file(READ "${OUT_FILE}" generated_output)

foreach(token IN LISTS EXPECTED_TOKENS)
  string(FIND "${generated_output}" "${token}" found_at)
  if(found_at EQUAL -1)
    message(FATAL_ERROR
      "pb_cli describe '${PIPELINE_NAME}' (--format=${FORMAT}) is missing expected token '${token}'.\n"
      "Output file: ${OUT_FILE}\n"
      "Output:\n${generated_output}")
  endif()
endforeach()

if(DEFINED EXPECTED_ORDERED_TOKENS)
  set(remaining_output "${generated_output}")
  foreach(token IN LISTS EXPECTED_ORDERED_TOKENS)
    string(FIND "${remaining_output}" "${token}" found_at)
    if(found_at EQUAL -1)
      message(FATAL_ERROR
        "pb_cli describe '${PIPELINE_NAME}' (--format=${FORMAT}) is missing expected ordered token '${token}' after the previous token.\n"
        "Output file: ${OUT_FILE}\n"
        "Output:\n${generated_output}")
    endif()
    string(LENGTH "${remaining_output}" remaining_length)
    string(LENGTH "${token}" token_length)
    math(EXPR next_at "${found_at} + ${token_length}")
    math(EXPR next_length "${remaining_length} - ${next_at}")
    string(SUBSTRING "${remaining_output}" ${next_at} ${next_length} remaining_output)
  endforeach()
endif()
