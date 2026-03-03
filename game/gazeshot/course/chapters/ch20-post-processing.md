# Chapter 20: 포스트 프로세싱

## 데모 미리보기

```
┌──────────────────────────────────────────────────────┐
│                                                      │
│   Scene Render ──→ [FBO A] ──→ Vignette ──→ [FBO B] │
│                        │                       │     │
│                        │    ┌───────────────────┘     │
│                        │    ▼                         │
│                    [FBO B] ──→ DOF Blur ──→ [FBO A]  │
│                                                │     │
│                        ┌───────────────────────┘     │
│                        ▼                             │
│                    [FBO A] ──→ Color Grade ──→ Screen│
│                                                      │
│  ╭────────────╮   Shot → 화면 전체 백색 플래시         │
│  │  ◎  스코프  │   Hit  → 화면 흔들림(Shake)           │
│  │   ＋       │   Scope → 비네트 + DOF + 색조         │
│  ╰────────────╯                                      │
│                                                      │
│  핑-퐁 FBO로 이펙트 체인 구현                          │
└──────────────────────────────────────────────────────┘
```

- **데모**: 스코프 뷰에 비네트(가장자리 어둡게) + 피사계심도(가장자리 블러) + 색조 필터 적용
- **사격 시**: 화면 전체 백색 플래시 → 흔들림 → 원래로 복귀
- **핵심**: 씬을 FBO에 먼저 그리고, 풀스크린 쿼드에 이펙트 셰이더를 순차 적용
- Ch.09에서 fragment shader에 넣었던 스코프 오버레이를 포스트 프로세싱으로 분리

---

## 학습 목표

1. Framebuffer Object(FBO)를 생성하고 오프스크린 렌더링을 수행한다
2. 풀스크린 쿼드에 텍스처를 그리는 기법을 익힌다
3. 핑-퐁 FBO 기법으로 여러 이펙트를 체이닝한다
4. 비네트, 간이 DOF, 색조 보정, 화면 흔들림, 플래시 이펙트를 구현한다
5. `std::function`과 함수 합성(function composition) 패턴을 실습한다

---

## 1. 배경 지식

### Framebuffer Object (FBO)

지금까지 우리는 화면(기본 프레임버퍼)에 직접 그렸다.
포스트 프로세싱은 **중간 결과물**이 필요하다:

```
기존 렌더링:
  씬 렌더 ──→ [화면]  (바로 표시)

포스트 프로세싱:
  씬 렌더 ──→ [FBO 텍스처] ──→ 이펙트 적용 ──→ [화면]
               ↑ 오프스크린      ↑ 풀스크린 쿼드
```

FBO는 GPU가 그릴 수 있는 **렌더 타겟**이다:
- **Color Attachment**: 색상 데이터를 저장하는 텍스처
- **Depth Attachment**: 깊이 버퍼 (3D 씬 렌더링 시 필요)
- **Stencil Attachment**: 스텐실 테스트용 (이번 챕터에서는 생략)

### 렌더 패스 (Render Pass)

포스트 프로세싱은 **여러 단계**를 거친다. 각 단계를 렌더 패스라고 한다:

```
Pass 0: 씬 렌더   → FBO에 3D 씬을 그린다 (depth 포함)
Pass 1: 비네트     → FBO 텍스처를 읽고, 가장자리를 어둡게 하여 다른 FBO에 쓴다
Pass 2: DOF 블러  → 가장자리에 블러를 적용하여 또 다른 FBO에 쓴다
Pass 3: 색조 보정  → 색상 톤을 조정하여 최종 화면에 그린다
```

### 풀스크린 쿼드

각 포스트 프로세스 패스는 화면 전체를 덮는 사각형(쿼드)에 텍스처를 그리는 것이다:

```
(-1,1)──────(1,1)     UV:
  │  ╲          │     (0,1)──(1,1)
  │    ╲        │       │      │
  │      ╲      │       │      │
  │        ╲    │       │      │
  │          ╲  │     (0,0)──(1,0)
(-1,-1)─────(1,-1)
```

두 개의 삼각형으로 구성. NDC(-1~1) 좌표로 정의하면 투영 행렬이 필요 없다.

### 핑-퐁 FBO 기법

이펙트를 여러 개 적용할 때 FBO 2개를 번갈아 사용한다:

