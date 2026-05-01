#include "render/RaylibContext.hpp"

#include "raymath.h"
#include "rlgl.h"

namespace {

constexpr double kTerrainCameraNear = 0.5;
constexpr double kTerrainCameraFar = 4000.0;

}

namespace planet_aces::render {

RaylibContext::RaylibContext(WindowConfig config) {
    SetConfigFlags(config.flags);
    InitWindow(config.width, config.height, config.title);
    SetTargetFPS(config.targetFps);
}

RaylibContext::~RaylibContext() {
    CloseWindow();
}

bool RaylibContext::should_close() const noexcept {
    return WindowShouldClose();
}

float RaylibContext::frame_time_seconds() const noexcept {
    return GetFrameTime();
}

void RaylibContext::begin_frame(Color clearColor) const {
    BeginDrawing();
    ClearBackground(clearColor);
}

void RaylibContext::end_frame() const {
    EndDrawing();
}

void RaylibContext::begin_world(const Camera3D& camera) const {
    rlDrawRenderBatchActive();

    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();

    const float aspect = static_cast<float>(GetScreenWidth()) / static_cast<float>(GetScreenHeight());
    if (camera.projection == CAMERA_PERSPECTIVE) {
        const double top = kTerrainCameraNear * std::tan(static_cast<double>(camera.fovy) * 0.5 * DEG2RAD);
        const double right = top * static_cast<double>(aspect);
        rlFrustum(-right, right, -top, top, kTerrainCameraNear, kTerrainCameraFar);
    } else {
        const double top = static_cast<double>(camera.fovy) / 2.0;
        const double right = top * static_cast<double>(aspect);
        rlOrtho(-right, right, -top, top, kTerrainCameraNear, kTerrainCameraFar);
    }

    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();

    const Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    rlMultMatrixf(MatrixToFloat(view));
    rlEnableDepthTest();
}

void RaylibContext::end_world() const {
    rlDrawRenderBatchActive();

    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();

    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlDisableDepthTest();
}

Vector3 to_vector3(const Vec3& value) {
    return {
        static_cast<float>(value.x * kMetersToRenderScale),
        static_cast<float>(value.y * kMetersToRenderScale),
        static_cast<float>(value.z * kMetersToRenderScale),
    };
}

}  // namespace planet_aces::render