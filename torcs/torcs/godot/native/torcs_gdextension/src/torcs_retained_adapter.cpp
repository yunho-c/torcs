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

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include <car.h>
#include <raceman.h>
#include <robottools.h>
#include <tgf.h>

#include <track.h>

#include "sim.h"
#include "trackinc.h"

static std::string
withTrailingSlash(const std::string& path)
{
	if (path.empty() || path[path.size() - 1] == '/' || path[path.size() - 1] == '\\') {
		return path;
	}

	return path + "/";
}

static bool
isAbsolutePath(const std::string& path)
{
	return !path.empty()
		&& (path[0] == '/'
			|| path[0] == '\\'
			|| (path.size() > 2 && path[1] == ':' && (path[2] == '/' || path[2] == '\\')));
}

static bool
stripDataPrefix(const std::string& path, std::string* suffix)
{
	if (path == "data" || path == "data/" || path == "data\\") {
		*suffix = "";
		return true;
	}

	if (path.rfind("data/", 0) == 0 || path.rfind("data\\", 0) == 0) {
		*suffix = path.substr(5);
		return true;
	}

	return false;
}

static std::string
resolveTrackXmlPath(const TorcsBridgeRuntimeConfig& runtimeConfig, const TorcsBridgeRaceConfig& raceConfig)
{
	if (raceConfig.trackXml.empty()) {
		return withTrailingSlash(runtimeConfig.dataRoot) + "tracks/road/wheel-2/wheel-2.xml";
	}

	if (isAbsolutePath(raceConfig.trackXml)) {
		return raceConfig.trackXml;
	}

	std::string dataRelativePath;
	if (stripDataPrefix(raceConfig.trackXml, &dataRelativePath)) {
		return withTrailingSlash(runtimeConfig.dataRoot) + dataRelativePath;
	}

	return withTrailingSlash(runtimeConfig.dataRoot) + raceConfig.trackXml;
}

static std::string
resolveCarXmlPath(const TorcsBridgeRuntimeConfig& runtimeConfig, const TorcsBridgeRaceConfig& raceConfig)
{
	if (raceConfig.carXml.empty()) {
		return withTrailingSlash(runtimeConfig.dataRoot) + "cars/models/car1-trb1/car1-trb1.xml";
	}

	if (isAbsolutePath(raceConfig.carXml)) {
		return raceConfig.carXml;
	}

	std::string dataRelativePath;
	if (stripDataPrefix(raceConfig.carXml, &dataRelativePath)) {
		return withTrailingSlash(runtimeConfig.dataRoot) + dataRelativePath;
	}

	return withTrailingSlash(runtimeConfig.dataRoot) + raceConfig.carXml;
}

static std::string
resolveCategoryXmlPath(const TorcsBridgeRuntimeConfig& runtimeConfig, const char* category)
{
	const std::string categoryName = category != nullptr ? category : "";
	return withTrailingSlash(runtimeConfig.dataRoot)
		+ "cars/categories/" + categoryName + "/" + categoryName + ".xml";
}

static void*
loadMergedCarHandle(const TorcsBridgeRuntimeConfig& runtimeConfig, const TorcsBridgeRaceConfig& raceConfig)
{
	const std::string carXmlPath = resolveCarXmlPath(runtimeConfig, raceConfig);
	void* carHandle = GfParmReadFile(carXmlPath.c_str(), GFPARM_RMODE_STD | GFPARM_RMODE_PRIVATE);
	if (carHandle == nullptr) {
		return nullptr;
	}

	const char* category = GfParmGetStr(carHandle, SECT_CAR, PRM_CATEGORY, nullptr);
	if (category == nullptr || category[0] == '\0') {
		GfParmReleaseHandle(carHandle);
		return nullptr;
	}

	const std::string categoryXmlPath = resolveCategoryXmlPath(runtimeConfig, category);
	void* categoryHandle = GfParmReadFile(categoryXmlPath.c_str(), GFPARM_RMODE_STD | GFPARM_RMODE_PRIVATE);
	if (categoryHandle == nullptr) {
		GfParmReleaseHandle(carHandle);
		return nullptr;
	}

	if (GfParmCheckHandle(categoryHandle, carHandle)) {
		GfParmReleaseHandle(categoryHandle);
		GfParmReleaseHandle(carHandle);
		return nullptr;
	}

	return GfParmMergeHandles(
		categoryHandle,
		carHandle,
		GFPARM_MMODE_SRC | GFPARM_MMODE_DST | GFPARM_MMODE_RELSRC | GFPARM_MMODE_RELDST);
}