```
[FBO A] ──(이펙트1)──→ [FBO B]
[FBO B] ──(이펙트2)──→ [FBO A]
[FBO A] ──(이펙트3)──→ [화면]

읽기 소스와 쓰기 대상을 매번 swap → "핑-퐁"
```

FBO를 이펙트 수만큼 만들 필요 없이 **2개만**으로 무한 체이닝이 가능하다.

---

## 2. 구현 가이드

### Step 1: Framebuffer 클래스

Ch.04에서 만든 렌더러 추상화에 Framebuffer를 추가한다.

```hpp
// renderer/include/gazeshot/renderer/Framebuffer.hpp
#pragma once

#include <gazeshot/core/Types.hpp>

namespace gazeshot::renderer {

struct FramebufferSpec {
    core::u32 width  = 800;
    core::u32 height = 600;
    bool hasDepth    = true;
};

class Framebuffer {
public:
    virtual ~Framebuffer() = default;

    virtual void bind() = 0;           // 이 FBO에 렌더링 시작
    virtual void unbind() = 0;         // 기본 프레임버퍼로 복귀
    virtual void resize(core::u32 width, core::u32 height) = 0;

    virtual core::u32 colorTextureId() const = 0;  // 결과 텍스처
    virtual core::u32 width() const = 0;
    virtual core::u32 height() const = 0;
};

} // namespace gazeshot::renderer
```

### Step 2: OpenGL FBO 구현

```cpp
// renderer/src/opengl/GLFramebuffer.hpp
#pragma once

#include <gazeshot/renderer/Framebuffer.hpp>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <stdexcept>

namespace gazeshot::renderer {

class GLFramebuffer : public Framebuffer {
public:
    explicit GLFramebuffer(const FramebufferSpec& spec)
        : width_(spec.width), height_(spec.height), hasDepth_(spec.hasDepth)
    {
        create();
    }

    ~GLFramebuffer() override { destroy(); }

    // 이동만 허용
    GLFramebuffer(GLFramebuffer&& o) noexcept
        : fbo_(std::exchange(o.fbo_, 0))
        , colorTex_(std::exchange(o.colorTex_, 0))
        , depthRbo_(std::exchange(o.depthRbo_, 0))
        , width_(o.width_), height_(o.height_)
        , hasDepth_(o.hasDepth_) {}

    GLFramebuffer(const GLFramebuffer&) = delete;
    GLFramebuffer& operator=(const GLFramebuffer&) = delete;

    void bind() override {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width_, height_);
    }

    void unbind() override {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void resize(core::u32 width, core::u32 height) override {
        if (width == width_ && height == height_) return;
        width_ = width;
        height_ = height;
        destroy();
        create();
    }

    core::u32 colorTextureId() const override { return colorTex_; }
    core::u32 width() const override { return width_; }
    core::u32 height() const override { return height_; }

private:
    void create() {
        // FBO 생성
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

        // Color attachment (텍스처)
        glGenTextures(1, &colorTex_);
        glBindTexture(GL_TEXTURE_2D, colorTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, colorTex_, 0);

        // Depth attachment (렌더버퍼)
        if (hasDepth_) {
            glGenRenderbuffers(1, &depthRbo_);
            glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                                  width_, height_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER, depthRbo_);
        }

        // 완전성 검사
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Framebuffer is not complete");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void destroy() {
        if (colorTex_) { glDeleteTextures(1, &colorTex_); colorTex_ = 0; }
        if (depthRbo_) { glDeleteRenderbuffers(1, &depthRbo_); depthRbo_ = 0; }
        if (fbo_)      { glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
    }

    unsigned int fbo_      = 0;
    unsigned int colorTex_ = 0;
    unsigned int depthRbo_ = 0;
    core::u32 width_, height_;
    bool hasDepth_;
};

} // namespace gazeshot::renderer
```

**핵심 포인트**:
- `glGenFramebuffers` → FBO 생성
- `glFramebufferTexture2D` → 색상 텍스처를 attachment로 연결
- `glCheckFramebufferStatus` → FBO 완전성 확인 (빠뜨리면 디버깅 지옥)
- RAII: 소멸자에서 `glDeleteFramebuffers` 자동 호출

### Step 3: 풀스크린 쿼드

