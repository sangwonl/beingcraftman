#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <gazeshot/renderer/Renderer.hpp>
#include <string>

using namespace gazeshot::core;

namespace gazeshot::renderer {

class GLVertexBuffer : public VertexBuffer {
 public:
  GLVertexBuffer(const void* data, u32 size, BufferUsage usage) {
    glGenBuffers(1, &id_);
    glBindBuffer(GL_ARRAY_BUFFER, id_);
    GLenum glUsage = GL_STATIC_DRAW;
    switch (usage) {
      case BufferUsage::Static:
        glUsage = GL_STATIC_DRAW;
        break;
      case BufferUsage::Dynamic:
        glUsage = GL_DYNAMIC_DRAW;
        break;
      case BufferUsage::Stream:
        glUsage = GL_STREAM_DRAW;
        break;
    }
    glBufferData(GL_ARRAY_BUFFER, size, data, glUsage);
  }
  ~GLVertexBuffer() override {
    if (id_) {
      glDeleteBuffers(1, &id_);
    }
  };

  // 이동
  GLVertexBuffer(GLVertexBuffer&& other) noexcept
      : id_(std::exchange(other.id_, 0)) {}
  GLVertexBuffer& operator=(GLVertexBuffer&&) noexcept;

  // 복사 방지
  GLVertexBuffer(const GLVertexBuffer&) = delete;
  GLVertexBuffer& operator=(const GLVertexBuffer&) = delete;

  void bind() const override { glBindBuffer(GL_ARRAY_BUFFER, id_); }
  void unbind() const override { glBindBuffer(GL_ARRAY_BUFFER, 0); }
  void updateData(const void* data, u32 size) override {
    glBindBuffer(GL_ARRAY_BUFFER, id_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
  }

 private:
  u32 id_ = 0;
};

class GLIndexBuffer : public IndexBuffer {
 public:
  GLIndexBuffer(const u32* data, u32 count) : count_(count) {
    glGenBuffers(1, &id_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, count * sizeof(u32), data, GL_STATIC_DRAW
    );
  }
  ~GLIndexBuffer() override {
    if (id_) {
      glDeleteBuffers(1, &id_);
    }
  }

  GLIndexBuffer(GLIndexBuffer&& other) noexcept
      : id_(std::exchange(other.id_, 0)),
        count_(std::exchange(other.count_, 0)) {}
  GLIndexBuffer& operator=(GLIndexBuffer&& other) noexcept {
    if (this != &other) {
      if (id_) {
        glDeleteBuffers(1, &id_);
      }
      id_ = std::exchange(other.id_, 0);
      count_ = std::exchange(other.count_, 0);
    }
    return *this;
  }

  GLIndexBuffer(const GLIndexBuffer&) = delete;
  GLIndexBuffer& operator=(const GLIndexBuffer&) = delete;

  void bind() const override { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_); }
  void unbind() const override { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }
  u32 count() const override { return count_; }

 private:
  u32 id_ = 0;
  u32 count_ = 0;
};

class GLShaderProgram : public ShaderProgram {
 public:
  GLShaderProgram(std::string_view vertSrc, std::string_view fragSrc) {
    auto vs = compile(GL_VERTEX_SHADER, processSource(vertSrc));
    auto fs = compile(GL_FRAGMENT_SHADER, processSource(fragSrc));
    id_ = glCreateProgram();
    glAttachShader(id_, vs);
    glAttachShader(id_, fs);
    glLinkProgram(id_);
    glDeleteShader(vs);
    glDeleteShader(fs);
  }
  ~GLShaderProgram() override {
    if (id_) {
      glDeleteProgram(id_);
    }
  }

  GLShaderProgram(GLShaderProgram&& other) noexcept
      : id_(std::exchange(other.id_, 0)) {}
  GLShaderProgram& operator=(GLShaderProgram&& other) noexcept {
    if (this != &other) {
      if (id_) {
        glDeleteProgram(id_);
      }
      id_ = std::exchange(other.id_, 0);
    }
    return *this;
  }

  GLShaderProgram(const GLShaderProgram&) = delete;
  GLShaderProgram& operator=(const GLShaderProgram&) = delete;

  void bind() const override { glUseProgram(id_); }
  void unbind() const override { glUseProgram(0); }

  void setInt(std::string_view name, i32 value) override {
    glUniform1i(loc(name), value);
  }
  void setFloat(std::string_view name, f32 value) override {
    glUniform1f(loc(name), value);
  }
  void setVec3(std::string_view name, const Vec3f& value) override {
    glUniform3f(loc(name), value.x, value.y, value.z);
  }
  void setVec4(std::string_view name, const Vec4f& value) override {
    glUniform4f(loc(name), value.x, value.y, value.z, value.w);
  }
  void setMat4(std::string_view name, const Mat4f& value) override {
    glUniformMatrix4fv(loc(name), 1, GL_TRUE, value.data());
  }

 private:
  u32 id_ = 0;
  i32 loc(std::string_view name) const {
    return glGetUniformLocation(id_, std::string(name).c_str());
  }

  static std::string processSource(std::string_view src) {
    std::string result;
#ifdef __EMSCRIPTEN__
    result = "#version 300 es\nprecision mediump float;\n";
#else
    result = "#version 330 core\n";
#endif
    result += src;
    return result;
  }

  static u32 compile(GLenum type, const std::string& src) {
    u32 shader = glCreateShader(type);
    const char* ptr = src.c_str();
    glShaderSource(shader, 1, &ptr, nullptr);
    glCompileShader(shader);
    return shader;
  }
};

class GLRenderer : public Renderer {
 public:
  void init() override {}
  void clear(const Vec4f& color) override {
    glClearColor(color.x, color.y, color.z, color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  void setDepthTest(bool enabled) override {
    if (enabled) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
  void setViewport(u32 x, u32 y, u32 w, u32 h) override {
    glViewport(x, y, w, h);
  }

  std::unique_ptr<VertexBuffer> createVertexBuffer(
      const void* data, u32 size, BufferUsage usage
  ) override {
    return std::make_unique<GLVertexBuffer>(data, size, usage);
  }

  std::unique_ptr<IndexBuffer> createIndexBuffer(
      const u32* data, u32 count
  ) override {
    return std::make_unique<GLIndexBuffer>(data, count);
  }

  std::unique_ptr<ShaderProgram> createShaderProgram(
      std::string_view vertexSrc, std::string_view fragmentSrc
  ) override {
    return std::make_unique<GLShaderProgram>(vertexSrc, fragmentSrc);
  }

  void drawIndexed(u32 indexCount) override {
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
  }
  void drawArrays(u32 vertexCount) override {
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
  }

  u32 createVertexArray() override {
    u32 vao;
    glGenVertexArrays(1, &vao);
    return vao;
  }
  void bindVertexArray(u32 vao) override { glBindVertexArray(vao); }
  void setVertexLayout(const VertexLayout& layout) override {
    for (u32 i = 0; i < layout.attribs().size(); ++i) {
      const auto& attr = layout.attribs()[i];
      glVertexAttribPointer(
          i,
          VertexLayout::attribComponentCount(attr.type),
          GL_FLOAT,
          attr.normalized ? GL_TRUE : GL_FALSE,
          layout.stride(),
          reinterpret_cast<const void*>(
              static_cast<uintptr_t>(layout.offsets()[i])
          )
      );
      glEnableVertexAttribArray(i);
    }
  }
};

std::unique_ptr<Renderer> createRenderer() {
  return std::make_unique<GLRenderer>();
}

}  // namespace gazeshot::renderer