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

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>

struct HarnessOptions {
	std::string dataRoot = TORCS_BRIDGE_DEFAULT_DATA_ROOT;
	std::string localRoot = TORCS_BRIDGE_DEFAULT_LOCAL_ROOT;
	std::string libraryRoot = TORCS_BRIDGE_DEFAULT_LIBRARY_ROOT;
	std::string outputPath;
	double seconds = 10.0;
	double sampleSeconds = TORCS_BRIDGE_ROBOT_STEP_SECONDS;
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
		<< "  --seconds <value>       Simulation duration, default 10.0.\n"
		<< "  --sample-seconds <val>  CSV sample period, default 0.02.\n"
		<< "  --output <path>         Write CSV to a file instead of stdout.\n"
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
		} else if (arg == "--output") {
			options->outputPath = value;
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
		} else {
			std::cerr << "Unknown option: " << arg << "\n";
			return HARNESS_PARSE_ERROR;
		}
	}

	if (options->seconds <= 0.0) {
		std::cerr << "--seconds must be positive.\n";
		return HARNESS_PARSE_ERROR;
	}

	if (options->sampleSeconds < TORCS_BRIDGE_SIM_STEP_SECONDS) {
		std::cerr << "--sample-seconds must be at least "
			<< TORCS_BRIDGE_SIM_STEP_SECONDS << ".\n";
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

static void
writeCsvHeader(std::ostream& out)
{
	out
		<< "time,substeps,car_id,torcs_x,torcs_y,torcs_z,"
		<< "godot_x,godot_y,godot_z,yaw,speed,rpm,gear,"
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
		<< car.yaw << ','
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

	TorcsRuntime runtime;
	if (!runtime.initialize({ options.dataRoot, options.localRoot, options.libraryRoot })) {
		std::cerr << "Failed to initialize TORCS bridge runtime.\n";
		return 1;
	}

	TorcsRace race(runtime);
	if (!race.load({
		"data/tracks/road/wheel-2/wheel-2.xml",
		"data/cars/models/car1-trb1/car1-trb1.xml",
		"car1-trb1",
		0
	})) {
		std::cerr << "Failed to load TORCS bridge race.\n";
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

	writeCsvHeader(*out);
	for (double elapsed = 0.0; elapsed < options.seconds; elapsed += options.sampleSeconds) {
		race.setHumanInput(0, scriptedInput(elapsed));
		const TorcsBridgeSnapshot snapshot = race.step(options.sampleSeconds);
		writeCsvRow(*out, snapshot);
	}

	race.shutdown();
	runtime.shutdown();

	return 0;
}
