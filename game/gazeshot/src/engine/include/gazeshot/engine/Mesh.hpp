#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/Vertex.hpp>
#include <gazeshot/renderer/Renderer.hpp>
#include <memory>
#include <span>
#include <vector>

namespace gazeshot::engine {

class Mesh {
 public:
  Mesh() = default;
  Mesh(std::vector<core::Vertex> vertices, std::vector<core::u32> indices)
      : vertices_(std::move(vertices)), indices_(std::move(indices)) {}

  void upload(renderer::Renderer& r) {
    vao_ = r.createVertexArray();
    r.bindVertexArray(vao_);

    vbo_ = r.createVertexBuffer(
        vertices_.data(),
        static_cast<core::u32>(vertices_.size() * sizeof(core::Vertex)),
        renderer::BufferUsage::Static
    );

    ibo_ = r.createIndexBuffer(
        indices_.data(), static_cast<core::u32>(indices_.size())
    );

    r.setVertexLayout(
        {{"aPos", renderer::AttribType::Float3},
         {"aNormal", renderer::AttribType::Float3},
         {"aTexCoord", renderer::AttribType::Float2}}
    );
  }

  void draw(renderer::Renderer& r) const {
    r.bindVertexArray(vao_);
    r.drawIndexed(static_cast<core::u32>(indices_.size()));
  }

  std::span<const core::Vertex> vertices() const { return vertices_; }
  std::span<const core::u32> indices() const { return indices_; }
  core::u32 vertexCount() const {
    return static_cast<core::u32>(vertices_.size());
  }
  core::u32 indexCount() const {
    return static_cast<core::u32>(indices_.size());
  }

 private:
  std::vector<core::Vertex> vertices_;
  std::vector<core::u32> indices_;

  core::u32 vao_ = 0;
  std::unique_ptr<renderer::VertexBuffer> vbo_;
  std::unique_ptr<renderer::IndexBuffer> ibo_;
};

}  // namespace gazeshot::engine