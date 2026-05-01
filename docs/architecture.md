# Architecture Notes

## Core decision

Use raylib for the first native rendering layer, but keep the actual game simulation in a pure C++ CMake library.

This avoids two common failure modes:

- building too much engine technology before the game exists
- tying core gameplay rules to one engine's scene graph and scripting API

## Responsibilities

### `planet_aces_sim`

Owns:

- aircraft state and flight dynamics
- projectiles and hit resolution
- sensors and targeting logic
- AI decision making
- terrain sampling interfaces
- collision queries against simplified world geometry
- deterministic update loop

Should avoid:

- renderer code
- engine scene nodes
- asset import logic
- UI code

### Renderer layer

Owns:

- world rendering
- sky, sun, fog, glow, and motion effects
- terrain mesh generation and streaming visuals
- building meshes and VFX
- HUD, cockpit UI, target markers, radar display
- input rebinding, menus, save data, and audio

## Simulation boundaries to keep clean

The simulation should not know anything about meshes or materials. It should consume compact data interfaces like these:

- `TerrainSampler::height_at(x, z)`
- `TerrainSampler::normal_at(x, z)`
- `WorldQuery::raycast(origin, direction, max_distance)`
- `WorldQuery::overlap_sphere(center, radius)`

That lets you start with a simple procedural terrain backend and later replace it with streamed tiles or imported scene data.

## Early subsystem order

1. Aircraft kinematics and control response
2. Lift, drag, thrust, and gravity stabilization
3. Terrain sampling and crash detection
4. Bullets and impact events
5. Enemy behavior and target selection
6. Sensor abstractions and HUD data feeds

## Flight model guidance

Do not aim for full study-sim realism first. Start with a controllable and stable model that has believable energy behavior:

- excess speed should widen turns and increase climb potential
- low speed should degrade control authority and lift
- high-G turns should trade energy for rate
- altitude should matter through air density later

The current scaffold is only the first step toward that.
