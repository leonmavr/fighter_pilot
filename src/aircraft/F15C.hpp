#pragma once

#include "aircraft/Aircraft.hpp"

namespace planet_aces {

class F15C final : public Aircraft {
public:
    F15C();

    static AircraftDefinition make_definition();
};

}  // namespace planet_aces