```hpp
// renderer/include/gazeshot/renderer/FullscreenQuad.hpp
#pragma once

#include <gazeshot/core/Types.hpp>

namespace gazeshot::renderer {

class FullscreenQuad {
public:
    void init() {
        // NDC 좌표 + UV
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,   // 좌상
            -1.0f, -1.0f,  0.0f, 0.0f,   // 좌하
             1.0f, -1.0f,  1.0f, 0.0f,   // 우하

            -1.0f,  1.0f,  0.0f, 1.0f,   // 좌상
             1.0f, -1.0f,  1.0f, 0.0f,   // 우하
             1.0f,  1.0f,  1.0f, 1.0f,   // 우상
        };

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices),
                     quadVertices, GL_STATIC_DRAW);

        // position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(float), (void*)0);
        // texCoord
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
    }

    void draw() const {
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    ~FullscreenQuad() {
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (vao_) glDeleteVertexArrays(1, &vao_);
    }

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
};

} // namespace gazeshot::renderer
```

### Step 4: 이펙트 셰이더들

각 포스트 프로세스 이펙트는 고유한 fragment shader를 갖는다.
vertex shader는 모든 이펙트가 동일하다.

```glsl
// shaders/post_vert.glsl
// 모든 포스트 프로세스가 공유하는 vertex shader

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform vec2 uOffset;  // 화면 흔들림용

void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos + uOffset, 0.0, 1.0);
}
```

```glsl
// shaders/vignette_frag.glsl
// 비네트: 화면 가장자리를 어둡게

out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D uScreen;
uniform float uRadius;     // 비네트 시작 반경 (0.0~1.0, 기본 0.4)
uniform float uSoftness;   // 부드러움 (0.0~1.0, 기본 0.5)

void main() {
    vec4 color = texture(uScreen, vTexCoord);

    // 화면 중심으로부터 거리 (0~1)
    vec2 center = vTexCoord - 0.5;
    float dist = length(center) * 2.0;  // 0~√2

    // 비네트 팩터: radius 안쪽은 1.0, 바깥으로 갈수록 0으로
    float vignette = smoothstep(uRadius + uSoftness, uRadius, dist);

    FragColor = vec4(color.rgb * vignette, 1.0);
}
```

```glsl
// shaders/dof_frag.glsl
// 간이 DOF: 가장자리에 블러 적용 (가우시안 5x5)

out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D uScreen;
uniform vec2 uTexelSize;   // 1.0 / 텍스처 크기
uniform float uFocusRadius;  // 포커스 영역 반경 (0.0~1.0)
uniform float uBlurStrength;

void main() {
    vec2 center = vTexCoord - 0.5;
    float dist = length(center) * 2.0;

    // 포커스 영역 안쪽은 블러 없음
    float blurAmount = smoothstep(uFocusRadius, uFocusRadius + 0.3, dist)
                     * uBlurStrength;

    if (blurAmount < 0.01) {
        FragColor = texture(uScreen, vTexCoord);
        return;
    }

    // 간이 가우시안 블러 (5x5 커널)
    vec3 result = vec3(0.0);
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216,
                                0.054054, 0.016216);

    // 수평 + 수직 결합 (분리 필터가 아닌 간소화 버전)
    result += texture(uScreen, vTexCoord).rgb * weights[0];

    for (int i = 1; i < 5; ++i) {
        vec2 offset = vec2(float(i)) * uTexelSize * blurAmount;
        result += texture(uScreen, vTexCoord + vec2(offset.x, 0.0)).rgb * weights[i];
        result += texture(uScreen, vTexCoord - vec2(offset.x, 0.0)).rgb * weights[i];
        result += texture(uScreen, vTexCoord + vec2(0.0, offset.y)).rgb * weights[i];
        result += texture(uScreen, vTexCoord - vec2(0.0, offset.y)).rgb * weights[i];
    }

    FragColor = vec4(result, 1.0);
}
```

```glsl
// shaders/colorgrade_frag.glsl
// 색조 보정: 스코프 특유의 색감 적용

out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D uScreen;
uniform vec3 uTint;        // 색조 (예: 따뜻한 앰버 = vec3(1.1, 1.0, 0.85))
uniform float uIntensity;  // 적용 강도 (0.0 = 없음, 1.0 = 100%)

void main() {
    vec3 color = texture(uScreen, vTexCoord).rgb;

    // 색조 적용
    vec3 graded = color * uTint;

    // 원본과 혼합
    FragColor = vec4(mix(color, graded, uIntensity), 1.0);
}
```

