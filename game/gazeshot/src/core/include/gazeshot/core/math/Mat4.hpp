#pragma once

#include <cassert>
#include <cmath>
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>

namespace gazeshot::core::math {

template <typename T = f32>
struct Mat4 {
  Vec4<T> cols[4] = {};

  constexpr Mat4() = default;

  constexpr explicit Mat4(T diagonal) {
    cols[0].x = diagonal;
    cols[1].y = diagonal;
    cols[2].z = diagonal;
    cols[3].w = diagonal;
  }

  constexpr Mat4(const Vec4<T>& c0, const Vec4<T>& c1, const Vec4<T>& c2,
                 const Vec4<T>& c3)
      : cols{c0, c1, c2, c3} {}
  constexpr Vec4<T>& operator[](usize i) {
    assert(i < 4);
    return cols[i];
  };
  constexpr const Vec4<T>& operator[](usize i) const {
    assert(i < 4);
    return cols[i];
  };

  static constexpr Mat4 identity() { return Mat4(T(1)); }

  constexpr const T* data() const { return cols[0].data(); }

  constexpr bool operator==(const Mat4&) const = default;
};

template <typename T>
constexpr Mat4<T> operator*(const Mat4<T>& a, const Mat4<T>& b) {
  Mat4<T> result;
  for (usize col = 0; col < 4; ++col) {
    for (usize row = 0; row < 4; ++row) {
      T sum = T(0);
      for (usize k = 0; k < 4; ++k) {
        sum += a[k][row] * b[col][k];
      }
      result[col][row] = sum;
    }
  }
  return result;
}

template <typename T>
constexpr Vec4<T> operator*(const Mat4<T>& m, const Vec4<T>& v) {
  return {m[0].x * v.x + m[1].x * v.y + m[2].x * v.z + m[3].x * v.w,
          m[0].y * v.x + m[1].y * v.y + m[2].y * v.z + m[3].y * v.w,
          m[0].z * v.x + m[1].z * v.y + m[2].z * v.z + m[3].z * v.w,
          m[0].w * v.x + m[1].w * v.y + m[2].w * v.z + m[3].w * v.w};
}

template <typename T>
constexpr Mat4<T> transpose(const Mat4<T>& m) {
  return {{m[0].x, m[1].x, m[2].x, m[3].x},
          {m[0].y, m[1].y, m[2].y, m[3].y},
          {m[0].z, m[1].z, m[2].z, m[3].z},
          {m[0].w, m[1].w, m[2].w, m[3].w}};
}

template <typename T>
Mat4<T> inverse(const Mat4<T>& m) {
  const T* v = m.data();
  // 16개 원소를 v[0]..v[15]로 접근 (colomn major order)

  T t0 = v[10] * v[15] - v[14] * v[11];
  T t1 = v[6] * v[15] - v[14] * v[7];
  T t2 = v[6] * v[11] - v[10] * v[7];
  T t3 = v[2] * v[15] - v[14] * v[3];
  T t4 = v[2] * v[11] - v[10] * v[3];
  T t5 = v[2] * v[7] - v[6] * v[3];

  T c0 = (v[5] * t0 - v[9] * t1 + v[13] * t2);
  T c1 = -(v[1] * t0 - v[9] * t3 + v[13] * t4);
  T c2 = (v[1] * t1 - v[5] * t3 + v[13] * t5);
  T c3 = -(v[1] * t2 - v[5] * t4 + v[9] * t5);

  T det = v[0] * c0 + v[4] * c1 + v[8] * c2 + v[12] * c3;
  assert(std::abs(det) > T(1e-8));

  T invDet = T(1) / det;

  T t6 = v[8] * v[15] - v[12] * v[11];
  T t7 = v[4] * v[15] - v[12] * v[7];
  T t8 = v[4] * v[11] - v[8] * v[7];
  T t9 = v[8] * v[13] - v[12] * v[9];
  T t10 = v[4] * v[13] - v[12] * v[5];
  T t11 = v[4] * v[9] - v[8] * v[5];

  T t12 = v[0] * v[15] - v[12] * v[3];
  T t13 = v[0] * v[11] - v[8] * v[3];
  T t14 = v[0] * v[7] - v[4] * v[3];
  T t15 = v[0] * v[13] - v[12] * v[1];
  T t16 = v[0] * v[9] - v[8] * v[1];
  T t17 = v[0] * v[5] - v[4] * v[1];

  Mat4<T> result;
  result[0] = Vec4<T>{c0, c1, c2, c3} * invDet;
  result[1] = Vec4<T>{-(v[4] * t0 - v[8] * t1 + v[12] * t2),
                      (v[0] * t0 - v[8] * t3 + v[12] * t4),
                      -(v[0] * t1 - v[4] * t3 + v[12] * t5),
                      (v[0] * t2 - v[4] * t4 + v[8] * t5)} *
              invDet;
  result[2] = Vec4<T>{-(v[5] * t6 - v[9] * t7 + v[13] * t8),
                      (v[1] * t6 - v[9] * t12 + v[13] * t13),
                      -(v[1] * t7 - v[5] * t12 + v[13] * t14),
                      (v[1] * t8 - v[5] * t13 + v[9] * t14)} *
              invDet;
  result[3] = Vec4<T>{-(v[6] * t9 - v[10] * t10 + v[14] * t11),
                      (v[2] * t9 - v[10] * t15 + v[14] * t16),
                      -(v[2] * t10 - v[6] * t15 + v[14] * t17),
                      (v[2] * t11 - v[6] * t16 + v[10] * t17)} *
              invDet;
  return result;
}

using Mat4f = Mat4<f32>;
using Mat4d = Mat4<f64>;

}  // namespace gazeshot::core::math