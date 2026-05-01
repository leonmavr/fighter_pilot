#include "fx/FxEntity.hpp"

#include <algorithm>

namespace planet_aces {

namespace {

unsigned char lerp_channel(unsigned char from, unsigned char to, float t) {
    const float fromValue = static_cast<float>(from);
    return static_cast<unsigned char>(std::clamp(fromValue + (static_cast<float>(to) - fromValue) * t, 0.0f, 255.0f));
}

}  // namespace

FxEntity::FxEntity(FxEntityParameters parameters)
    : parameters_(parameters) {}

bool FxEntity::is_alive() const noexcept {
    return parameters_.lifetimeSeconds <= 0.0f || normalized_age() < 1.0f;
}

void FxEntity::advance_lifetime(float dtSeconds) noexcept {
    ageSeconds_ += std::max(dtSeconds, 0.0f);
}

float FxEntity::normalized_age() const noexcept {
    if (parameters_.lifetimeSeconds <= 0.0f) {
        return 0.0f;
    }

    return std::clamp(ageSeconds_ / parameters_.lifetimeSeconds, 0.0f, 1.0f);
}

Color FxEntity::current_color() const noexcept {
    const float t = normalized_age();
    return {
        lerp_channel(parameters_.startColor.r, parameters_.endColor.r, t),
        lerp_channel(parameters_.startColor.g, parameters_.endColor.g, t),
        lerp_channel(parameters_.startColor.b, parameters_.endColor.b, t),
        lerp_channel(parameters_.startColor.a, parameters_.endColor.a, t),
    };
}

const FxEntityParameters& FxEntity::parameters() const noexcept {
    return parameters_;
}

}  // namespace planet_aces