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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

static int
fail(const std::string& message)
{
	std::cerr << "FAIL: " << message << "\n";
	return 1;
}

static TorcsBridgeRuntimeConfig
defaultRuntimeConfig()
{
	return {
		TORCS_BRIDGE_DEFAULT_DATA_ROOT,
		TORCS_BRIDGE_DEFAULT_LOCAL_ROOT,
		TORCS_BRIDGE_DEFAULT_LIBRARY_ROOT
	};
}

static TorcsBridgeRaceConfig
defaultRaceConfig()
{
	return {
		"data/tracks/road/wheel-2/wheel-2.xml",
		"data/cars/models/car1-trb1/car1-trb1.xml",
		"car1-trb1",
		0
	};
}

static bool
loadDefaultRace(TorcsRetainedAdapter* adapter)
{
	return adapter->initialize(defaultRuntimeConfig())
		&& adapter->loadOneCarFreeDrive(defaultRaceConfig());
}

static TorcsBridgeInputState
makeInput(double steer, double throttle, double brake)
{
	TorcsBridgeInputState input{};
	input.steer = steer;
	input.throttle = throttle;
	input.brake = brake;
	input.gear = 1;
	input.brakeBalance = 0.5;
	return input;
}

static TorcsBridgeSnapshot
advance(TorcsRetainedAdapter* adapter, const TorcsBridgeInputState& input, double seconds)
{
	TorcsBridgeSnapshot snapshot = adapter->getSnapshot();
	adapter->setHumanInput(input);
	for (double elapsed = 0.0; elapsed < seconds;) {
		const double stepSeconds = std::min(TORCS_BRIDGE_ROBOT_STEP_SECONDS, seconds - elapsed);
		snapshot = adapter->step(stepSeconds);
		elapsed += stepSeconds;
	}
	return snapshot;
}

static bool
nearlyEqual(double left, double right)
{
	return std::fabs(left - right) <= 1e-7;
}

static bool
compareSnapshots(const TorcsBridgeSnapshot& left, const TorcsBridgeSnapshot& right)
{
	if (!nearlyEqual(left.raceTime, right.raceTime)
		|| left.completedSubsteps != right.completedSubsteps
		|| left.cars.size() != right.cars.size()) {
		return false;
	}

	if (left.cars.empty()) {
		return true;
	}

	const TorcsBridgeCarSnapshot& leftCar = left.cars[0];
	const TorcsBridgeCarSnapshot& rightCar = right.cars[0];
	return nearlyEqual(leftCar.torcsPosition.x, rightCar.torcsPosition.x)
		&& nearlyEqual(leftCar.torcsPosition.y, rightCar.torcsPosition.y)
		&& nearlyEqual(leftCar.torcsPosition.z, rightCar.torcsPosition.z)
		&& nearlyEqual(leftCar.torcsLinearVelocity.x, rightCar.torcsLinearVelocity.x)
		&& nearlyEqual(leftCar.torcsLinearVelocity.y, rightCar.torcsLinearVelocity.y)
		&& nearlyEqual(leftCar.yaw, rightCar.yaw)
		&& nearlyEqual(leftCar.speed, rightCar.speed)
		&& leftCar.gear == rightCar.gear;
}

static bool
runDeterministicSequence(std::vector<TorcsBridgeSnapshot>* samples)
{
	TorcsRetainedAdapter adapter;
	if (!loadDefaultRace(&adapter)) {
		return false;
	}

	samples->push_back(advance(&adapter, makeInput(0.0, 0.85, 0.0), 0.5));
	samples->push_back(advance(&adapter, makeInput(0.35, 0.85, 0.0), 0.5));
	samples->push_back(advance(&adapter, makeInput(-0.20, 0.65, 0.0), 0.5));
	samples->push_back(advance(&adapter, makeInput(0.0, 0.0, 0.45), 0.5));
	adapter.shutdown();
	return true;
}

int
main()
{
	TorcsRetainedAdapter adapter;
	if (!loadDefaultRace(&adapter)) {
		return fail("retained adapter loads wheel-2");
	}

	const TorcsBridgeSnapshot snapshot = adapter.getSnapshot();
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

	const TorcsBridgeSnapshot acceleratedSnapshot = advance(&adapter, makeInput(0.0, 0.85, 0.0), 1.0);
	if (acceleratedSnapshot.cars.empty()
		|| acceleratedSnapshot.cars[0].speed <= steppedSnapshot.cars[0].speed + 0.1) {
		return fail("retained throttle increases speed");
	}

	const TorcsBridgeSnapshot brakedSnapshot = advance(&adapter, makeInput(0.0, 0.0, 0.65), 1.0);
	if (brakedSnapshot.cars.empty()
		|| brakedSnapshot.cars[0].speed >= acceleratedSnapshot.cars[0].speed - 0.1) {
		return fail("retained brake reduces speed after acceleration");
	}

	adapter.shutdown();
	if (adapter.isLoaded()) {
		return fail("retained adapter unloads on shutdown");
	}

	TorcsRetainedAdapter steeringAdapter;
	if (!loadDefaultRace(&steeringAdapter)) {
		return fail("retained steering adapter loads wheel-2");
	}
	const TorcsBridgeSnapshot steeringStart = steeringAdapter.getSnapshot();
	const TorcsBridgeSnapshot steeringSnapshot = advance(&steeringAdapter, makeInput(0.45, 0.85, 0.0), 1.5);
	if (steeringSnapshot.cars.empty() || steeringStart.cars.empty()) {
		return fail("retained steering snapshots are present");
	}
	const TorcsBridgeCarSnapshot& steeringStartCar = steeringStart.cars[0];
	const TorcsBridgeCarSnapshot& steeringCar = steeringSnapshot.cars[0];
	const double yawDelta = std::fabs(steeringCar.yaw - steeringStartCar.yaw);
	const double lateralDelta = std::fabs(steeringCar.torcsPosition.y - steeringStartCar.torcsPosition.y);
	if (yawDelta <= 1e-4 && lateralDelta <= 1e-3) {
		return fail("retained steering changes yaw or track position");
	}
	steeringAdapter.shutdown();

	std::vector<TorcsBridgeSnapshot> firstRun;
	std::vector<TorcsBridgeSnapshot> secondRun;
	if (!runDeterministicSequence(&firstRun) || !runDeterministicSequence(&secondRun)) {
		return fail("retained deterministic sequence loads");
	}
	if (firstRun.size() != secondRun.size()) {
		return fail("retained deterministic sequence sample count matches");
	}
	for (size_t i = 0; i < firstRun.size(); i++) {
		if (!compareSnapshots(firstRun[i], secondRun[i])) {
			return fail("retained repeated runs are deterministic");
		}
	}

	return 0;
}
