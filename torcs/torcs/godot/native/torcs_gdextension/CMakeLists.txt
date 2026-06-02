cmake_minimum_required(VERSION 3.20)

project(torcs_godot_bridge LANGUAGES CXX)

include(CTest)

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

set(TORCS_BRIDGE_DEFAULT_TORCS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(torcs_bridge_harness
	src/torcs_bridge_harness.cpp
)

target_link_libraries(torcs_bridge_harness
	PRIVATE
		torcs_bridge_core
)

target_compile_definitions(torcs_bridge_harness
	PRIVATE
		TORCS_BRIDGE_DEFAULT_DATA_ROOT="${TORCS_BRIDGE_DEFAULT_TORCS_ROOT}/data"
		TORCS_BRIDGE_DEFAULT_LOCAL_ROOT="${TORCS_BRIDGE_DEFAULT_TORCS_ROOT}"
		TORCS_BRIDGE_DEFAULT_LIBRARY_ROOT="${TORCS_BRIDGE_DEFAULT_TORCS_ROOT}"
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
