# Chapter 14: HUD와 스코프 오버레이

## 데모 미리보기

```
┌──────────────────────────────────────────────────────┐
│                                                      │
│           ╭──────────────────╮                        │
│        ╭──│                  │──╮                     │
│       │░░░│     3D Scene     │░░░│  ← 비네트 (어두움) │
│       │░░░│                  │░░░│                    │
│       │░░░│        ＋        │░░░│  ← 레티클          │
│       │░░░│                  │░░░│                    │
│        ╰──│                  │──╯                     │
│           ╰──────────────────╯                        │
│                                                      │
│  Ammo: 12/20    Score: 0350    Targets: 5/9          │
│                                   ← 2D HUD 텍스트    │
└──────────────────────────────────────────────────────┘
```

- **데모**: 스코프 원형 마스크 + 비네트 + 레티클이 FBO 포스트프로세스로 렌더링
- **HUD**: 화면 하단에 잔탄, 점수, 타겟 피격 수가 비트맵 폰트로 표시
- **핵심**: Ch.09의 fragment shader 해킹 대신 FBO 기반 정석 파이프라인
- 블로그에 "2D/3D 파이프라인 분리" 아키텍처 다이어그램 포함 가능

---

## 학습 목표

1. FBO(Framebuffer Object) 기반 포스트프로세스 파이프라인을 구현한다
2. 직교 투영(orthographic projection)으로 2D HUD를 렌더링한다
3. 비트맵 폰트 텍스처 아틀라스로 텍스트를 화면에 그린다
4. 스코프 마스크, 비네트, 레티클을 풀스크린 쿼드에 합성한다
5. `std::string_view`, `std::format`, `std::span`을 실습한다

---

## 1. 배경 지식

### Ch.09의 문제점과 FBO 기반 해결

Ch.09에서 스코프 오버레이를 phong.frag에 직접 넣었다:

```glsl
// Ch.09 방식 — 모든 오브젝트의 fragment shader에서 스코프를 그림
void main() {
    vec3 color = ambient + diffuse + specular;
    vec2 screenUV = gl_FragCoord.xy / uScreenSize * 2.0 - 1.0;
    if (length(screenUV) > uScopeRadius) color *= 0.05;
}
```

문제점:
- **모든 오브젝트**의 셰이더에 스코프 로직이 들어감 (관심사 혼합)
- HUD 텍스트를 3D 셰이더 안에서 그릴 수 없음
- 포스트프로세스 효과(블룸, 색보정 등)를 추가할 수 없음

해결: 씬을 FBO에 렌더링하고, 별도 패스에서 스코프와 HUD를 합성한다.

```
[Pass 1: 3D 렌더링]               [Pass 2: 포스트프로세스]
┌──────────────┐                  ┌──────────────┐
│ 씬을 FBO에   │ colorTexture     │ 풀스크린 쿼드 │
│ 렌더링       │ ────────────→    │ + 스코프 마스크│
│ (depth ON)   │                  │ + 비네트/레티클│
└──────────────┘                  └──────────────┘
                                         │
                                  [Pass 3: HUD]
                                  ┌──────────────┐
                                  │ 직교 투영     │
                                  │ depth OFF     │
                                  │ 텍스트 오버레이│
                                  └──────────────┘
```

### 직교 투영과 비트맵 폰트

HUD는 orthographic 투영으로 픽셀 좌표계에서 렌더링한다:

```
ortho(0, screenWidth, 0, screenHeight, -1, 1)
→ (0,0)이 좌하단, (width, height)가 우상단, 픽셀 단위로 위치 지정
```

비트맵 폰트는 텍스처 아틀라스에 문자를 격자로 배치한다:

```
텍스처 아틀라스 (16x8 격자 = 128 문자):
┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
│ │!│"│#│$│%│&│'│(│)│*│+│,│-│.│/│  row 0
├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
│0│1│2│3│4│5│6│7│8│9│:│;│<│=│>│?│  row 1
├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
│@│A│B│C│D│E│F│G│H│I│J│K│L│M│N│O│  row 2
└───────────────────────────────────┘

'A' = ASCII 65 → col=(65-32)%16=1, row=(65-32)/16=2
UV = (col/16, row/8) ~ ((col+1)/16, (row+1)/8)
```

