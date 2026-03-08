#include <doctest/doctest.h>

#include <gazeshot/core/math/Math.hpp>

using namespace gazeshot::core::math;
using namespace gazeshot::core::math::literals;

constexpr float EPS = 1e-5f;

TEST_CASE("translate") {
  auto m = translate(Vec3f{5, 10, 15});
  auto p = m * Vec4f{0, 0, 0, 1};
  CHECK(p.x == doctest::Approx(5.0f));
  CHECK(p.y == doctest::Approx(10.0f));
  CHECK(p.z == doctest::Approx(15.0f));
}

TEST_CASE("translate 는 방향벡터에 영향 없음") {
  auto m = translate(Vec3f{5, 10, 15});
  auto d = m * Vec4f{1, 0, 0, 0};  // w = 0
  CHECK(d.x == doctest::Approx(1.0f));
  CHECK(d.y == doctest::Approx(0.0f));
}

TEST_CASE("scale") {
  auto m = scale(Vec3f{2, 3, 4});
  auto p = m * Vec4f{1, 1, 1, 1};
  CHECK(p.x == doctest::Approx(2.0f));
  CHECK(p.y == doctest::Approx(3.0f));
  CHECK(p.z == doctest::Approx(4.0f));
}

TEST_CASE("rotateY 90 degrees") {
  auto m = rotateY(90.0_deg);
  // x축 위 점, Y축 90도 회전 (Z축 음수 방향으로 이동)
  auto p = m * Vec4f{1, 0, 0, 1};
  CHECK(p.x == doctest::Approx(0.0f).epsilon(EPS));
  CHECK(p.z == doctest::Approx(-1.0f).epsilon(EPS));
}

TEST_CASE("rotate arbitrary axis") {
  auto m = rotate(90.0_deg, Vec3f{0, 1, 0});
  auto p = m * Vec4f{1, 0, 0, 1};
  CHECK(p.x == doctest::Approx(0.0f).epsilon(EPS));
  CHECK(p.z == doctest::Approx(-1.0f).epsilon(EPS));
}

TEST_CASE("lookAt") {
  auto v = lookAt(Vec3f{0, 0, 5}, Vec3f{0, 0, 0}, Vec3f{0, 1, 0});
  // 카메라가 (0, 0, 5)에서 원점을 봄
  // 원점의 점의 카메라 앞 5만큼에 있어야 함
  auto p = v * Vec4f{0, 0, 0, 1};
  CHECK(p.z == doctest::Approx(-5.0f).epsilon(EPS));
}

TEST_CASE("perspective") {
  auto p = perspective(90.0_deg, 1.0f, 0.1f, 100.0f);
  // near plane 위의 점
  auto result = p * Vec4f{0, 0, -0.1f, 1};
  // perspective divide 후 z값이 -1이 되어야 함
  float ndcZ = result.z / result.w;
  CHECK(ndcZ == doctest::Approx(-1.0f).epsilon(EPS));
}

TEST_CASE("Quat fromAxisAngle -> toMat4 일관성") {
  auto a = Quatf::fromAxisAngle({0, 1, 0}, 0.0_deg);
  auto b = Quatf::fromAxisAngle({0, 1, 0}, 90.0_deg);
  auto mid = slerp(a, b, 0.5f);
  auto m = mid.toMat4();

  // 45도 회전과 동일해야 함
  auto expected = rotateY(45.0_deg);
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      CHECK(m[r][c] == doctest::Approx(expected[r][c]).epsilon(EPS));
    }
  }
}

TEST_CASE("ortho") {
  auto m = ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
  // 원점은 NDC 원점에 매핑
  auto p = m * Vec4f{0, 0, -50.05f, 1};
  CHECK(p.x == doctest::Approx(0.0f).epsilon(EPS));
  CHECK(p.y == doctest::Approx(0.0f).epsilon(EPS));
  // 경계값: 좌측 하단
  auto corner = m * Vec4f{-1, -1, -0.1f, 1};
  CHECK(corner.x == doctest::Approx(-1.0f).epsilon(EPS));
  CHECK(corner.y == doctest::Approx(-1.0f).epsilon(EPS));
}

TEST_CASE("Quat slerp 최단 경로 (cosHalf < 0)") {
  // 180도 이상 차이나는 쿼터니언 — 부정 분기 진입
  auto a = Quatf::fromAxisAngle({0, 1, 0}, 0.0_deg);
  auto b = Quatf::fromAxisAngle({0, 1, 0}, 270.0_deg);
  auto mid = slerp(a, b, 0.5f);
  auto m = mid.toMat4();

  // 최단 경로는 -90도 방향이므로 중간값은 -45도 = 315도
  auto expected = rotateY(-45.0_deg);
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      CHECK(m[r][c] == doctest::Approx(expected[r][c]).epsilon(EPS));
    }
  }
}

TEST_CASE("user-defined literals _deg") {
  CHECK(radians(90.0f) == doctest::Approx(90.0_deg).epsilon(EPS));
  CHECK(radians(45.0f) == doctest::Approx(45.0_deg).epsilon(EPS));
}