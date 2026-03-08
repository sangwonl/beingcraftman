#pragma once

#include <cmath>
#include <gazeshot/core/math/Mat4.hpp>
#include <gazeshot/core/math/Vec3.hpp>

namespace gazeshot::core::math {

template <typename T = f32>
struct Quat {
  T w{1}, x{}, y{}, z{};

  constexpr Quat() = default;
  constexpr Quat(T w, T x, T y, T z) : w(w), x(x), y(y), z(z) {}

  static Quat fromAxisAngle(const Vec3<T>& axis, T radians) {
    auto a = normalize(axis);
    T half = radians / T(2);
    T s = std::sin(half);
    return {std::cos(half), a.x * s, a.y * s, a.z * s};
  }

  static Quat fromEuler(T pitch, T yaw, T roll) {
    T cp = std::cos(pitch / T(2)), sp = std::sin(pitch / T(2));
    T cy = std::cos(yaw / T(2)), sy = std::sin(yaw / T(2));
    T cr = std::cos(roll / T(2)), sr = std::sin(roll / T(2));
    return {
        cr * cp * cy + sr * sp * sy,  // w
        sr * cp * cy - cr * sp * sy,  // x
        cr * sp * cy + sr * cp * sy,  // y
        cr * cp * sy - sr * sp * cy   // z
    };
  }

  T lengthSquared() const { return w * w + x * x + y * y + z * z; }
  T length() const { return std::sqrt(lengthSquared()); }

  [[nodiscard]] Mat4<T> toMat4() const {
    T xx = x * x, yy = y * y, zz = z * z;
    T xy = x * y, xz = x * z, yz = y * z;
    T wx = w * x, wy = w * y, wz = w * z;

    Mat4<T> m{};
    m[0] = Vec4<T>{T(1) - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy), T(0)};
    m[1] = Vec4<T>{2 * (xy + wz), T(1) - 2 * (xx + zz), 2 * (yz - wx), T(0)};
    m[2] = Vec4<T>{2 * (xz - wy), 2 * (yz + wx), T(1) - 2 * (xx + yy), T(0)};
    m[3] = Vec4<T>{T(0), T(0), T(0), T(1)};

    return m;
  }
};

template <typename T>
constexpr Quat<T> operator*(const Quat<T>& a, const Quat<T>& b) {
  return {
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,  // w
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,  // x
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,  // y
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w   // z
  };
}

template <typename T>
Quat<T> normalize(const Quat<T>& q) {
  T len = q.length();
  return {q.w / len, q.x / len, q.y / len, q.z / len};
}

// 구면 선형 보간
template <typename T>
Quat<T> slerp(const Quat<T>& a, const Quat<T>& b, T t) {
  T cosHalf = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;

  // 최단 경로 보장
  Quat<T> b2 = b;
  if (cosHalf < T(0)) {
    b2 = {-b.w, -b.x, -b.y, -b.z};
    cosHalf = -cosHalf;
  }

  // 거의 같으면 lerp 으로 대체
  if (cosHalf > T(0.9999)) {
    return normalize(Quat<T>{a.w + t * (b2.w - a.w), a.x + t * (b2.x - a.x),
                             a.y + t * (b2.y - a.y), a.z + t * (b2.z - a.z)});
  }

  T halfAngle = std::acos(cosHalf);
  T sinHalf = std::sin(halfAngle);
  T wa = std::sin((T(1) - t) * halfAngle) / sinHalf;
  T wb = std::sin(t * halfAngle) / sinHalf;

  return {wa * a.w + wb * b2.w, wa * a.x + wb * b2.x, wa * a.y + wb * b2.y,
          wa * a.z + wb * b2.z};
}

using Quatf = Quat<f32>;
using Quatd = Quat<f64>;

}  // namespace gazeshot::core::math