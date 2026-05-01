#pragma once

#include "raylib.h"

namespace planet_aces {

struct FxEntityParameters {
    Color startColor {210, 214, 222, 180};
    Color endColor {210, 214, 222, 45};
    float lifetimeSeconds = 0.5f;
};

class FxEntity {
public:
    explicit FxEntity(FxEntityParameters parameters = {});
    virtual ~FxEntity() = default;

    virtual void update(float dtSeconds) = 0;
    virtual void draw() const = 0;

    bool is_alive() const noexcept;

protected:
    void advance_lifetime(float dtSeconds) noexcept;
    float normalized_age() const noexcept;
    Color current_color() const noexcept;
    const FxEntityParameters& parameters() const noexcept;

private:
    FxEntityParameters parameters_;
    float ageSeconds_ = 0.0f;
};

}  // namespace planet_aces