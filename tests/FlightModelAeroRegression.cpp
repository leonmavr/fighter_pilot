#include "physics/FlightPhysics.hpp"

#include <cmath>
#include <iostream>

namespace {

bool run_level_trim_check() {
    planet_aces::FlightModel model;
    planet_aces::AircraftState state;
    state.positionMeters = {0.0, 1'200.0, 0.0};
    state.velocityMetersPerSecond = {0.0, 0.0, 170.0};

    const double startingAltitude = state.positionMeters.y;
    for (int step = 0; step < 900; ++step) {
        planet_aces::AircraftInput input;
        input.throttle01 = 0.86;
        model.step(state, input, 1.0 / 120.0);
    }

    return state.positionMeters.y > startingAltitude - 150.0
        && state.airspeedMetersPerSecond > 120.0
        && state.loadFactorG > 0.2;
}

bool run_pitch_pull_response_check() {
    planet_aces::FlightModel model;
    planet_aces::AircraftState state;
    state.positionMeters = {0.0, 1'200.0, 0.0};
    state.velocityMetersPerSecond = {0.0, 0.0, 185.0};

    for (int step = 0; step < 180; ++step) {
        planet_aces::AircraftInput input;
        input.throttle01 = 0.82;
        input.pitch01 = 0.9;
        model.step(state, input, 1.0 / 120.0);
    }

    return state.pitchRadians > planet_aces::deg_to_rad(3.0)
        && state.loadFactorG > 0.15;
}

bool run_nose_down_check() {
    planet_aces::FlightModel model;
    planet_aces::AircraftState state;
    state.positionMeters = {0.0, 1'200.0, 0.0};
    state.velocityMetersPerSecond = {0.0, -15.0, 170.0};
    state.pitchRadians = planet_aces::deg_to_rad(-12.0);

    const double startingAltitude = state.positionMeters.y;
    for (int step = 0; step < 480; ++step) {
        planet_aces::AircraftInput input;
        input.throttle01 = 0.72;
        model.step(state, input, 1.0 / 120.0);
    }

    return state.positionMeters.y < startingAltitude - 40.0;
}

bool run_banked_pull_check() {
    planet_aces::FlightModel model;
    planet_aces::AircraftState state;
    state.positionMeters = {0.0, 1'200.0, 0.0};
    state.velocityMetersPerSecond = {0.0, 0.0, 185.0};
    state.rollRadians = planet_aces::deg_to_rad(85.0);

    const double startingAltitude = state.positionMeters.y;
    for (int step = 0; step < 240; ++step) {
        planet_aces::AircraftInput input;
        input.throttle01 = 0.84;
        input.pitch01 = 0.9;
        model.step(state, input, 1.0 / 120.0);
    }

    const double lateralOffset = std::abs(state.positionMeters.x);
    const double altitudeGain = state.positionMeters.y - startingAltitude;
    return lateralOffset > 12.0 && lateralOffset > std::abs(altitudeGain) * 0.55;
}

bool run_banked_climb_cost_check() {
    auto simulate_pull = [](double rollDegrees) {
        planet_aces::FlightModel model;
        planet_aces::AircraftState state;
        state.positionMeters = {0.0, 1'200.0, 0.0};
        state.velocityMetersPerSecond = {0.0, 0.0, 185.0};
        state.rollRadians = planet_aces::deg_to_rad(rollDegrees);

        const double startingAltitude = state.positionMeters.y;
        const double startingSpeed = planet_aces::length(state.velocityMetersPerSecond);
        for (int step = 0; step < 240; ++step) {
            planet_aces::AircraftInput input;
            input.throttle01 = 0.82;
            input.pitch01 = 0.9;
            model.step(state, input, 1.0 / 120.0);
        }

        return std::pair<double, double> {
            state.positionMeters.y - startingAltitude,
            startingSpeed - planet_aces::length(state.velocityMetersPerSecond),
        };
    };

    const auto [levelClimb, levelSpeedLoss] = simulate_pull(0.0);
    const auto [bankedClimb, bankedSpeedLoss] = simulate_pull(60.0);
    const auto [knifeEdgeClimb, knifeEdgeSpeedLoss] = simulate_pull(85.0);

    return bankedClimb < levelClimb * 0.65
        && knifeEdgeClimb < bankedClimb
        && bankedSpeedLoss > levelSpeedLoss
        && knifeEdgeSpeedLoss > bankedSpeedLoss;
}

bool run_speed_cap_check() {
    planet_aces::AircraftParameters parameters;
    parameters.maxAirspeedMetersPerSecond = 210.0;
    parameters.maxThrustNewtons = 180'000.0;

    planet_aces::FlightModel model(parameters);
    planet_aces::AircraftState state;
    state.velocityMetersPerSecond = {0.0, 0.0, 205.0};

    for (int step = 0; step < 600; ++step) {
        planet_aces::AircraftInput input;
        input.throttle01 = 1.0;
        model.step(state, input, 1.0 / 120.0);
    }

    return state.airspeedMetersPerSecond <= 211.0;
}

bool run_afterburner_check() {
    planet_aces::AircraftParameters parameters;
    parameters.maxThrustNewtons = 120'000.0;
    parameters.afterburnerThrustMultiplier = 1.8;
    parameters.afterburnerFuelCapacityLiters = 20.0;
    parameters.afterburnerBurnLitersPerSecond = 10.0;

    auto simulate = [&](bool afterburnerRequested) {
        planet_aces::FlightModel model(parameters);
        planet_aces::AircraftState state;
        state.positionMeters = {0.0, 1'200.0, 0.0};
        state.velocityMetersPerSecond = {0.0, 0.0, 160.0};

        for (int step = 0; step < 240; ++step) {
            planet_aces::AircraftInput input;
            input.throttle01 = 1.0;
            input.afterburnerRequested = afterburnerRequested;
            model.step(state, input, 1.0 / 120.0);
        }

        return state;
    };

    const planet_aces::AircraftState dryState = simulate(false);
    const planet_aces::AircraftState burnerState = simulate(true);

    return burnerState.airspeedMetersPerSecond > dryState.airspeedMetersPerSecond + 8.0
        && burnerState.afterburnerFuelRemainingLiters < parameters.afterburnerFuelCapacityLiters
        && burnerState.afterburnerFuelRemainingLiters >= 0.0;
}

bool run_engine_acceleration_tuning_check() {
    auto simulate = [](double tunedAcceleration) {
        planet_aces::AircraftParameters parameters;
        parameters.maxThrustNewtons = 70'000.0;
        parameters.maxAirspeedMetersPerSecond = 320.0;
        parameters.engineAccelerationMetersPerSecondSquared = tunedAcceleration;

        planet_aces::FlightModel model(parameters);
        planet_aces::AircraftState state;
        state.positionMeters = {0.0, 1'200.0, 0.0};
        state.velocityMetersPerSecond = {0.0, 0.0, 140.0};

        for (int step = 0; step < 240; ++step) {
            planet_aces::AircraftInput input;
            input.throttle01 = 1.0;
            model.step(state, input, 1.0 / 120.0);
        }

        return state.airspeedMetersPerSecond;
    };

    const double baselineSpeed = simulate(0.0);
    const double tunedSpeed = simulate(18.0);
    return tunedSpeed > baselineSpeed + 10.0;
}

}  // namespace

int main() {
    const bool levelTrimOk = run_level_trim_check();
    const bool pitchPullOk = run_pitch_pull_response_check();
    const bool noseDownOk = run_nose_down_check();
    const bool bankedPullOk = run_banked_pull_check();
    const bool bankedClimbCostOk = run_banked_climb_cost_check();
    const bool speedCapOk = run_speed_cap_check();
    const bool afterburnerOk = run_afterburner_check();
    const bool engineAccelerationOk = run_engine_acceleration_tuning_check();

    if (!levelTrimOk || !pitchPullOk || !noseDownOk || !bankedPullOk || !bankedClimbCostOk || !speedCapOk || !afterburnerOk || !engineAccelerationOk) {
        std::cerr << "Aerodynamic regression failed"
                  << " levelTrimOk=" << levelTrimOk
                  << " pitchPullOk=" << pitchPullOk
                  << " noseDownOk=" << noseDownOk
                  << " bankedPullOk=" << bankedPullOk
                  << " bankedClimbCostOk=" << bankedClimbCostOk
                  << " speedCapOk=" << speedCapOk
                  << " afterburnerOk=" << afterburnerOk
                  << " engineAccelerationOk=" << engineAccelerationOk << '\n';
        return 1;
    }

    return 0;
}