#pragma once

#include <cassert>
#include <cmath>
#include <gazeshot/core/Types.hpp>

namespace gazeshot::core::math {

template <typename T = f32>
struct Vec3 {
  T x{}, y{}, z{};

  constexpr Vec3() = default;
  constexpr Vec3(T x, T y, T z) : x(x), y(y), z(z) {}
  constexpr explicit Vec3(T scalar) : x(scalar), y(scalar), z(scalar) {}

  constexpr T& operator[](usize i) {
    assert(i < 3);
    return (&x)[i];
  }
  constexpr const T& operator[](usize i) const {
    assert(i < 3);
    return (&x)[i];
  }
  constexpr Vec3& operator+=(const Vec3& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
  }
  constexpr Vec3& operator-=(const Vec3& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
  }
  constexpr Vec3& operator*=(T scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
  }
  constexpr Vec3& operator/=(T scalar) {
    assert(scalar != 0);
    x /= scalar;
    y /= scalar;
    z /= scalar;
    return *this;
  }
  constexpr Vec3 operator-() const { return {-x, -y, -z}; }
  constexpr bool operator==(const Vec3&) const = default;

  constexpr const T* data() const { return &x; }
  constexpr T* data() { return &x; }
};

template <typename T>
constexpr Vec3<T> operator+(Vec3<T> a, const Vec3<T>& b) {
  return a += b;
}

template <typename T>
constexpr Vec3<T> operator-(Vec3<T> a, const Vec3<T>& b) {
  return a -= b;
}

template <typename T>
constexpr Vec3<T> operator*(T scalar, Vec3<T> v) {
  return v *= scalar;
}

template <typename T>
constexpr Vec3<T> operator*(Vec3<T> v, T scalar) {
  return v *= scalar;
}

template <typename T>
constexpr Vec3<T> operator/(Vec3<T> v, T scalar) {
  return v /= scalar;
}

template <typename T>
constexpr T dot(const Vec3<T>& a, const Vec3<T>& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
constexpr Vec3<T> cross(const Vec3<T>& a, const Vec3<T>& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

template <typename T>
constexpr T lengthSquared(const Vec3<T>& v) {
  return dot(v, v);
}

template <typename T>
T length(const Vec3<T>& v) {
  return std::sqrt(lengthSquared(v));
}

template <typename T>
Vec3<T> normalize(const Vec3<T>& v) {
  T len = length(v);
  assert(len > T(0));
  return v / len;
}

template <typename T>
constexpr Vec3<T> lerp(const Vec3<T>& a, const Vec3<T>& b, T t) {
  return a + (b - a) * t;
}  // namespace gazeshot::core::math

using Vec3f = Vec3<f32>;
using Vec3d = Vec3<f64>;
using Vec3i = Vec3<i32>;

}  // namespace gazeshot::core::math