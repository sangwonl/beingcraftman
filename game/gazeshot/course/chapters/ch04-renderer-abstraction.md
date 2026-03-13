# Chapter 04: 렌더링 추상화 레이어

## 데모 미리보기

```
┌─────────────────────────────────────┐
│                                     │
│       ◆ (같은 3D 큐브)              │
│      ╱  ╲                           │
│     ╱    ╲  이번엔 game 코드에       │
│    ◆──────◆  gl* 호출이 없다         │
│                                     │
│  renderer->drawIndexed(vao, 36);    │
└─────────────────────────────────────┘
```

- **데모**: Ch.03과 동일한 회전 큐브, 하지만 game 코드에서 OpenGL 호출이 사라짐
- **코드 변화**: `glDrawElements` → `renderer->drawIndexed()`
- 블로그에 "Before / After" 코드 비교를 올릴 수 있다

---

## 학습 목표

1. 그래픽스 API 독립적인 추상 인터페이스를 설계한다
2. OpenGL ES 3.0 백엔드를 구현한다
3. VertexBuffer, IndexBuffer, VertexArray, ShaderProgram, Texture 추상화
4. RAII로 GPU 리소스를 관리한다 (소멸자 해제, 이동 시맨틱)
5. Vertex Layout 시스템으로 attribute를 선언적으로 정의한다
6. Desktop/WASM에서 GLSL 버전을 자동 전환한다

---

## 1. 배경 지식

### 추상화의 목적

현재 `main.cpp`에 `glCreateShader`, `glBufferData` 등이 직접 들어있다.
문제:

- 게임 코드가 OpenGL에 묶인다
- WebGL 2.0과 Desktop OpenGL의 미묘한 차이를 매번 `#ifdef`로 처리해야 한다
- GPU 리소스 해제를 빠뜨리기 쉽다

추상화 후:

```cpp
// Before (OpenGL 직접)
unsigned int vbo;
glGenBuffers(1, &vbo);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
// ... 100줄 뒤에 glDeleteBuffers(1, &vbo) 잊어먹음

// After (추상화)
auto vbo = renderer->createVertexBuffer(data, size, BufferUsage::Static);
// 소멸자가 자동으로 glDeleteBuffers 호출
```

### RAII와 GPU 리소스

**RAII (Resource Acquisition Is Initialization)**:

- 생성자에서 리소스 획득 (glGenBuffers)
- 소멸자에서 리소스 해제 (glDeleteBuffers)
- 스코프를 벗어나면 자동 정리

**이동 시맨틱**:

- GPU 핸들(ID)은 복사 불가 → 이중 해제 위험
- 이동만 허용 → 핸들 소유권 이전

---

## 2. 설계

### 타입 계층

```
renderer/
├── interface/           (API 독립 — 순수 가상)
│   ├── VertexBuffer.hpp
│   ├── IndexBuffer.hpp
│   ├── VertexArray.hpp
│   ├── ShaderProgram.hpp
│   ├── Texture2D.hpp
│   ├── VertexLayout.hpp
│   └── Renderer.hpp
└── opengl/              (OpenGL 구현)
    ├── GLVertexBuffer.hpp / .cpp
    ├── GLIndexBuffer.hpp / .cpp
    ├── GLVertexArray.hpp / .cpp
    ├── GLShaderProgram.hpp / .cpp
    ├── GLTexture2D.hpp / .cpp
    └── GLRenderer.hpp / .cpp
```

### Vertex Layout 시스템

정점 데이터의 구조를 선언적으로 정의:

```cpp
VertexLayout layout = {
    { "aPos",      AttribType::Float3 },
    { "aNormal",   AttribType::Float3 },
    { "aTexCoord", AttribType::Float2 },
};
// stride = 3+3+2 = 8 floats = 32 bytes (자동 계산)
```

---

## 3. 구현 가이드

### Step 1: Vertex Layout