static TorcsBridgeVec3
makeBridgeVec3(const t3Dd& point)
{
	return { point.x, point.y, point.z };
}

static TorcsBridgeVec3
midpoint(const t3Dd& left, const t3Dd& right)
{
	return {
		(static_cast<double>(left.x) + static_cast<double>(right.x)) * 0.5,
		(static_cast<double>(left.y) + static_cast<double>(right.y)) * 0.5,
		(static_cast<double>(left.z) + static_cast<double>(right.z)) * 0.5
	};
}

static void
appendTrackDebugPoint(TorcsBridgeTrackSnapshot* snapshot, const t3Dd& left, const t3Dd& right)
{
	TorcsBridgeTrackDebugPoint point{};
	point.torcsCenter = midpoint(left, right);
	point.torcsLeftBorder = makeBridgeVec3(left);
	point.torcsRightBorder = makeBridgeVec3(right);
	point.godotCenter = TorcsBridgeTorcsToGodotPosition(point.torcsCenter);
	point.godotLeftBorder = TorcsBridgeTorcsToGodotPosition(point.torcsLeftBorder);
	point.godotRightBorder = TorcsBridgeTorcsToGodotPosition(point.torcsRightBorder);
	snapshot->debugPoints.push_back(point);
}

static TorcsBridgeTrackSnapshot
makeTrackSnapshot(const tTrack* track, const TorcsBridgeRaceConfig& raceConfig)
{
	TorcsBridgeTrackSnapshot snapshot{};
	snapshot.trackId = track->internalname != nullptr ? track->internalname : raceConfig.trackXml;
	snapshot.length = track->length;
	snapshot.width = track->width;

	if (track->seg == nullptr || track->nseg <= 0) {
		return snapshot;
	}

	static constexpr int MAX_DEBUG_POINTS = 256;
	const int stride = std::max(1, (track->nseg + MAX_DEBUG_POINTS - 1) / MAX_DEBUG_POINTS);
	const tTrackSeg* seg = track->seg;
	const tTrackSeg* lastIncluded = nullptr;

	for (int index = 0; index < track->nseg && seg != nullptr; index++) {
		if (index % stride == 0) {
			appendTrackDebugPoint(&snapshot, seg->vertex[TR_SL], seg->vertex[TR_SR]);
			lastIncluded = seg;
		}
		seg = seg->next;
	}

	if (lastIncluded != nullptr) {
		appendTrackDebugPoint(&snapshot, lastIncluded->vertex[TR_EL], lastIncluded->vertex[TR_ER]);
	}

	return snapshot;
}

