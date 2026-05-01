#pragma once

#include <string_view>

namespace planet_aces {

struct FlightHudData {
    double airspeedMetersPerSecond = 0.0;
    double altitudeMeters = 0.0;
    float throttle01 = 0.0f;
    double loadFactorG = 1.0;
    std::string_view cameraName;
    bool afterburnerActive = false;
    double afterburnerFuelRemainingLiters = 0.0;
};

class HudRenderer {
public:
    void draw(const FlightHudData& data) const;
};

}  // namespace planet_aces