```hpp
// renderer/include/gazeshot/renderer/VertexLayout.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <string>
#include <vector>

namespace gazeshot::renderer {

enum class AttribType : core::u8 {
    Float1, Float2, Float3, Float4,
    Int1,   Int2,   Int3,   Int4,
};

struct VertexAttrib {
    std::string name;
    AttribType  type;
    bool        normalized = false;
};

class VertexLayout {
public:
    VertexLayout(std::initializer_list<VertexAttrib> attribs)
        : attribs_(attribs) { calculateOffsets(); }

    const std::vector<VertexAttrib>& attribs() const { return attribs_; }
    const std::vector<core::u32>& offsets() const { return offsets_; }
    core::u32 stride() const { return stride_; }

    static core::u32 attribSize(AttribType type) {
        switch (type) {
            case AttribType::Float1: return 4;
            case AttribType::Float2: return 8;
            case AttribType::Float3: return 12;
            case AttribType::Float4: return 16;
            case AttribType::Int1:   return 4;
            case AttribType::Int2:   return 8;
            case AttribType::Int3:   return 12;
            case AttribType::Int4:   return 16;
        }
        return 0;
    }

    static core::u32 attribComponentCount(AttribType type) {
        switch (type) {
            case AttribType::Float1: case AttribType::Int1: return 1;
            case AttribType::Float2: case AttribType::Int2: return 2;
            case AttribType::Float3: case AttribType::Int3: return 3;
            case AttribType::Float4: case AttribType::Int4: return 4;
        }
        return 0;
    }

private:
    void calculateOffsets() {
        offsets_.clear();
        stride_ = 0;
        for (auto& a : attribs_) {
            offsets_.push_back(stride_);
            stride_ += attribSize(a.type);
        }
    }

    std::vector<VertexAttrib> attribs_;
    std::vector<core::u32> offsets_;
    core::u32 stride_ = 0;
};

} // namespace gazeshot::renderer
```

### Step 2: 추상 인터페이스

```hpp
// renderer/include/gazeshot/renderer/VertexBuffer.hpp

#pragma once

#include <gazeshot/core/Types.hpp>

namespace gazeshot::renderer {

enum class BufferUsage : core::u8 { Static, Dynamic, Stream };

class VertexBuffer {
public:
    virtual ~VertexBuffer() = default;
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
    virtual void updateData(const void* data, core::u32 size) = 0;
};

} // namespace gazeshot::renderer
```

```hpp
// renderer/include/gazeshot/renderer/IndexBuffer.hpp

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

} // namespace gazeshot::renderer
```

```hpp
// renderer/include/gazeshot/renderer/ShaderProgram.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>
#include <gazeshot/core/math/Mat4.hpp>
#include <string_view>

namespace gazeshot::renderer {

class ShaderProgram {
public:
    virtual ~ShaderProgram() = default;
    virtual void bind() const = 0;
    virtual void unbind() const = 0;

    virtual void setInt(std::string_view name, core::i32 value) = 0;
    virtual void setFloat(std::string_view name, core::f32 value) = 0;
    virtual void setVec3(std::string_view name, const core::math::Vec3f& v) = 0;
    virtual void setVec4(std::string_view name, const core::math::Vec4f& v) = 0;
    virtual void setMat4(std::string_view name, const core::math::Mat4f& m) = 0;
};

} // namespace gazeshot::renderer
```

```hpp
// renderer/include/gazeshot/renderer/Renderer.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec4.hpp>
#include <gazeshot/renderer/VertexBuffer.hpp>
#include <gazeshot/renderer/IndexBuffer.hpp>
#include <gazeshot/renderer/ShaderProgram.hpp>
#include <gazeshot/renderer/VertexLayout.hpp>

#include <memory>
#include <string_view>

namespace gazeshot::renderer {

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void init() = 0;
    virtual void clear(const core::math::Vec4f& color) = 0;
    virtual void setViewport(core::i32 x, core::i32 y,
                             core::i32 w, core::i32 h) = 0;
    virtual void setDepthTest(bool enabled) = 0;

    // ── 리소스 생성 (팩토리) ──
    virtual std::unique_ptr<VertexBuffer> createVertexBuffer(
        const void* data, core::u32 size, BufferUsage usage) = 0;

    virtual std::unique_ptr<IndexBuffer> createIndexBuffer(
        const core::u32* data, core::u32 count) = 0;

    virtual std::unique_ptr<ShaderProgram> createShaderProgram(
        std::string_view vertexSrc, std::string_view fragmentSrc) = 0;

    // ── 그리기 ──
    virtual void drawIndexed(core::u32 indexCount) = 0;
    virtual void drawArrays(core::u32 vertexCount) = 0;

    // ── VAO 관련 ──
    virtual core::u32 createVertexArray() = 0;
    virtual void bindVertexArray(core::u32 vao) = 0;
    virtual void setVertexLayout(const VertexLayout& layout) = 0;
};

// ── 팩토리 ──
std::unique_ptr<Renderer> createRenderer();  // 플랫폼에 맞는 구현체 반환

} // namespace gazeshot::renderer
```

