/***************************************************************************

    file                 : torcs_retained_adapter.cpp
    created              : Tue Jun 02 2026
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

#include "torcs_retained_adapter.h"

/*
 * This file is compiled only when TORCS_BRIDGE_ENABLE_RETAINED_CORE is ON and
 * the retained-core dependency preflight passes. The next implementation step
 * is to include the TORCS interfaces here, then replace this lifecycle skeleton
 * with direct calls to GfInit, TrackBuildv1, SimInit, SimConfig, SimUpdate, and
 * SimShutdown as documented in TORCS_CORE_SURVEY.md.
 */

bool
TorcsRetainedAdapter::initialize(const TorcsBridgeRuntimeConfig& config)
{
	runtimeConfig = config;
	initialized = !runtimeConfig.dataRoot.empty();
	return initialized;
}

bool
TorcsRetainedAdapter::loadOneCarFreeDrive(const TorcsBridgeRaceConfig& config)
{
	if (!initialized) {
		return false;
	}

	raceConfig = config;
	loaded = false;
	snapshot = {};
	return false;
}

void
TorcsRetainedAdapter::setHumanInput(const TorcsBridgeInputState& bridgeInput)
{
	input = TorcsBridgeClampInput(bridgeInput);
}

TorcsBridgeSnapshot
TorcsRetainedAdapter::step(double /* seconds */)
{
	return snapshot;
}

const TorcsBridgeSnapshot&
TorcsRetainedAdapter::getSnapshot() const
{
	return snapshot;
}

void
TorcsRetainedAdapter::shutdown()
{
	runtimeConfig = {};
	raceConfig = {};
	input = {};
	snapshot = {};
	initialized = false;
	loaded = false;
}

bool
TorcsRetainedAdapter::isLoaded() const
{
	return loaded;
}
