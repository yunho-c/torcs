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
#include <string>

#include <car.h>
#include <tgf.h>

#include <track.h>

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
makeInitialCarSnapshot(const TorcsBridgeRaceConfig& raceConfig, const TorcsBridgeInputState& input, void* carHandle)
{
	TorcsBridgeCarSnapshot car{};
	car.id = 0;
	car.carId = raceConfig.carId.empty() ? "car1-trb1" : raceConfig.carId;
	car.godotPosition = TorcsBridgeTorcsToGodotPosition(car.torcsPosition);
	car.godotYaw = TorcsBridgeTorcsToGodotYaw(car.yaw);
	car.gear = 1;
	car.fuel = GfParmGetNum(carHandle, SECT_CAR, PRM_FUEL, "l", 0.0f);
	car.input = input;
	return car;
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

	input = TorcsBridgeClampInput(input);
	snapshot.track = makeTrackSnapshot(static_cast<tTrack*>(track), raceConfig);
	snapshot.cars.push_back(makeInitialCarSnapshot(raceConfig, input, carHandle));
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
	loaded = false;
}

bool
TorcsRetainedAdapter::isLoaded() const
{
	return loaded;
}