**C++ 학습 포인트: `std::unique_ptr`을 반환하는 팩토리**

```cpp
// 호출 측
auto vbo = renderer->createVertexBuffer(data, size, BufferUsage::Static);
// vbo의 타입: std::unique_ptr<VertexBuffer>
// 실제 객체: GLVertexBuffer (OpenGL 구현체)
// vbo가 스코프를 벗어나면 → ~GLVertexBuffer() → glDeleteBuffers()
```

`std::unique_ptr`이 소유권을 가지고 있으므로:

- 복사 불가 → 이중 해제 방지
- 스코프 종료 시 자동 해제 → 리소스 누수 방지
- `std::move`로 소유권 이전 가능

### Step 3: OpenGL 백엔드

```cpp
// renderer/src/opengl/GLRenderer.cpp  (핵심 부분만 발췌)

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

namespace gazeshot::renderer {

class GLVertexBuffer : public VertexBuffer {
public:
    GLVertexBuffer(const void* data, core::u32 size, BufferUsage usage) {
        glGenBuffers(1, &id_);
        glBindBuffer(GL_ARRAY_BUFFER, id_);
        GLenum glUsage = (usage == BufferUsage::Static)  ? GL_STATIC_DRAW
                       : (usage == BufferUsage::Dynamic) ? GL_DYNAMIC_DRAW
                       : GL_STREAM_DRAW;
        glBufferData(GL_ARRAY_BUFFER, size, data, glUsage);
    }
    ~GLVertexBuffer() override { if (id_) glDeleteBuffers(1, &id_); }

    // 이동
    GLVertexBuffer(GLVertexBuffer&& o) noexcept : id_(std::exchange(o.id_, 0)) {}
    GLVertexBuffer& operator=(GLVertexBuffer&&) noexcept;

    // 복사 금지
    GLVertexBuffer(const GLVertexBuffer&) = delete;
    GLVertexBuffer& operator=(const GLVertexBuffer&) = delete;

    void bind() const override { glBindBuffer(GL_ARRAY_BUFFER, id_); }
    void unbind() const override { glBindBuffer(GL_ARRAY_BUFFER, 0); }
    void updateData(const void* data, core::u32 size) override {
        glBindBuffer(GL_ARRAY_BUFFER, id_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }
private:
    core::u32 id_ = 0;
};

class GLIndexBuffer : public IndexBuffer {
public:
    GLIndexBuffer(const core::u32* data, core::u32 count) : count_(count) {
        glGenBuffers(1, &id_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     count * sizeof(core::u32), data, GL_STATIC_DRAW);
    }
    ~GLIndexBuffer() override { if (id_) glDeleteBuffers(1, &id_); }

    GLIndexBuffer(GLIndexBuffer&& o) noexcept
        : id_(std::exchange(o.id_, 0)), count_(std::exchange(o.count_, 0)) {}
    GLIndexBuffer& operator=(GLIndexBuffer&& o) noexcept {
        if (this != &o) {
            if (id_) glDeleteBuffers(1, &id_);
            id_ = std::exchange(o.id_, 0);
            count_ = std::exchange(o.count_, 0);
        }
        return *this;
    }

    GLIndexBuffer(const GLIndexBuffer&) = delete;
    GLIndexBuffer& operator=(const GLIndexBuffer&) = delete;

    void bind() const override { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_); }
    void unbind() const override { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }
    core::u32 count() const override { return count_; }
private:
    core::u32 id_ = 0;
    core::u32 count_ = 0;
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
    ~GLShaderProgram() override { if (id_) glDeleteProgram(id_); }

    GLShaderProgram(GLShaderProgram&& o) noexcept
        : id_(std::exchange(o.id_, 0)) {}
    GLShaderProgram& operator=(GLShaderProgram&& o) noexcept {
        if (this != &o) {
            if (id_) glDeleteProgram(id_);
            id_ = std::exchange(o.id_, 0);
        }
        return *this;
    }

    GLShaderProgram(const GLShaderProgram&) = delete;
    GLShaderProgram& operator=(const GLShaderProgram&) = delete;

    void bind() const override { glUseProgram(id_); }
    void unbind() const override { glUseProgram(0); }

    void setInt(std::string_view name, core::i32 value) override {
        glUniform1i(loc(name), value);
    }
    void setFloat(std::string_view name, core::f32 value) override {
        glUniform1f(loc(name), value);
    }
    void setVec3(std::string_view name, const core::math::Vec3f& v) override {
        glUniform3f(loc(name), v.x, v.y, v.z);
    }
    void setVec4(std::string_view name, const core::math::Vec4f& v) override {
        glUniform4f(loc(name), v.x, v.y, v.z, v.w);
    }
    void setMat4(std::string_view name, const core::math::Mat4f& m) override {
        glUniformMatrix4fv(loc(name), 1, GL_TRUE, m.data());
    }

private:
    core::u32 id_ = 0;

    core::i32 loc(std::string_view name) const {
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

    static core::u32 compile(GLenum type, const std::string& src) {
        core::u32 shader = glCreateShader(type);
        const char* ptr = src.c_str();
        glShaderSource(shader, 1, &ptr, nullptr);
        glCompileShader(shader);
        return shader;
    }
};

class GLRenderer : public Renderer {
public:
    void init() override {
        // 현재는 비어있다 — depth test 등은 게임 코드에서 명시적으로 호출.
        // 이후 챕터에서 blending, face culling 등 기본 GL 상태를 여기서 세팅한다.
    }
    void clear(const core::math::Vec4f& color) override {
        glClearColor(color.x, color.y, color.z, color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    void setDepthTest(bool enabled) override {
        if (enabled) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
    }
    void setViewport(core::u32 x, core::u32 y, core::u32 w, core::u32 h) override {
        glViewport(x, y, w, h);
    }

    std::unique_ptr<VertexBuffer> createVertexBuffer(
        const void* data, core::u32 size, BufferUsage usage) override {
        return std::make_unique<GLVertexBuffer>(data, size, usage);
    }

    std::unique_ptr<IndexBuffer> createIndexBuffer(
        const core::u32* data, core::u32 count) override {
        return std::make_unique<GLIndexBuffer>(data, count);
    }

    std::unique_ptr<ShaderProgram> createShaderProgram(
        std::string_view vertexSrc, std::string_view fragmentSrc) override {
        return std::make_unique<GLShaderProgram>(vertexSrc, fragmentSrc);
    }

    void drawIndexed(core::u32 indexCount) override {
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    }
    void drawArrays(core::u32 vertexCount) override {
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }

    core::u32 createVertexArray() override {
        core::u32 vao = 0;
        glGenVertexArrays(1, &vao);
        return vao;
    }
    void bindVertexArray(core::u32 vao) override {
        glBindVertexArray(vao);
    }
    void setVertexLayout(const VertexLayout& layout) override {
        for (core::u32 i = 0; i < layout.attribs().size(); ++i) {
            auto& attr = layout.attribs()[i];
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

// 팩토리 함수
std::unique_ptr<Renderer> createRenderer() {
    return std::make_unique<GLRenderer>();
}

} // namespace gazeshot::renderer
```