```glsl
// shaders/flash_frag.glsl
// 사격 시 화면 플래시

out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D uScreen;
uniform float uFlashAlpha;  // 0.0 = 플래시 없음, 1.0 = 완전 백색

void main() {
    vec3 color = texture(uScreen, vTexCoord).rgb;
    FragColor = vec4(mix(color, vec3(1.0), uFlashAlpha), 1.0);
}
```

### Step 5: 포스트 프로세싱 파이프라인

```hpp
// engine/include/gazeshot/engine/PostProcessPipeline.hpp
#pragma once

#include <gazeshot/renderer/Framebuffer.hpp>
#include <gazeshot/renderer/FullscreenQuad.hpp>
#include <gazeshot/renderer/ShaderProgram.hpp>
#include <gazeshot/core/Types.hpp>

#include <functional>
#include <vector>
#include <memory>
#include <utility>  // std::swap

namespace gazeshot::engine {

// ── 포스트 이펙트: FBO src를 읽어 FBO dst에 쓰는 함수 ──
using PostEffect = std::function<void(renderer::Framebuffer& src,
                                      renderer::Framebuffer& dst)>;

class PostProcessPipeline {
public:
    void init(core::u32 width, core::u32 height) {
        renderer::FramebufferSpec spec{width, height, true};

        // 씬 렌더용 (depth 필요)
        sceneFbo_ = std::make_unique<renderer::GLFramebuffer>(spec);

        // 핑-퐁 FBO (포스트 프로세스용, depth 불필요)
        spec.hasDepth = false;
        pingFbo_ = std::make_unique<renderer::GLFramebuffer>(spec);
        pongFbo_ = std::make_unique<renderer::GLFramebuffer>(spec);

        quad_.init();
    }

    void resize(core::u32 width, core::u32 height) {
        sceneFbo_->resize(width, height);
        pingFbo_->resize(width, height);
        pongFbo_->resize(width, height);
    }

    // ── 이펙트 등록 ──
    void addEffect(PostEffect effect) {
        effects_.push_back(std::move(effect));
    }

    void clearEffects() {
        effects_.clear();
    }

    // ── 씬 렌더링 시작: FBO에 바인딩 ──
    renderer::Framebuffer& beginScene() {
        sceneFbo_->bind();
        return *sceneFbo_;
    }

    // ── 씬 렌더링 끝 → 이펙트 체이닝 → 화면 출력 ──
    void endScene() {
        sceneFbo_->unbind();

        if (effects_.empty()) {
            // 이펙트가 없으면 씬 FBO를 바로 화면에 그린다
            drawToScreen(*sceneFbo_);
            return;
        }

        // 씬 FBO → ping으로 복사 (첫 이펙트 소스)
        blit(*sceneFbo_, *pingFbo_);

        // ── 핑-퐁 체이닝 ──
        auto* src = pingFbo_.get();
        auto* dst = pongFbo_.get();

        for (std::size_t i = 0; i < effects_.size(); ++i) {
            bool isLast = (i == effects_.size() - 1);

            if (isLast) {
                // 마지막 이펙트는 기본 프레임버퍼(화면)에 직접 그린다
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, src->width(), src->height());
                applyEffect(effects_[i], *src);
            } else {
                dst->bind();
                applyEffect(effects_[i], *src);
                dst->unbind();
                std::swap(src, dst);
            }
        }
    }

    const renderer::FullscreenQuad& quad() const { return quad_; }

private:
    void applyEffect(PostEffect& effect, renderer::Framebuffer& src) {
        glDisable(GL_DEPTH_TEST);
        effect(src, *pongFbo_);  // dst는 현재 바인딩된 타겟
        glEnable(GL_DEPTH_TEST);
    }

    void drawToScreen(renderer::Framebuffer& fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.colorTextureId());
        quad_.draw();
        glEnable(GL_DEPTH_TEST);
    }

    void blit(renderer::Framebuffer& src, renderer::Framebuffer& dst) {
        dst.bind();
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src.colorTextureId());
        quad_.draw();
        dst.unbind();
    }

    std::unique_ptr<renderer::GLFramebuffer> sceneFbo_;
    std::unique_ptr<renderer::GLFramebuffer> pingFbo_;
    std::unique_ptr<renderer::GLFramebuffer> pongFbo_;
    renderer::FullscreenQuad quad_;

    std::vector<PostEffect> effects_;
};

} // namespace gazeshot::engine
```

