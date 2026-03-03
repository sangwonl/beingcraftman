#include <doctest/doctest.h>
#include <gazeshot/core/Types.hpp>

#include <type_traits>

using namespace gazeshot::core;

TEST_CASE("타입 크기 검증") {
	CHECK(sizeof(u8) == 1);
	CHECK(sizeof(u16) == 2);
	CHECK(sizeof(u32) == 4);
	CHECK(sizeof(u64) == 8);

	CHECK(sizeof(i8) == 1);
	CHECK(sizeof(i16) == 2);
	CHECK(sizeof(i32) == 4);
	CHECK(sizeof(i64) == 8);

	CHECK(sizeof(f32) == 4);
	CHECK(sizeof(f64) == 8);

	// usize는 플랫폼에 따라 다르지만, 일반적으로 포인터 크기와 동일해야 함
	CHECK(sizeof(usize) == sizeof(void*));
}

TEST_CASE("타입 특성 검증") {
	CHECK(std::is_unsigned_v<u8>);
	CHECK(std::is_unsigned_v<u16>);
	CHECK(std::is_unsigned_v<u32>);
	CHECK(std::is_unsigned_v<u64>);

	CHECK(std::is_signed_v<i8>);
	CHECK(std::is_signed_v<i16>);
	CHECK(std::is_signed_v<i32>);
	CHECK(std::is_signed_v<i64>);

	CHECK(std::is_floating_point_v<f32>);
	CHECK(std::is_floating_point_v<f64>);
}

TEST_CASE("f32 정밀도") {
	f32 a = 0.1f;
	f32 b = 0.2f;
	f32 c = a + b;

	CHECK(c == doctest::Approx(0.3f));
	CHECK(c == doctest::Approx(0.3f).epsilon(0.0001f));
}