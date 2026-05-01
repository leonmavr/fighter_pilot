#include "app/FlightLabApp.hpp"

#include "aircraft/Aircraft.hpp"
#include "aircraft/F15C.hpp"
#include "core/AssetPaths.hpp"
#include "core/Transforms.hpp"
#include "fx/SmokeProfileAsset.hpp"
#include "fx/SmokeTrail.hpp"
#include "physics/FlightPhysics.hpp"
#include "render/CameraController.hpp"
#include "render/HudRenderer.hpp"
#include "render/ObjModelRenderer.hpp"
#include "render/RaylibContext.hpp"
#include "world/terrain.hpp"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace {

constexpr double kFixedDtSeconds = 1.0 / 120.0;
constexpr float kMaxFrameTimeSeconds = 0.05f;
constexpr float kInitialThrottle = 0.82f;

void update_throttle(float& throttle, float frameTimeSeconds) {
    throttle += IsKeyDown(KEY_LEFT_SHIFT) ? 0.35f * frameTimeSeconds : 0.0f;
    throttle -= IsKeyDown(KEY_LEFT_CONTROL) ? 0.35f * frameTimeSeconds : 0.0f;
    throttle = std::clamp(throttle, 0.0f, 1.0f);
}

planet_aces::AircraftInput poll_aircraft_input(float throttle) {
    planet_aces::AircraftInput input;
    input.throttle01 = throttle;
    input.pitch01 = (IsKeyDown(KEY_W) ? 1.0 : 0.0) - (IsKeyDown(KEY_S) ? 1.0 : 0.0);
    input.roll01 = (IsKeyDown(KEY_D) ? 1.0 : 0.0) - (IsKeyDown(KEY_A) ? 1.0 : 0.0);
    input.yaw01 = (IsKeyDown(KEY_E) ? 1.0 : 0.0) - (IsKeyDown(KEY_Q) ? 1.0 : 0.0);
    input.afterburnerRequested = IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_KP_ENTER);
    return input;
}

void spawn_engine_smoke(planet_aces::SmokeTrail& smokeTrail, const planet_aces::Aircraft& aircraft) {
    const planet_aces::AircraftState& state = aircraft.state();
    const planet_aces::Vec3 forwardAxis = planet_aces::forward_vector(state);
    const planet_aces::Vec3 rightAxis = planet_aces::right_vector(state);
    const planet_aces::Vec3 upAxis = planet_aces::up_vector(state);

    for (const planet_aces::Vec3& localOffsetMeters : aircraft.definition().engineFxOffsetsMeters) {
        const planet_aces::Vec3 spawnMeters = planet_aces::world_from_local_offset(
            state.positionMeters,
            rightAxis,
            upAxis,
            forwardAxis,
            localOffsetMeters);
        const planet_aces::Vec3 driftMetersPerSecond = state.velocityMetersPerSecond * 0.18
            - forwardAxis * 6.0
            + upAxis * 1.2;
        smokeTrail.spawn(spawnMeters, driftMetersPerSecond);
    }
}

planet_aces::FlightHudData make_hud_data(
    const planet_aces::Aircraft& aircraft,
    float throttle,
    const planet_aces::AircraftCameraController& cameraController) {
    const planet_aces::AircraftState& state = aircraft.state();

    planet_aces::FlightHudData data;
    data.airspeedMetersPerSecond = state.airspeedMetersPerSecond;
    data.altitudeMeters = state.positionMeters.y;
    data.throttle01 = throttle;
    data.loadFactorG = state.loadFactorG;
    data.cameraName = cameraController.active_preset_name();
    data.afterburnerActive = state.afterburnerActive;
    data.afterburnerFuelRemainingLiters = state.afterburnerFuelRemainingLiters;
    return data;
}

}  // namespace

namespace planet_aces::app {

int FlightLabApp::run(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--smoke") == 0) {
        return run_headless_smoke();
    }

    return run_interactive();
}

int FlightLabApp::run_headless_smoke() {
    F15C aircraft;
    AircraftState& state = aircraft.state();
    state.positionMeters = {0.0, 1'200.0, 0.0};
    state.velocityMetersPerSecond = {0.0, 0.0, 160.0};
    double throttle = 0.9;

    for (int frame = 0; frame < 720; ++frame) {
        AircraftInput input;
        input.throttle01 = throttle;
        input.pitch01 = frame < 160 ? 0.08 : (frame < 320 ? -0.03 : 0.0);
        input.roll01 = (frame >= 260 && frame < 520) ? 0.28 : 0.0;
        aircraft.step(input, kFixedDtSeconds);
        throttle = std::clamp(throttle + 0.0002, 0.65, 1.0);
    }

    std::cout << std::fixed << std::setprecision(1)
              << "smoke alt=" << state.positionMeters.y
              << " speed=" << state.airspeedMetersPerSecond
              << " load=" << state.loadFactorG << '\n';
    return state.airspeedMetersPerSecond > 80.0 ? 0 : 1;
}

int FlightLabApp::run_interactive() {
    render::RaylibContext context;
    F15C aircraft;
    ObjModelRenderer aircraftRenderer(
        aircraft.definition().modelAssetPath,
        assets::kAircraftVertexShaderPath,
        assets::kAircraftFragmentShaderPath,
        aircraft.definition().renderConfig);
    SmokeTrail smokeTrail(load_smoke_trail_file(assets::kSmokeProfileAssetPath));
    Terrain terrain;
    AircraftCameraController cameraController;
    HudRenderer hudRenderer;

    float throttle = kInitialThrottle;
    float smokeSpawnTimerSeconds = 0.0f;
    double accumulator = 0.0;

    while (!context.should_close()) {
        const float frameTimeSeconds = std::min(context.frame_time_seconds(), kMaxFrameTimeSeconds);
        accumulator += frameTimeSeconds;

        if (IsKeyPressed(KEY_C)) {
            cameraController.cycle_preset();
        }

        update_throttle(throttle, frameTimeSeconds);
        smokeSpawnTimerSeconds += frameTimeSeconds;

        while (accumulator >= kFixedDtSeconds) {
            const AircraftInput input = poll_aircraft_input(throttle);
            aircraft.step(input, kFixedDtSeconds);

            const AircraftState& state = aircraft.state();
            aircraft.clamp_above_ground(
                terrain.sample_height_meters(state.positionMeters.x, state.positionMeters.z),
                10.0);

            while (smokeSpawnTimerSeconds >= smokeTrail.parameters().spawnIntervalSeconds) {
                spawn_engine_smoke(smokeTrail, aircraft);
                smokeSpawnTimerSeconds -= smokeTrail.parameters().spawnIntervalSeconds;
            }

            smokeTrail.update(static_cast<float>(kFixedDtSeconds));
            accumulator -= kFixedDtSeconds;
        }

        cameraController.update(
            aircraft.state(),
            IsKeyDown(KEY_V),
            aircraft.state().afterburnerActive,
            frameTimeSeconds);

        context.begin_frame(Color {135, 186, 232, 255});
        context.begin_world(cameraController.camera());
        terrain.draw(cameraController.camera());
        smokeTrail.draw();
        aircraftRenderer.draw(aircraft.state());
        context.end_world();
        hudRenderer.draw(make_hud_data(aircraft, throttle, cameraController));
        context.end_frame();
    }

    return 0;
}

}  // namespace planet_aces::app