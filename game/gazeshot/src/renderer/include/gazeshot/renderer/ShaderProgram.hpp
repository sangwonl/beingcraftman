#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Mat4.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>
#include <string_view>

using namespace gazeshot::core;
using namespace gazeshot::core::math;

namespace gazeshot::renderer {

class ShaderProgram {
 public:
  virtual ~ShaderProgram() = default;
  virtual void bind() const = 0;
  virtual void unbind() const = 0;

  virtual void setInt(std::string_view name, i32 value) = 0;
  virtual void setFloat(std::string_view name, f32 value) = 0;
  virtual void setVec3(std::string_view name, const Vec3f& value) = 0;
  virtual void setVec4(std::string_view name, const Vec4f& value) = 0;
  virtual void setMat4(std::string_view name, const Mat4f& value) = 0;
};

}  // namespace gazeshot::renderer