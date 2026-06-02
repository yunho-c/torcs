/***************************************************************************

    file                 : torcs_retained_adapter.h
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

#ifndef _TORCS_RETAINED_ADAPTER_H_
#define _TORCS_RETAINED_ADAPTER_H_

#include "torcs_bridge.h"

class TorcsRetainedAdapter {
public:
	bool initialize(const TorcsBridgeRuntimeConfig& config);
	bool loadOneCarFreeDrive(const TorcsBridgeRaceConfig& config);
	void setHumanInput(const TorcsBridgeInputState& input);
	TorcsBridgeSnapshot step(double seconds);
	const TorcsBridgeSnapshot& getSnapshot() const;
	void shutdown();
	bool isLoaded() const;

private:
	void unloadRace();

	TorcsBridgeRuntimeConfig runtimeConfig;
	TorcsBridgeRaceConfig raceConfig;
	TorcsBridgeInputState input{};
	TorcsBridgeSnapshot snapshot;
	void *track = nullptr;
	void *carHandle = nullptr;
	void *raceInfo = nullptr;
	double accumulator = 0.0;
	bool simulationStarted = false;
	bool initialized = false;
	bool loaded = false;
};

#endif /* _TORCS_RETAINED_ADAPTER_H_ */
