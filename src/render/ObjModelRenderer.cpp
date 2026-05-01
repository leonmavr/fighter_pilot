#include "render/ObjModelRenderer.hpp"

#include "render/RaylibContext.hpp"

#include "rlgl.h"

#include <algorithm>
#include <fstream>

namespace planet_aces {

namespace {

void draw_double_sided_triangle(Vector3 a, Vector3 b, Vector3 c, Color color) {
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(a, c, b, color);
}

void draw_low_poly_jet(const Vector3& position, float yawDegrees, float pitchDegrees, float rollDegrees) {
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(yawDegrees, 0.0f, 1.0f, 0.0f);
    rlRotatef(pitchDegrees, 1.0f, 0.0f, 0.0f);
    rlRotatef(-rollDegrees, 0.0f, 0.0f, 1.0f);

    DrawCube({0.0f, 0.0f, 0.0f}, 1.0f, 0.7f, 5.8f, Color {192, 198, 206, 255});
    DrawCube({0.0f, 0.15f, 2.5f}, 0.45f, 0.35f, 1.4f, Color {238, 244, 250, 255});
    DrawCube({0.0f, 0.25f, -2.1f}, 0.28f, 1.0f, 1.0f, Color {170, 178, 188, 255});

    draw_double_sided_triangle({0.0f, 0.0f, 0.4f}, {-4.6f, 0.0f, -0.8f}, {-0.6f, 0.0f, -1.4f}, Color {145, 155, 170, 255});
    draw_double_sided_triangle({0.0f, 0.0f, 0.4f}, {4.6f, 0.0f, -0.8f}, {0.6f, 0.0f, -1.4f}, Color {145, 155, 170, 255});
    draw_double_sided_triangle({0.0f, 1.1f, -1.9f}, {0.0f, 0.2f, -2.5f}, {0.0f, 0.2f, -1.0f}, Color {126, 136, 148, 255});

    DrawCylinderEx({-0.45f, -0.1f, -2.7f}, {-0.45f, -0.1f, -3.6f}, 0.22f, 0.16f, 10, Color {84, 94, 108, 255});
    DrawCylinderEx({0.45f, -0.1f, -2.7f}, {0.45f, -0.1f, -3.6f}, 0.22f, 0.16f, 10, Color {84, 94, 108, 255});

    rlPopMatrix();
}

}  // namespace

ObjModelRenderer::ObjModelRenderer(
    std::string modelPath,
    std::string vertexShaderPath,
    std::string fragmentShaderPath,
    AircraftRenderConfig renderConfig)
    : renderConfig_(renderConfig) {
    std::ifstream modelFile(modelPath);
    if (!modelFile) {
        return;
    }

    model_ = LoadModel(modelPath.c_str());
    const BoundingBox bounds = GetModelBoundingBox(model_);
    const Vector3 extents {
        bounds.max.x - bounds.min.x,
        bounds.max.y - bounds.min.y,
        bounds.max.z - bounds.min.z,
    };

    const float maxExtent = std::max(extents.x, std::max(extents.y, extents.z));
    if (maxExtent <= 0.001f) {
        UnloadModel(model_);
        model_ = {};
        return;
    }

    const float uniformScale = renderConfig_.targetLengthRenderUnits / maxExtent;
    scale_ = {uniformScale, uniformScale, uniformScale};
    pivotOffset_ = {
        -0.5f * (bounds.min.x + bounds.max.x),
        -0.5f * (bounds.min.y + bounds.max.y),
        -0.5f * (bounds.min.z + bounds.max.z),
    };

    shader_ = LoadShader(vertexShaderPath.c_str(), fragmentShaderPath.c_str());
    shaderLoaded_ = shader_.id > 0;
    if (shaderLoaded_) {
        const Vector3 lightDirection {0.3f, -1.0f, 0.2f};
        const Vector3 lightColor {0.92f, 0.94f, 0.98f};
        const Vector3 ambientColor {0.28f, 0.31f, 0.36f};
        const int lightDirLoc = GetShaderLocation(shader_, "lightDir");
        const int lightColorLoc = GetShaderLocation(shader_, "lightColor");
        const int ambientLoc = GetShaderLocation(shader_, "ambientColor");

        if (lightDirLoc >= 0) {
            SetShaderValue(shader_, lightDirLoc, &lightDirection, SHADER_UNIFORM_VEC3);
        }
        if (lightColorLoc >= 0) {
            SetShaderValue(shader_, lightColorLoc, &lightColor, SHADER_UNIFORM_VEC3);
        }
        if (ambientLoc >= 0) {
            SetShaderValue(shader_, ambientLoc, &ambientColor, SHADER_UNIFORM_VEC3);
        }

        for (int materialIndex = 0; materialIndex < model_.materialCount; ++materialIndex) {
            model_.materials[materialIndex].shader = shader_;
            model_.materials[materialIndex].maps[MATERIAL_MAP_DIFFUSE].color = Color {214, 218, 228, 255};
        }
    }

    loaded_ = true;
}

ObjModelRenderer::~ObjModelRenderer() {
    if (loaded_) {
        if (shaderLoaded_) {
            UnloadShader(shader_);
        }
        UnloadModel(model_);
    }
}

void ObjModelRenderer::draw(const AircraftState& state) const {
    if (!loaded_) {
        draw_fallback(state);
        return;
    }

    const Vector3 position = render::to_vector3(state.positionMeters);
    const float yawDegrees = static_cast<float>(rad_to_deg(state.yawRadians));
    const float pitchDegrees = static_cast<float>(rad_to_deg(state.pitchRadians));
    const float rollDegrees = static_cast<float>(rad_to_deg(state.rollRadians));

    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(yawDegrees, 0.0f, 1.0f, 0.0f);
    rlRotatef(-pitchDegrees, 1.0f, 0.0f, 0.0f);
    rlRotatef(-rollDegrees, 0.0f, 0.0f, 1.0f);
    rlRotatef(renderConfig_.alignYawDegrees, 0.0f, 1.0f, 0.0f);
    rlRotatef(renderConfig_.alignPitchDegrees, 1.0f, 0.0f, 0.0f);
    rlScalef(scale_.x, scale_.y, scale_.z);
    rlTranslatef(pivotOffset_.x, pivotOffset_.y, pivotOffset_.z);
    rlDisableBackfaceCulling();
    DrawModel(model_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    rlEnableBackfaceCulling();
    rlPopMatrix();
}

void ObjModelRenderer::draw_fallback(const AircraftState& state) const {
    draw_low_poly_jet(
        render::to_vector3(state.positionMeters),
        static_cast<float>(rad_to_deg(state.yawRadians)),
        static_cast<float>(rad_to_deg(state.pitchRadians)),
        static_cast<float>(rad_to_deg(state.rollRadians)));
}

}  // namespace planet_aces