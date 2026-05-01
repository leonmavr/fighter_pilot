#include "fx/SmokeTrail.hpp"

#include "render/RaylibContext.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace planet_aces {

namespace {

class SmokeEntity final : public FxEntity {
public:
    SmokeEntity(const SmokeTrailParameters& parameters, Vector3 position, Vector3 velocity)
        : FxEntity(parameters.entityParameters)
        , parameters_(parameters)
        , position_(position)
        , velocity_(velocity) {}

    void update(float dtSeconds) override {
        advance_lifetime(dtSeconds);
        position_.x += velocity_.x * dtSeconds;
        position_.y += velocity_.y * dtSeconds;
        position_.z += velocity_.z * dtSeconds;

        const float dampingFrames = dtSeconds * 120.0f;
        velocity_.x *= std::pow(parameters_.velocityDampingPerFrame.x, dampingFrames);
        velocity_.y = velocity_.y * std::pow(parameters_.velocityDampingPerFrame.y, dampingFrames)
            + parameters_.upwardAccelerationRenderUnits * dtSeconds;
        velocity_.z *= std::pow(parameters_.velocityDampingPerFrame.z, dampingFrames);
    }

    void draw() const override {
        DrawCubeV(position_, {current_size(), current_size(), current_size()}, current_color());
    }

private:
    float current_size() const noexcept {
        if (parameters_.sizeOverLife.empty()) {
            return 1.0f;
        }

        const int decayIndex = std::min(
            static_cast<int>(normalized_age() * static_cast<float>(parameters_.sizeOverLife.size())),
            static_cast<int>(parameters_.sizeOverLife.size()) - 1);
        return parameters_.sizeOverLife[decayIndex];
    }

    SmokeTrailParameters parameters_;
    Vector3 position_;
    Vector3 velocity_;
};

}  // namespace

SmokeTrail::SmokeTrail(SmokeTrailParameters parameters)
    : parameters_(std::move(parameters)) {}

void SmokeTrail::spawn(const Vec3& positionMeters, const Vec3& velocityMetersPerSecond) {
    entities_.push_back(std::make_unique<SmokeEntity>(
        parameters_,
        render::to_vector3(positionMeters),
        render::to_vector3(velocityMetersPerSecond)));
}

void SmokeTrail::update(float dtSeconds) {
    for (const std::unique_ptr<FxEntity>& entity : entities_) {
        entity->update(dtSeconds);
    }

    entities_.erase(
        std::remove_if(
            entities_.begin(),
            entities_.end(),
            [](const std::unique_ptr<FxEntity>& entity) {
                return !entity->is_alive();
            }),
        entities_.end());
}

void SmokeTrail::draw() const {
    for (const std::unique_ptr<FxEntity>& entity : entities_) {
        entity->draw();
    }
}

const SmokeTrailParameters& SmokeTrail::parameters() const noexcept {
    return parameters_;
}

}  // namespace planet_aces