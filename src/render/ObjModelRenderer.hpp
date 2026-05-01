#pragma once

#include "aircraft/Aircraft.hpp"
#include "physics/FlightPhysics.hpp"

#include "raylib.h"

#include <string>

namespace planet_aces {

class ObjModelRenderer {
public:
    ObjModelRenderer(
        std::string modelPath,
        std::string vertexShaderPath,
        std::string fragmentShaderPath,
        AircraftRenderConfig renderConfig = {});
    ~ObjModelRenderer();

    ObjModelRenderer(const ObjModelRenderer&) = delete;
    ObjModelRenderer& operator=(const ObjModelRenderer&) = delete;

    void draw(const AircraftState& state) const;

private:
    void draw_fallback(const AircraftState& state) const;

    Model model_ {};
    Shader shader_ {};
    Vector3 scale_ {1.0f, 1.0f, 1.0f};
    Vector3 pivotOffset_ {0.0f, 0.0f, 0.0f};
    AircraftRenderConfig renderConfig_;
    bool loaded_ = false;
    bool shaderLoaded_ = false;
};

}  // namespace planet_aces