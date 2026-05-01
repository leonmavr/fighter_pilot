#pragma once

#include "physics/FlightPhysics.hpp"

#include <string>

namespace planet_aces {

struct AircraftParametersLoadResult {
    AircraftParameters parameters;
    bool loaded = false;
    std::string error;
};

AircraftParametersLoadResult load_aircraft_parameters_file(const std::string& filePath);

}  // namespace planet_aces