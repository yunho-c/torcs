set(TORCS_BRIDGE_GDEXTENSION_AVAILABLE OFF)
set(TORCS_BRIDGE_GDEXTENSION_MISSING "")
set(TORCS_BRIDGE_GODOT_CPP_ROOT "" CACHE PATH "Root directory of a Godot 4.6-compatible godot-cpp build/install.")

find_path(TORCS_BRIDGE_GODOT_CPP_INCLUDE_DIR
	NAMES godot_cpp/godot.hpp
	HINTS
		${TORCS_BRIDGE_GODOT_CPP_ROOT}
		${TORCS_BRIDGE_GODOT_CPP_ROOT}/include
	DOC "Directory containing godot-cpp public headers."
)

find_path(TORCS_BRIDGE_GODOT_CPP_GEN_INCLUDE_DIR
	NAMES godot_cpp/classes/ref_counted.hpp
	HINTS
		${TORCS_BRIDGE_GODOT_CPP_ROOT}
		${TORCS_BRIDGE_GODOT_CPP_ROOT}/gen/include
		${TORCS_BRIDGE_GODOT_CPP_ROOT}/include
	DOC "Directory containing generated godot-cpp class headers."
)

find_library(TORCS_BRIDGE_GODOT_CPP_LIBRARY
	NAMES godot-cpp libgodot-cpp
	HINTS
		${TORCS_BRIDGE_GODOT_CPP_ROOT}
		${TORCS_BRIDGE_GODOT_CPP_ROOT}/bin
		${TORCS_BRIDGE_GODOT_CPP_ROOT}/lib
	DOC "Built godot-cpp binding library."
)

if(NOT TORCS_BRIDGE_GODOT_CPP_INCLUDE_DIR)
	list(APPEND TORCS_BRIDGE_GDEXTENSION_MISSING "godot-cpp public headers")
endif()

if(NOT TORCS_BRIDGE_GODOT_CPP_GEN_INCLUDE_DIR)
	list(APPEND TORCS_BRIDGE_GDEXTENSION_MISSING "godot-cpp generated headers")
endif()

if(NOT TORCS_BRIDGE_GODOT_CPP_LIBRARY)
	list(APPEND TORCS_BRIDGE_GDEXTENSION_MISSING "godot-cpp library")
endif()

if(TORCS_BRIDGE_GDEXTENSION_MISSING)
	string(REPLACE ";" ", " TORCS_BRIDGE_GDEXTENSION_MISSING_TEXT "${TORCS_BRIDGE_GDEXTENSION_MISSING}")
	message(STATUS "GDExtension preflight: missing ${TORCS_BRIDGE_GDEXTENSION_MISSING_TEXT}; GDExtension target disabled.")
else()
	set(TORCS_BRIDGE_GDEXTENSION_AVAILABLE ON)
	message(STATUS "GDExtension preflight: godot-cpp dependencies found.")
endif()

if(TORCS_BRIDGE_ENABLE_GDEXTENSION AND NOT TORCS_BRIDGE_GDEXTENSION_AVAILABLE)
	message(FATAL_ERROR
		"TORCS_BRIDGE_ENABLE_GDEXTENSION is ON, but GDExtension dependencies are missing: "
		"${TORCS_BRIDGE_GDEXTENSION_MISSING_TEXT}. "
		"Build godot-cpp for the target Godot version or provide "
		"TORCS_BRIDGE_GODOT_CPP_INCLUDE_DIR, "
		"TORCS_BRIDGE_GODOT_CPP_GEN_INCLUDE_DIR, and "
		"TORCS_BRIDGE_GODOT_CPP_LIBRARY."
	)
endif()