### Step 6: 스코프 이펙트 구성

```cpp
// game/src/PostEffects.hpp
#pragma once

#include <gazeshot/engine/PostProcessPipeline.hpp>
#include <gazeshot/renderer/ShaderProgram.hpp>
#include <gazeshot/renderer/FullscreenQuad.hpp>
#include <gazeshot/core/Types.hpp>

#include <cmath>

namespace gazeshot::game {

// ── 화면 흔들림 상태 ──
struct ScreenShake {
    core::f32 trauma = 0.0f;       // 0~1, 서서히 감소
    core::f32 decayRate = 3.0f;

    void trigger(core::f32 amount) {
        trauma = std::min(trauma + amount, 1.0f);
    }

    void update(core::f32 dt) {
        trauma = std::max(0.0f, trauma - decayRate * dt);
    }

    // 흔들림 강도 (trauma²로 비선형)
    core::f32 intensity() const { return trauma * trauma; }

    core::f32 offsetX(core::f32 time) const {
        return intensity() * 0.02f * std::sin(time * 37.0f);
    }

    core::f32 offsetY(core::f32 time) const {
        return intensity() * 0.015f * std::cos(time * 53.0f);
    }
};

// ── 플래시 상태 ──
struct ScreenFlash {
    core::f32 alpha = 0.0f;
    core::f32 decayRate = 8.0f;

    void trigger() { alpha = 1.0f; }

    void update(core::f32 dt) {
        alpha = std::max(0.0f, alpha - decayRate * dt);
    }
};

// ── 이펙트 함수 생성 ──
inline engine::PostEffect makeVignette(
    renderer::ShaderProgram& shader,
    const renderer::FullscreenQuad& quad)
{
    return [&shader, &quad](renderer::Framebuffer& src,
                            renderer::Framebuffer& /*dst*/) {
        shader.bind();
        shader.setInt("uScreen", 0);
        shader.setFloat("uRadius", 0.4f);
        shader.setFloat("uSoftness", 0.5f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src.colorTextureId());
        quad.draw();
    };
}

inline engine::PostEffect makeDOF(
    renderer::ShaderProgram& shader,
    const renderer::FullscreenQuad& quad,
    core::u32 width, core::u32 height)
{
    return [&shader, &quad, width, height](
        renderer::Framebuffer& src, renderer::Framebuffer& /*dst*/) {

        shader.bind();
        shader.setInt("uScreen", 0);
        shader.setFloat("uFocusRadius", 0.3f);
        shader.setFloat("uBlurStrength", 3.0f);
        shader.setVec2("uTexelSize",
            {1.0f / static_cast<core::f32>(width),
             1.0f / static_cast<core::f32>(height)});

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src.colorTextureId());
        quad.draw();
    };
}

inline engine::PostEffect makeColorGrade(
    renderer::ShaderProgram& shader,
    const renderer::FullscreenQuad& quad)
{
    return [&shader, &quad](renderer::Framebuffer& src,
                            renderer::Framebuffer& /*dst*/) {
        shader.bind();
        shader.setInt("uScreen", 0);
        // 따뜻한 앰버 색조 (스코프 특유의 느낌)
        shader.setVec3("uTint", {1.05f, 0.98f, 0.85f});
        shader.setFloat("uIntensity", 0.6f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src.colorTextureId());
        quad.draw();
    };
}

inline engine::PostEffect makeFlash(
    renderer::ShaderProgram& shader,
    const renderer::FullscreenQuad& quad,
    const ScreenFlash& flash)
{
    return [&shader, &quad, &flash](renderer::Framebuffer& src,
                                     renderer::Framebuffer& /*dst*/) {
        if (flash.alpha < 0.001f) {
            // 플래시가 없으면 패스스루
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, src.colorTextureId());
            // 기본 복사 셰이더로 대체하거나 그냥 그린다
        }
        shader.bind();
        shader.setInt("uScreen", 0);
        shader.setFloat("uFlashAlpha", flash.alpha);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src.colorTextureId());
        quad.draw();
    };
}

} // namespace gazeshot::game
```

