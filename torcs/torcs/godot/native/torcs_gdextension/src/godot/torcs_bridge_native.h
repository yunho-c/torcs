/***************************************************************************

    file                 : torcs_bridge_native.h
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

#ifndef _TORCS_BRIDGE_NATIVE_H_
#define _TORCS_BRIDGE_NATIVE_H_

#include "torcs_retained_adapter.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>

class TorcsBridgeNative : public godot::RefCounted {
	GDCLASS(TorcsBridgeNative, godot::RefCounted);

protected:
	static void _bind_methods();

public:
	TorcsBridgeNative() = default;
	~TorcsBridgeNative() override;

	bool initialize(const godot::String& dataRoot, const godot::String& localRoot, const godot::String& libraryRoot);
	void shutdown();
	bool load(const godot::Dictionary& config);
	void set_human_input(int carIndex, const godot::Dictionary& input);
	godot::Dictionary step(double seconds);
	godot::Dictionary get_snapshot() const;

private:
	TorcsRetainedAdapter adapter;
};

#endif /* _TORCS_BRIDGE_NATIVE_H_ */
