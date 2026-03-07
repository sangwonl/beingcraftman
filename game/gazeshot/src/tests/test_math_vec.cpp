#include <doctest/doctest.h>

#include <gazeshot/core/math/Math.hpp>

using namespace gazeshot::core::math;

TEST_CASE("Vec3 기본 생성") {
  Vec3f v;
  CHECK(v.x == 0.0f);
  CHECK(v.y == 0.0f);
  CHECK(v.z == 0.0f);
}

TEST_CASE("Vec3 스칼라 생성") {
  Vec3f v(5.0f);
  CHECK(v.x == 5.0f);
  CHECK(v.y == 5.0f);
  CHECK(v.z == 5.0f);
}

TEST_CASE("Vec3 인덱스 접근") {
  Vec3f v{10, 20, 30};
  CHECK(v[0] == 10.0f);
  CHECK(v[1] == 20.0f);
  CHECK(v[2] == 30.0f);
}

TEST_CASE("Vec3 덧셈") {
  Vec3f a{1, 2, 3};
  Vec3f b{4, 5, 6};
  auto c = a + b;
  CHECK(c.x == doctest::Approx(5.0f));
  CHECK(c.y == doctest::Approx(7.0f));
  CHECK(c.z == doctest::Approx(9.0f));
}

TEST_CASE("Vec3 뺄셈") {
  auto c = Vec3f{5, 7, 9} - Vec3f{4, 5, 6};
  CHECK(c == Vec3f(1, 2, 3));
}

TEST_CASE("Vec3 스칼라 곱셈") {
  Vec3f v{1, 2, 3};
  CHECK(v * 2.0f == Vec3f{2, 4, 6});
  CHECK(2.0f * v == Vec3f{2, 4, 6});
}

TEST_CASE("Vec3 단항 마이너스") {
  Vec3f v{1, -2, 3};
  CHECK(-v == Vec3f{-1, 2, -3});
}

TEST_CASE("Vec3 dot product") {
  Vec3f a{1, 0, 0};
  Vec3f b{0, 1, 0};
  CHECK(dot(a, b) == doctest::Approx(0.0f));
}

TEST_CASE("Vec3 cross product") {
  Vec3f x{1, 0, 0};
  Vec3f y{0, 1, 0};
  Vec3f z = cross(x, y);
  CHECK(z.x == doctest::Approx(0.0f));
  CHECK(z.y == doctest::Approx(0.0f));
  CHECK(z.z == doctest::Approx(1.0f));

  Vec3f neg_z = cross(y, x);
  CHECK(neg_z == -z);
}

TEST_CASE("Vec3 length & normalize") {
  Vec3f v{3, 4, 0};
  CHECK(length(v) == doctest::Approx(5.0f));

  Vec3f n = normalize(v);
  CHECK(length(n) == doctest::Approx(1.0f));
  CHECK(n.x == doctest::Approx(0.6f));
  CHECK(n.y == doctest::Approx(0.8f));
}

TEST_CASE("Vec3 lerp") {
  Vec3f a{0, 0, 0};
  Vec3f b{10, 20, 30};
  auto mid = lerp(a, b, 0.5f);
  CHECK(mid == Vec3f{5, 10, 15});
}

TEST_CASE("Vec3 constexpr 검증") {
  constexpr Vec3f a{1, 2, 3};
  constexpr Vec3f b{4, 5, 6};
  constexpr auto c = a + b;
  constexpr auto d = dot(a, b);

  CHECK(c == Vec3f(5, 7, 9));
  CHECK(d == doctest::Approx(32.0f));
}