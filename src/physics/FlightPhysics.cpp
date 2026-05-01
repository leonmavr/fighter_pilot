#include "physics/FlightPhysics.hpp"

#include <cmath>

namespace planet_aces {

namespace {

constexpr double kGravityMetersPerSecondSquared = 9.81;
constexpr double kAirDensityKgPerM3 = 1.225;
constexpr double kAtmosphereScaleHeightMeters = 8'500.0;

Vec3 euler_forward(double pitchRadians, double yawRadians) noexcept {
    const double cosPitch = std::cos(pitchRadians);
    return normalized({
        cosPitch * std::sin(yawRadians),
        std::sin(pitchRadians),
        cosPitch * std::cos(yawRadians),
    });
}

Vec3 stable_right_from_forward(const Vec3& forward) noexcept {
    const Vec3 worldUp {0.0, 1.0, 0.0};
    Vec3 right = cross(worldUp, forward);
    if (length_squared(right) < 1e-8) {
        right = {1.0, 0.0, 0.0};
    }
    return normalized(right);
}

double air_density_at_altitude(double altitudeMeters) noexcept {
    const double clampedAltitude = std::max(0.0, altitudeMeters);
    return kAirDensityKgPerM3 * std::exp(-clampedAltitude / kAtmosphereScaleHeightMeters);
}

bool has_valid_orientation(const AircraftState& state) noexcept {
    return length_squared(state.forwardDirection) > 0.5 && length_squared(state.upDirection) > 0.5;
}

void initialize_orientation_from_euler(AircraftState& state) noexcept {
    const Vec3 forward = euler_forward(state.pitchRadians, state.yawRadians);
    const Vec3 baseRight = stable_right_from_forward(forward);
    const Vec3 baseUp = normalized(cross(forward, baseRight));
    const Vec3 rolledRight = normalized(baseRight * std::cos(state.rollRadians) + baseUp * std::sin(state.rollRadians));
    state.forwardDirection = forward;
    state.upDirection = normalized(cross(forward, rolledRight));
}

void orthonormalize_orientation(AircraftState& state) noexcept {
    state.forwardDirection = normalized(state.forwardDirection);
    Vec3 right = cross(state.upDirection, state.forwardDirection);
    if (length_squared(right) < 1e-8) {
        right = stable_right_from_forward(state.forwardDirection);
    } else {
        right = normalized(right);
    }
    state.upDirection = normalized(cross(state.forwardDirection, right));
}

void sync_euler_from_orientation(AircraftState& state) noexcept {
    orthonormalize_orientation(state);
    const Vec3 forward = state.forwardDirection;
    const Vec3 up = state.upDirection;
    const Vec3 right = normalized(cross(up, forward));
    const Vec3 baseRight = stable_right_from_forward(forward);

    state.pitchRadians = std::asin(clamp(forward.y, -1.0, 1.0));
    state.yawRadians = std::atan2(forward.x, forward.z);
    state.rollRadians = std::atan2(dot(cross(baseRight, right), forward), dot(baseRight, right));
}

double control_authority_factor(double speedMetersPerSecond, const AircraftParameters& parameters) noexcept {
    const double speedRatio = speedMetersPerSecond / std::max(1.0, parameters.maneuverAuthorityAirspeedMetersPerSecond);
    return clamp(speedRatio / (0.55 + speedRatio), 0.2, 1.0);
}

}  // namespace

Vec3 forward_vector(const AircraftState& state) noexcept {
    if (has_valid_orientation(state)) {
        return normalized(state.forwardDirection);
    }
    return euler_forward(state.pitchRadians, state.yawRadians);
}

Vec3 right_vector(const AircraftState& state) noexcept {
    const Vec3 forward = forward_vector(state);
    if (has_valid_orientation(state)) {
        const Vec3 right = cross(state.upDirection, forward);
        if (length_squared(right) > 1e-8) {
            return normalized(right);
        }
    }
    const Vec3 baseRight = stable_right_from_forward(forward);
    const Vec3 baseUp = normalized(cross(forward, baseRight));
    return normalized(baseRight * std::cos(state.rollRadians) + baseUp * std::sin(state.rollRadians));
}

Vec3 up_vector(const AircraftState& state) noexcept {
    if (has_valid_orientation(state)) {
        return normalized(state.upDirection);
    }
    const Vec3 forward = forward_vector(state);
    const Vec3 right = right_vector(state);
    return normalized(cross(forward, right));
}

FlightPhysics::FlightPhysics(AircraftParameters parameters)
    : parameters_(parameters) {}

void FlightPhysics::step(AircraftState& state, const AircraftInput& input, double dtSeconds) const {
    const double dt = std::max(dtSeconds, 0.0);
    if (dt <= 0.0) {
        return;
    }

    if (!has_valid_orientation(state)) {
        initialize_orientation_from_euler(state);
    }

    if (state.afterburnerFuelRemainingLiters < 0.0) {
        state.afterburnerFuelRemainingLiters = parameters_.afterburnerFuelCapacityLiters;
    }

    const double throttle = clamp(input.throttle01, 0.0, 1.0);
    state.afterburnerActive = false;

    const double speed = length(state.velocityMetersPerSecond);
    const double controlAuthority = control_authority_factor(speed, parameters_);
    const double pitchInput = clamp(input.pitch01 * parameters_.controlSensitivity, -1.0, 1.0);
    const double rollInput = clamp(input.roll01 * parameters_.controlSensitivity, -1.0, 1.0);
    const double yawInput = clamp(input.yaw01 * parameters_.controlSensitivity, -1.0, 1.0);

    const double targetPitchRate = pitchInput * parameters_.maxPitchRateRadiansPerSecond * controlAuthority;
    const double targetRollRate = rollInput * parameters_.maxRollRateRadiansPerSecond * controlAuthority;
    const double targetYawRate = yawInput * parameters_.maxYawRateRadiansPerSecond * controlAuthority;

    state.pitchRateRadiansPerSecond += (targetPitchRate - state.pitchRateRadiansPerSecond)
        * parameters_.pitchResponsePerSecond * dt;
    state.rollRateRadiansPerSecond += (targetRollRate - state.rollRateRadiansPerSecond)
        * parameters_.rollResponsePerSecond * dt;
    state.yawRateRadiansPerSecond += (targetYawRate - state.yawRateRadiansPerSecond)
        * parameters_.yawResponsePerSecond * dt;

    state.pitchRateRadiansPerSecond = clamp(
        state.pitchRateRadiansPerSecond,
        -parameters_.maxPitchRateRadiansPerSecond,
        parameters_.maxPitchRateRadiansPerSecond);
    state.rollRateRadiansPerSecond = clamp(
        state.rollRateRadiansPerSecond,
        -parameters_.maxRollRateRadiansPerSecond,
        parameters_.maxRollRateRadiansPerSecond);
    state.yawRateRadiansPerSecond = clamp(
        state.yawRateRadiansPerSecond,
        -parameters_.maxYawRateRadiansPerSecond,
        parameters_.maxYawRateRadiansPerSecond);

    {
        const Vec3 forward = forward_vector(state);
        const Vec3 right = right_vector(state);
        const Vec3 up = up_vector(state);
        const Vec3 angularVelocityWorld =
            right * (-state.pitchRateRadiansPerSecond)
            + forward * state.rollRateRadiansPerSecond
            + up * state.yawRateRadiansPerSecond;

        state.forwardDirection += cross(angularVelocityWorld, state.forwardDirection) * dt;
        state.upDirection += cross(angularVelocityWorld, state.upDirection) * dt;
        sync_euler_from_orientation(state);
    }

    const Vec3 forward = forward_vector(state);
    const Vec3 right = right_vector(state);
    const Vec3 up = up_vector(state);
    const Vec3 worldUp {0.0, 1.0, 0.0};
    const Vec3 airVelocity = state.velocityMetersPerSecond;
    const double updatedSpeed = length(airVelocity);
    const Vec3 velocityDirection = updatedSpeed > 1e-6 ? airVelocity / updatedSpeed : forward;
    const double sideSlip = dot(velocityDirection, right);
    const double climbSlip = dot(velocityDirection, up);
    const double bankFactor = std::sqrt(std::max(0.0, 1.0 - clamp(dot(up, worldUp), -1.0, 1.0) * clamp(dot(up, worldUp), -1.0, 1.0)));
    const double positivePitchInput = std::max(0.0, pitchInput);
    const double bankedLiftEfficiency = 1.0 - parameters_.bankedLiftPenalty * positivePitchInput * bankFactor * bankFactor;
    const double commandedLiftCoefficient = parameters_.baseLiftCoefficient
        + (pitchInput >= 0.0
            ? pitchInput * parameters_.pitchCommandLiftCoefficient
            : pitchInput * parameters_.negativePitchLiftCoefficient)
        + parameters_.trimAngleRadians * parameters_.liftSlopePerRad * 0.15;
    const double liftCoefficient = clamp(
        commandedLiftCoefficient * controlAuthority * std::max(0.35, bankedLiftEfficiency),
        -0.35,
        parameters_.maxLiftCoefficient);
    const double maneuverDragCoefficient = parameters_.maneuverDragFactor
        * positivePitchInput
        * (0.35 + bankFactor * bankFactor)
        * (0.55 + std::abs(liftCoefficient));
    const double overspeedRatio = std::max(
        0.0,
        updatedSpeed - parameters_.maxAirspeedMetersPerSecond) / std::max(1.0, parameters_.maxAirspeedMetersPerSecond);
    const double overspeedDragCoefficient = 3.5 * overspeedRatio * overspeedRatio;
    const double dragCoefficient = parameters_.zeroLiftDragCoefficient
        + parameters_.inducedDragFactor * liftCoefficient * liftCoefficient
        + 0.18 * sideSlip * sideSlip
        + 0.10 * climbSlip * climbSlip
        + maneuverDragCoefficient
        + overspeedDragCoefficient;

    Vec3 liftDirection = up - velocityDirection * dot(up, velocityDirection);
    if (length_squared(liftDirection) < 1e-8) {
        liftDirection = normalized(cross(velocityDirection, right));
    } else {
        liftDirection = normalized(liftDirection);
    }

    const double airDensity = air_density_at_altitude(state.positionMeters.y);
    const double dynamicPressure = 0.5 * airDensity * updatedSpeed * updatedSpeed;
    const double engineFactor = clamp(1.0 - state.positionMeters.y * parameters_.engineAltitudeLossPerMeter, 0.65, 1.0);
    double thrustMultiplier = 1.0;
    if (input.afterburnerRequested
        && parameters_.afterburnerFuelCapacityLiters > 0.0
        && parameters_.afterburnerBurnLitersPerSecond > 0.0
        && state.afterburnerFuelRemainingLiters > 0.0) {
        const double fuelBurned = std::min(
            state.afterburnerFuelRemainingLiters,
            parameters_.afterburnerBurnLitersPerSecond * dt);
        state.afterburnerFuelRemainingLiters -= fuelBurned;
        state.afterburnerActive = fuelBurned > 0.0;
        if (state.afterburnerActive) {
            thrustMultiplier = parameters_.afterburnerThrustMultiplier;
        }
    }
    const Vec3 thrust = forward * (throttle * parameters_.maxThrustNewtons * engineFactor * thrustMultiplier);
    const Vec3 drag = velocityDirection * (-dynamicPressure * parameters_.wingAreaM2 * dragCoefficient);
    const Vec3 lift = liftDirection * (dynamicPressure * parameters_.wingAreaM2 * liftCoefficient);
    const Vec3 sideForce = right * (-dynamicPressure * parameters_.wingAreaM2 * parameters_.sideForceCoefficient * sideSlip);
    const Vec3 gravity {0.0, -parameters_.massKg * kGravityMetersPerSecondSquared, 0.0};

    Vec3 acceleration = (thrust + drag + lift + sideForce + gravity) / parameters_.massKg;
    const double speedRatio = updatedSpeed / std::max(1.0, parameters_.maxAirspeedMetersPerSecond);
    const double accelerationAssistFactor = clamp(1.0 - speedRatio * speedRatio, 0.15, 1.0);
    acceleration += forward * (throttle * parameters_.engineAccelerationMetersPerSecondSquared * engineFactor * accelerationAssistFactor);

    state.velocityMetersPerSecond += acceleration * dt;
    const double cappedSpeed = length(state.velocityMetersPerSecond);
    if (cappedSpeed > parameters_.maxAirspeedMetersPerSecond) {
        state.velocityMetersPerSecond = state.velocityMetersPerSecond
            * (parameters_.maxAirspeedMetersPerSecond / cappedSpeed);
    }
    state.positionMeters += state.velocityMetersPerSecond * dt;
    state.airspeedMetersPerSecond = length(state.velocityMetersPerSecond);
    state.loadFactorG = dot(lift, up) / (parameters_.massKg * kGravityMetersPerSecondSquared);
    sync_euler_from_orientation(state);
}

const AircraftParameters& FlightPhysics::parameters() const noexcept {
    return parameters_;
}

}  // namespace planet_aces