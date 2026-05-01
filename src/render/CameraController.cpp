#include "render/CameraController.hpp"

#include "core/Transforms.hpp"
#include "render/RaylibContext.hpp"

#include "raymath.h"

#include <algorithm>
#include <iterator>

namespace planet_aces {

namespace {

struct CameraRigPreset {
    const char* name;
    Vec3 positionOffsetMeters;
    Vec3 targetOffsetMeters;
    float fovDegrees;
    bool alignUpToAircraft;
};

constexpr CameraRigPreset kCameraPresets[] = {
    {
        "cockpit",
        {0.0, 3.2, 4.8},
        {0.0, 2.8, 140.0},
        72.0f,
        true,
    },
    {
        "behind",
        {0.0, 20.0, -60.0},
        {0.0, 5.0, 120.0},
        58.0f,
        true,
    },
    {
        "far behind",
        {0.0, 34.0, -110.0},
        {0.0, 6.0, 140.0},
        54.0f,
        true,
    },
};

constexpr CameraRigPreset kRearLookPreset {
    "rear look",
    {0.0, 10.0, 0.0},
    {0.0, 8.0, -140.0},
    68.0f,
    true,
};

constexpr float kAfterburnerCameraPullbackMeters = 2.6f;
constexpr float kAfterburnerCameraTransitionSeconds = 0.5f;

void apply_camera_preset(Camera3D& camera, const AircraftState& state, const CameraRigPreset& preset) {
    const Vec3 forwardAxis = forward_vector(state);
    const Vec3 rightAxis = right_vector(state);
    const Vec3 upAxis = up_vector(state);

    camera.position = render::to_vector3(world_from_local_offset(
        state.positionMeters,
        rightAxis,
        upAxis,
        forwardAxis,
        preset.positionOffsetMeters));
    camera.target = render::to_vector3(world_from_local_offset(
        state.positionMeters,
        rightAxis,
        upAxis,
        forwardAxis,
        preset.targetOffsetMeters));
    camera.up = preset.alignUpToAircraft ? render::to_vector3(upAxis) : Vector3 {0.0f, 1.0f, 0.0f};
    camera.fovy = preset.fovDegrees;
}

}  // namespace

AircraftCameraController::AircraftCameraController() {
    camera_.position = {0.0f, 6.0f, -14.0f};
    camera_.target = {0.0f, 0.0f, 0.0f};
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = kCameraPresets[presetIndex_].fovDegrees;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void AircraftCameraController::cycle_preset() {
    presetIndex_ = (presetIndex_ + 1) % static_cast<int>(std::size(kCameraPresets));
}

void AircraftCameraController::update(const AircraftState& state, bool rearLookActive, bool afterburnerActive, float dtSeconds) {
    rearLookActive_ = rearLookActive;
    const CameraRigPreset& activePreset = rearLookActive_ ? kRearLookPreset : kCameraPresets[presetIndex_];
    const float afterburnerPullbackTarget = afterburnerActive ? kAfterburnerCameraPullbackMeters : 0.0f;
    const float afterburnerPullbackStep = kAfterburnerCameraPullbackMeters * dtSeconds / kAfterburnerCameraTransitionSeconds;

    if (afterburnerPullbackMeters_ < afterburnerPullbackTarget) {
        afterburnerPullbackMeters_ = std::min(afterburnerPullbackMeters_ + afterburnerPullbackStep, afterburnerPullbackTarget);
    } else {
        afterburnerPullbackMeters_ = std::max(afterburnerPullbackMeters_ - afterburnerPullbackStep, afterburnerPullbackTarget);
    }

    apply_camera_preset(camera_, state, activePreset);
    if (afterburnerPullbackMeters_ > 0.001f) {
        const Vector3 cameraOffset = Vector3Subtract(camera_.position, camera_.target);
        const float offsetLength = Vector3Length(cameraOffset);
        if (offsetLength > 0.001f) {
            const Vector3 offsetDirection = Vector3Scale(cameraOffset, 1.0f / offsetLength);
            camera_.position = Vector3Add(camera_.position, Vector3Scale(offsetDirection, afterburnerPullbackMeters_));
        }
    }
}

const Camera3D& AircraftCameraController::camera() const noexcept {
    return camera_;
}

const char* AircraftCameraController::active_preset_name() const noexcept {
    return rearLookActive_ ? kRearLookPreset.name : kCameraPresets[presetIndex_].name;
}

}  // namespace planet_aces