static TorcsBridgeCarSnapshot
makeCarSnapshotFromCarElt(const TorcsBridgeRaceConfig& raceConfig, const TorcsBridgeInputState& input, const tCarElt* carElt)
{
	TorcsBridgeCarSnapshot car{};
	car.id = 0;
	car.carId = raceConfig.carId.empty() ? "car1-trb1" : raceConfig.carId;
	car.torcsPosition = { carElt->_pos_X, carElt->_pos_Y, carElt->_pos_Z };
	car.godotPosition = TorcsBridgeTorcsToGodotPosition(car.torcsPosition);
	car.torcsLinearVelocity = { carElt->pub.DynGCg.vel.x, carElt->pub.DynGCg.vel.y, carElt->pub.DynGCg.vel.z };
	car.godotLinearVelocity = TorcsBridgeTorcsToGodotLinearVelocity(car.torcsLinearVelocity);
	car.torcsAngularVelocity = { carElt->pub.DynGC.vel.ax, carElt->pub.DynGC.vel.ay, carElt->pub.DynGC.vel.az };
	car.godotAngularVelocity = TorcsBridgeTorcsToGodotAngularVelocity(car.torcsAngularVelocity);
	car.yaw = carElt->_yaw;
	car.godotYaw = TorcsBridgeTorcsToGodotYaw(car.yaw);
	car.speed = carElt->pub.speed;
	car.rpm = carElt->_enginerpm;
	car.gear = carElt->_gear;
	car.fuel = carElt->_fuel;
	car.damage = carElt->_dammage;
	car.collision = carElt->priv.collision != 0 || carElt->priv.simcollision != 0;
	car.input = input;

	for (int i = 0; i < 4; i++) {
		const tWheelState& wheel = carElt->priv.wheel[i];
		car.wheels[i].spinVelocity = wheel.spinVel;
		car.wheels[i].rideHeight = wheel.relPos.z;
		car.wheels[i].slipSide = wheel.slipSide;
		car.wheels[i].slipAccel = wheel.slipAccel;
		car.wheels[i].skid = carElt->_skid[i];
		car.wheels[i].surfaceId = 0;
		car.skid = std::max(car.skid, static_cast<double>(std::fabs(carElt->_skid[i])));
	}

	return car;
}

static void
applyHumanInput(tCarElt* car, const TorcsBridgeInputState& input)
{
	std::memset(&car->ctrl, 0, sizeof(car->ctrl));
	car->ctrl.steer = static_cast<tdble>(input.steer);
	car->ctrl.accelCmd = static_cast<tdble>(input.throttle);
	car->ctrl.brakeCmd = static_cast<tdble>(input.brake);
	car->ctrl.clutchCmd = static_cast<tdble>(input.clutch);
	car->ctrl.gear = input.gear;
	car->ctrl.raceCmd = input.pitRequest ? RM_CMD_PIT_ASKED : RM_CMD_NONE;
	car->ctrl.lightCmd = input.lights ? (RM_LIGHT_HEAD1 | RM_LIGHT_HEAD2) : 0;
	car->ctrl.brakeRepartitionCmd = static_cast<int>(std::lround((input.brakeBalance - 0.5) * 20.0));
}

static void
copyLimited(char* destination, size_t destinationSize, const std::string& source)
{
	if (destinationSize == 0) {
		return;
	}

	std::strncpy(destination, source.c_str(), destinationSize - 1);
	destination[destinationSize - 1] = '\0';
}

static tCarElt*
retainedCar(tRmInfo* info)
{
	return info != nullptr && info->carList != nullptr ? &info->carList[0] : nullptr;
}

static void
freeRaceInfo(tRmInfo* info)
{
	if (info == nullptr) {
		return;
	}

	if (info->s != nullptr) {
		std::free(info->s->cars);
		info->s->cars = nullptr;
		std::free(info->s);
		info->s = nullptr;
	}
	std::free(info->carList);
	info->carList = nullptr;
	std::free(info);
}

static tdble
getTrackGridNum(tTrack* raceTrack, const char* attribute, tdble defaultValue)
{
	if (raceTrack == nullptr || raceTrack->params == nullptr) {
		return defaultValue;
	}

	return GfParmGetNum(
		raceTrack->params,
		RM_SECT_STARTINGGRID,
		attribute,
		static_cast<char*>(nullptr),
		defaultValue);
}

static const char*
getTrackGridStr(tTrack* raceTrack, const char* attribute, const char* defaultValue)
{
	if (raceTrack == nullptr || raceTrack->params == nullptr) {
		return defaultValue;
	}

	return GfParmGetStr(raceTrack->params, RM_SECT_STARTINGGRID, attribute, defaultValue);
}

