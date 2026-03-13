#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Mat4.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>
#include <string_view>

namespace gazeshot::renderer {

class ShaderProgram {
 public:
  virtual ~ShaderProgram() = default;
  virtual void bind() const = 0;
  virtual void unbind() const = 0;

  virtual void setInt(std::string_view name, core::i32 value) = 0;
  virtual void setFloat(std::string_view name, core::f32 value) = 0;
  virtual void setVec3(std::string_view name, const core::math::Vec3f& value) = 0;
  virtual void setVec4(std::string_view name, const core::math::Vec4f& value) = 0;
  virtual void setMat4(std::string_view name, const core::math::Mat4f& value) = 0;
};

}  // namespace gazeshot::renderer