### Step 4: CMake 변경

renderer가 이제 OpenGL 소스 코드를 가지므로, INTERFACE → STATIC 라이브러리로 변경한다.
OpenGL 링크도 game에서 renderer로 옮긴다:

```cmake
# renderer/CMakeLists.txt

add_library(gazeshot_renderer STATIC
    src/opengl/GLRenderer.cpp
)

target_include_directories(gazeshot_renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gazeshot_renderer
    PUBLIC gazeshot_core
    PRIVATE SDL3::Headers
)

if (NOT EMSCRIPTEN)
    find_package(OpenGL REQUIRED)
    target_link_libraries(gazeshot_renderer PRIVATE OpenGL::GL)
endif()
```

```cmake
# game/CMakeLists.txt — OpenGL 직접 링크 제거
# (renderer가 OpenGL을 내부적으로 링크하므로 game은 몰라도 됨)

target_link_libraries(gazeshot_game PRIVATE
    gazeshot_platform
    gazeshot_engine
    SDL3::Headers
)
# find_package(OpenGL) 제거됨!
```

### Step 5: Game 코드 변환 (Before / After)

위 `GLShaderProgram::processSource()`가 GLSL 버전을 자동으로 붙여주므로,
게임 코드에서는 `#version` 없이 셰이더를 작성하면 된다:

