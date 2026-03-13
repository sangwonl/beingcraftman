#pragma once

#include <cmath>
#include <gazeshot/core/math/Mat4.hpp>
#include <gazeshot/core/math/Vec4.hpp>

namespace gazeshot::core::math {

template <typename T>
[[nodiscard]] constexpr Mat4<T> translate(const Vec3<T>& offset) {
  Mat4<T> m = Mat4<T>::identity();
  m[0][3] = offset.x;
  m[1][3] = offset.y;
  m[2][3] = offset.z;
  return m;
}

template <typename T>
[[nodiscard]] constexpr Mat4<T> scale(const Vec3<T>& s) {
  Mat4<T> m{};
  m[0].x = s.x;
  m[1].y = s.y;
  m[2].z = s.z;
  m[3].w = T(1);
  return m;
}

template <typename T>
[[nodiscard]] Mat4<T> rotate(T radians, Vec3<T> axis) {
  axis = normalize(axis);
  T c = std::cos(radians);
  T s = std::sin(radians);
  T t = T(1) - c;

  T x = axis.x, y = axis.y, z = axis.z;

  return Mat4<T>{
      Vec4<T>{t * x * x + c, t * x * y - s * z, t * x * z + s * y, T(0)},
      Vec4<T>{t * x * y + s * z, t * y * y + c, t * y * z - s * x, T(0)},
      Vec4<T>{t * x * z - s * y, t * y * z + s * x, t * z * z + c, T(0)},
      Vec4<T>{T(0), T(0), T(0), T(1)}};
}

template <typename T>
[[nodiscard]] Mat4<T> rotateX(T radians) {
  T c = std::cos(radians);
  T s = std::sin(radians);

  Mat4<T> m = Mat4<T>::identity();
  m[1][1] = c;
  m[1][2] = -s;
  m[2][1] = s;
  m[2][2] = c;
  return m;
}

template <typename T>
[[nodiscard]] Mat4<T> rotateY(T radians) {
  T c = std::cos(radians);
  T s = std::sin(radians);

  Mat4<T> m = Mat4<T>::identity();
  m[0][0] = c;
  m[0][2] = s;
  m[2][0] = -s;
  m[2][2] = c;
  return m;
}

template <typename T>
[[nodiscard]] Mat4<T> rotateZ(T radians) {
  T c = std::cos(radians);
  T s = std::sin(radians);

  Mat4<T> m = Mat4<T>::identity();
  m[0][0] = c;
  m[0][1] = -s;
  m[1][0] = s;
  m[1][1] = c;
  return m;
}

template <typename T>
[[nodiscard]] Mat4<T> lookAt(const Vec3<T>& eye, const Vec3<T>& target,
                             const Vec3<T>& worldUp) {
  Vec3<T> forward = normalize(eye - target);
  Vec3<T> right = normalize(cross(worldUp, forward));
  Vec3<T> up = cross(forward, right);

  Mat4<T> m{};
  m[0] = Vec4<T>{right.x, right.y, right.z, -dot(right, eye)};
  m[1] = Vec4<T>{up.x, up.y, up.z, -dot(up, eye)};
  m[2] = Vec4<T>{forward.x, forward.y, forward.z, -dot(forward, eye)};
  m[3] = Vec4<T>{T(0), T(0), T(0), T(1)};
  return m;
}

template <typename T>
[[nodiscard]] constexpr Mat4<T> perspective(T fovRadians, T aspect, T near,
                                            T far) {
  T f = T(1) / std::tan(fovRadians / T(2));

  Mat4<T> m{};
  m[0][0] = f / aspect;
  m[1][1] = f;
  m[2][2] = (far + near) / (near - far);
  m[2][3] = (T(2) * far * near) / (near - far);
  m[3][2] = T(-1);
  return m;
}

template <typename T>
[[nodiscard]] constexpr Mat4<T> ortho(T left, T right, T bottom, T top, T near,
                                      T far) {
  Mat4<T> m{};
  m[0][0] = T(2) / (right - left);
  m[1][1] = T(2) / (top - bottom);
  m[2][2] = T(-2) / (far - near);
  m[0][3] = -(right + left) / (right - left);
  m[1][3] = -(top + bottom) / (top - bottom);
  m[2][3] = -(far + near) / (far - near);
  m[3][3] = T(1);
  return m;
}

inline constexpr f32 PI = 3.14159265358979323846f;
inline constexpr f64 PI_D = 3.14159265358979323846;

template <typename T>
constexpr T radians(T degrees) {
  return degrees * T(PI_D) / T(180);
}

template <typename T>
constexpr T degrees(T radians) {
  return radians * T(180) / T(PI_D);
}

}  // namespace gazeshot::core::math