### Step 7: 통합 — 게임 루프에서 사용

```cpp
// game/src/main.cpp (Ch.20 — 포스트 프로세싱 통합)

#include <gazeshot/engine/PostProcessPipeline.hpp>
#include "PostEffects.hpp"

struct App {
    // ... 기존 멤버들 ...
    engine::PostProcessPipeline postProcess;

    // 이펙트 셰이더들
    std::unique_ptr<renderer::ShaderProgram> vignetteShader;
    std::unique_ptr<renderer::ShaderProgram> dofShader;
    std::unique_ptr<renderer::ShaderProgram> colorGradeShader;
    std::unique_ptr<renderer::ShaderProgram> flashShader;

    game::ScreenShake shake;
    game::ScreenFlash flash;
    core::f32 time = 0.0f;
};

void init(App& app) {
    // ... 기존 초기화 ...

    core::u32 w = app.window.width();
    core::u32 h = app.window.height();

    // 포스트 프로세싱 초기화
    app.postProcess.init(w, h);

    // 이펙트 셰이더 로드
    app.vignetteShader = app.renderer->createShaderProgram(
        POST_VERT_SRC, VIGNETTE_FRAG_SRC);
    app.dofShader = app.renderer->createShaderProgram(
        POST_VERT_SRC, DOF_FRAG_SRC);
    app.colorGradeShader = app.renderer->createShaderProgram(
        POST_VERT_SRC, COLORGRADE_FRAG_SRC);
    app.flashShader = app.renderer->createShaderProgram(
        POST_VERT_SRC, FLASH_FRAG_SRC);

    // ── 이펙트 파이프라인 구성 ──
    auto& quad = app.postProcess.quad();

    app.postProcess.addEffect(
        game::makeVignette(*app.vignetteShader, quad));
    app.postProcess.addEffect(
        game::makeDOF(*app.dofShader, quad, w, h));
    app.postProcess.addEffect(
        game::makeColorGrade(*app.colorGradeShader, quad));
    app.postProcess.addEffect(
        game::makeFlash(*app.flashShader, quad, app.flash));
}

void update(App& app, core::f32 dt) {
    app.time += dt;

    // 사격 처리
    if (app.input.isMousePressed(MouseButton::Left)) {
        shoot(app);
        app.shake.trigger(0.4f);   // 화면 흔들림
        app.flash.trigger();        // 백색 플래시
    }

    app.shake.update(dt);
    app.flash.update(dt);
}

void render(App& app) {
    // ── Pass 0: 씬을 FBO에 렌더링 ──
    auto& sceneFbo = app.postProcess.beginScene();

    app.renderer->clear({0.05f, 0.05f, 0.08f, 1.0f});

    // 화면 흔들림을 뷰 행렬에 적용
    core::f32 shakeX = app.shake.offsetX(app.time);
    core::f32 shakeY = app.shake.offsetY(app.time);

    auto view = app.camera.viewMatrix();
    // 흔들림은 post vertex shader의 uOffset으로도 적용 가능
    app.vignetteShader->bind();
    app.vignetteShader->setVec2("uOffset", {shakeX, shakeY});

    // 씬 렌더링 (기존 코드와 동일)
    app.scene.render(*app.renderer, *app.sceneShader, view, proj, viewPos);

    // ── Pass 1~N: 포스트 프로세싱 체이닝 ──
    app.postProcess.endScene();

    app.window.swapBuffers();
}
```

---

## 3. C++ 학습 포인트

### 함수 합성 (Function Composition)

포스트 프로세싱 파이프라인은 **함수 합성** 패턴의 전형이다.
각 이펙트는 "텍스처를 받아 텍스처를 출력하는 함수"이며, 이를 순차 적용한다.

