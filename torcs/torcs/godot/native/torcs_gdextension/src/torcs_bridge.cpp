/***************************************************************************

    file                 : torcs_bridge.cpp
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

#include "torcs_bridge.h"

#include <algorithm>
#include <cmath>

static double
clampDouble(double value, double low, double high)
{
	return std::max(low, std::min(value, high));
}

static TorcsBridgeInputState
defaultInput()
{
	TorcsBridgeInputState input{};
	input.gear = 1;
	input.brakeBalance = 0.5;
	return input;
}

static TorcsBridgeCarSnapshot
defaultCarSnapshot(const TorcsBridgeRaceConfig& config)
{
	TorcsBridgeCarSnapshot car{};
	car.id = 0;
	car.carId = config.carId.empty() ? "car1-trb1" : config.carId;
	car.godotPosition = TorcsBridgeTorcsToGodotPosition(car.torcsPosition);
	car.gear = 1;
	car.fuel = 1.0;
	car.input = defaultInput();
	return car;
}

TorcsBridgeInputState
TorcsBridgeClampInput(const TorcsBridgeInputState& input)
{
	TorcsBridgeInputState clamped = input;

	clamped.steer = clampDouble(clamped.steer, -1.0, 1.0);
	clamped.throttle = clampDouble(clamped.throttle, 0.0, 1.0);
	clamped.brake = clampDouble(clamped.brake, 0.0, 1.0);
	clamped.clutch = clampDouble(clamped.clutch, 0.0, 1.0);
	clamped.gear = std::max(-1, std::min(clamped.gear, 6));
	clamped.brakeBalance = clampDouble(clamped.brakeBalance, 0.0, 1.0);

	return clamped;
}

TorcsBridgeVec3
TorcsBridgeTorcsToGodotPosition(const TorcsBridgeVec3& position)
{
	return { position.x, position.z, position.y };
}

bool
TorcsRuntime::initialize(const TorcsBridgeRuntimeConfig& runtimeConfig)
{
	config = runtimeConfig;
	initialized = !config.dataRoot.empty();
	return initialized;
}

void
TorcsRuntime::shutdown()
{
	initialized = false;
	config = {};
}

bool
TorcsRuntime::isInitialized() const
{
	return initialized;
}

const TorcsBridgeRuntimeConfig&
TorcsRuntime::getConfig() const
{
	return config;
}

TorcsRace::TorcsRace(TorcsRuntime& torcsRuntime) :
	runtime(torcsRuntime),
	humanInput(defaultInput())
{
}

bool
TorcsRace::load(const TorcsBridgeRaceConfig& raceConfig)
{
	if (!runtime.isInitialized()) {
		return false;
	}

	config = raceConfig;
	loaded = true;
	accumulator = 0.0;
	snapshot = {};
	snapshot.cars.push_back(defaultCarSnapshot(config));

	return true;
}

void
TorcsRace::setHumanInput(int carIndex, const TorcsBridgeInputState& input)
{
	if (carIndex != 0) {
		return;
	}

	humanInput = TorcsBridgeClampInput(input);
}

TorcsBridgeSnapshot
TorcsRace::step(double seconds)
{
	snapshot.completedSubsteps = 0;

	if (!loaded || seconds <= 0.0) {
		return snapshot;
	}

	accumulator += seconds;
	while (accumulator >= TORCS_BRIDGE_SIM_STEP_SECONDS) {
		stepOneSubstep();
		accumulator -= TORCS_BRIDGE_SIM_STEP_SECONDS;
		snapshot.completedSubsteps++;
	}

	return snapshot;
}

const TorcsBridgeSnapshot&
TorcsRace::getSnapshot() const
{
	return snapshot;
}

void
TorcsRace::shutdown()
{
	loaded = false;
	accumulator = 0.0;
	config = {};
	snapshot = {};
	humanInput = defaultInput();
}

bool
TorcsRace::isLoaded() const
{
	return loaded;
}

void
TorcsRace::stepOneSubstep()
{
	TorcsBridgeCarSnapshot& car = snapshot.cars[0];
	const double dt = TORCS_BRIDGE_SIM_STEP_SECONDS;
	const double longitudinalAccel = humanInput.throttle * 8.0 - humanInput.brake * 12.0;
	const double newSpeed = std::max(0.0, car.speed + longitudinalAccel * dt);
	const double yawRate = humanInput.steer * std::min(newSpeed, 30.0) * 0.025;

	car.yaw += yawRate * dt;
	car.speed = newSpeed;
	car.rpm = 900.0 + car.speed * 120.0 + humanInput.throttle * 1800.0;
	car.gear = humanInput.gear;
	car.input = humanInput;
	car.torcsLinearVelocity = {
		std::cos(car.yaw) * car.speed,
		std::sin(car.yaw) * car.speed,
		0.0
	};
	car.torcsPosition.x += car.torcsLinearVelocity.x * dt;
	car.torcsPosition.y += car.torcsLinearVelocity.y * dt;
	car.godotPosition = TorcsBridgeTorcsToGodotPosition(car.torcsPosition);
	car.skid = std::abs(humanInput.steer) * car.speed * 0.02;

	for (TorcsBridgeWheelSnapshot& wheel : car.wheels) {
		wheel.spinVelocity = car.speed / 0.33;
		wheel.rideHeight = 0.12;
		wheel.slipSide = std::abs(humanInput.steer) * car.speed * 0.01;
		wheel.slipAccel = std::abs(longitudinalAccel) * 0.05;
		wheel.skid = car.skid;
		wheel.surfaceId = 0;
	}

	snapshot.raceTime += dt;
}
