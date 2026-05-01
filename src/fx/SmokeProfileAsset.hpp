#pragma once

#include "fx/SmokeTrail.hpp"

#include <string>

namespace planet_aces {

SmokeTrailParameters load_smoke_trail_file(const std::string& filePath);

}  // namespace planet_aces