---

## 2. 구현 가이드

### Step 1: Framebuffer 추상화

```hpp
// engine/include/gazeshot/engine/Framebuffer.hpp

#pragma once
#include <gazeshot/core/Types.hpp>
#include <utility>  // std::exchange

namespace gazeshot::engine {

class Framebuffer {
public:
    Framebuffer(core::i32 width, core::i32 height)
        : width_(width), height_(height) {}

    ~Framebuffer() { destroy(); }

    // 이동만 허용, 복사 금지
    Framebuffer(Framebuffer&& o) noexcept
        : fbo_(std::exchange(o.fbo_, 0))
        , colorTexture_(std::exchange(o.colorTexture_, 0))
        , depthRbo_(std::exchange(o.depthRbo_, 0))
        , width_(o.width_), height_(o.height_) {}
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void create() {
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

        // 색상 텍스처 (씬 렌더링 결과를 저장)
        glGenTextures(1, &colorTexture_);
        glBindTexture(GL_TEXTURE_2D, colorTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, colorTexture_, 0);

        // 깊이 렌더버퍼
        glGenRenderbuffers(1, &depthRbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                              width_, height_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depthRbo_);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void bind() const   { glBindFramebuffer(GL_FRAMEBUFFER, fbo_); }
    void unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

    void bindColorTexture(core::u32 unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, colorTexture_);
    }

    core::i32 width() const  { return width_; }
    core::i32 height() const { return height_; }

private:
    void destroy() {
        if (colorTexture_) glDeleteTextures(1, &colorTexture_);
        if (depthRbo_)     glDeleteRenderbuffers(1, &depthRbo_);
        if (fbo_)          glDeleteFramebuffers(1, &fbo_);
    }

    unsigned int fbo_ = 0, colorTexture_ = 0, depthRbo_ = 0;
    core::i32 width_, height_;
};

} // namespace gazeshot::engine
```

### Step 2: 풀스크린 쿼드

```hpp
// engine/include/gazeshot/engine/FullscreenQuad.hpp

#pragma once
#include <gazeshot/renderer/Renderer.hpp>
#include <memory>

namespace gazeshot::engine {

class FullscreenQuad {
public:
    void create(renderer::Renderer& r) {
        float vertices[] = {
            // pos       uv
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
        };
        core::u32 indices[] = { 0, 1, 2, 0, 2, 3 };

        vao_ = r.createVertexArray();
        r.bindVertexArray(vao_);
        vbo_ = r.createVertexBuffer(vertices, sizeof(vertices),
                                    renderer::BufferUsage::Static);
        ibo_ = r.createIndexBuffer(indices, 6);
        r.setVertexLayout({
            {"aPosition", renderer::AttribType::Float2},
            {"aTexCoord", renderer::AttribType::Float2},
        });
    }

    void draw(renderer::Renderer& r) const {
        r.bindVertexArray(vao_);
        r.drawIndexed(6);
    }

private:
    core::u32 vao_ = 0;
    std::unique_ptr<renderer::VertexBuffer> vbo_;
    std::unique_ptr<renderer::IndexBuffer> ibo_;
};

} // namespace gazeshot::engine
```

### Step 3: 스코프 포스트프로세스 셰이더

```glsl
// assets/shaders/scope_post.vert

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
```

