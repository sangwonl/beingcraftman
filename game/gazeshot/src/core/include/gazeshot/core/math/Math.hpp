#pragma once

#include <gazeshot/core/math/Literals.hpp>
#include <gazeshot/core/math/Mat4.hpp>
#include <gazeshot/core/math/Quat.hpp>
#include <gazeshot/core/math/Transform.hpp>
#include <gazeshot/core/math/Vec2.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>

static_assert(sizeof(gazeshot::core::math::Vec2f) == 8, "Vec2f should be 8 bytes (2 floats)");
static_assert(sizeof(gazeshot::core::math::Vec3f) == 12, "Vec3f should be 12 bytes (3 floats)");
static_assert(sizeof(gazeshot::core::math::Vec4f) == 16, "Vec4f should be 16 bytes (4 floats)");
static_assert(sizeof(gazeshot::core::math::Mat4f) == 64, "Mat4f should be 64 bytes (16 floats)");
static_assert(sizeof(gazeshot::core::math::Quatf) == 16, "Quatf should be 16 bytes (4 floats)");