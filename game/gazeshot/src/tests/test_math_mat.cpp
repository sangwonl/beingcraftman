#include <doctest/doctest.h>

#include <gazeshot/core/math/Math.hpp>

using namespace gazeshot::core::math;

TEST_CASE("Mat4 identity") {
  auto I = Mat4f::identity();
  CHECK(I[0].x == 1.0f);
  CHECK(I[1].y == 1.0f);
  CHECK(I[2].z == 1.0f);
  CHECK(I[3].w == 1.0f);

  // 비대각 원소는 0
  CHECK(I[0].y == 0.0f);
  CHECK(I[0].z == 0.0f);
  CHECK(I[0].w == 0.0f);
  CHECK(I[1].x == 0.0f);
  CHECK(I[1].z == 0.0f);
  CHECK(I[1].w == 0.0f);
  CHECK(I[2].x == 0.0f);
  CHECK(I[2].y == 0.0f);
  CHECK(I[2].w == 0.0f);
  CHECK(I[3].x == 0.0f);
  CHECK(I[3].y == 0.0f);
  CHECK(I[3].z == 0.0f);
}

TEST_CASE("Mat4 identity * identity = identity") {
  auto I = Mat4f::identity();
  auto result = I * I;
  CHECK(result == I);
}

TEST_CASE("Mat4 * Vec4") {
  auto I = Mat4f::identity();
  Vec4f v{1, 2, 3, 1};
  auto result = I * v;
  CHECK(result == v);
}

TEST_CASE("Mat4 이동 행렬") {
  auto m = Mat4f::identity();
  m[3] = Vec4f{5, 10, 15, 1};

  Vec4f point{0, 0, 0, 1};
  auto moved = m * point;
  CHECK(moved.x == doctest::Approx(5.0f));
  CHECK(moved.y == doctest::Approx(10.0f));
  CHECK(moved.z == doctest::Approx(15.0f));

  Vec4f dir{1, 0, 0, 0};
  auto same = m * dir;
  CHECK(same.x == doctest::Approx(1.0f));
  CHECK(same.y == doctest::Approx(0.0f));
}

TEST_CASE("Mat4 inverse") {
  auto m = Mat4f::identity();
  m[3] = Vec4f{5, 10, 15, 1};

  auto inv = inverse(m);
  auto result = m * inv;

  // m * m^(-1) = I
  auto I = Mat4f::identity();
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      CHECK(result[c][r] == doctest::Approx(I[c][r]).epsilon(1e-5));
    }
  }
}

TEST_CASE("Mat4 transpose") {
  Mat4f m{{1, 5, 9, 13}, {2, 6, 10, 14}, {3, 7, 11, 15}, {4, 8, 12, 16}};

  auto t = transpose(m);
  CHECK(t[0] == Vec4f{1, 2, 3, 4});
  CHECK(t[1] == Vec4f{5, 6, 7, 8});
  CHECK(t[2] == Vec4f{9, 10, 11, 12});
  CHECK(t[3] == Vec4f{13, 14, 15, 16});
}

TEST_CASE("Mat4 data() 메모리 레이아웃") {
  auto m = Mat4f::identity();
  const float* ptr = m.data();

  // column major: 첫 4개 = 첫 번째 열
  CHECK(ptr[0] == 1.0f);  // m[0][0]
  CHECK(ptr[1] == 0.0f);  // m[0][1]
  CHECK(ptr[2] == 0.0f);  // m[0][2]
  CHECK(ptr[3] == 0.0f);  // m[0][3]
  // 다음 4개 = 두 번째 열
  CHECK(ptr[4] == 0.0f);  // m[1][0]
  CHECK(ptr[5] == 1.0f);  // m[1][1]
  CHECK(ptr[6] == 0.0f);  // m[1][2]
  CHECK(ptr[7] == 0.0f);  // m[1][3]
  // 다음 4개 = 세 번째 열
  CHECK(ptr[8] == 0.0f);   // m[2][0]
  CHECK(ptr[9] == 0.0f);   // m[2][1]
  CHECK(ptr[10] == 1.0f);  // m[2][2]
  CHECK(ptr[11] == 0.0f);  // m[2][3]
  // 마지막 4개 = 네 번째 열
  CHECK(ptr[12] == 0.0f);  // m[3][0]
  CHECK(ptr[13] == 0.0f);  // m[3][1]
  CHECK(ptr[14] == 0.0f);  // m[3][2]
  CHECK(ptr[15] == 1.0f);  // m[3][3]
}