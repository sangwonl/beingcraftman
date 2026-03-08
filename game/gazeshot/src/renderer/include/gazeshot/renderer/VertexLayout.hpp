#pragma once

#include <gazeshot/core/Types.hpp>
#include <string>
#include <vector>

namespace gazeshot::renderer {

enum class AttribType : core::u8 {
  Float1,
  Float2,
  Float3,
  Float4,
  Int1,
  Int2,
  Int3,
  Int4,
};

struct VertexAttrib {
  std::string name;
  AttribType type;
  bool normalized = false;
};

class VertexLayout {
 public:
  VertexLayout(std::initializer_list<VertexAttrib> attribs)
      : attribs_(attribs) {
    calculateOffsets();
  }

  const std::vector<VertexAttrib>& attribs() const { return attribs_; }
  const std::vector<core::u32>& offsets() const { return offsets_; }
  core::u32 stride() const { return stride_; }

  static core::u32 attribSize(AttribType type) {
    switch (type) {
      case AttribType::Float1:
        return 4;
      case AttribType::Float2:
        return 8;
      case AttribType::Float3:
        return 12;
      case AttribType::Float4:
        return 16;
      case AttribType::Int1:
        return 4;
      case AttribType::Int2:
        return 8;
      case AttribType::Int3:
        return 12;
      case AttribType::Int4:
        return 16;
    }
    return 0;  // Should never reach here
  }

  static core::u32 attribComponentCount(AttribType type) {
    switch (type) {
      case AttribType::Float1:
      case AttribType::Int1:
        return 1;
      case AttribType::Float2:
      case AttribType::Int2:
        return 2;
      case AttribType::Float3:
      case AttribType::Int3:
        return 3;
      case AttribType::Float4:
      case AttribType::Int4:
        return 4;
    }
    return 0;  // Should never reach here
  }

 private:
  void calculateOffsets() {
    offsets_.clear();
    stride_ = 0;
    for (const auto& a : attribs_) {
      offsets_.push_back(stride_);
      stride_ += attribSize(a.type);
    }
  }

  std::vector<VertexAttrib> attribs_;
  std::vector<core::u32> offsets_;
  core::u32 stride_ = 0;
};

}  // namespace gazeshot::renderer