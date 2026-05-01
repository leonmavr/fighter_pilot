#pragma once

#include "core/Math.hpp"

#include "raylib.h"

namespace planet_aces::render {

inline constexpr float kMetersToRenderScale = 0.1f;

struct WindowConfig {
    int width = 1600;
    int height = 900;
    const char* title = "Planet Aces Flight Lab";
    int targetFps = 120;
    unsigned int flags = FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
};

class RaylibContext {
public:
    explicit RaylibContext(WindowConfig config = {});
    ~RaylibContext();

    RaylibContext(const RaylibContext&) = delete;
    RaylibContext& operator=(const RaylibContext&) = delete;

    bool should_close() const noexcept;
    float frame_time_seconds() const noexcept;
    void begin_frame(Color clearColor) const;
    void end_frame() const;
    void begin_world(const Camera3D& camera) const;
    void end_world() const;
};

Vector3 to_vector3(const Vec3& value);

}  // namespace planet_aces::render