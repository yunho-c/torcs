/***************************************************************************

    file                 : torcs_bridge_harness.cpp
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

#ifdef TORCS_BRIDGE_HAS_RETAINED_BACKEND
#include "torcs_retained_adapter.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <string>

enum HarnessOutputFormat {
	HARNESS_OUTPUT_CSV,
	HARNESS_OUTPUT_JSON
};

enum HarnessBackend {
	HARNESS_BACKEND_STUB,
	HARNESS_BACKEND_RETAINED
};

struct HarnessOptions {
	std::string dataRoot = TORCS_BRIDGE_DEFAULT_DATA_ROOT;
	std::string localRoot = TORCS_BRIDGE_DEFAULT_LOCAL_ROOT;
	std::string libraryRoot = TORCS_BRIDGE_DEFAULT_LIBRARY_ROOT;
	std::string trackXml = "data/tracks/road/wheel-2/wheel-2.xml";
	std::string carXml = "data/cars/models/car1-trb1/car1-trb1.xml";
	std::string carId = "car1-trb1";
	std::string outputPath;
	HarnessOutputFormat outputFormat = HARNESS_OUTPUT_CSV;
	HarnessBackend backend = HARNESS_BACKEND_STUB;
	double seconds = 10.0;
	double sampleSeconds = TORCS_BRIDGE_ROBOT_STEP_SECONDS;
	int laps = 0;
};

static void
printUsage(const char* program)
{
	std::cerr
		<< "Usage: " << program << " [options]\n"
		<< "\n"
		<< "Options:\n"
		<< "  --data-root <path>      TORCS data root.\n"
		<< "  --local-root <path>     TORCS local root.\n"
		<< "  --library-root <path>   TORCS native library root.\n"
		<< "  --track-xml <path>      Track XML path, default wheel-2.\n"
		<< "  --car-xml <path>        Car XML path, default car1-trb1.\n"
		<< "  --car-id <id>           Car id for snapshots, default car1-trb1.\n"
		<< "  --laps <count>          Race lap count, default 0 free-drive.\n"
		<< "  --backend <stub|retained> Simulation backend, default stub.\n"
		<< "  --seconds <value>       Simulation duration, default 10.0.\n"
		<< "  --sample-seconds <val>  Snapshot sample period, default 0.02.\n"
		<< "  --output <path>         Write output to a file instead of stdout.\n"
		<< "  --format <csv|json>     Output format, default csv.\n"
		<< "  --help                  Show this message.\n";
}

static bool
parseDouble(const char* value, double* result)
{
	char* end = nullptr;
	const double parsed = std::strtod(value, &end);
	if (end == value || *end != '\0') {
		return false;
	}

	*result = parsed;
	return true;
}

static bool
parseInt(const char* value, int* result)
{
	char* end = nullptr;
	const long parsed = std::strtol(value, &end, 10);
	if (end == value || *end != '\0'
		|| parsed < std::numeric_limits<int>::min()
		|| parsed > std::numeric_limits<int>::max()) {
		return false;
	}

	*result = static_cast<int>(parsed);
	return true;
}

enum HarnessParseResult {
	HARNESS_PARSE_OK,
	HARNESS_PARSE_HELP,
	HARNESS_PARSE_ERROR
};

static HarnessParseResult
parseOptions(int argc, char** argv, HarnessOptions* options)
{
	for (int i = 1; i < argc; i++) {
		const std::string arg = argv[i];
		if (arg == "--help") {
			printUsage(argv[0]);
			return HARNESS_PARSE_HELP;
		}

		if (i + 1 >= argc) {
			std::cerr << "Missing value for " << arg << "\n";
			return HARNESS_PARSE_ERROR;
		}

		const char* value = argv[++i];
		if (arg == "--data-root") {
			options->dataRoot = value;
		} else if (arg == "--local-root") {
			options->localRoot = value;
		} else if (arg == "--library-root") {
			options->libraryRoot = value;
		} else if (arg == "--track-xml") {
			options->trackXml = value;
		} else if (arg == "--car-xml") {
			options->carXml = value;
		} else if (arg == "--car-id") {
			options->carId = value;
		} else if (arg == "--output") {
			options->outputPath = value;
		} else if (arg == "--format") {
			const std::string format = value;
			if (format == "csv") {
				options->outputFormat = HARNESS_OUTPUT_CSV;
			} else if (format == "json") {
				options->outputFormat = HARNESS_OUTPUT_JSON;
			} else {
				std::cerr << "Invalid --format value: " << value << "\n";
				return HARNESS_PARSE_ERROR;
			}
		} else if (arg == "--backend") {
			const std::string backend = value;
			if (backend == "stub") {
				options->backend = HARNESS_BACKEND_STUB;
			} else if (backend == "retained") {
#ifdef TORCS_BRIDGE_HAS_RETAINED_BACKEND
				options->backend = HARNESS_BACKEND_RETAINED;
#else
				std::cerr << "Retained backend is not available in this build.\n";
				return HARNESS_PARSE_ERROR;
#endif
			} else {
				std::cerr << "Invalid --backend value: " << value << "\n";
				return HARNESS_PARSE_ERROR;
			}
		} else if (arg == "--seconds") {
			if (!parseDouble(value, &options->seconds)) {
				std::cerr << "Invalid --seconds value: " << value << "\n";
				return HARNESS_PARSE_ERROR;
			}
		} else if (arg == "--sample-seconds") {
			if (!parseDouble(value, &options->sampleSeconds)) {
				std::cerr << "Invalid --sample-seconds value: " << value << "\n";
				return HARNESS_PARSE_ERROR;
			}
		} else if (arg == "--laps") {
			if (!parseInt(value, &options->laps)) {
				std::cerr << "Invalid --laps value: " << value << "\n";
				return HARNESS_PARSE_ERROR;
			}
		} else {
			std::cerr << "Unknown option: " << arg << "\n";
			return HARNESS_PARSE_ERROR;
		}
	}

	if (!std::isfinite(options->seconds) || options->seconds <= 0.0) {
		std::cerr << "--seconds must be finite and positive.\n";
		return HARNESS_PARSE_ERROR;
	}

	if (!std::isfinite(options->sampleSeconds)
		|| options->sampleSeconds < TORCS_BRIDGE_SIM_STEP_SECONDS) {
		std::cerr << "--sample-seconds must be finite and at least "
			<< TORCS_BRIDGE_SIM_STEP_SECONDS << ".\n";
		return HARNESS_PARSE_ERROR;
	}

	if (options->trackXml.empty()) {
		std::cerr << "--track-xml must not be empty.\n";
		return HARNESS_PARSE_ERROR;
	}

	if (options->carXml.empty()) {
		std::cerr << "--car-xml must not be empty.\n";
		return HARNESS_PARSE_ERROR;
	}

	if (options->carId.empty()) {
		std::cerr << "--car-id must not be empty.\n";
		return HARNESS_PARSE_ERROR;
	}

	if (options->laps < 0) {
		std::cerr << "--laps must not be negative.\n";
		return HARNESS_PARSE_ERROR;
	}

	return HARNESS_PARSE_OK;
}

static TorcsBridgeInputState
scriptedInput(double time)
{
	TorcsBridgeInputState input{};
	input.gear = 1;
	input.brakeBalance = 0.55;

	if (time < 6.0) {
		input.throttle = 0.85;
	}

	if (time >= 1.5 && time < 4.0) {
		input.steer = 0.35;
	} else if (time >= 4.0 && time < 6.0) {
		input.steer = -0.20;
	}

	if (time >= 6.0 && time < 8.5) {
		input.brake = 0.45;
	}

	return input;
}

class HarnessBackendRunner {
public:
	virtual ~HarnessBackendRunner() = default;
	virtual bool initialize(const HarnessOptions& options) = 0;
	virtual void setHumanInput(const TorcsBridgeInputState& input) = 0;
	virtual TorcsBridgeSnapshot step(double seconds) = 0;
	virtual const TorcsBridgeSnapshot& getSnapshot() const = 0;
	virtual void shutdown() = 0;
};

class StubBackendRunner : public HarnessBackendRunner {
public:
	bool initialize(const HarnessOptions& options) override
	{
		if (!runtime.initialize({ options.dataRoot, options.localRoot, options.libraryRoot })) {
			std::cerr << "Failed to initialize TORCS bridge runtime.\n";
			return false;
		}

		race.reset(new TorcsRace(runtime));
		if (!race->load({
			options.trackXml,
			options.carXml,
			options.carId,
			options.laps
		})) {
			std::cerr << "Failed to load TORCS bridge race.\n";
			return false;
		}

		return true;
	}

	void setHumanInput(const TorcsBridgeInputState& input) override
	{
		race->setHumanInput(0, input);
	}

	TorcsBridgeSnapshot step(double seconds) override
	{
		return race->step(seconds);
	}

	const TorcsBridgeSnapshot& getSnapshot() const override
	{
		return race->getSnapshot();
	}

	void shutdown() override
	{
		if (race) {
			race->shutdown();
		}
		runtime.shutdown();
	}

private:
	TorcsRuntime runtime;
	std::unique_ptr<TorcsRace> race;
};

#ifdef TORCS_BRIDGE_HAS_RETAINED_BACKEND
class RetainedBackendRunner : public HarnessBackendRunner {
public:
	bool initialize(const HarnessOptions& options) override
	{
		if (!adapter.initialize({ options.dataRoot, options.localRoot, options.libraryRoot })) {
			std::cerr << "Failed to initialize retained TORCS bridge runtime.\n";
			return false;
		}

		if (!adapter.loadOneCarFreeDrive({
			options.trackXml,
			options.carXml,
			options.carId,
			options.laps
		})) {
			std::cerr << "Failed to load retained TORCS bridge race.\n";
			return false;
		}

		return true;
	}

	void setHumanInput(const TorcsBridgeInputState& input) override
	{
		adapter.setHumanInput(input);
	}

	TorcsBridgeSnapshot step(double seconds) override
	{
		return adapter.step(seconds);
	}

	const TorcsBridgeSnapshot& getSnapshot() const override
	{
		return adapter.getSnapshot();
	}

	void shutdown() override
	{
		adapter.shutdown();
	}

private:
	TorcsRetainedAdapter adapter;
};
#endif

static std::unique_ptr<HarnessBackendRunner>
createBackendRunner(HarnessBackend backend)
{
	if (backend == HARNESS_BACKEND_STUB) {
		return std::unique_ptr<HarnessBackendRunner>(new StubBackendRunner());
	}

#ifdef TORCS_BRIDGE_HAS_RETAINED_BACKEND
	return std::unique_ptr<HarnessBackendRunner>(new RetainedBackendRunner());
#else
	return nullptr;
#endif
}

static void
writeCsvHeader(std::ostream& out)
{
	out
		<< "time,substeps,car_id,torcs_x,torcs_y,torcs_z,"
		<< "godot_x,godot_y,godot_z,"
		<< "torcs_lvx,torcs_lvy,torcs_lvz,godot_lvx,godot_lvy,godot_lvz,"
		<< "torcs_avx,torcs_avy,torcs_avz,godot_avx,godot_avy,godot_avz,"
		<< "yaw,godot_yaw,speed,rpm,gear,"
		<< "steer,throttle,brake,clutch,damage,skid\n";
}

static void
writeCsvRow(std::ostream& out, const TorcsBridgeSnapshot& snapshot)
{
	const TorcsBridgeCarSnapshot& car = snapshot.cars[0];
	out
		<< std::fixed << std::setprecision(6)
		<< snapshot.raceTime << ','
		<< snapshot.completedSubsteps << ','
		<< car.carId << ','
		<< car.torcsPosition.x << ','
		<< car.torcsPosition.y << ','
		<< car.torcsPosition.z << ','
		<< car.godotPosition.x << ','
		<< car.godotPosition.y << ','
		<< car.godotPosition.z << ','
		<< car.torcsLinearVelocity.x << ','
		<< car.torcsLinearVelocity.y << ','
		<< car.torcsLinearVelocity.z << ','
		<< car.godotLinearVelocity.x << ','
		<< car.godotLinearVelocity.y << ','
		<< car.godotLinearVelocity.z << ','
		<< car.torcsAngularVelocity.x << ','
		<< car.torcsAngularVelocity.y << ','
		<< car.torcsAngularVelocity.z << ','
		<< car.godotAngularVelocity.x << ','
		<< car.godotAngularVelocity.y << ','
		<< car.godotAngularVelocity.z << ','
		<< car.yaw << ','
		<< car.godotYaw << ','
		<< car.speed << ','
		<< car.rpm << ','
		<< car.gear << ','
		<< car.input.steer << ','
		<< car.input.throttle << ','
		<< car.input.brake << ','
		<< car.input.clutch << ','
		<< car.damage << ','
		<< car.skid << '\n';
}

static void
writeJsonString(std::ostream& out, const std::string& value)
{
	out << '"';
	for (const char ch : value) {
		switch (ch) {
		case '\\':
			out << "\\\\";
			break;
		case '"':
			out << "\\\"";
			break;
		case '\n':
			out << "\\n";
			break;
		case '\r':
			out << "\\r";
			break;
		case '\t':
			out << "\\t";
			break;
		default:
			out << ch;
			break;
		}
	}
	out << '"';
}

static void
writeJsonVec3(std::ostream& out, const TorcsBridgeVec3& vec)
{
	out
		<< "{\"x\":" << vec.x
		<< ",\"y\":" << vec.y
		<< ",\"z\":" << vec.z
		<< '}';
}

static void
writeJsonInput(std::ostream& out, const TorcsBridgeInputState& input)
{
	out
		<< "{\"steer\":" << input.steer
		<< ",\"throttle\":" << input.throttle
		<< ",\"brake\":" << input.brake
		<< ",\"clutch\":" << input.clutch
		<< ",\"gear\":" << input.gear
		<< ",\"lights\":" << (input.lights ? "true" : "false")
		<< ",\"pit_request\":" << (input.pitRequest ? "true" : "false")
		<< ",\"brake_balance\":" << input.brakeBalance
		<< '}';
}

static void
writeJsonTrack(std::ostream& out, const TorcsBridgeTrackSnapshot& track)
{
	out << "{\"track_id\":";
	writeJsonString(out, track.trackId);
	out
		<< ",\"length\":" << track.length
		<< ",\"width\":" << track.width
		<< ",\"debug_points\":[";

	for (size_t i = 0; i < track.debugPoints.size(); i++) {
		const TorcsBridgeTrackDebugPoint& point = track.debugPoints[i];
		if (i != 0) {
			out << ',';
		}
		out
			<< "{\"segment_id\":" << point.segmentId
			<< ",\"segment_name\":";
		writeJsonString(out, point.segmentName);
		out
			<< ",\"distance_from_start\":" << point.distanceFromStart
			<< ",\"surface_id\":" << point.surfaceId
			<< ",\"surface_name\":";
		writeJsonString(out, point.surfaceName);
		out
			<< ",\"start_line\":" << (point.startLine ? "true" : "false")
			<< ",\"finish_line\":" << (point.finishLine ? "true" : "false")
			<< ",\"torcs_center\":";
		writeJsonVec3(out, point.torcsCenter);
		out << ",\"godot_center\":";
		writeJsonVec3(out, point.godotCenter);
		out << ",\"torcs_left_border\":";
		writeJsonVec3(out, point.torcsLeftBorder);
		out << ",\"godot_left_border\":";
		writeJsonVec3(out, point.godotLeftBorder);
		out << ",\"torcs_right_border\":";
		writeJsonVec3(out, point.torcsRightBorder);
		out << ",\"godot_right_border\":";
		writeJsonVec3(out, point.godotRightBorder);
		out << '}';
	}

	out << "]}";
}

static void
writeJsonTrackLocalPosition(std::ostream& out, const TorcsBridgeTrackLocalPosition& position)
{
	out
		<< "{\"segment_id\":" << position.segmentId
		<< ",\"segment_name\":";
	writeJsonString(out, position.segmentName);
	out
		<< ",\"distance_from_start\":" << position.distanceFromStart
		<< ",\"to_start\":" << position.toStart
		<< ",\"to_right\":" << position.toRight
		<< ",\"to_middle\":" << position.toMiddle
		<< ",\"to_left\":" << position.toLeft
		<< ",\"surface_id\":" << position.surfaceId
		<< ",\"surface_name\":";
	writeJsonString(out, position.surfaceName);
	out
		<< ",\"start_line\":" << (position.startLine ? "true" : "false")
		<< ",\"finish_line\":" << (position.finishLine ? "true" : "false")
		<< '}';
}

static void
writeJsonWheel(std::ostream& out, const TorcsBridgeWheelSnapshot& wheel)
{
	out
		<< "{\"spin_velocity\":" << wheel.spinVelocity
		<< ",\"ride_height\":" << wheel.rideHeight
		<< ",\"slip_side\":" << wheel.slipSide
		<< ",\"slip_accel\":" << wheel.slipAccel
		<< ",\"skid\":" << wheel.skid
		<< ",\"surface_id\":" << wheel.surfaceId
		<< ",\"surface_name\":";
	writeJsonString(out, wheel.surfaceName);
	out
		<< ",\"has_contact\":" << (wheel.hasContact ? "true" : "false")
		<< ",\"torcs_contact_point\":";
	writeJsonVec3(out, wheel.torcsContactPoint);
	out << ",\"godot_contact_point\":";
	writeJsonVec3(out, wheel.godotContactPoint);
	out << ",\"torcs_surface_normal\":";
	writeJsonVec3(out, wheel.torcsSurfaceNormal);
	out << ",\"godot_surface_normal\":";
	writeJsonVec3(out, wheel.godotSurfaceNormal);
	out << '}';
}

static void
writeJsonCar(std::ostream& out, const TorcsBridgeCarSnapshot& car)
{
	out
		<< "{\"id\":" << car.id
		<< ",\"car_id\":";
	writeJsonString(out, car.carId);
	out << ",\"torcs_position\":";
	writeJsonVec3(out, car.torcsPosition);
	out << ",\"godot_position\":";
	writeJsonVec3(out, car.godotPosition);
	out << ",\"torcs_linear_velocity\":";
	writeJsonVec3(out, car.torcsLinearVelocity);
	out << ",\"godot_linear_velocity\":";
	writeJsonVec3(out, car.godotLinearVelocity);
	out << ",\"torcs_angular_velocity\":";
	writeJsonVec3(out, car.torcsAngularVelocity);
	out << ",\"godot_angular_velocity\":";
	writeJsonVec3(out, car.godotAngularVelocity);
	out
		<< ",\"yaw\":" << car.yaw
		<< ",\"godot_yaw\":" << car.godotYaw
		<< ",\"speed\":" << car.speed
		<< ",\"rpm\":" << car.rpm
		<< ",\"gear\":" << car.gear
		<< ",\"fuel\":" << car.fuel
		<< ",\"damage\":" << car.damage
		<< ",\"skid\":" << car.skid
		<< ",\"collision\":" << (car.collision ? "true" : "false")
		<< ",\"input\":";
	writeJsonInput(out, car.input);
	out << ",\"track_position\":";
	writeJsonTrackLocalPosition(out, car.trackPosition);
	out << ",\"wheels\":[";
	for (size_t i = 0; i < car.wheels.size(); i++) {
		if (i != 0) {
			out << ',';
		}
		writeJsonWheel(out, car.wheels[i]);
	}
	out << "]}";
}

static void
writeJsonSample(std::ostream& out, const TorcsBridgeSnapshot& snapshot)
{
	out
		<< "{\"time\":" << snapshot.raceTime
		<< ",\"substeps\":" << snapshot.completedSubsteps
		<< ",\"cars\":[";

	for (size_t i = 0; i < snapshot.cars.size(); i++) {
		if (i != 0) {
			out << ',';
		}
		writeJsonCar(out, snapshot.cars[i]);
	}

	out << "]}";
}

int
main(int argc, char** argv)
{
	HarnessOptions options;
	const HarnessParseResult parseResult = parseOptions(argc, argv, &options);
	if (parseResult == HARNESS_PARSE_HELP) {
		return 0;
	}
	if (parseResult == HARNESS_PARSE_ERROR) {
		return 2;
	}

	std::unique_ptr<HarnessBackendRunner> backend = createBackendRunner(options.backend);
	if (!backend || !backend->initialize(options)) {
		return 1;
	}

	std::unique_ptr<std::ofstream> file;
	std::ostream* out = &std::cout;
	if (!options.outputPath.empty()) {
		file.reset(new std::ofstream(options.outputPath));
		if (!file->is_open()) {
			std::cerr << "Failed to open output file: " << options.outputPath << "\n";
			return 1;
		}
		out = file.get();
	}

	out->setf(std::ios::fixed);
	out->precision(6);

	if (options.outputFormat == HARNESS_OUTPUT_CSV) {
		writeCsvHeader(*out);
	} else {
		*out << "{\"track\":";
		writeJsonTrack(*out, backend->getSnapshot().track);
		*out << ",\"samples\":[";
	}

	bool wroteJsonSample = false;
	for (double elapsed = 0.0; elapsed < options.seconds;) {
		const double stepSeconds = std::min(options.sampleSeconds, options.seconds - elapsed);
		backend->setHumanInput(scriptedInput(elapsed));
		const TorcsBridgeSnapshot snapshot = backend->step(stepSeconds);
		if (options.outputFormat == HARNESS_OUTPUT_CSV) {
			writeCsvRow(*out, snapshot);
		} else {
			if (wroteJsonSample) {
				*out << ',';
			}
			writeJsonSample(*out, snapshot);
			wroteJsonSample = true;
		}
		elapsed += stepSeconds;
	}

	if (options.outputFormat == HARNESS_OUTPUT_JSON) {
		*out << "]}\n";
	}

	backend->shutdown();

	return 0;
}
