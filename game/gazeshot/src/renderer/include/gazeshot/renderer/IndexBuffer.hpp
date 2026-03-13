#pragma once

#include <gazeshot/core/Types.hpp>

namespace gazeshot::renderer {

class IndexBuffer {
 public:
  virtual ~IndexBuffer() = default;
  virtual void bind() const = 0;
  virtual void unbind() const = 0;
  virtual core::u32 count() const = 0;
};

}  // namespace gazeshot::renderer