```glsl
// assets/shaders/scope_post.frag

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSceneTexture;
uniform vec2  uReticlePos;    // 레티클 위치 (NDC, -1~1)
uniform float uScopeRadius;   // 스코프 반지름 (0~1)
uniform float uAspectRatio;   // width / height

void main() {
    vec3 sceneColor = texture(uSceneTexture, vTexCoord).rgb;

    // UV → NDC, 종횡비 보정
    vec2 ndc = vTexCoord * 2.0 - 1.0;
    ndc.x *= uAspectRatio;

    float dist = length(ndc);

    // ── 스코프 밖: 검은색 ──
    if (dist > uScopeRadius) {
        FragColor = vec4(sceneColor * 0.03, 1.0);
        return;
    }

    // ── 비네트 (가장자리 어두움) ──
    float vignetteStart = uScopeRadius * 0.80;
    float vignette = 1.0;
    if (dist > vignetteStart) {
        float t = (dist - vignetteStart) / (uScopeRadius - vignetteStart);
        vignette = mix(1.0, 0.2, smoothstep(0.0, 1.0, t));
    }
    vec3 color = sceneColor * vignette;

    // ── 스코프 테두리 링 ──
    float ringDist = abs(dist - uScopeRadius);
    if (ringDist < 0.015) {
        color = mix(color, vec3(0.05), 1.0 - ringDist / 0.015);
    }

    // ── 레티클 (십자선 + 중심 갭) ──
    vec2 reticleNDC = uReticlePos;
    reticleNDC.x *= uAspectRatio;
    vec2 delta = ndc - reticleNDC;

    float thick = 0.002, len = 0.035, gap = 0.008;
    bool onV = abs(delta.x) < thick && abs(delta.y) < len && abs(delta.y) > gap;
    bool onH = abs(delta.y) < thick && abs(delta.x) < len && abs(delta.x) > gap;
    if (onV || onH) color = vec3(1.0, 0.15, 0.15);
    if (length(delta) < 0.003) color = vec3(1.0, 0.15, 0.15);

    FragColor = vec4(color, 1.0);
}
```

### Step 4: 비트맵 폰트 시스템

```hpp
// engine/include/gazeshot/engine/BitmapFont.hpp

#pragma once
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec2.hpp>

#include <string_view>
#include <span>
#include <vector>

namespace gazeshot::engine {

struct HudVertex {
    core::math::Vec2f position;
    core::math::Vec2f texCoord;
};

class BitmapFont {
public:
    struct Config {
        core::u32 gridCols = 16, gridRows = 8;
        core::u32 startChar = 32;
        core::f32 charWidth = 12.0f, charHeight = 20.0f;
    };

    explicit BitmapFont(const Config& config = {}) : config_(config) {}

    void loadTexture(unsigned int textureId) { fontTexture_ = textureId; }

    // std::string_view: 문자열을 복사 없이 참조
    void buildText(core::math::Vec2f pos, std::string_view text,
                   core::f32 scale = 1.0f) {
        vertices_.clear();
        indices_.clear();

        core::f32 cursorX = pos.x, cursorY = pos.y;
        core::f32 cw = config_.charWidth * scale;
        core::f32 ch = config_.charHeight * scale;
        core::f32 uvW = 1.0f / static_cast<core::f32>(config_.gridCols);
        core::f32 uvH = 1.0f / static_cast<core::f32>(config_.gridRows);

        for (char c : text) {
            if (c == '\n') { cursorX = pos.x; cursorY -= ch; continue; }

            core::u32 idx = static_cast<core::u32>(c) - config_.startChar;
            core::u32 col = idx % config_.gridCols;
            core::u32 row = idx / config_.gridCols;

            core::f32 u0 = static_cast<core::f32>(col) * uvW;
            core::f32 v0 = 1.0f - static_cast<core::f32>(row) * uvH;
            core::f32 u1 = u0 + uvW;
            core::f32 v1 = v0 - uvH;

            core::u32 base = static_cast<core::u32>(vertices_.size());
            vertices_.push_back({{cursorX,      cursorY     }, {u0, v1}});
            vertices_.push_back({{cursorX + cw, cursorY     }, {u1, v1}});
            vertices_.push_back({{cursorX + cw, cursorY + ch}, {u1, v0}});
            vertices_.push_back({{cursorX,      cursorY + ch}, {u0, v0}});
            indices_.insert(indices_.end(),
                {base, base+1, base+2, base, base+2, base+3});

            cursorX += cw;
        }
    }

    // std::span: 소유하지 않는 연속 메모리 뷰
    std::span<const HudVertex> vertices() const { return vertices_; }
    std::span<const core::u32> indices() const { return indices_; }
    unsigned int textureId() const { return fontTexture_; }

private:
    Config config_;
    unsigned int fontTexture_ = 0;
    std::vector<HudVertex> vertices_;
    std::vector<core::u32> indices_;
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: `std::string_view`**

```cpp
void buildText(core::math::Vec2f pos, std::string_view text, core::f32 scale = 1.0f);