static tRmInfo*
allocateOneCarRaceInfo(tTrack* raceTrack, void* mergedCarHandle, const TorcsBridgeRaceConfig& raceConfig)
{
	tRmInfo* info = static_cast<tRmInfo*>(std::calloc(1, sizeof(tRmInfo)));
	if (info == nullptr) {
		return nullptr;
	}

	info->s = static_cast<tSituation*>(std::calloc(1, sizeof(tSituation)));
	info->carList = static_cast<tCarElt*>(std::calloc(1, sizeof(tCarElt)));
	if (info->s == nullptr || info->carList == nullptr) {
		freeRaceInfo(info);
		return nullptr;
	}

	info->s->cars = static_cast<tCarElt**>(std::calloc(1, sizeof(tCarElt*)));
	if (info->s->cars == nullptr) {
		freeRaceInfo(info);
		return nullptr;
	}

	info->track = raceTrack;
	info->s->cars[0] = &info->carList[0];
	info->s->_ncars = 1;
	info->s->_totLaps = raceConfig.laps > 0 ? raceConfig.laps : 0;
	info->s->_raceType = RM_TYPE_PRACTICE;
	info->s->_raceState = RM_RACE_RUNNING;
	info->s->deltaTime = TORCS_BRIDGE_SIM_STEP_SECONDS;
	info->raceRules.fuelFactor = 0.0f;
	info->raceRules.damageFactor = 0.0f;
	info->raceRules.tireFactor = 0.0f;

	tCarElt* car = &info->carList[0];
	GF_TAILQ_INIT(&car->_penaltyList);
	car->index = 0;
	car->_carHandle = mergedCarHandle;
	car->_paramsHandle = mergedCarHandle;
	car->_driverType = RM_DRV_HUMAN;
	car->_startRank = 0;
	car->_pos = 1;
	car->_remainingLaps = info->s->_totLaps;
	car->_gear = 1;
	car->_fuel = GfParmGetNum(mergedCarHandle, SECT_CAR, PRM_FUEL, "l", 0.0f);

	const std::string carId = raceConfig.carId.empty() ? "car1-trb1" : raceConfig.carId;
	copyLimited(car->_name, sizeof(car->_name), carId);
	copyLimited(car->_carName, sizeof(car->_carName), carId);
	copyLimited(car->_teamname, sizeof(car->_teamname), "Godot Bridge");
	copyLimited(car->_category, sizeof(car->_category),
		GfParmGetStr(mergedCarHandle, SECT_CAR, PRM_CATEGORY, ""));
	RtInitCarPitSetup(mergedCarHandle, &car->pitcmd.setup, false);

	return info;
}