```cpp
// game/src/main.cpp  (Ch.04 — OpenGL 호출 제거)

#include <gazeshot/platform/Window.hpp>
#include <gazeshot/renderer/Renderer.hpp>
#include <gazeshot/core/math/Math.hpp>

using namespace gazeshot;
using namespace core::math;
using namespace core::math::literals;

const char* VERT_SRC = R"(
layout(location = 0) in vec3 aPos;
uniform mat4 uTransform;
void main() { gl_Position = uTransform * vec4(aPos, 1.0); }
)";

const char* FRAG_SRC = R"(
out vec4 FragColor;
void main() { FragColor = vec4(0.95, 0.55, 0.15, 1.0); }
)";

struct App {
    platform::Window window;
    std::unique_ptr<renderer::Renderer> renderer;
    std::unique_ptr<renderer::ShaderProgram> shader;
    std::unique_ptr<renderer::VertexBuffer> vbo;
    std::unique_ptr<renderer::IndexBuffer> ibo;
    core::u32 vao = 0;
    float time = 0.0f;
};

void init(App& app) {
    app.renderer = renderer::createRenderer();
    app.renderer->init();
    app.renderer->setDepthTest(true);

    // 셰이더
    app.shader = app.renderer->createShaderProgram(VERT_SRC, FRAG_SRC);

    // 큐브 정점
    float vertices[] = { /* ... 같은 큐브 데이터 ... */ };
    core::u32 indices[] = { /* ... */ };

    app.vao = app.renderer->createVertexArray();
    app.renderer->bindVertexArray(app.vao);

    app.vbo = app.renderer->createVertexBuffer(
        vertices, sizeof(vertices), renderer::BufferUsage::Static);

    app.ibo = app.renderer->createIndexBuffer(indices, 36);

    app.renderer->setVertexLayout({
        { "aPos", renderer::AttribType::Float3 }
    });
}

void oneFrame(void* arg) {
    auto* app = static_cast<App*>(arg);
    app->window.pollEvents();
    if (app->window.shouldClose()) { /* ... */ return; }

    app->time += 1.0f / 60.0f;

    // ── 이 코드에 gl* 호출이 전혀 없다! ──
    app->renderer->clear({0.12f, 0.12f, 0.15f, 1.0f});
    app->renderer->setViewport(0, 0, app->window.width(), app->window.height());

    Mat4f model = rotateY(app->time) * rotateX(app->time * 0.7f);
    Mat4f view = lookAt(Vec3f{0,0,3}, Vec3f{0,0,0}, Vec3f{0,1,0});
    float aspect = (float)app->window.width() / (float)app->window.height();
    Mat4f mvp = perspective(45.0_deg, aspect, 0.1f, 100.0f) * view * model;

    app->shader->bind();
    app->shader->setMat4("uTransform", mvp);
    app->renderer->bindVertexArray(app->vao);
    app->renderer->drawIndexed(36);

    app->window.swapBuffers();
}
```

**핵심 변화**:

- `#include <SDL3/SDL_opengl.h>` 사라짐
- `glClearColor`, `glDrawElements` 등 직접 호출 사라짐
- GPU 리소스가 `unique_ptr`로 관리 → 소멸자에서 자동 해제
- 셰이더에 `#version` 없음 → 렌더러가 플랫폼에 맞게 자동 추가

---

## 4. 검증 체크리스트

| 항목             | 확인 방법                              |
| ---------------- | -------------------------------------- |
| 동일한 큐브      | Ch.03과 같은 회전 큐브가 보인다        |
| gl\* 호출 없음   | main.cpp에서 `gl` 검색 시 0건          |
| WASM 동작        | 브라우저에서도 동일                    |
| 리소스 해제      | 종료 시 GL 에러 없음                   |
| 셰이더 자동 버전 | GLSL에 `#version` 작성하지 않아도 동작 |

---

## 5. 블로그 데모 아이디어

1. **Before / After 코드 비교**: main.cpp 줄 수 감소, `gl*` 호출 제거
2. **아키텍처 다이어그램**: Renderer interface → GLRenderer 관계
3. **RAII 시연**: "프로그램 종료 시 GPU 리소스가 자동 해제되는 로그"
4. **같은 큐브, 다른 코드**: 화면은 같지만 코드가 완전히 달라졌다

---

## 다음 챕터 예고

**Chapter 05: 윈도우, 입력, 게임 루프**

SDL3 이벤트를 `std::variant`로 래핑하고, 고정 시간 스텝 게임 루프를 구현한다.
데모: 키보드/마우스 입력으로 큐브의 회전 속도와 색상을 실시간 제어한다.
