#pragma once

#include "core/Math.hpp"

namespace planet_aces {

struct AircraftInput {
    double throttle01 = 1.0;
    double pitch01 = 0.0;
    double roll01 = 0.0;
    double yaw01 = 0.0;
    bool afterburnerRequested = false;
    bool triggerPrimary = false;
};

struct AircraftParameters {
    double massKg = 9'500.0;
    double wingAreaM2 = 27.5;
    double maxThrustNewtons = 82'000.0;
    double engineAccelerationMetersPerSecondSquared = 0.0;
    double afterburnerThrustMultiplier = 1.65;
    double afterburnerFuelCapacityLiters = 500.0;
    double afterburnerBurnLitersPerSecond = 50.0;
    double maxAirspeedMetersPerSecond = 340.0;
    double zeroLiftDragCoefficient = 0.021;
    double inducedDragFactor = 0.075;
    double maxLiftCoefficient = 1.45;
    double liftSlopePerRad = 4.4;
    double stallAngleRadians = deg_to_rad(16.0);
    double trimAngleRadians = deg_to_rad(2.5);
    double baseLiftCoefficient = 0.19;
    double pitchCommandLiftCoefficient = 0.82;
    double negativePitchLiftCoefficient = 0.42;
    double sideForceCoefficient = 0.72;
    double bankedLiftPenalty = 0.58;
    double maneuverDragFactor = 0.34;
    double engineAltitudeLossPerMeter = 0.00002;
    double maxPitchRateRadiansPerSecond = deg_to_rad(40.0);
    double maxRollRateRadiansPerSecond = deg_to_rad(120.0);
    double maxYawRateRadiansPerSecond = deg_to_rad(18.0);
    double pitchResponsePerSecond = 3.8;
    double rollResponsePerSecond = 5.4;
    double yawResponsePerSecond = 3.0;
    double controlSensitivity = 1.0;
    double maneuverAuthorityAirspeedMetersPerSecond = 115.0;
};

struct AircraftState {
    Vec3 positionMeters {0.0, 1'200.0, 0.0};
    Vec3 velocityMetersPerSecond {0.0, 0.0, 140.0};
    Vec3 forwardDirection;
    Vec3 upDirection;
    double pitchRadians = 0.0;
    double rollRadians = 0.0;
    double yawRadians = 0.0;
    double pitchRateRadiansPerSecond = 0.0;
    double rollRateRadiansPerSecond = 0.0;
    double yawRateRadiansPerSecond = 0.0;
    double afterburnerFuelRemainingLiters = -1.0;
    double airspeedMetersPerSecond = 0.0;
    double loadFactorG = 1.0;
    bool afterburnerActive = false;
};

Vec3 forward_vector(const AircraftState& state) noexcept;
Vec3 right_vector(const AircraftState& state) noexcept;
Vec3 up_vector(const AircraftState& state) noexcept;

class FlightPhysics {
public:
    explicit FlightPhysics(AircraftParameters parameters = {});

    void step(AircraftState& state, const AircraftInput& input, double dtSeconds) const;
    const AircraftParameters& parameters() const noexcept;

private:
    AircraftParameters parameters_;
};

using FlightModel = FlightPhysics;

}  // namespace planet_aces