// 이 모든 호출이 복사 없이 동작:
font.buildText({10, 10}, "Score: 100");       // 문자열 리터럴
font.buildText({10, 10}, someStdString);       // std::string
font.buildText({10, 10}, sv.substr(0, 5));     // 부분 문자열

// string_view 내부 = const char* + size_t (16바이트, 힙 할당 없음)
// 주의: 원본이 사라지면 dangling 참조. 함수 파라미터로는 안전.
```

**C++ 학습 포인트: `std::format` (C++20)**

```cpp
#include <format>

auto scoreText = std::format("Score: {:04d}", score);    // "Score: 0042"
auto timerText = std::format("Time: {:.1f}s", 45.3f);    // "Time: 45.3s"
auto ammoText  = std::format("Ammo: {:2d}/{:2d}", 12, 20); // "Ammo: 12/20"

// sprintf와 달리 타입 불일치 = 컴파일 에러 (안전!)
// sprintf(buf, "%d", 3.14f);  → 런타임 크래시 (UB)
// std::format("{:d}", 3.14f); → 컴파일 에러
```

### Step 5: HUD 렌더러

```hpp
// game/include/gazeshot/game/HudRenderer.hpp

#pragma once
#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/engine/BitmapFont.hpp>
#include <gazeshot/renderer/Renderer.hpp>

#include <format>
#include <string_view>
#include <memory>

namespace gazeshot::game {

struct HudState {
    core::u32 ammo = 20, maxAmmo = 20;
    core::u32 score = 0;
    core::u32 hitTargets = 0, totalTargets = 9;
    core::f32 remainingTime = 60.0f;
};

class HudRenderer {
public:
    void init(renderer::Renderer& r, core::i32 screenW, core::i32 screenH) {
        screenW_ = screenW;
        screenH_ = screenH;

        projection_ = core::math::ortho(
            0.0f, static_cast<core::f32>(screenW),
            0.0f, static_cast<core::f32>(screenH),
            -1.0f, 1.0f
        );

        hudShader_ = r.createShaderProgram(HUD_VERT_SRC, HUD_FRAG_SRC);

        // 동적 VBO (매 프레임 텍스트가 바뀜)
        hudVao_ = r.createVertexArray();
        r.bindVertexArray(hudVao_);
        hudVbo_ = r.createVertexBuffer(
            nullptr, 4096 * sizeof(engine::HudVertex),
            renderer::BufferUsage::Dynamic);
        r.setVertexLayout({
            {"aPosition", renderer::AttribType::Float2},
            {"aTexCoord", renderer::AttribType::Float2},
        });
    }

    void render(renderer::Renderer& r, const HudState& state) {
        r.setDepthTest(false);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        hudShader_->bind();
        hudShader_->setMat4("uProjection", projection_);
        hudShader_->setInt("uFontTexture", 0);

        core::f32 margin = 20.0f;
        core::f32 y = margin;
        core::f32 scale = 1.2f;

        drawText(r, {margin, y},
            std::format("Ammo: {:2d}/{:2d}", state.ammo, state.maxAmmo), scale);
        drawText(r, {margin + 220, y},
            std::format("Score: {:04d}", state.score), scale);
        drawText(r, {margin + 480, y},
            std::format("Targets: {}/{}", state.hitTargets, state.totalTargets),
            scale);
        drawText(r,
            {static_cast<core::f32>(screenW_) - 180.0f,
             static_cast<core::f32>(screenH_) - margin - 24.0f},
            std::format("Time: {:.1f}s", state.remainingTime), scale);

        glDisable(GL_BLEND);
        r.setDepthTest(true);
    }

private:
    // std::string_view를 받아 텍스트 렌더링
    void drawText(renderer::Renderer& r, core::math::Vec2f pos,
                  std::string_view text, core::f32 scale = 1.0f) {
        font_.buildText(pos, text, scale);

        // std::span으로 소유권 없이 데이터 전달
        auto verts = font_.vertices();
        if (verts.empty()) return;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, font_.textureId());

