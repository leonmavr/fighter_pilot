#pragma once

#include <algorithm>
#include <cmath>

namespace planet_aces {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

constexpr Vec3 operator+(const Vec3& left, const Vec3& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

constexpr Vec3 operator-(const Vec3& left, const Vec3& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

constexpr Vec3 operator*(const Vec3& value, double scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

constexpr Vec3 operator*(double scalar, const Vec3& value) noexcept {
    return value * scalar;
}

constexpr Vec3 operator/(const Vec3& value, double scalar) noexcept {
    return {value.x / scalar, value.y / scalar, value.z / scalar};
}

constexpr Vec3& operator+=(Vec3& left, const Vec3& right) noexcept {
    left.x += right.x;
    left.y += right.y;
    left.z += right.z;
    return left;
}

constexpr Vec3& operator-=(Vec3& left, const Vec3& right) noexcept {
    left.x -= right.x;
    left.y -= right.y;
    left.z -= right.z;
    return left;
}

inline double dot(const Vec3& left, const Vec3& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline Vec3 cross(const Vec3& left, const Vec3& right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

inline double length_squared(const Vec3& value) noexcept {
    return dot(value, value);
}

inline double length(const Vec3& value) noexcept {
    return std::sqrt(length_squared(value));
}

inline Vec3 normalized(const Vec3& value) noexcept {
    const double magnitude = length(value);
    if (magnitude <= 1e-9) {
        return {};
    }
    return value / magnitude;
}

template <typename T>
constexpr T clamp(T value, T minValue, T maxValue) noexcept {
    return std::clamp(value, minValue, maxValue);
}

constexpr double pi() noexcept {
    return 3.14159265358979323846;
}

constexpr double deg_to_rad(double degrees) noexcept {
    return degrees * pi() / 180.0;
}

constexpr double rad_to_deg(double radians) noexcept {
    return radians * 180.0 / pi();
}

}  // namespace planet_aces