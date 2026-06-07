/***************************************************************************

    file                 : torcs_bridge_tests.cpp
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

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

static int Failures = 0;

static void
expectTrue(bool condition, const std::string& message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << "\n";
		Failures++;
	}
}

static void
expectNear(double actual, double expected, double tolerance, const std::string& message)
{
	if (std::fabs(actual - expected) > tolerance) {
		std::cerr
			<< "FAIL: " << message
			<< " actual=" << actual
			<< " expected=" << expected
			<< " tolerance=" << tolerance << "\n";
		Failures++;
	}
}

static void
testInputClamp()
{
	TorcsBridgeInputState input{};
	input.steer = 2.0;
	input.throttle = -1.0;
	input.brake = 3.0;
	input.clutch = -0.5;
	input.gear = 99;
	input.brakeBalance = 2.0;

	const TorcsBridgeInputState clamped = TorcsBridgeClampInput(input);
	expectNear(clamped.steer, 1.0, 0.0, "steer clamps high");
	expectNear(clamped.throttle, 0.0, 0.0, "throttle clamps low");
	expectNear(clamped.brake, 1.0, 0.0, "brake clamps high");
	expectNear(clamped.clutch, 0.0, 0.0, "clutch clamps low");
	expectTrue(clamped.gear == 6, "gear clamps high");
	expectNear(clamped.brakeBalance, 1.0, 0.0, "brake balance clamps high");
}

static void
testNonFiniteInputClamp()
{
	TorcsBridgeInputState input{};
	input.steer = std::numeric_limits<double>::quiet_NaN();
	input.throttle = std::numeric_limits<double>::infinity();
	input.brake = -std::numeric_limits<double>::infinity();
	input.clutch = std::numeric_limits<double>::quiet_NaN();
	input.gear = 1;
	input.brakeBalance = std::numeric_limits<double>::quiet_NaN();

	const TorcsBridgeInputState clamped = TorcsBridgeClampInput(input);
	expectNear(clamped.steer, 0.0, 0.0, "NaN steer falls back to neutral");
	expectNear(clamped.throttle, 1.0, 0.0, "infinite throttle clamps high");
	expectNear(clamped.brake, 0.0, 0.0, "negative infinite brake clamps low");
	expectNear(clamped.clutch, 0.0, 0.0, "NaN clutch falls back to neutral");
	expectNear(clamped.brakeBalance, 0.5, 0.0, "NaN brake balance falls back to default");

	TorcsRuntime runtime;
	expectTrue(runtime.initialize({ "data", ".", "." }), "runtime initializes for non-finite input");

	TorcsRace race(runtime);
	expectTrue(race.load({ "track.xml", "car.xml", "test-car", 0 }), "race loads for non-finite input");
	race.setHumanInput(0, input);

	const TorcsBridgeSnapshot snapshot = race.step(TORCS_BRIDGE_SIM_STEP_SECONDS);
	expectTrue(std::isfinite(snapshot.cars[0].torcsPosition.x), "non-finite input keeps finite X position");
	expectTrue(std::isfinite(snapshot.cars[0].torcsPosition.y), "non-finite input keeps finite Y position");
	expectTrue(std::isfinite(snapshot.cars[0].yaw), "non-finite input keeps finite yaw");
	expectTrue(std::isfinite(snapshot.cars[0].torcsAngularVelocity.z),
		"non-finite input keeps finite angular velocity");
}

static void
testCoordinateMapping()
{
	const TorcsBridgeVec3 torcsPosition{ 1.0, 2.0, 3.0 };
	const TorcsBridgeVec3 godotPosition = TorcsBridgeTorcsToGodotPosition(torcsPosition);

	expectNear(godotPosition.x, 1.0, 0.0, "Godot X uses TORCS X");
	expectNear(godotPosition.y, 3.0, 0.0, "Godot Y uses TORCS Z");
	expectNear(godotPosition.z, 2.0, 0.0, "Godot Z uses TORCS Y");

	const TorcsBridgeVec3 torcsVelocity{ 4.0, 5.0, 6.0 };
	const TorcsBridgeVec3 godotVelocity = TorcsBridgeTorcsToGodotLinearVelocity(torcsVelocity);
	expectNear(godotVelocity.x, 4.0, 0.0, "Godot linear velocity X uses TORCS X");
	expectNear(godotVelocity.y, 6.0, 0.0, "Godot linear velocity Y uses TORCS Z");
	expectNear(godotVelocity.z, 5.0, 0.0, "Godot linear velocity Z uses TORCS Y");

	const TorcsBridgeVec3 torcsAngularVelocity{ 7.0, 8.0, 9.0 };
	const TorcsBridgeVec3 godotAngularVelocity =
		TorcsBridgeTorcsToGodotAngularVelocity(torcsAngularVelocity);
	expectNear(godotAngularVelocity.x, -7.0, 0.0,
		"Godot angular velocity X flips TORCS roll sign");
	expectNear(godotAngularVelocity.y, -9.0, 0.0,
		"Godot angular velocity Y maps negative TORCS yaw");
	expectNear(godotAngularVelocity.z, -8.0, 0.0,
		"Godot angular velocity Z flips TORCS pitch sign");

	expectNear(TorcsBridgeTorcsToGodotYaw(0.0), -1.5707963267948966, 1e-12,
		"TORCS zero yaw maps to Godot forward down +X");
	expectNear(TorcsBridgeTorcsToGodotYaw(0.25), -1.8207963267948966, 1e-12,
		"Godot yaw decreases as TORCS yaw increases");
}

static void
testSubstepAccumulator()
{
	TorcsRuntime runtime;
	expectTrue(runtime.initialize({ "data", ".", "." }), "runtime initializes with data root");

	TorcsRace race(runtime);
	expectTrue(race.load({ "track.xml", "car.xml", "test-car", 0 }), "race loads after runtime init");

	TorcsBridgeInputState input{};
	input.throttle = 1.0;
	input.gear = 1;
	race.setHumanInput(0, input);

	TorcsBridgeSnapshot snapshot = race.step(0.001);
	expectTrue(snapshot.completedSubsteps == 0, "partial step does not advance substeps");
	expectNear(snapshot.raceTime, 0.0, 0.0, "partial step keeps race time");

	snapshot = race.step(0.001);
	expectTrue(snapshot.completedSubsteps == 1, "accumulated partial steps advance one substep");
	expectNear(snapshot.raceTime, TORCS_BRIDGE_SIM_STEP_SECONDS, 1e-12, "one substep advances race time");

	snapshot = race.step(TORCS_BRIDGE_ROBOT_STEP_SECONDS);
	expectTrue(snapshot.completedSubsteps == 10, "robot step advances ten simulation substeps");
	expectNear(snapshot.raceTime, 0.022, 1e-12, "race time includes previous and robot substeps");
	expectTrue(!snapshot.cars.empty(), "snapshot includes one car");
	expectTrue(snapshot.cars[0].speed > 0.0, "throttle increases speed");
	expectNear(snapshot.cars[0].torcsAngularVelocity.z, 0.0, 1e-12,
		"straight input keeps zero yaw angular velocity");
	expectNear(snapshot.cars[0].godotPosition.y, snapshot.cars[0].torcsPosition.z, 1e-12,
		"snapshot stores mapped Godot Y");
	expectNear(snapshot.cars[0].godotLinearVelocity.z, snapshot.cars[0].torcsLinearVelocity.y, 1e-12,
		"snapshot stores mapped Godot linear velocity Z");
	expectNear(snapshot.cars[0].godotYaw, TorcsBridgeTorcsToGodotYaw(snapshot.cars[0].yaw), 1e-12,
		"snapshot stores mapped Godot yaw");
}

static void
testAngularVelocitySnapshot()
{
	TorcsRuntime runtime;
	expectTrue(runtime.initialize({ "data", ".", "." }), "runtime initializes for angular velocity");

	TorcsRace race(runtime);
	expectTrue(race.load({ "track.xml", "car.xml", "test-car", 0 }), "race loads for angular velocity");

	TorcsBridgeInputState input{};
	input.throttle = 1.0;
	input.steer = 1.0;
	input.gear = 1;
	race.setHumanInput(0, input);

	const TorcsBridgeSnapshot snapshot = race.step(TORCS_BRIDGE_SIM_STEP_SECONDS);
	expectTrue(snapshot.cars[0].torcsAngularVelocity.z > 0.0,
		"steering input writes positive yaw angular velocity");
	expectNear(snapshot.cars[0].godotAngularVelocity.y,
		-snapshot.cars[0].torcsAngularVelocity.z,
		1e-12,
		"snapshot maps TORCS yaw rate to negative Godot Y angular velocity");
	expectNear(snapshot.cars[0].torcsAngularVelocity.x, 0.0, 1e-12,
		"placeholder angular velocity has no roll rate");
	expectNear(snapshot.cars[0].torcsAngularVelocity.y, 0.0, 1e-12,
		"placeholder angular velocity has no pitch rate");
}

static void
testTrackDebugSnapshot()
{
	TorcsRuntime runtime;
	expectTrue(runtime.initialize({ "data", ".", "." }), "runtime initializes for track snapshot");

	TorcsRace race(runtime);
	expectTrue(race.load({ "data/tracks/road/wheel-2/wheel-2.xml", "car.xml", "test-car", 0 }),
		"race loads for track snapshot");

	const TorcsBridgeSnapshot& snapshot = race.getSnapshot();
	expectTrue(!snapshot.track.debugPoints.empty(), "track snapshot includes debug points");
	expectNear(snapshot.track.width, 10.0, 0.0, "track snapshot stores debug width");
	expectTrue(snapshot.track.debugPoints[0].segmentId == 0, "track debug point stores segment id");
	expectTrue(snapshot.track.debugPoints[0].surfaceName == "asphalt", "track debug point stores surface name");
	expectTrue(snapshot.track.debugPoints[0].startLine, "track debug point marks start line");
	expectNear(snapshot.track.debugPoints[0].godotCenter.y, snapshot.track.debugPoints[0].torcsCenter.z, 0.0,
		"track center maps Godot Y from TORCS Z");
	expectNear(snapshot.track.debugPoints[0].godotLeftBorder.z, snapshot.track.debugPoints[0].torcsLeftBorder.y, 0.0,
		"track left border maps Godot Z from TORCS Y");
	expectNear(snapshot.track.debugPoints.back().torcsCenter.x, 120.0, 0.0,
		"track debug points cover placeholder length");
	expectTrue(snapshot.track.debugPoints.back().finishLine, "track debug point marks finish line");
	expectTrue(snapshot.cars[0].trackPosition.segmentId == 0, "car snapshot stores track-local segment");
	expectNear(snapshot.cars[0].trackPosition.toMiddle, 0.0, 0.0, "car starts on track center");
	expectTrue(snapshot.cars[0].trackPosition.surfaceName == "asphalt", "car snapshot stores surface name");
	expectTrue(snapshot.cars[0].wheels[0].hasContact, "wheel snapshot marks contact");
	expectTrue(snapshot.cars[0].wheels[0].surfaceName == "asphalt", "wheel snapshot stores surface name");
	expectNear(snapshot.cars[0].wheels[0].godotContactPoint.y,
		snapshot.cars[0].wheels[0].torcsContactPoint.z,
		0.0,
		"wheel contact maps Godot Y from TORCS Z");
}

static void
testSubstepCatchUpCap()
{
	TorcsRuntime runtime;
	expectTrue(runtime.initialize({ "data", ".", "." }), "runtime initializes for catch-up cap");

	TorcsRace race(runtime);
	expectTrue(race.load({ "track.xml", "car.xml", "test-car", 0 }), "race loads for catch-up cap");

	TorcsBridgeInputState input{};
	input.throttle = 1.0;
	input.gear = 1;
	race.setHumanInput(0, input);

	TorcsBridgeSnapshot snapshot = race.step(1.0);
	expectTrue(snapshot.completedSubsteps == TORCS_BRIDGE_MAX_SUBSTEPS_PER_STEP,
		"large delta caps substeps per call");
	expectNear(snapshot.raceTime,
		TORCS_BRIDGE_MAX_SUBSTEPS_PER_STEP * TORCS_BRIDGE_SIM_STEP_SECONDS,
		1e-12,
		"large delta advances only capped substeps");

	snapshot = race.step(0.0);
	expectTrue(snapshot.completedSubsteps == 0, "zero delta does not drain pending accumulator");

	snapshot = race.step(TORCS_BRIDGE_SIM_STEP_SECONDS);
	expectTrue(snapshot.completedSubsteps == TORCS_BRIDGE_MAX_SUBSTEPS_PER_STEP,
		"pending accumulator is consumed on later positive step");
}

static void
testNonFiniteStepDeltaDoesNotPoisonAccumulator()
{
	TorcsRuntime runtime;
	expectTrue(runtime.initialize({ "data", ".", "." }), "runtime initializes for non-finite delta");

	TorcsRace race(runtime);
	expectTrue(race.load({ "track.xml", "car.xml", "test-car", 0 }), "race loads for non-finite delta");

	TorcsBridgeInputState input{};
	input.throttle = 1.0;
	input.gear = 1;
	race.setHumanInput(0, input);

	TorcsBridgeSnapshot snapshot = race.step(std::numeric_limits<double>::quiet_NaN());
	expectTrue(snapshot.completedSubsteps == 0, "NaN delta does not advance substeps");
	expectNear(snapshot.raceTime, 0.0, 0.0, "NaN delta does not advance race time");

	snapshot = race.step(std::numeric_limits<double>::infinity());
	expectTrue(snapshot.completedSubsteps == 0, "infinite delta does not advance substeps");
	expectNear(snapshot.raceTime, 0.0, 0.0, "infinite delta does not advance race time");

	snapshot = race.step(TORCS_BRIDGE_SIM_STEP_SECONDS);
	expectTrue(snapshot.completedSubsteps == 1, "finite delta still advances after non-finite deltas");
	expectNear(snapshot.raceTime, TORCS_BRIDGE_SIM_STEP_SECONDS, 1e-12,
		"finite delta advances one substep after non-finite deltas");
}

int
main()
{
	testInputClamp();
	testNonFiniteInputClamp();
	testCoordinateMapping();
	testSubstepAccumulator();
	testAngularVelocitySnapshot();
	testTrackDebugSnapshot();
	testSubstepCatchUpCap();
	testNonFiniteStepDeltaDoesNotPoisonAccumulator();

	if (Failures != 0) {
		std::cerr << Failures << " bridge test failure(s).\n";
		return 1;
	}

	std::cout << "All bridge tests passed.\n";
	return 0;
}
