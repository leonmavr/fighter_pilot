#include "assets/AircraftParametersAsset.hpp"

#include <fstream>
#include <string>
#include <unordered_map>

namespace planet_aces {

namespace {

std::string trim_copy(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool set_parameter_value(AircraftParameters& parameters, const std::string& key, double value) {
    static const std::unordered_map<std::string, double AircraftParameters::*> kFieldMap {
        {"massKg", &AircraftParameters::massKg},
        {"wingAreaM2", &AircraftParameters::wingAreaM2},
        {"maxThrustNewtons", &AircraftParameters::maxThrustNewtons},
        {"engineAccelerationMetersPerSecondSquared", &AircraftParameters::engineAccelerationMetersPerSecondSquared},
        {"afterburnerThrustMultiplier", &AircraftParameters::afterburnerThrustMultiplier},
        {"afterburnerFuelCapacityLiters", &AircraftParameters::afterburnerFuelCapacityLiters},
        {"afterburnerBurnLitersPerSecond", &AircraftParameters::afterburnerBurnLitersPerSecond},
        {"maxAirspeedMetersPerSecond", &AircraftParameters::maxAirspeedMetersPerSecond},
        {"zeroLiftDragCoefficient", &AircraftParameters::zeroLiftDragCoefficient},
        {"inducedDragFactor", &AircraftParameters::inducedDragFactor},
        {"maxLiftCoefficient", &AircraftParameters::maxLiftCoefficient},
        {"liftSlopePerRad", &AircraftParameters::liftSlopePerRad},
        {"stallAngleRadians", &AircraftParameters::stallAngleRadians},
        {"trimAngleRadians", &AircraftParameters::trimAngleRadians},
        {"baseLiftCoefficient", &AircraftParameters::baseLiftCoefficient},
        {"pitchCommandLiftCoefficient", &AircraftParameters::pitchCommandLiftCoefficient},
        {"negativePitchLiftCoefficient", &AircraftParameters::negativePitchLiftCoefficient},
        {"sideForceCoefficient", &AircraftParameters::sideForceCoefficient},
        {"bankedLiftPenalty", &AircraftParameters::bankedLiftPenalty},
        {"maneuverDragFactor", &AircraftParameters::maneuverDragFactor},
        {"engineAltitudeLossPerMeter", &AircraftParameters::engineAltitudeLossPerMeter},
        {"maxPitchRateRadiansPerSecond", &AircraftParameters::maxPitchRateRadiansPerSecond},
        {"maxRollRateRadiansPerSecond", &AircraftParameters::maxRollRateRadiansPerSecond},
        {"maxYawRateRadiansPerSecond", &AircraftParameters::maxYawRateRadiansPerSecond},
        {"pitchResponsePerSecond", &AircraftParameters::pitchResponsePerSecond},
        {"rollResponsePerSecond", &AircraftParameters::rollResponsePerSecond},
        {"yawResponsePerSecond", &AircraftParameters::yawResponsePerSecond},
        {"controlSensitivity", &AircraftParameters::controlSensitivity},
        {"maneuverAuthorityAirspeedMetersPerSecond", &AircraftParameters::maneuverAuthorityAirspeedMetersPerSecond},
    };

    const auto it = kFieldMap.find(key);
    if (it == kFieldMap.end()) {
        return false;
    }

    parameters.*(it->second) = value;
    return true;
}

}  // namespace

AircraftParametersLoadResult load_aircraft_parameters_file(const std::string& filePath) {
    AircraftParametersLoadResult result;

    std::ifstream input(filePath);
    if (!input) {
        result.error = "could not open file";
        return result;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) {
            result.error = "invalid line " + std::to_string(lineNumber);
            return result;
        }

        const std::string key = trim_copy(trimmed.substr(0, equals));
        const std::string rawValue = trim_copy(trimmed.substr(equals + 1));

        try {
            const double value = std::stod(rawValue);
            if (!set_parameter_value(result.parameters, key, value)) {
                result.error = "unknown key '" + key + "'";
                return result;
            }
        } catch (...) {
            result.error = "invalid numeric value on line " + std::to_string(lineNumber);
            return result;
        }
    }

    result.loaded = true;
    return result;
}

}  // namespace planet_aces