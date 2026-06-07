/***************************************************************************

    file                 : torcs_bridge_native.cpp
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

#include "torcs_bridge_native.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <array>
#include <string>

using godot::Array;
using godot::Dictionary;
using godot::String;
using godot::Variant;
using godot::Vector3;

static Variant
getValue(const Dictionary& dictionary, const char* key, const Variant& fallback)
{
	const String stringKey(key);
	if (!dictionary.has(stringKey)) {
		return fallback;
	}

	return dictionary.get(stringKey, fallback);
}

static std::string
toStdString(const String& value)
{
	const godot::CharString utf8 = value.utf8();
	return utf8.get_data();
}

static std::string
toStdString(const Variant& value)
{
	const String stringValue = value;
	return toStdString(stringValue);
}

static double
getDouble(const Dictionary& dictionary, const char* key, double fallback)
{
	const Variant value = getValue(dictionary, key, fallback);
	return static_cast<double>(value);
}

static int
getInt(const Dictionary& dictionary, const char* key, int fallback)
{
	const Variant value = getValue(dictionary, key, fallback);
	return static_cast<int>(value);
}

static bool
getBool(const Dictionary& dictionary, const char* key, bool fallback)
{
	const Variant value = getValue(dictionary, key, fallback);
	return static_cast<bool>(value);
}

static std::string
getString(const Dictionary& dictionary, const char* key, const char* fallback)
{
	return toStdString(getValue(dictionary, key, String(fallback)));
}

static Vector3
toGodotVec3(const TorcsBridgeVec3& vec)
{
	return Vector3(
		static_cast<godot::real_t>(vec.x),
		static_cast<godot::real_t>(vec.y),
		static_cast<godot::real_t>(vec.z)
	);
}

static Dictionary
toGodotInput(const TorcsBridgeInputState& input)
{
	Dictionary dictionary;
	dictionary["steer"] = input.steer;
	dictionary["throttle"] = input.throttle;
	dictionary["brake"] = input.brake;
	dictionary["clutch"] = input.clutch;
	dictionary["gear"] = input.gear;
	dictionary["lights"] = input.lights;
	dictionary["pit_request"] = input.pitRequest;
	dictionary["brake_balance"] = input.brakeBalance;
	return dictionary;
}

static TorcsBridgeInputState
fromGodotInput(const Dictionary& dictionary)
{
	TorcsBridgeInputState input{};
	input.gear = 1;
	input.brakeBalance = 0.5;

	input.steer = getDouble(dictionary, "steer", input.steer);
	input.throttle = getDouble(dictionary, "throttle", input.throttle);
	input.brake = getDouble(dictionary, "brake", input.brake);
	input.clutch = getDouble(dictionary, "clutch", input.clutch);
	input.gear = getInt(dictionary, "gear", input.gear);
	input.lights = getBool(dictionary, "lights", input.lights);
	input.pitRequest = getBool(dictionary, "pit_request", input.pitRequest);
	input.brakeBalance = getDouble(dictionary, "brake_balance", input.brakeBalance);
	return input;
}

static Dictionary
toGodotWheel(const TorcsBridgeWheelSnapshot& wheel)
{
	Dictionary dictionary;
	dictionary["spin_velocity"] = wheel.spinVelocity;
	dictionary["ride_height"] = wheel.rideHeight;
	dictionary["slip_side"] = wheel.slipSide;
	dictionary["slip_accel"] = wheel.slipAccel;
	dictionary["skid"] = wheel.skid;
	dictionary["surface_id"] = wheel.surfaceId;
	dictionary["surface_name"] = String(wheel.surfaceName.c_str());
	dictionary["has_contact"] = wheel.hasContact;
	dictionary["torcs_contact_point"] = toGodotVec3(wheel.torcsContactPoint);
	dictionary["godot_contact_point"] = toGodotVec3(wheel.godotContactPoint);
	dictionary["torcs_surface_normal"] = toGodotVec3(wheel.torcsSurfaceNormal);
	dictionary["godot_surface_normal"] = toGodotVec3(wheel.godotSurfaceNormal);
	return dictionary;
}

static Dictionary
toGodotTrackDebugPoint(const TorcsBridgeTrackDebugPoint& point)
{
	Dictionary dictionary;
	dictionary["segment_id"] = point.segmentId;
	dictionary["segment_name"] = String(point.segmentName.c_str());
	dictionary["distance_from_start"] = point.distanceFromStart;
	dictionary["surface_id"] = point.surfaceId;
	dictionary["surface_name"] = String(point.surfaceName.c_str());
	dictionary["start_line"] = point.startLine;
	dictionary["finish_line"] = point.finishLine;
	dictionary["torcs_center"] = toGodotVec3(point.torcsCenter);
	dictionary["godot_center"] = toGodotVec3(point.godotCenter);
	dictionary["torcs_left_border"] = toGodotVec3(point.torcsLeftBorder);
	dictionary["godot_left_border"] = toGodotVec3(point.godotLeftBorder);
	dictionary["torcs_right_border"] = toGodotVec3(point.torcsRightBorder);
	dictionary["godot_right_border"] = toGodotVec3(point.godotRightBorder);
	return dictionary;
}

static Dictionary
toGodotTrackLocalPosition(const TorcsBridgeTrackLocalPosition& position)
{
	Dictionary dictionary;
	dictionary["segment_id"] = position.segmentId;
	dictionary["segment_name"] = String(position.segmentName.c_str());
	dictionary["distance_from_start"] = position.distanceFromStart;
	dictionary["to_start"] = position.toStart;
	dictionary["to_right"] = position.toRight;
	dictionary["to_middle"] = position.toMiddle;
	dictionary["to_left"] = position.toLeft;
	dictionary["surface_id"] = position.surfaceId;
	dictionary["surface_name"] = String(position.surfaceName.c_str());
	dictionary["start_line"] = position.startLine;
	dictionary["finish_line"] = position.finishLine;
	return dictionary;
}

static Dictionary
toGodotTrack(const TorcsBridgeTrackSnapshot& track)
{
	Array debugPoints;
	for (const TorcsBridgeTrackDebugPoint& point : track.debugPoints) {
		debugPoints.append(toGodotTrackDebugPoint(point));
	}

	Dictionary dictionary;
	dictionary["track_id"] = String(track.trackId.c_str());
	dictionary["length"] = track.length;
	dictionary["width"] = track.width;
	dictionary["debug_points"] = debugPoints;
	return dictionary;
}

static Dictionary
toGodotCar(const TorcsBridgeCarSnapshot& car)
{
	Array wheels;
	for (const TorcsBridgeWheelSnapshot& wheel : car.wheels) {
		wheels.append(toGodotWheel(wheel));
	}

	Dictionary dictionary;
	dictionary["id"] = car.id;
	dictionary["car_id"] = String(car.carId.c_str());
	dictionary["torcs_position"] = toGodotVec3(car.torcsPosition);
	dictionary["godot_position"] = toGodotVec3(car.godotPosition);
	dictionary["torcs_linear_velocity"] = toGodotVec3(car.torcsLinearVelocity);
	dictionary["godot_linear_velocity"] = toGodotVec3(car.godotLinearVelocity);
	dictionary["torcs_angular_velocity"] = toGodotVec3(car.torcsAngularVelocity);
	dictionary["godot_angular_velocity"] = toGodotVec3(car.godotAngularVelocity);
	dictionary["yaw"] = car.yaw;
	dictionary["godot_yaw"] = car.godotYaw;
	dictionary["speed"] = car.speed;
	dictionary["rpm"] = car.rpm;
	dictionary["gear"] = car.gear;
	dictionary["fuel"] = car.fuel;
	dictionary["damage"] = car.damage;
	dictionary["skid"] = car.skid;
	dictionary["collision"] = car.collision;
	dictionary["input"] = toGodotInput(car.input);
	dictionary["track_position"] = toGodotTrackLocalPosition(car.trackPosition);
	dictionary["wheels"] = wheels;
	return dictionary;
}

static Dictionary
toGodotSnapshot(const TorcsBridgeSnapshot& snapshot)
{
	Array cars;
	for (const TorcsBridgeCarSnapshot& car : snapshot.cars) {
		cars.append(toGodotCar(car));
	}

	Dictionary dictionary;
	dictionary["race_time"] = snapshot.raceTime;
	dictionary["completed_substeps"] = snapshot.completedSubsteps;
	dictionary["track"] = toGodotTrack(snapshot.track);
	dictionary["cars"] = cars;
	return dictionary;
}

void
TorcsBridgeNative::_bind_methods()
{
	godot::ClassDB::bind_method(godot::D_METHOD("initialize", "data_root", "local_root", "library_root"), &TorcsBridgeNative::initialize);
	godot::ClassDB::bind_method(godot::D_METHOD("shutdown"), &TorcsBridgeNative::shutdown);
	godot::ClassDB::bind_method(godot::D_METHOD("load", "config"), &TorcsBridgeNative::load);
	godot::ClassDB::bind_method(godot::D_METHOD("set_human_input", "car_index", "input"), &TorcsBridgeNative::set_human_input);
	godot::ClassDB::bind_method(godot::D_METHOD("step", "seconds"), &TorcsBridgeNative::step);
	godot::ClassDB::bind_method(godot::D_METHOD("get_snapshot"), &TorcsBridgeNative::get_snapshot);
}

TorcsBridgeNative::~TorcsBridgeNative()
{
	shutdown();
}

bool
TorcsBridgeNative::initialize(const String& dataRoot, const String& localRoot, const String& libraryRoot)
{
	return adapter.initialize({
		toStdString(dataRoot),
		toStdString(localRoot),
		toStdString(libraryRoot)
	});
}

void
TorcsBridgeNative::shutdown()
{
	adapter.shutdown();
}

bool
TorcsBridgeNative::load(const Dictionary& config)
{
	return adapter.loadOneCarFreeDrive({
		getString(config, "track_xml", "data/tracks/road/wheel-2/wheel-2.xml"),
		getString(config, "car_xml", "data/cars/models/car1-trb1/car1-trb1.xml"),
		getString(config, "car_id", "car1-trb1"),
		getInt(config, "laps", 0)
	});
}

void
TorcsBridgeNative::set_human_input(int carIndex, const Dictionary& input)
{
	if (carIndex != 0) {
		return;
	}

	adapter.setHumanInput(fromGodotInput(input));
}

Dictionary
TorcsBridgeNative::step(double seconds)
{
	return toGodotSnapshot(adapter.step(seconds));
}

Dictionary
TorcsBridgeNative::get_snapshot() const
{
	return toGodotSnapshot(adapter.getSnapshot());
}
