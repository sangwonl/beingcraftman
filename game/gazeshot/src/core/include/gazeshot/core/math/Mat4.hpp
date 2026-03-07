#pragma once

#include <cassert>
#include <cmath>
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>

namespace gazeshot::core::math {

template <typename T = f32>
struct Mat4 {
  // Row-major: rows[0] = 첫 번째 행
  Vec4<T> rows[4] = {};

  constexpr Mat4() = default;

  constexpr explicit Mat4(T diagonal) {
    rows[0].x = diagonal;
    rows[1].y = diagonal;
    rows[2].z = diagonal;
    rows[3].w = diagonal;
  }

  constexpr Mat4(const Vec4<T>& r0, const Vec4<T>& r1, const Vec4<T>& r2,
                 const Vec4<T>& r3)
      : rows{r0, r1, r2, r3} {}

  // m[row][col] — 수학 표기와 일치
  constexpr Vec4<T>& operator[](usize i) {
    assert(i < 4);
    return rows[i];
  };
  constexpr const Vec4<T>& operator[](usize i) const {
    assert(i < 4);
    return rows[i];
  };

  static constexpr Mat4 identity() { return Mat4(T(1)); }

  constexpr const T* data() const { return rows[0].data(); }

  constexpr bool operator==(const Mat4&) const = default;
};

// m[row][col] 기준 행렬 곱: result[i][j] = sum_k(a[i][k] * b[k][j])
template <typename T>
constexpr Mat4<T> operator*(const Mat4<T>& a, const Mat4<T>& b) {
  Mat4<T> result;
  for (usize row = 0; row < 4; ++row) {
    for (usize col = 0; col < 4; ++col) {
      T sum = T(0);
      for (usize k = 0; k < 4; ++k) {
        sum += a[row][k] * b[k][col];
      }
      result[row][col] = sum;
    }
  }
  return result;
}

// 행렬 * 벡터: result[i] = dot(row_i, v)
template <typename T>
constexpr Vec4<T> operator*(const Mat4<T>& m, const Vec4<T>& v) {
  return {m[0].x * v.x + m[0].y * v.y + m[0].z * v.z + m[0].w * v.w,
          m[1].x * v.x + m[1].y * v.y + m[1].z * v.z + m[1].w * v.w,
          m[2].x * v.x + m[2].y * v.y + m[2].z * v.z + m[2].w * v.w,
          m[3].x * v.x + m[3].y * v.y + m[3].z * v.z + m[3].w * v.w};
}

template <typename T>
constexpr Mat4<T> transpose(const Mat4<T>& m) {
  return {{m[0].x, m[1].x, m[2].x, m[3].x},
          {m[0].y, m[1].y, m[2].y, m[3].y},
          {m[0].z, m[1].z, m[2].z, m[3].z},
          {m[0].w, m[1].w, m[2].w, m[3].w}};
}

// 역행렬 (코팩터 전개, m[row][col] 접근)
template <typename T>
Mat4<T> inverse(const Mat4<T>& m) {
  // 상위 2행의 2x2 소행렬식
  T s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
  T s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
  T s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
  T s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
  T s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
  T s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];

  // 하위 2행의 2x2 소행렬식
  T c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
  T c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
  T c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
  T c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
  T c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
  T c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];

  T det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
  assert(std::abs(det) > T(1e-8));
  T inv = T(1) / det;

  Mat4<T> r;
  r[0][0] = ( m[1][1] * c5 - m[1][2] * c4 + m[1][3] * c3) * inv;
  r[0][1] = (-m[0][1] * c5 + m[0][2] * c4 - m[0][3] * c3) * inv;
  r[0][2] = ( m[3][1] * s5 - m[3][2] * s4 + m[3][3] * s3) * inv;
  r[0][3] = (-m[2][1] * s5 + m[2][2] * s4 - m[2][3] * s3) * inv;

  r[1][0] = (-m[1][0] * c5 + m[1][2] * c2 - m[1][3] * c1) * inv;
  r[1][1] = ( m[0][0] * c5 - m[0][2] * c2 + m[0][3] * c1) * inv;
  r[1][2] = (-m[3][0] * s5 + m[3][2] * s2 - m[3][3] * s1) * inv;
  r[1][3] = ( m[2][0] * s5 - m[2][2] * s2 + m[2][3] * s1) * inv;

  r[2][0] = ( m[1][0] * c4 - m[1][1] * c2 + m[1][3] * c0) * inv;
  r[2][1] = (-m[0][0] * c4 + m[0][1] * c2 - m[0][3] * c0) * inv;
  r[2][2] = ( m[3][0] * s4 - m[3][1] * s2 + m[3][3] * s0) * inv;
  r[2][3] = (-m[2][0] * s4 + m[2][1] * s2 - m[2][3] * s0) * inv;

  r[3][0] = (-m[1][0] * c3 + m[1][1] * c1 - m[1][2] * c0) * inv;
  r[3][1] = ( m[0][0] * c3 - m[0][1] * c1 + m[0][2] * c0) * inv;
  r[3][2] = (-m[3][0] * s3 + m[3][1] * s1 - m[3][2] * s0) * inv;
  r[3][3] = ( m[2][0] * s3 - m[2][1] * s1 + m[2][2] * s0) * inv;

  return r;
}

using Mat4f = Mat4<f32>;
using Mat4d = Mat4<f64>;

}  // namespace gazeshot::core::math