        r.bindVertexArray(hudVao_);
        hudVbo_->updateData(verts.data(),
            static_cast<core::u32>(verts.size_bytes()));
        r.drawIndexed(static_cast<core::u32>(font_.indices().size()));
    }

    static constexpr const char* HUD_VERT_SRC = R"(
        layout(location = 0) in vec2 aPosition;
        layout(location = 1) in vec2 aTexCoord;
        uniform mat4 uProjection;
        out vec2 vTexCoord;
        void main() {
            gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
            vTexCoord = aTexCoord;
        }
    )";

    static constexpr const char* HUD_FRAG_SRC = R"(
        in vec2 vTexCoord;
        out vec4 FragColor;
        uniform sampler2D uFontTexture;
        void main() {
            vec4 texColor = texture(uFontTexture, vTexCoord);
            if (texColor.a < 0.1) discard;
            FragColor = texColor;
        }
    )";

    core::i32 screenW_ = 0, screenH_ = 0;
    core::math::Mat4f projection_;
    engine::BitmapFont font_;
    std::unique_ptr<renderer::ShaderProgram> hudShader_;
    core::u32 hudVao_ = 0;
    std::unique_ptr<renderer::VertexBuffer> hudVbo_;
};

} // namespace gazeshot::game
```

**C++ 학습 포인트: `std::span`**

```cpp
auto verts = font.vertices();  // std::span<const HudVertex>
verts.data();                   // const HudVertex* (raw 포인터)
verts.size();                   // 개수
verts.size_bytes();             // 바이트 크기 = size() * sizeof(HudVertex)

// vector, array, C 배열 → span 자동 변환
void upload(std::span<const HudVertex> data);
upload(myVector);   // OK
upload(myCArray);   // OK
```

### Step 6: 전체 렌더 루프 통합

```cpp
// game/src/main.cpp (Ch.14)

struct App {
    platform::Window window;
    std::unique_ptr<renderer::Renderer> renderer;

    // 3D 씬
    std::unique_ptr<renderer::ShaderProgram> phongShader;
    engine::Scene scene;
    game::SniperCamera camera;

    // 포스트프로세스
    engine::Framebuffer sceneFbo{1280, 720};
    engine::FullscreenQuad fsQuad;
    std::unique_ptr<renderer::ShaderProgram> scopeShader;

    // HUD
    game::HudRenderer hud;
    game::HudState hudState;
};

void init(App& app) {
    app.renderer = renderer::createRenderer();
    app.renderer->init();
    app.sceneFbo.create();
    app.fsQuad.create(*app.renderer);
    app.scopeShader = app.renderer->createShaderProgram(
        SCOPE_POST_VERT_SRC, SCOPE_POST_FRAG_SRC);
    app.hud.init(*app.renderer, app.window.width(), app.window.height());
    // ... (씬, 카메라 등 기존 초기화)
}

