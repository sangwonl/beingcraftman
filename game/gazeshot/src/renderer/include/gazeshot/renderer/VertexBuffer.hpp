#pragma once

#include <gazeshot/core/Types.hpp>

using namespace gazeshot::core;

namespace gazeshot::renderer {

enum class BufferUsage : u8 { Static, Dynamic, Stream };

class VertexBuffer {
 public:
  virtual ~VertexBuffer() = default;
  virtual void bind() const = 0;
  virtual void unbind() const = 0;
  virtual void updateData(const void* data, u32 size) = 0;
};

}  // namespace gazeshot::renderer