/***************************************************************************

    file                 : torcs_bridge.h
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

#ifndef _TORCS_BRIDGE_H_
#define _TORCS_BRIDGE_H_

#include <array>
#include <string>
#include <vector>

constexpr double TORCS_BRIDGE_SIM_STEP_SECONDS = 0.002;
constexpr double TORCS_BRIDGE_ROBOT_STEP_SECONDS = 0.02;
constexpr int TORCS_BRIDGE_MAX_SUBSTEPS_PER_STEP = 50;

struct TorcsBridgeVec3 {
	double x;
	double y;
	double z;
};

struct TorcsBridgeRuntimeConfig {
	std::string dataRoot;
	std::string localRoot;
	std::string libraryRoot;
};

struct TorcsBridgeRaceConfig {
	std::string trackXml;
	std::string carXml;
	std::string carId;
	int laps;
};

struct TorcsBridgeInputState {
	double steer;
	double throttle;
	double brake;
	double clutch;
	int gear;
	bool lights;
	bool pitRequest;
	double brakeBalance;
};

struct TorcsBridgeWheelSnapshot {
	double spinVelocity;
	double rideHeight;
	double slipSide;
	double slipAccel;
	double skid;
	int surfaceId;
};

struct TorcsBridgeTrackDebugPoint {
	TorcsBridgeVec3 torcsCenter;
	TorcsBridgeVec3 godotCenter;
	TorcsBridgeVec3 torcsLeftBorder;
	TorcsBridgeVec3 godotLeftBorder;
	TorcsBridgeVec3 torcsRightBorder;
	TorcsBridgeVec3 godotRightBorder;
};

struct TorcsBridgeTrackSnapshot {
	std::string trackId;
	double length;
	double width;
	std::vector<TorcsBridgeTrackDebugPoint> debugPoints;
};

struct TorcsBridgeCarSnapshot {
	int id;
	std::string carId;
	TorcsBridgeVec3 torcsPosition;
	TorcsBridgeVec3 godotPosition;
	TorcsBridgeVec3 torcsLinearVelocity;
	TorcsBridgeVec3 godotLinearVelocity;
	TorcsBridgeVec3 torcsAngularVelocity;
	TorcsBridgeVec3 godotAngularVelocity;
	double yaw;
	double godotYaw;
	double speed;
	double rpm;
	int gear;
	double fuel;
	double damage;
	double skid;
	bool collision;
	TorcsBridgeInputState input;
	std::array<TorcsBridgeWheelSnapshot, 4> wheels;
};

struct TorcsBridgeSnapshot {
	double raceTime;
	int completedSubsteps;
	TorcsBridgeTrackSnapshot track;
	std::vector<TorcsBridgeCarSnapshot> cars;
};

TorcsBridgeInputState TorcsBridgeClampInput(const TorcsBridgeInputState& input);
TorcsBridgeVec3 TorcsBridgeTorcsToGodotPosition(const TorcsBridgeVec3& position);
TorcsBridgeVec3 TorcsBridgeTorcsToGodotLinearVelocity(const TorcsBridgeVec3& velocity);
TorcsBridgeVec3 TorcsBridgeTorcsToGodotAngularVelocity(const TorcsBridgeVec3& velocity);
double TorcsBridgeTorcsToGodotYaw(double yaw);

class TorcsRuntime {
public:
	bool initialize(const TorcsBridgeRuntimeConfig& config);
	void shutdown();
	bool isInitialized() const;
	const TorcsBridgeRuntimeConfig& getConfig() const;

private:
	bool initialized = false;
	TorcsBridgeRuntimeConfig config;
};

class TorcsRace {
public:
	explicit TorcsRace(TorcsRuntime& runtime);

	bool load(const TorcsBridgeRaceConfig& config);
	void setHumanInput(int carIndex, const TorcsBridgeInputState& input);
	TorcsBridgeSnapshot step(double seconds);
	const TorcsBridgeSnapshot& getSnapshot() const;
	void shutdown();
	bool isLoaded() const;

private:
	void stepOneSubstep();

	TorcsRuntime& runtime;
	bool loaded = false;
	double accumulator = 0.0;
	TorcsBridgeRaceConfig config;
	TorcsBridgeInputState humanInput;
	TorcsBridgeSnapshot snapshot;
};

#endif /* _TORCS_BRIDGE_H_ */
