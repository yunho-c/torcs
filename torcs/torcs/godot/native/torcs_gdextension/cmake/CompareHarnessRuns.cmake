if(NOT DEFINED TORCS_BRIDGE_HARNESS)
	message(FATAL_ERROR "TORCS_BRIDGE_HARNESS is required.")
endif()

if(NOT DEFINED TORCS_BRIDGE_OUTPUT_DIR)
	message(FATAL_ERROR "TORCS_BRIDGE_OUTPUT_DIR is required.")
endif()

file(MAKE_DIRECTORY "${TORCS_BRIDGE_OUTPUT_DIR}")

set(first_output "${TORCS_BRIDGE_OUTPUT_DIR}/harness-first.csv")
set(second_output "${TORCS_BRIDGE_OUTPUT_DIR}/harness-second.csv")

execute_process(
	COMMAND "${TORCS_BRIDGE_HARNESS}" --seconds 0.12 --sample-seconds 0.02 --output "${first_output}"
	RESULT_VARIABLE first_result
)
if(NOT first_result EQUAL 0)
	message(FATAL_ERROR "First harness run failed with exit code ${first_result}.")
endif()

execute_process(
	COMMAND "${TORCS_BRIDGE_HARNESS}" --seconds 0.12 --sample-seconds 0.02 --output "${second_output}"
	RESULT_VARIABLE second_result
)
if(NOT second_result EQUAL 0)
	message(FATAL_ERROR "Second harness run failed with exit code ${second_result}.")
endif()

execute_process(
	COMMAND "${CMAKE_COMMAND}" -E compare_files "${first_output}" "${second_output}"
	RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
	message(FATAL_ERROR "Harness output differs between repeated runs.")
endif()
