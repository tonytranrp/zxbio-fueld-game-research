# Runs pb_cli describe with caller-supplied argument shape and validates status,
# stdout or output file tokens, and optional stderr tokens.
# Successful calls keep stderr empty; successful --out calls also keep stdout empty.

if(NOT DEFINED CLI_PATH)
  message(FATAL_ERROR "CLI_PATH is required")
endif()
if(NOT DEFINED PIPELINE_NAME)
  message(FATAL_ERROR "PIPELINE_NAME is required")
endif()
if(NOT DEFINED DESCRIBE_ARGS)
  set(DESCRIBE_ARGS)
endif()
if(NOT DEFINED EXPECT_FAIL)
  set(EXPECT_FAIL "OFF")
endif()

if(DEFINED OUT_FILE AND EXISTS "${OUT_FILE}")
  file(REMOVE "${OUT_FILE}")
endif()

execute_process(
  COMMAND "${CLI_PATH}" describe "${PIPELINE_NAME}" ${DESCRIBE_ARGS}
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
  if(DEFINED OUT_FILE AND EXISTS "${OUT_FILE}")
    message(FATAL_ERROR
      "pb_cli describe failure unexpectedly produced '${OUT_FILE}'.
"
      "stdout: ${cli_stdout}
stderr: ${cli_stderr}")
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

if(NOT cli_stderr STREQUAL "")
  message(FATAL_ERROR
    "pb_cli describe '${PIPELINE_NAME}' wrote to stderr on a successful run.
"
    "stderr:
${cli_stderr}")
endif()

set(generated_output "${cli_stdout}")
if(DEFINED OUT_FILE)
  if(NOT cli_stdout STREQUAL "")
    message(FATAL_ERROR
      "pb_cli describe '${PIPELINE_NAME}' wrote to stdout while --out was used.
"
      "stdout:
${cli_stdout}")
  endif()
  if(NOT EXISTS "${OUT_FILE}")
    message(FATAL_ERROR "pb_cli describe did not produce '${OUT_FILE}'.")
  endif()
  file(READ "${OUT_FILE}" generated_output)
endif()

foreach(token IN LISTS EXPECTED_TOKENS)
  string(FIND "${generated_output}" "${token}" found_at)
  if(found_at EQUAL -1)
    message(FATAL_ERROR
      "pb_cli describe '${PIPELINE_NAME}' is missing expected token '${token}'.\n"
      "Output:\n${generated_output}")
  endif()
endforeach()