```cpp
// 수학에서의 함수 합성: (f ∘ g ∘ h)(x) = f(g(h(x)))
// 우리 파이프라인: colorGrade(dof(vignette(scene)))

using PostEffect = std::function<void(Framebuffer& src, Framebuffer& dst)>;

std::vector<PostEffect> postPipeline = {
    vignette,       // h: 가장자리 어둡게
    dofBlur,        // g: 가장자리 블러
    colorGrade,     // f: 색조 보정
};

// 핑-퐁으로 순차 적용
auto* ping = &fboA;
auto* pong = &fboB;

for (auto& effect : postPipeline) {
    pong->bind();
    effect(*ping, *pong);
    pong->unbind();
    std::swap(ping, pong);  // 읽기/쓰기 교환
}
```

핵심:
- 각 이펙트는 **독립적**이다 (서로의 내부를 모른다)
- 순서를 바꾸면 **결과가 달라진다** (합성은 비교환적)
- 이펙트 추가/제거가 `push_back`/`erase` 한 줄

### `std::function`과 런타임 이펙트 체인

```cpp
// std::function은 호출 가능한 모든 것을 담는 타입 소거 래퍼
using PostEffect = std::function<void(Framebuffer& src, Framebuffer& dst)>;

// 람다, 함수 포인터, 함수 객체 모두 담을 수 있다
PostEffect vignetteEffect = [&](Framebuffer& src, Framebuffer& dst) {
    vignetteShader.bind();
    // ...
};

// 런타임에 이펙트 체인을 변경할 수 있다
if (isScoped) {
    pipeline.addEffect(vignetteEffect);
    pipeline.addEffect(dofEffect);
}
if (justFired) {
    pipeline.addEffect(flashEffect);
}
```

`std::function`의 트레이드오프:
- **장점**: 어떤 callable이든 담을 수 있어 유연하다
- **단점**: 힙 할당 가능성, 가상 호출 오버헤드
- **대안**: 이펙트 수가 고정이면 `std::variant` + `std::visit`

### 렌더 패스를 함수 합성으로 보기

```
렌더링 전체를 함수 합성으로 모델링:

Pipeline = screenOutput ∘ flash ∘ colorGrade ∘ dof ∘ vignette ∘ sceneRender

각 단계의 시그니처:
  sceneRender : ()           → Texture
  vignette    : Texture      → Texture
  dof         : Texture      → Texture
  colorGrade  : Texture      → Texture
  flash       : Texture      → Texture
  screenOutput: Texture      → ()

이는 Unix 파이프와 동일한 구조:
  scene | vignette | dof | colorgrade | flash | screen
```

이 패턴은 게임 엔진뿐 아니라 이미지 처리, 데이터 파이프라인, 미들웨어 체인 등
소프트웨어 전반에서 널리 사용된다.

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| FBO 렌더링 | 씬이 정상적으로 보인다 (FBO → 화면 경유) |
| 비네트 | 화면 가장자리가 자연스럽게 어두워진다 |
| DOF 블러 | 스코프 가장자리가 살짝 흐려진다 |
| 색조 보정 | 전체적으로 따뜻한 앰버 톤이 느껴진다 |
| 사격 플래시 | 클릭 시 화면이 백색으로 번쩍 → 빠르게 복귀 |
| 화면 흔들림 | 사격 시 화면이 미세하게 흔들린다 |
| 이펙트 순서 | 이펙트 순서를 바꿔보면 결과가 달라진다 |
| 창 리사이즈 | 창 크기 변경 시 FBO가 올바르게 재생성된다 |
| Ch.09 비교 | fragment shader의 스코프 코드를 제거해도 동일한 결과 |

---

## 블로그 데모 아이디어

1. **이펙트 ON/OFF 비교 GIF**: 비네트·DOF·색조를 하나씩 켜면서 변화 보여주기
2. **핑-퐁 다이어그램**: FBO A↔B가 번갈아 사용되는 과정 시각화
3. **사격 연출 GIF**: 클릭 → 플래시 → 흔들림 → 복귀 시퀀스
4. **셰이더 코드 해설**: 비네트 수식(`smoothstep`)을 그래프와 함께 설명
5. **Before / After**: Ch.09의 fragment shader 기반 vs Ch.20의 FBO 기반 비교

---

## 다음 챕터 예고

**Chapter 21: 환경 렌더링**

스카이박스(cubemap)와 바닥 그리드를 추가하여 사격장에 공간감을 부여한다.
데모: 하늘이 보이고, 바닥에 격자 패턴이 깔린 사격장에서 스코프로 조준한다.
