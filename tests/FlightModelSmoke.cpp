#include "physics/FlightPhysics.hpp"

#include <cmath>
#include <iostream>

int main() {
    planet_aces::FlightModel model;
    planet_aces::AircraftState state;

    const double dtSeconds = 1.0 / 120.0;
    for (int step = 0; step < 1200; ++step) {
        planet_aces::AircraftInput input;
        input.throttle01 = 1.0;
        input.pitch01 = step < 240 ? 0.06 : 0.0;
        model.step(state, input, dtSeconds);
    }

    const bool finiteState = std::isfinite(state.positionMeters.x)
        && std::isfinite(state.positionMeters.y)
        && std::isfinite(state.positionMeters.z)
        && std::isfinite(state.airspeedMetersPerSecond)
        && std::isfinite(state.loadFactorG);

    const bool sensibleFlight = state.airspeedMetersPerSecond > 80.0
        && state.positionMeters.y > 100.0
        && state.loadFactorG > 0.1;

    if (!finiteState || !sensibleFlight) {
        std::cerr << "Smoke test failed: alt=" << state.positionMeters.y
                  << " speed=" << state.airspeedMetersPerSecond
                  << " load=" << state.loadFactorG << '\n';
        return 1;
    }

    return 0;
}