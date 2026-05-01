#pragma once

#include "core/Math.hpp"

namespace planet_aces {

inline Vec3 world_from_local_offset(
    const Vec3& originMeters,
    const Vec3& rightAxis,
    const Vec3& upAxis,
    const Vec3& forwardAxis,
    const Vec3& localOffsetMeters) {
    return originMeters
        + rightAxis * localOffsetMeters.x
        + upAxis * localOffsetMeters.y
        + forwardAxis * localOffsetMeters.z;
}

}  // namespace planet_aces