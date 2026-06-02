cmake_minimum_required(VERSION 3.20)

project(torcs_godot_bridge LANGUAGES CXX)

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
