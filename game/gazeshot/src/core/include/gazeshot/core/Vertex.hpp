#pragma once

#include <gazeshot/core/math/Vec2.hpp>
#include <gazeshot/core/math/Vec3.hpp>

namespace gazeshot::core {

struct Vertex {
  math::Vec3f position;
  math::Vec3f normal;
  math::Vec2f texCoord;
};

static_assert(
    sizeof(Vertex) == 32, "Vertex struct must be 32 bytes for GPU alignment"
);

}  // namespace gazeshot::core
