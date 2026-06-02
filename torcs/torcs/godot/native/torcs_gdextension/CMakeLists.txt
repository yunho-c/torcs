cmake_minimum_required(VERSION 3.20)

project(torcs_godot_bridge LANGUAGES CXX)

include(CTest)

set(TORCS_BRIDGE_TORCS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
option(TORCS_BRIDGE_ENABLE_RETAINED_CORE "Enable retained TORCS track/simuv2 prototype targets." OFF)
include(cmake/TorcsRetainedCorePreflight.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(torcs_bridge_core STATIC
	src/torcs_bridge.cpp
)

target_include_directories(torcs_bridge_core
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
	target_compile_options(torcs_bridge_core PRIVATE /W4)
else()
	target_compile_options(torcs_bridge_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(torcs_bridge_harness
	src/torcs_bridge_harness.cpp
)

target_link_libraries(torcs_bridge_harness
	PRIVATE
		torcs_bridge_core
)

target_compile_definitions(torcs_bridge_harness
	PRIVATE
		TORCS_BRIDGE_DEFAULT_DATA_ROOT="${TORCS_BRIDGE_TORCS_ROOT}/data"
		TORCS_BRIDGE_DEFAULT_LOCAL_ROOT="${TORCS_BRIDGE_TORCS_ROOT}"
		TORCS_BRIDGE_DEFAULT_LIBRARY_ROOT="${TORCS_BRIDGE_TORCS_ROOT}"
)

if(MSVC)
	target_compile_options(torcs_bridge_harness PRIVATE /W4)
else()
	target_compile_options(torcs_bridge_harness PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(torcs_bridge_tests
	tests/torcs_bridge_tests.cpp
)

target_link_libraries(torcs_bridge_tests
	PRIVATE
		torcs_bridge_core
)

if(MSVC)
	target_compile_options(torcs_bridge_tests PRIVATE /W4)
else()
	target_compile_options(torcs_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME torcs_bridge_tests COMMAND torcs_bridge_tests)

add_test(NAME torcs_bridge_harness_help COMMAND torcs_bridge_harness --help)
set_tests_properties(torcs_bridge_harness_help PROPERTIES
	PASS_REGULAR_EXPRESSION "Usage:"
)

add_test(NAME torcs_bridge_harness_csv_smoke
	COMMAND torcs_bridge_harness --seconds 0.04 --sample-seconds 0.02
)
set_tests_properties(torcs_bridge_harness_csv_smoke PROPERTIES
	PASS_REGULAR_EXPRESSION "time,substeps"
)

add_test(NAME torcs_bridge_harness_bounded_duration
	COMMAND torcs_bridge_harness --seconds 0.01 --sample-seconds 0.02
)
set_tests_properties(torcs_bridge_harness_bounded_duration PROPERTIES
	PASS_REGULAR_EXPRESSION "0.010000,5,"
)

add_test(NAME torcs_bridge_harness_race_config
	COMMAND torcs_bridge_harness --seconds 0.01 --track-xml custom-track.xml --car-xml custom-car.xml --car-id custom-car --laps 3
)
set_tests_properties(torcs_bridge_harness_race_config PROPERTIES
	PASS_REGULAR_EXPRESSION "custom-car"
)

add_test(NAME torcs_bridge_harness_json_smoke
	COMMAND torcs_bridge_harness --seconds 0.04 --sample-seconds 0.02 --format json
)
set_tests_properties(torcs_bridge_harness_json_smoke PROPERTIES
	PASS_REGULAR_EXPRESSION "\"samples\""
)

if(TORCS_BRIDGE_ENABLE_RETAINED_CORE AND TORCS_BRIDGE_RETAINED_CORE_AVAILABLE)
	add_library(torcs_retained_core STATIC
		src/torcs_retained_adapter.cpp
	)

	target_include_directories(torcs_retained_core
		PUBLIC
			${CMAKE_CURRENT_SOURCE_DIR}/include
		PRIVATE
			${TORCS_BRIDGE_TORCS_ROOT}
			${TORCS_BRIDGE_TORCS_ROOT}/src/interfaces
			${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf
			${TORCS_BRIDGE_TORCS_ROOT}/src/libs/robottools
			${TORCS_BRIDGE_TORCS_ROOT}/src/modules/track
			${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2
			${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/include
			${TORCS_BRIDGE_PLIB_INCLUDE_DIR}
	)

	target_link_libraries(torcs_retained_core
		PUBLIC
			torcs_bridge_core
		PRIVATE
			${TORCS_BRIDGE_PLIB_SG_LIBRARY}
			${TORCS_BRIDGE_PLIB_UL_LIBRARY}
	)

	if(MSVC)
		target_compile_options(torcs_retained_core PRIVATE /W4)
	else()
		target_compile_options(torcs_retained_core PRIVATE -Wall -Wextra -Wpedantic)
	endif()
endif()
