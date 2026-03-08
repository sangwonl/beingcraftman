#pragma once

#include <gazeshot/core/Types.hpp>

using namespace gazeshot::core;

namespace gazeshot::renderer {

class IndexBuffer {
 public:
  virtual ~IndexBuffer() = default;
  virtual void bind() const = 0;
  virtual void unbind() const = 0;
  virtual u32 count() const = 0;
};

}  // namespace gazeshot::renderer