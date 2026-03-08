#pragma once

#include <cassert>
#include <cmath>
#include <gazeshot/core/Types.hpp>

namespace gazeshot::core::math {

template <typename T = f32>
struct Vec4 {
  T x{}, y{}, z{}, w{};

  constexpr Vec4() = default;
  constexpr Vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
  constexpr explicit Vec4(T scalar)
      : x(scalar), y(scalar), z(scalar), w(scalar) {}

  constexpr T& operator[](usize i) {
    assert(i < 4);
    return (&x)[i];
  }
  constexpr const T& operator[](usize i) const {
    assert(i < 4);
    return (&x)[i];
  }
  constexpr Vec4& operator+=(const Vec4& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    w += rhs.w;
    return *this;
  }
  constexpr Vec4& operator-=(const Vec4& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    w -= rhs.w;
    return *this;
  }
  constexpr Vec4& operator*=(T scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    w *= scalar;
    return *this;
  }
  constexpr Vec4& operator/=(T scalar) {
    assert(scalar != 0);
    x /= scalar;
    y /= scalar;
    z /= scalar;
    w /= scalar;
    return *this;
  }
  constexpr Vec4 operator-() const { return {-x, -y, -z, -w}; }
  constexpr bool operator==(const Vec4&) const = default;

  constexpr const T* data() const { return &x; }
  constexpr T* data() { return &x; }
};

template <typename T>
constexpr Vec4<T> operator+(Vec4<T> a, const Vec4<T>& b) {
  return a += b;
}

template <typename T>
constexpr Vec4<T> operator-(Vec4<T> a, const Vec4<T>& b) {
  return a -= b;
}

template <typename T>
constexpr Vec4<T> operator*(T scalar, const Vec4<T>& v) {
  return v *= scalar;
}

template <typename T>
constexpr Vec4<T> operator*(Vec4<T> v, T scalar) {
  return v *= scalar;
}

template <typename T>
constexpr Vec4<T> operator/(Vec4<T> v, T scalar) {
  return v /= scalar;
}

template <typename T>
constexpr T dot(const Vec4<T>& a, const Vec4<T>& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

template <typename T>
constexpr Vec4<T> cross(const Vec4<T>& a, const Vec4<T>& b) {
  // 4D 벡터의 외적은 일반적으로 정의되지 않지만
  // 여기서는 3D 부분에 대해서만 계산
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x,
          T(0)};
}

template <typename T>
constexpr T lengthSquared(const Vec4<T>& v) {
  return dot(v, v);
}

template <typename T>
T length(const Vec4<T>& v) {
  return std::sqrt(lengthSquared(v));
}

template <typename T>
constexpr Vec4<T> normalize(const Vec4<T>& v) {
  T len = length(v);
  assert(len != T(0));
  return v / len;
}

template <typename T>
constexpr Vec4<T> lerp(const Vec4<T>& a, const Vec4<T>& b, T t) {
  return a + (b - a) * t;
}

using Vec4f = Vec4<f32>;
using Vec4d = Vec4<f64>;
using Vec4i = Vec4<i32>;

}  // namespace gazeshot::core::math