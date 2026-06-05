/***************************************************************************

    file                 : register_types.cpp
    created              : Fri Jun 05 2026
    copyright            : (C) 2026 by TORCS Project
    email                : torcs@free.fr

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "register_types.h"

#include "torcs_bridge_native.h"

#include <godot_cpp/godot.hpp>

using godot::GDExtensionBinding;
using godot::ModuleInitializationLevel;

void
initialize_torcs_bridge_module(ModuleInitializationLevel level)
{
	if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	godot::ClassDB::register_class<TorcsBridgeNative>();
}

void
uninitialize_torcs_bridge_module(ModuleInitializationLevel level)
{
	if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {

GDExtensionBool GDE_EXPORT
torcs_bridge_library_init(
	GDExtensionInterfaceGetProcAddress getProcAddress,
	GDExtensionClassLibraryPtr library,
	GDExtensionInitialization* initialization)
{
	GDExtensionBinding::InitObject initObject(getProcAddress, library, initialization);
	initObject.register_initializer(initialize_torcs_bridge_module);
	initObject.register_terminator(uninitialize_torcs_bridge_module);
	initObject.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
	return initObject.init();
}

}