void render(App& app) {
    auto& r = *app.renderer;
    auto& cam = app.camera;
    core::f32 aspect = static_cast<core::f32>(app.window.width())
                     / static_cast<core::f32>(app.window.height());

    // ═══════════════════════════════════
    // Pass 1: 3D 씬을 FBO에 렌더링
    // ═══════════════════════════════════
    app.sceneFbo.bind();
    r.setViewport(0, 0, app.sceneFbo.width(), app.sceneFbo.height());
    r.clear({0.05f, 0.05f, 0.08f, 1.0f});
    r.setDepthTest(true);

    Mat4f view = cam.viewMatrix();
    Mat4f proj = cam.projectionMatrix(aspect);
    Vec3f viewPos = cam.position()
        + Vec3f{cam.headOffset().x, cam.headOffset().y, 0.0f};
    app.scene.render(r, *app.phongShader, view, proj, viewPos);
    app.sceneFbo.unbind();

    // ═══════════════════════════════════
    // Pass 2: 스코프 포스트프로세스
    // ═══════════════════════════════════
    r.setViewport(0, 0, app.window.width(), app.window.height());
    r.clear({0.0f, 0.0f, 0.0f, 1.0f});
    r.setDepthTest(false);

    app.scopeShader->bind();
    app.sceneFbo.bindColorTexture(0);
    app.scopeShader->setInt("uSceneTexture", 0);
    app.scopeShader->setFloat("uScopeRadius", 0.85f);
    app.scopeShader->setFloat("uAspectRatio", aspect);
    app.scopeShader->setVec2("uReticlePos", cam.gazePoint());
    app.fsQuad.draw(r);

    // ═══════════════════════════════════
    // Pass 3: HUD (2D 오버레이)
    // ═══════════════════════════════════
    app.hud.render(r, app.hudState);

    app.window.swapBuffers();
}
```

### Step 7: phong.frag 정리

Ch.09에서 추가했던 스코프 코드를 제거한다:

```glsl
// phong.frag — Ch.14 이후 (라이팅만 담당, 깔끔!)

in vec3 vWorldPos;
in vec3 vNormal;
out vec4 FragColor;

uniform vec3 uLightPos, uLightColor, uViewPos;
uniform vec3 uAmbient, uDiffuse, uSpecular;
uniform float uShininess;

void main() {
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vWorldPos);
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 reflectDir = reflect(-lightDir, norm);

    vec3 ambient  = uAmbient;
    vec3 diffuse  = uDiffuse * max(dot(norm, lightDir), 0.0);
    vec3 specular = uSpecular * pow(max(dot(viewDir, reflectDir), 0.0), uShininess);

    FragColor = vec4((ambient + diffuse + specular) * uLightColor, 1.0);
    // ← Ch.09의 스코프 마스크, 비네트, 레티클 코드가 모두 사라짐!
}
```

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| FBO 동작 | 3D 씬이 화면에 정상 렌더링 (FBO 경유) |
| 스코프 마스크 | 원형 영역 밖이 어두움 |
| 비네트 | 스코프 가장자리가 부드럽게 어두워짐 |
| 원형 유지 | 창 크기를 변경해도 스코프가 타원이 되지 않음 |
| 레티클 | 마우스 이동 시 십자선이 스코프 내에서 이동 |
| HUD 텍스트 | 화면 하단에 Ammo, Score, Targets 표시 |
| 텍스트 업데이트 | 사격/피격 시 숫자가 실시간 변경 |
| phong.frag 정리 | 스코프 관련 코드가 없음 (grep으로 확인) |

---

## 블로그 데모 아이디어

1. **Before/After 비교**: Ch.09 phong.frag vs Ch.14 분리 후 코드 diff
2. **3-pass 파이프라인 다이어그램**: FBO → 포스트프로세스 → HUD 흐름도
3. **스코프 뷰 스크린샷**: 비네트 + 레티클 + HUD가 모두 보이는 화면
4. **비트맵 폰트 아틀라스**: 텍스처 이미지와 UV 계산 과정 시각화
5. **phong.frag diff**: "관심사 분리의 위력" — 셰이더가 얼마나 깔끔해졌는지

---

## 다음 챕터 예고

**Chapter 15: 게임 스테이트와 점수 시스템**

게임의 상태 전환(대기 → 플레이 → 결과)과 점수 계산 로직을 설계한다.
데모: 카운트다운 후 게임 시작, 타겟 피격 시 점수 누적, 시간 종료 시 결과 화면 표시.
