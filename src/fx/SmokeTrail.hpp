#pragma once

#include "core/Math.hpp"
#include "fx/FxEntity.hpp"

#include <memory>
#include <vector>

namespace planet_aces {

struct SmokeTrailParameters {
    float spawnIntervalSeconds = 0.12f;
    float stepIntervalSeconds = 0.10f;
    std::vector<float> sizeOverLife = {3.2f, 2.5f, 1.9f, 1.3f, 0.8f};
    FxEntityParameters entityParameters;
    Vector3 velocityDampingPerFrame {0.992f, 0.988f, 0.992f};
    float upwardAccelerationRenderUnits = 0.02f;
};

class SmokeTrail {
public:
    explicit SmokeTrail(SmokeTrailParameters parameters = {});

    void spawn(const Vec3& positionMeters, const Vec3& velocityMetersPerSecond);
    void update(float dtSeconds);
    void draw() const;

    const SmokeTrailParameters& parameters() const noexcept;

private:
    SmokeTrailParameters parameters_;
    std::vector<std::unique_ptr<FxEntity>> entities_;
};

}  // namespace planet_aces