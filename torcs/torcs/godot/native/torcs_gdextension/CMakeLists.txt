cmake_minimum_required(VERSION 3.20)

project(torcs_godot_bridge LANGUAGES C CXX)

include(CTest)
include(CheckSymbolExists)

set(TORCS_BRIDGE_TORCS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
if(WIN32)
	set(TORCS_BRIDGE_OS_INCLUDE_DIR "${TORCS_BRIDGE_TORCS_ROOT}/src/windows")
else()
	set(TORCS_BRIDGE_OS_INCLUDE_DIR "${TORCS_BRIDGE_TORCS_ROOT}/src/linux")
endif()
option(TORCS_BRIDGE_ENABLE_RETAINED_CORE "Enable retained TORCS track/simuv2 prototype targets." OFF)
option(TORCS_BRIDGE_ENABLE_GDEXTENSION "Enable Godot GDExtension binding target." OFF)
include(cmake/TorcsRetainedCorePreflight.cmake)
include(cmake/TorcsGDExtensionPreflight.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

check_symbol_exists(strndup "string.h" TORCS_BRIDGE_HAVE_STRNDUP)

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
	PASS_REGULAR_EXPRESSION "godot_lvx,.*godot_avz,.*godot_yaw"
)

add_test(NAME torcs_bridge_harness_deterministic_csv
	COMMAND ${CMAKE_COMMAND}
		-DTORCS_BRIDGE_HARNESS=$<TARGET_FILE:torcs_bridge_harness>
		-DTORCS_BRIDGE_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/harness-determinism
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompareHarnessRuns.cmake
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

add_test(NAME torcs_bridge_harness_rejects_infinite_seconds
	COMMAND torcs_bridge_harness --seconds inf
)
set_tests_properties(torcs_bridge_harness_rejects_infinite_seconds PROPERTIES
	WILL_FAIL TRUE
)

add_test(NAME torcs_bridge_harness_json_smoke
	COMMAND torcs_bridge_harness --seconds 0.04 --sample-seconds 0.02 --format json
)
set_tests_properties(torcs_bridge_harness_json_smoke PROPERTIES
	PASS_REGULAR_EXPRESSION "\"godot_linear_velocity\".*\"godot_angular_velocity\".*\"godot_yaw\""
)

if(TORCS_BRIDGE_ENABLE_RETAINED_CORE AND TORCS_BRIDGE_RETAINED_CORE_AVAILABLE)
	set(TORCS_BRIDGE_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/torcs-generated-include")
	file(MAKE_DIRECTORY "${TORCS_BRIDGE_GENERATED_INCLUDE_DIR}/tmath")
	foreach(TORCS_BRIDGE_TMATH_HEADER
		linalg_t.h
		straight2_t.h
		v2_t.h
		v3_t.h
		v4_t.h
	)
		configure_file(
			${TORCS_BRIDGE_TORCS_ROOT}/src/libs/math/${TORCS_BRIDGE_TMATH_HEADER}
			${TORCS_BRIDGE_GENERATED_INCLUDE_DIR}/tmath/${TORCS_BRIDGE_TMATH_HEADER}
			COPYONLY
		)
	endforeach()

	set(TORCS_BRIDGE_TGF_SOURCES
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf/module.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf/os.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf/params.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf/tgf.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf/trace.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf/directory.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf/profiler.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf/hash.cpp
	)

	set(TORCS_BRIDGE_TXML_SOURCES
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/txml/xmltok.c
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/txml/xmlrole.c
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/txml/xmlparse.c
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/txml/hashtable.c
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/txml/xml.cpp
	)

	set(TORCS_BRIDGE_ROBOTTOOLS_SOURCES
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/robottools/rttrack.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/libs/robottools/rttelem.cpp
	)

	set(TORCS_BRIDGE_TRACK_SOURCES
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/track/track.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/track/trackitf.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/track/trackutil.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/track/track3.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/track/track4.cpp
	)

	set(TORCS_BRIDGE_SIMUV2_SOURCES
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/aero.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/atmosphere.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/axle.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/brake.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/car.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/categories.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/collide.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/differential.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/engine.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/simu.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/simuitf.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/steer.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/susp.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/transmission.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/wheel.cpp
	)

	set(TORCS_BRIDGE_SOLID_SOURCES
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Transform.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Convex.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Box.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Cone.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Cylinder.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Sphere.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Simplex.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Polygon.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Polyhedron.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Complex.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/BBoxTree.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Endpoint.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Object.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/Response.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/RespTable.cpp
		${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src/C-api.cpp
	)

	add_library(torcs_retained_core STATIC
		src/torcs_retained_adapter.cpp
		${TORCS_BRIDGE_TGF_SOURCES}
		${TORCS_BRIDGE_TXML_SOURCES}
		${TORCS_BRIDGE_ROBOTTOOLS_SOURCES}
		${TORCS_BRIDGE_TRACK_SOURCES}
		${TORCS_BRIDGE_SIMUV2_SOURCES}
		${TORCS_BRIDGE_SOLID_SOURCES}
	)

	target_include_directories(torcs_retained_core
		PUBLIC
			${CMAKE_CURRENT_SOURCE_DIR}/include
	)

	target_include_directories(torcs_retained_core
		SYSTEM PRIVATE
			${TORCS_BRIDGE_TORCS_ROOT}
			${TORCS_BRIDGE_GENERATED_INCLUDE_DIR}
			${TORCS_BRIDGE_OS_INCLUDE_DIR}
			${TORCS_BRIDGE_TORCS_ROOT}/src/interfaces
			${TORCS_BRIDGE_TORCS_ROOT}/src/libs/portability
			${TORCS_BRIDGE_TORCS_ROOT}/src/libs/tgf
			${TORCS_BRIDGE_TORCS_ROOT}/src/libs/txml
			${TORCS_BRIDGE_TORCS_ROOT}/src/libs/robottools
			${TORCS_BRIDGE_TORCS_ROOT}/src/modules/track
			${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2
			${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/include
			${TORCS_BRIDGE_TORCS_ROOT}/src/modules/simu/simuv2/SOLID-2.0/src
			${TORCS_BRIDGE_PLIB_INCLUDE_DIR}
	)

	if(TORCS_BRIDGE_HAVE_STRNDUP)
		target_compile_definitions(torcs_retained_core PRIVATE HAVE_STRNDUP=1)
	endif()

	target_link_libraries(torcs_retained_core
		PUBLIC
			torcs_bridge_core
		PRIVATE
			${TORCS_BRIDGE_PLIB_SG_LIBRARY}
			${TORCS_BRIDGE_PLIB_UL_LIBRARY}
	)

	if(MSVC)
		set_source_files_properties(src/torcs_retained_adapter.cpp PROPERTIES COMPILE_OPTIONS /W4)
	else()
		set_source_files_properties(src/torcs_retained_adapter.cpp PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic")
	endif()

	if(TORCS_BRIDGE_RETAINED_CORE_LINKABLE)
		add_executable(torcs_retained_smoke
			tests/torcs_retained_smoke.cpp
		)

		target_link_libraries(torcs_retained_smoke
			PRIVATE
				torcs_retained_core
		)

		target_compile_definitions(torcs_retained_smoke
			PRIVATE
				TORCS_BRIDGE_DEFAULT_DATA_ROOT="${TORCS_BRIDGE_TORCS_ROOT}/data"
				TORCS_BRIDGE_DEFAULT_LOCAL_ROOT="${TORCS_BRIDGE_TORCS_ROOT}"
				TORCS_BRIDGE_DEFAULT_LIBRARY_ROOT="${TORCS_BRIDGE_TORCS_ROOT}"
		)

		if(MSVC)
			target_compile_options(torcs_retained_smoke PRIVATE /W4)
		else()
			target_compile_options(torcs_retained_smoke PRIVATE -Wall -Wextra -Wpedantic)
		endif()

		add_test(NAME torcs_retained_smoke COMMAND torcs_retained_smoke)
	endif()
endif()
