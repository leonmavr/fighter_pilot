#pragma once

#include "physics/FlightPhysics.hpp"

#include <string>
#include <vector>

namespace planet_aces {

struct AircraftRenderConfig {
    float alignPitchDegrees = -90.0f;
    float alignYawDegrees = -90.0f;
    float targetLengthRenderUnits = 5.8f;
};

struct AircraftDefinition {
    std::string name;
    std::string flightModelAssetPath;
    std::string modelAssetPath;
    AircraftParameters defaultParameters;
    AircraftState initialState;
    AircraftRenderConfig renderConfig;
    std::vector<Vec3> engineFxOffsetsMeters;
};

class Aircraft {
public:
    explicit Aircraft(AircraftDefinition definition);
    virtual ~Aircraft() = default;

    void step(const AircraftInput& input, double dtSeconds);
    void clamp_above_ground(double groundHeightMeters, double clearanceMeters);

    const AircraftDefinition& definition() const noexcept;
    const AircraftParameters& parameters() const noexcept;
    const FlightPhysics& physics() const noexcept;
    const AircraftState& state() const noexcept;
    AircraftState& state() noexcept;

protected:
    static AircraftParameters load_parameters_or_default(const AircraftDefinition& definition);

private:
    AircraftDefinition definition_;
    AircraftParameters parameters_;
    FlightPhysics physics_;
    AircraftState state_;
};

}  // namespace planet_aces