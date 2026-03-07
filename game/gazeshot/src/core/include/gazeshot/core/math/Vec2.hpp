#pragma once

#include <cassert>
#include <cmath>
#include <gazeshot/core/Types.hpp>

namespace gazeshot::core::math {

template <typename T = f32>
struct Vec2 {
  T x{}, y{};

  constexpr Vec2() = default;
  constexpr Vec2(T x, T y) : x(x), y(y) {}
  constexpr explicit Vec2(T scalar) : x(scalar), y(scalar) {}

  constexpr T& operator[](usize i) {
    assert(i < 2);
    return (&x)[i];
  }
  constexpr const T& operator[](usize i) const {
    assert(i < 2);
    return (&x)[i];
  }
  constexpr Vec2& operator+=(const Vec2& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }
  constexpr Vec2& operator-=(const Vec2& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }
  constexpr Vec2& operator*=(const Vec2& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }
  constexpr Vec2& operator/=(const Vec2& rhs) {
    assert(rhs.x != 0 && rhs.y != 0);
    x /= rhs.x;
    y /= rhs.y;
    return *this;
  }
  constexpr Vec2 operator-() const { return {-x, -y}; }
  constexpr bool operator==(const Vec2&) const = default;

  constexpr const T* data() const { return &x; }
  constexpr T* data() { return &x; }
};

template <typename T>
constexpr Vec2<T> operator+(Vec2<T> a, const Vec2<T>& b) {
  return a += b;
}

template <typename T>
constexpr Vec2<T> operator-(Vec2<T> a, const Vec2<T>& b) {
  return a -= b;
}

template <typename T>
constexpr Vec2<T> operator*(T scalar, const Vec2<T>& v) {
  return v *= scalar;
}

template <typename T>
constexpr Vec2<T> operator*(Vec2<T> v, T scalar) {
  return v *= scalar;
}

template <typename T>
constexpr Vec2<T> operator/(Vec2<T> v, T scalar) {
  return v /= scalar;
}

template <typename T>
constexpr T dot(const Vec2<T>& a, const Vec2<T>& b) {
  return a.x * b.x + a.y * b.y;
}

template <typename T>
constexpr T cross(const Vec2<T>& a, const Vec2<T>& b) {
  return a.x * b.y - a.y * b.x;  // 외적의 z 성분
}

template <typename T>
constexpr T lengthSquared(const Vec2<T>& v) {
  return dot(v, v);
}

template <typename T>
T length(const Vec2<T>& v) {
  return std::sqrt(lengthSquared(v));
}

template <typename T>
Vec2<T> normalize(const Vec2<T>& v) {
  T len = length(v);
  assert(len > T(0));
  return v / len;
}

template <typename T>
constexpr Vec2<T> lerp(const Vec2<T>& a, const Vec2<T>& b, T t) {
  return a + (b - a) * t;
}

using Vec2f = Vec2<f32>;
using Vec2d = Vec2<f64>;
using Vec2i = Vec2<i32>;

}  // namespace gazeshot::core::math
