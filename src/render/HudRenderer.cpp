#include "render/HudRenderer.hpp"

#include "raylib.h"

namespace planet_aces {

void HudRenderer::draw(const FlightHudData& data) const {
    DrawRectangle(24, 24, 430, 178, Fade(BLACK, 0.35f));
    DrawText("PLANET ACES // FLIGHT LAB", 40, 38, 24, RAYWHITE);
    DrawText(TextFormat("SPD %03.0f m/s", data.airspeedMetersPerSecond), 40, 72, 26, Color {255, 214, 102, 255});
    DrawText(TextFormat("ALT %04.0f m", data.altitudeMeters), 40, 100, 26, Color {162, 230, 255, 255});
    DrawText(TextFormat("THR %02.0f%%", data.throttle01 * 100.0f), 220, 72, 26, Color {255, 160, 122, 255});
    DrawText(TextFormat("G %0.1f", data.loadFactorG), 220, 100, 26, Color {185, 255, 168, 255});
    DrawText(TextFormat("CAM %s", data.cameraName.data()), 40, 128, 22, Color {244, 244, 244, 255});
    DrawText("C cycle cockpit / behind / far behind", 40, 152, 20, Color {210, 220, 234, 255});
    DrawText(TextFormat("AB %s  FUEL %03.0f L", data.afterburnerActive ? "ON" : "OFF", data.afterburnerFuelRemainingLiters), 220, 128, 22, Color {255, 174, 96, 255});
    DrawText("W/S pitch  A/D roll  Q/E yaw  Shift/Ctrl throttle  Enter afterburner  Hold V rear look", 40, 858, 22, RAYWHITE);
}

}  // namespace planet_aces