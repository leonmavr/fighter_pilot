#include "aircraft/F15C.hpp"

#include "core/AssetPaths.hpp"

namespace planet_aces {

namespace {

AircraftParameters make_default_f15_parameters() {
    AircraftParameters parameters;
    parameters.massKg = 12'850.0;
    parameters.wingAreaM2 = 56.5;
    parameters.maxThrustNewtons = 104'000.0;
    parameters.engineAccelerationMetersPerSecondSquared = 30.0;
    parameters.afterburnerThrustMultiplier = 1.72;
    parameters.afterburnerFuelCapacityLiters = 500.0;
    parameters.afterburnerBurnLitersPerSecond = 50.0;
    parameters.maxAirspeedMetersPerSecond = 355.0;
    parameters.zeroLiftDragCoefficient = 0.02;
    parameters.inducedDragFactor = 0.11;
    parameters.maxLiftCoefficient = 1.38;
    parameters.liftSlopePerRad = 4.1;
    parameters.stallAngleRadians = 0.28;
    parameters.trimAngleRadians = 0.026;
    parameters.baseLiftCoefficient = 0.17;
    parameters.pitchCommandLiftCoefficient = 0.72;
    parameters.negativePitchLiftCoefficient = 0.36;
    parameters.sideForceCoefficient = 0.94;
    parameters.bankedLiftPenalty = 0.74;
    parameters.maneuverDragFactor = 0.52;
    parameters.engineAltitudeLossPerMeter = 0.000018;
    parameters.maxPitchRateRadiansPerSecond = 0.61;
    parameters.maxRollRateRadiansPerSecond = 1.95;
    parameters.maxYawRateRadiansPerSecond = 0.28;
    parameters.pitchResponsePerSecond = 3.2;
    parameters.rollResponsePerSecond = 4.8;
    parameters.yawResponsePerSecond = 2.7;
    parameters.controlSensitivity = 1.0;
    parameters.maneuverAuthorityAirspeedMetersPerSecond = 125.0;
    return parameters;
}

}  // namespace

F15C::F15C()
    : Aircraft(make_definition()) {}

AircraftDefinition F15C::make_definition() {
    AircraftDefinition definition;
    definition.name = "F-15C";
    definition.flightModelAssetPath = assets::kF15FlightModelAssetPath;
    definition.modelAssetPath = assets::kF15ModelAssetPath;
    definition.defaultParameters = make_default_f15_parameters();
    definition.initialState.positionMeters = {0.0, 220.0, 0.0};
    definition.initialState.velocityMetersPerSecond = {0.0, 0.0, 160.0};
    definition.renderConfig.alignPitchDegrees = -90.0f;
    definition.renderConfig.alignYawDegrees = -90.0f;
    definition.renderConfig.targetLengthRenderUnits = 5.8f;
    definition.engineFxOffsetsMeters = {
        {-3.8, -0.4, -24.0},
        {3.8, -0.4, -24.0},
    };
    return definition;
}

}  // namespace planet_aces