static bool
placeOneCarStartingGrid(tRmInfo* info)
{
	if (info == nullptr || info->track == nullptr || info->track->seg == nullptr || info->carList == nullptr) {
		return false;
	}

	tTrack* raceTrack = info->track;
	tCarElt* car = &info->carList[0];
	tTrackSeg* curseg = raceTrack->seg->next;
	const char* pole = "right";

	for (int i = 0; i < raceTrack->nseg && curseg != nullptr; i++) {
		if (curseg->type != TR_STR) {
			pole = curseg->type == TR_LFT ? "left" : "right";
			break;
		}
		curseg = curseg->next;
	}
	pole = getTrackGridStr(raceTrack, RM_ATTR_POLE, pole);

	tdble a;
	tdble b;
	if (std::strcmp(pole, "left") == 0) {
		a = raceTrack->width;
		b = -a;
	} else {
		a = 0.0f;
		b = raceTrack->width;
	}

	int rows = static_cast<int>(getTrackGridNum(raceTrack, RM_ATTR_ROWS, 2.0f));
	if (rows < 1) {
		rows = 1;
	}

	if (raceTrack->length <= 0.0f) {
		return false;
	}

	const tdble distanceToStart = getTrackGridNum(raceTrack, RM_ATTR_TOSTART, 10.0f);
	const tdble heightInit = getTrackGridNum(raceTrack, RM_ATTR_INITHEIGHT, 0.3f);
	const tdble speedInit = getTrackGridNum(raceTrack, RM_ATTR_INITSPEED, 0.0f);
	tdble startpos = raceTrack->length - distanceToStart;
	while (startpos < 0.0f) {
		startpos += raceTrack->length;
	}
	while (startpos > raceTrack->length) {
		startpos -= raceTrack->length;
	}
	const tdble toRight = a + b / static_cast<tdble>(rows + 1);

	curseg = raceTrack->seg;
	for (int i = 0; i < raceTrack->nseg && curseg != nullptr && startpos < curseg->lgfromstart; i++) {
		curseg = curseg->prev;
	}
	if (curseg == nullptr || startpos < curseg->lgfromstart) {
		return false;
	}

	const tdble toStart = startpos - curseg->lgfromstart;
	car->_speed_x = speedInit;
	car->_commitBestLapTime = true;
	car->_trkPos.seg = curseg;
	car->_trkPos.toRight = toRight;
	car->_trkPos.toMiddle = toRight - curseg->width * 0.5f;
	car->_trkPos.toLeft = curseg->width - toRight;
	switch (curseg->type) {
		case TR_STR:
			car->_trkPos.toStart = toStart;
			RtTrackLocal2Global(&car->_trkPos, &car->_pos_X, &car->_pos_Y, TR_TORIGHT);
			car->_yaw = curseg->angle[TR_ZS];
			break;
		case TR_RGT:
			car->_trkPos.toStart = toStart / curseg->radius;
			RtTrackLocal2Global(&car->_trkPos, &car->_pos_X, &car->_pos_Y, TR_TORIGHT);
			car->_yaw = curseg->angle[TR_ZS] - car->_trkPos.toStart;
			break;
		case TR_LFT:
			car->_trkPos.toStart = toStart / curseg->radius;
			RtTrackLocal2Global(&car->_trkPos, &car->_pos_X, &car->_pos_Y, TR_TORIGHT);
			car->_yaw = curseg->angle[TR_ZS] + car->_trkPos.toStart;
			break;
		default:
			return false;
	}

	car->_pos_Z = RtTrackHeightL(&car->_trkPos) + heightInit;
	NORM0_2PI(car->_yaw);
	SimConfig(car, info);
	car->_fuel = GfParmGetNum(car->_carHandle, SECT_CAR, PRM_FUEL, "l", 0.0f);

	return true;
}

static void
initializeTgfOnce()
{
	static const bool initialized = []() {
		GfInit();
		return true;
	}();
	(void)initialized;
}

bool
TorcsRetainedAdapter::initialize(const TorcsBridgeRuntimeConfig& config)
{
	unloadRace();

	runtimeConfig = config;
	initialized = !runtimeConfig.dataRoot.empty();
	if (!initialized) {
		return false;
	}

	initializeTgfOnce();
	const std::string dataRoot = withTrailingSlash(runtimeConfig.dataRoot);
	const std::string localRoot = withTrailingSlash(runtimeConfig.localRoot);
	const std::string libraryRoot = withTrailingSlash(runtimeConfig.libraryRoot);
	SetDataDir(const_cast<char*>(dataRoot.c_str()));
	SetLocalDir(const_cast<char*>(localRoot.c_str()));
	SetLibDir(const_cast<char*>(libraryRoot.c_str()));

	return initialized;
}

