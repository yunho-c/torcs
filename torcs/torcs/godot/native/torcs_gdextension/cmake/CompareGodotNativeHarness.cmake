set(TORCS_BRIDGE_PARITY_SECONDS "0.2" CACHE STRING "Native parity run duration.")
set(TORCS_BRIDGE_PARITY_SAMPLE_SECONDS "0.02" CACHE STRING "Native parity sample period.")

if(NOT DEFINED TORCS_BRIDGE_HARNESS)
	message(FATAL_ERROR "TORCS_BRIDGE_HARNESS is required.")
endif()
if(NOT DEFINED TORCS_BRIDGE_GODOT_EXECUTABLE)
	message(FATAL_ERROR "TORCS_BRIDGE_GODOT_EXECUTABLE is required.")
endif()
if(NOT DEFINED TORCS_BRIDGE_GODOT_PROJECT_DIR)
	message(FATAL_ERROR "TORCS_BRIDGE_GODOT_PROJECT_DIR is required.")
endif()
if(NOT DEFINED TORCS_BRIDGE_OUTPUT_DIR)
	message(FATAL_ERROR "TORCS_BRIDGE_OUTPUT_DIR is required.")
endif()
if(NOT DEFINED TORCS_BRIDGE_PYTHON)
	message(FATAL_ERROR "TORCS_BRIDGE_PYTHON is required.")
endif()

file(MAKE_DIRECTORY "${TORCS_BRIDGE_OUTPUT_DIR}")
set(TORCS_BRIDGE_HARNESS_JSON "${TORCS_BRIDGE_OUTPUT_DIR}/retained-harness.json")
set(TORCS_BRIDGE_GODOT_JSON "${TORCS_BRIDGE_OUTPUT_DIR}/godot-native.json")

execute_process(
	COMMAND
		${TORCS_BRIDGE_HARNESS}
		--backend retained
		--seconds ${TORCS_BRIDGE_PARITY_SECONDS}
		--sample-seconds ${TORCS_BRIDGE_PARITY_SAMPLE_SECONDS}
		--format json
		--output ${TORCS_BRIDGE_HARNESS_JSON}
	RESULT_VARIABLE TORCS_BRIDGE_HARNESS_RESULT
)
if(NOT TORCS_BRIDGE_HARNESS_RESULT EQUAL 0)
	message(FATAL_ERROR "Retained harness parity capture failed with exit ${TORCS_BRIDGE_HARNESS_RESULT}.")
endif()

execute_process(
	COMMAND
		${CMAKE_COMMAND} -E env
		HOME=/private/tmp/torcs-godot-home
		${TORCS_BRIDGE_GODOT_EXECUTABLE}
		--headless
		--path ${TORCS_BRIDGE_GODOT_PROJECT_DIR}
		--script res://scripts/bridge_native_capture.gd
		--
		--seconds ${TORCS_BRIDGE_PARITY_SECONDS}
		--sample-seconds ${TORCS_BRIDGE_PARITY_SAMPLE_SECONDS}
		--output ${TORCS_BRIDGE_GODOT_JSON}
	RESULT_VARIABLE TORCS_BRIDGE_GODOT_RESULT
)
if(NOT TORCS_BRIDGE_GODOT_RESULT EQUAL 0)
	message(FATAL_ERROR "Godot native parity capture failed with exit ${TORCS_BRIDGE_GODOT_RESULT}.")
endif()

execute_process(
	COMMAND
		${TORCS_BRIDGE_PYTHON}
		${CMAKE_CURRENT_LIST_DIR}/compare_bridge_parity.py
		--harness ${TORCS_BRIDGE_HARNESS_JSON}
		--godot ${TORCS_BRIDGE_GODOT_JSON}
	RESULT_VARIABLE TORCS_BRIDGE_COMPARE_RESULT
)
if(NOT TORCS_BRIDGE_COMPARE_RESULT EQUAL 0)
	message(FATAL_ERROR "Godot native parity comparison failed with exit ${TORCS_BRIDGE_COMPARE_RESULT}.")
endif()
