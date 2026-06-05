/***************************************************************************

    file                 : register_types.h
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

#ifndef _REGISTER_TYPES_H_
#define _REGISTER_TYPES_H_

#include <godot_cpp/core/class_db.hpp>

void initialize_torcs_bridge_module(godot::ModuleInitializationLevel level);
void uninitialize_torcs_bridge_module(godot::ModuleInitializationLevel level);

#endif /* _REGISTER_TYPES_H_ */