bool
TorcsRetainedAdapter::loadOneCarFreeDrive(const TorcsBridgeRaceConfig& config)
{
	if (!initialized) {
		return false;
	}

	unloadRace();

	raceConfig = config;

	const std::string trackXmlPath = resolveTrackXmlPath(runtimeConfig, raceConfig);
	std::string mutableTrackXmlPath = trackXmlPath;
	track = TrackBuildv1(&mutableTrackXmlPath[0]);
	if (track == nullptr) {
		return false;
	}

	carHandle = loadMergedCarHandle(runtimeConfig, raceConfig);
	if (carHandle == nullptr) {
		unloadRace();
		return false;
	}

	raceInfo = allocateOneCarRaceInfo(static_cast<tTrack*>(track), carHandle, raceConfig);
	if (raceInfo == nullptr) {
		unloadRace();
		return false;
	}

	tRmInfo* retainedRaceInfo = static_cast<tRmInfo*>(raceInfo);
	SimInit(
		1,
		static_cast<tTrack*>(track),
		retainedRaceInfo->raceRules.fuelFactor,
		retainedRaceInfo->raceRules.damageFactor,
		retainedRaceInfo->raceRules.tireFactor);
	simulationStarted = true;
	if (!placeOneCarStartingGrid(retainedRaceInfo)) {
		unloadRace();
		return false;
	}

	tCarElt* car = retainedCar(retainedRaceInfo);
	if (car == nullptr) {
		unloadRace();
		return false;
	}

	input = TorcsBridgeClampInput(input);
	accumulator = 0.0;
	snapshot.track = makeTrackSnapshot(static_cast<tTrack*>(track), raceConfig);
	snapshot.raceTime = retainedRaceInfo->s->currentTime;
	snapshot.cars.push_back(makeCarSnapshotFromCarElt(raceConfig, input, car));
	loaded = true;

	return true;
}

void
TorcsRetainedAdapter::setHumanInput(const TorcsBridgeInputState& bridgeInput)
{
	input = TorcsBridgeClampInput(bridgeInput);
}

TorcsBridgeSnapshot
TorcsRetainedAdapter::step(double seconds)
{
	snapshot.completedSubsteps = 0;

	if (!loaded || !std::isfinite(seconds) || seconds <= 0.0) {
		return snapshot;
	}

	tRmInfo* retainedRaceInfo = static_cast<tRmInfo*>(raceInfo);
	tCarElt* car = retainedCar(retainedRaceInfo);
	if (retainedRaceInfo == nullptr || retainedRaceInfo->s == nullptr || car == nullptr) {
		return snapshot;
	}

	accumulator += seconds;
	while (accumulator >= TORCS_BRIDGE_SIM_STEP_SECONDS
		&& snapshot.completedSubsteps < TORCS_BRIDGE_MAX_SUBSTEPS_PER_STEP) {
		applyHumanInput(car, input);
		retainedRaceInfo->s->deltaTime = TORCS_BRIDGE_SIM_STEP_SECONDS;
		SimUpdate(retainedRaceInfo->s, TORCS_BRIDGE_SIM_STEP_SECONDS, -1);
		retainedRaceInfo->s->currentTime += TORCS_BRIDGE_SIM_STEP_SECONDS;
		accumulator -= TORCS_BRIDGE_SIM_STEP_SECONDS;
		snapshot.completedSubsteps++;
	}

	snapshot.raceTime = retainedRaceInfo->s->currentTime;
	if (!snapshot.cars.empty()) {
		snapshot.cars[0] = makeCarSnapshotFromCarElt(raceConfig, input, car);
	}

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
	unloadRace();

	runtimeConfig = {};
	input = {};
	initialized = false;
}

void
TorcsRetainedAdapter::unloadRace()
{
	if (simulationStarted) {
		SimShutdown();
		simulationStarted = false;
	}

	if (raceInfo != nullptr) {
		freeRaceInfo(static_cast<tRmInfo*>(raceInfo));
		raceInfo = nullptr;
	}

	if (carHandle != nullptr) {
		GfParmReleaseHandle(carHandle);
		carHandle = nullptr;
	}

	if (track != nullptr) {
		TrackShutdown();
		track = nullptr;
	}

	raceConfig = {};
	snapshot = {};
	accumulator = 0.0;
	loaded = false;
}

bool
TorcsRetainedAdapter::isLoaded() const
{
	return loaded;
}
