#include "aircraft/Aircraft.hpp"

#include "assets/AircraftParametersAsset.hpp"

#include <algorithm>
#include <iostream>
#include <utility>

namespace planet_aces {

Aircraft::Aircraft(AircraftDefinition definition)
    : definition_(std::move(definition))
    , parameters_(load_parameters_or_default(definition_))
    , physics_(parameters_)
    , state_(definition_.initialState) {}

void Aircraft::step(const AircraftInput& input, double dtSeconds) {
    physics_.step(state_, input, dtSeconds);
}

void Aircraft::clamp_above_ground(double groundHeightMeters, double clearanceMeters) {
    if (state_.positionMeters.y < groundHeightMeters + clearanceMeters) {
        state_.positionMeters.y = groundHeightMeters + clearanceMeters;
        state_.velocityMetersPerSecond.y = std::max(0.0, state_.velocityMetersPerSecond.y * 0.15);
    }
}

const AircraftDefinition& Aircraft::definition() const noexcept {
    return definition_;
}

const AircraftParameters& Aircraft::parameters() const noexcept {
    return parameters_;
}

const FlightPhysics& Aircraft::physics() const noexcept {
    return physics_;
}

const AircraftState& Aircraft::state() const noexcept {
    return state_;
}

AircraftState& Aircraft::state() noexcept {
    return state_;
}

AircraftParameters Aircraft::load_parameters_or_default(const AircraftDefinition& definition) {
    const AircraftParametersLoadResult result = load_aircraft_parameters_file(definition.flightModelAssetPath);
    if (!result.loaded) {
        std::cerr << "warning: could not load aircraft tuning from "
                  << definition.flightModelAssetPath
                  << " (" << result.error << "), using defaults\n";
        return definition.defaultParameters;
    }

    return result.parameters;
}

}  // namespace planet_aces