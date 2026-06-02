/***************************************************************************

    file                 : torcs_retained_smoke.cpp
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

#include <cmath>
#include <iostream>
#include <string>

static int
fail(const std::string& message)
{
	std::cerr << "FAIL: " << message << "\n";
	return 1;
}

int
main()
{
	TorcsRetainedAdapter adapter;
	if (!adapter.initialize({
		TORCS_BRIDGE_DEFAULT_DATA_ROOT,
		TORCS_BRIDGE_DEFAULT_LOCAL_ROOT,
		TORCS_BRIDGE_DEFAULT_LIBRARY_ROOT
	})) {
		return fail("retained adapter initializes");
	}

	if (!adapter.loadOneCarFreeDrive({
		"data/tracks/road/wheel-2/wheel-2.xml",
		"data/cars/models/car1-trb1/car1-trb1.xml",
		"car1-trb1",
		0
	})) {
		return fail("retained adapter loads wheel-2");
	}

	const TorcsBridgeSnapshot& snapshot = adapter.getSnapshot();
	if (!adapter.isLoaded()) {
		return fail("retained adapter reports loaded");
	}
	if (snapshot.track.trackId != "wheel-2") {
		return fail("track id is wheel-2");
	}
	if (!std::isfinite(snapshot.track.length) || snapshot.track.length <= 0.0) {
		return fail("track length is positive and finite");
	}
	if (!std::isfinite(snapshot.track.width) || snapshot.track.width <= 0.0) {
		return fail("track width is positive and finite");
	}
	if (snapshot.track.debugPoints.size() < 2) {
		return fail("track debug points were copied");
	}
	if (snapshot.cars.size() != 1 || snapshot.cars[0].carId != "car1-trb1") {
		return fail("sim car snapshot is present");
	}
	if (!std::isfinite(snapshot.cars[0].torcsPosition.x)
		|| !std::isfinite(snapshot.cars[0].torcsPosition.y)
		|| !std::isfinite(snapshot.cars[0].torcsPosition.z)) {
		return fail("sim car position is finite");
	}
	if (!std::isfinite(snapshot.cars[0].wheels[0].rideHeight)) {
		return fail("sim wheel state is finite");
	}
	if (std::fabs(snapshot.cars[0].fuel - 94.0) > 1e-6) {
		return fail("car/category XML merge provides initial fuel");
	}

	TorcsBridgeInputState input{};
	input.throttle = 1.0;
	input.gear = 1;
	input.brakeBalance = 0.5;
	adapter.setHumanInput(input);

	const TorcsBridgeSnapshot steppedSnapshot = adapter.step(TORCS_BRIDGE_ROBOT_STEP_SECONDS);
	if (steppedSnapshot.completedSubsteps != 10) {
		return fail("retained step advances ten simulation substeps");
	}
	if (std::fabs(steppedSnapshot.raceTime - TORCS_BRIDGE_ROBOT_STEP_SECONDS) > 1e-9) {
		return fail("retained step advances race time");
	}
	if (steppedSnapshot.cars.empty() || !std::isfinite(steppedSnapshot.cars[0].speed)) {
		return fail("retained step refreshes finite car snapshot");
	}

	adapter.shutdown();
	if (adapter.isLoaded()) {
		return fail("retained adapter unloads on shutdown");
	}

	return 0;
}
