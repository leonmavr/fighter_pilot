#pragma once

#include "physics/FlightPhysics.hpp"

#include "raylib.h"

namespace planet_aces {

class AircraftCameraController {
public:
    AircraftCameraController();

    void cycle_preset();
    void update(const AircraftState& state, bool rearLookActive, bool afterburnerActive, float dtSeconds);

    const Camera3D& camera() const noexcept;
    const char* active_preset_name() const noexcept;

private:
    Camera3D camera_ {};
    int presetIndex_ = 0;
    float afterburnerPullbackMeters_ = 0.0f;
    bool rearLookActive_ = false;
};

}  // namespace planet_aces