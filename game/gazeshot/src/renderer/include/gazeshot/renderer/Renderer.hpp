#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec4.hpp>
#include <gazeshot/renderer/IndexBuffer.hpp>
#include <gazeshot/renderer/ShaderProgram.hpp>
#include <gazeshot/renderer/VertexBuffer.hpp>
#include <gazeshot/renderer/VertexLayout.hpp>
#include <memory>
#include <string_view>

using namespace gazeshot::core::math;

namespace gazeshot::renderer {

class Renderer {
 public:
  virtual ~Renderer() = default;

  virtual void init() = 0;
  virtual void clear(const Vec4f& color) = 0;
  virtual void setViewport(core::u32 x, core::u32 y, core::u32 w,
                           core::u32 h) = 0;
  virtual void setDepthTest(bool enabled) = 0;

  virtual std::unique_ptr<VertexBuffer> createVertexBuffer(
      const void* data, core::u32 size, BufferUsage usage) = 0;

  virtual std::unique_ptr<IndexBuffer> createIndexBuffer(const core::u32* data,
                                                         core::u32 count) = 0;
  virtual std::unique_ptr<ShaderProgram> createShaderProgram(
      std::string_view vertexSrc, std::string_view fragmentSrc) = 0;

  virtual void drawIndexed(core::u32 indexCount) = 0;
  virtual void drawArrays(core::u32 vertexCount) = 0;

  virtual core::u32 createVertexArray() = 0;
  virtual void bindVertexArray(core::u32 vao) = 0;
  virtual void setVertexLayout(const VertexLayout& layout) = 0;
};

std::unique_ptr<Renderer> createRenderer();

}  // namespace gazeshot::renderer