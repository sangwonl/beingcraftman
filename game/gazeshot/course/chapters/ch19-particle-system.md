# Chapter 19: 파티클 시스템

## 데모 미리보기

```
┌──────────────────────────────────────────────────┐
│                                                  │
│         ◎ target                                 │
│        ╱╲╱╲  ← 파편 (debris)                     │
│       · · ·   · ·                                │
│      · · ·  ·  ·  ← 먼지 (impact dust)           │
│                                                  │
│                          ╳ ← 총구                 │
│                        ✦✦✦ ← 머즐 플래시          │
│                        ✦✦                        │
│                                                  │
│  Click: 사격 → 머즐 플래시 + 트레이서              │
│  Hit obstacle: 충돌 지점에 먼지 파티클              │
│  Hit target: 타겟 파편 + 점수                      │
│  모든 파티클이 빌보딩으로 카메라를 향함              │
└──────────────────────────────────────────────────┘
```

- **데모**: 사격 시 머즐 플래시, 장애물 충돌 시 먼지, 타겟 피격 시 파편이 파티클로 표현
- **특징**: 오브젝트 풀로 동적 할당 없이 수백 개 파티클 처리
- 블로그에 "파티클 이펙트 before/after GIF" 포함 가능

---

## 학습 목표

1. Particle 구조체와 ParticleEmitter 클래스를 설계한다
2. 오브젝트 풀 패턴으로 프레임 당 동적 할당 없이 파티클을 관리한다
3. 빌보딩 기법으로 파티클이 항상 카메라를 향하게 한다
4. 알파 블렌딩과 가산 블렌딩의 차이를 이해하고 적용한다
5. `placement new`, `std::pmr`, `std::ranges`를 실습한다

---

## 1. 배경 지식

### 파티클 시스템이란?

```
파티클 시스템 = 대량의 작은 스프라이트로 시각 효과를 만드는 기법

  Emitter (발생기)
    │
    ├── emit(count, config)   → 파티클 N개 생성
    ├── update(dt)            → 위치/속도/수명 갱신
    └── render(camera)        → 살아있는 파티클만 그리기

  각 파티클의 생애:
    생성 → [위치 이동 + 크기/색상 변화] → 수명 종료 → 재활용
```

### 빌보딩 (Billboarding)

파티클은 2D 쿼드인데, 항상 카메라를 향해야 자연스럽다:

```
  View Matrix에서 right/up 벡터를 추출:
  ┌                  ┐
  │ Rx  Ry  Rz  ...  │  ← right 벡터 (row 0)
  │ Ux  Uy  Uz  ...  │  ← up 벡터 (row 1)
  │ Fx  Fy  Fz  ...  │  ← forward 벡터 (row 2)
  └                  ┘

  쿼드 정점 = center + right * offset.x + up * offset.y
  → 어떤 각도에서 봐도 파티클이 정면을 향한다
```

### 블렌딩 모드

```
Alpha Blending (반투명):   result = src * src.a + dst * (1 - src.a)
  → 연기, 먼지 등 반투명 효과

Additive Blending (가산):  result = src + dst
  → 불꽃, 머즐 플래시 등 빛나는 효과 (겹칠수록 밝아짐)
```

---

## 2. 구현 가이드

### Step 1: Particle 구조체

```hpp
// engine/include/gazeshot/engine/Particle.hpp
#pragma once
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>
#include <gazeshot/core/Types.hpp>

namespace gazeshot::engine {

struct Particle {
    core::math::Vec3f position{0, 0, 0};
    core::math::Vec3f velocity{0, 0, 0};
    core::math::Vec4f color{1, 1, 1, 1};
    core::f32 life      = 0.0f;       // 남은 수명 (초)
    core::f32 maxLife   = 1.0f;       // 초기 수명
    core::f32 size      = 0.1f;       // 쿼드 반지름
    core::f32 sizeDecay = 0.0f;       // 매 초 줄어드는 크기

    [[nodiscard]] bool isAlive() const { return life > 0.0f; }
    [[nodiscard]] core::f32 lifeFraction() const {
        return (maxLife > 0.0f) ? (life / maxLife) : 0.0f;
    }
};

} // namespace gazeshot::engine
```

### Step 2: ParticlePool (오브젝트 풀)

```hpp
// engine/include/gazeshot/engine/ParticlePool.hpp
#pragma once
#include <gazeshot/engine/Particle.hpp>
#include <vector>
#include <cassert>

namespace gazeshot::engine {

class ParticlePool {
public:
    explicit ParticlePool(size_t capacity)
        : pool_(capacity), activeCount_(0) {}

    // ── 파티클 생성: O(1) ──
    Particle* spawn() {
        if (activeCount_ >= pool_.size()) return nullptr;
        return &pool_[activeCount_++];
    }

    // ── 파티클 제거: O(1) — 마지막 활성 파티클과 swap ──
    void kill(size_t index) {
        assert(index < activeCount_);
        --activeCount_;
        if (index != activeCount_) {
            pool_[index] = pool_[activeCount_];
        }
    }

    Particle& operator[](size_t i)             { return pool_[i]; }
    const Particle& operator[](size_t i) const { return pool_[i]; }
    size_t activeCount() const { return activeCount_; }
    size_t capacity()    const { return pool_.size(); }
    Particle* activeBegin() { return pool_.data(); }

private:
    std::vector<Particle> pool_;   // 고정 크기, 사전 할당
    size_t activeCount_;
};

} // namespace gazeshot::engine
```

**핵심: swap-and-pop 전략**

```
[A][B][C][D][E]  activeCount=5    C가 죽으면?
1. C와 마지막(E) swap: [A][B][E][D][C]
2. activeCount--:       [A][B][E][D] | C    activeCount=4
→ 삭제 O(1), 순서 보존 불필요, 메모리 재할당 없음
```

### Step 3: EmitterConfig와 ParticleEmitter

```hpp
// engine/include/gazeshot/engine/ParticleEmitter.hpp
#pragma once
#include <gazeshot/engine/ParticlePool.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <random>

namespace gazeshot::engine {

struct EmitterConfig {
    core::math::Vec3f positionMin{0, 0, 0};
    core::math::Vec3f positionMax{0, 0, 0};
    core::math::Vec3f velocityMin{-1, -1, -1};
    core::math::Vec3f velocityMax{1, 1, 1};
    core::math::Vec4f colorStart{1, 1, 1, 1};
    core::math::Vec4f colorEnd{1, 1, 1, 0};    // 알파 0 = 페이드아웃
    core::f32 lifeMin    = 0.5f;
    core::f32 lifeMax    = 1.5f;
    core::f32 sizeStart  = 0.1f;
    core::f32 sizeEnd    = 0.02f;
    core::math::Vec3f gravity{0, -9.8f, 0};
};

class ParticleEmitter {
public:
    explicit ParticleEmitter(size_t poolCapacity = 512)
        : pool_(poolCapacity), rng_(std::random_device{}()) {}

    void emit(const EmitterConfig& config, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            Particle* p = pool_.spawn();
            if (!p) break;

            p->position = randomVec3(config.positionMin, config.positionMax);
            p->velocity = randomVec3(config.velocityMin, config.velocityMax);
            p->color    = config.colorStart;
            p->life     = randomFloat(config.lifeMin, config.lifeMax);
            p->maxLife  = p->life;
            p->size     = config.sizeStart;
            p->sizeDecay = (config.sizeStart - config.sizeEnd) / p->maxLife;
        }
        currentConfig_ = config;
    }

    void update(core::f32 dt) {
        size_t i = 0;
        while (i < pool_.activeCount()) {
            auto& p = pool_[i];
            p.life -= dt;

            if (p.life <= 0.0f) {
                pool_.kill(i);  // kill이 swap하므로 i 증가하지 않음
                continue;
            }

            p.velocity = p.velocity + currentConfig_.gravity * dt;
            p.position = p.position + p.velocity * dt;
            p.size -= p.sizeDecay * dt;
            if (p.size < 0.0f) p.size = 0.0f;

            core::f32 t = 1.0f - p.lifeFraction();
            p.color = core::math::lerp(
                currentConfig_.colorStart, currentConfig_.colorEnd, t);
            ++i;
        }
    }

    const ParticlePool& pool() const { return pool_; }

private:
    ParticlePool pool_;
    EmitterConfig currentConfig_;
    std::mt19937 rng_;

    core::f32 randomFloat(core::f32 a, core::f32 b) {
        return std::uniform_real_distribution<core::f32>(a, b)(rng_);
    }
    core::math::Vec3f randomVec3(const core::math::Vec3f& a,
                                  const core::math::Vec3f& b) {
        return {randomFloat(a.x, b.x), randomFloat(a.y, b.y),
                randomFloat(a.z, b.z)};
    }
};

} // namespace gazeshot::engine
```

### Step 4: 이펙트 프리셋

```cpp
// engine/include/gazeshot/engine/ParticleEffects.hpp
#pragma once
#include <gazeshot/engine/ParticleEmitter.hpp>

namespace gazeshot::engine {

// ── 머즐 플래시: 짧은 수명, 밝은 노란색, 가산 블렌딩 ──
inline EmitterConfig muzzleFlashConfig(const core::math::Vec3f& gunPos,
                                        const core::math::Vec3f& gunDir) {
    return {
        .positionMin = gunPos,               .positionMax = gunPos,
        .velocityMin = gunDir * 3.0f + core::math::Vec3f{-0.5f, -0.5f, -0.5f},
        .velocityMax = gunDir * 6.0f + core::math::Vec3f{ 0.5f,  0.5f,  0.5f},
        .colorStart  = {1.0f, 0.9f, 0.3f, 1.0f},   // 밝은 노란색
        .colorEnd    = {1.0f, 0.3f, 0.0f, 0.0f},   // 붉은색 → 페이드
        .lifeMin = 0.05f, .lifeMax = 0.15f,          // 매우 짧은 수명
        .sizeStart = 0.08f, .sizeEnd = 0.02f,
        .gravity = {0, 0, 0},                        // 중력 없음
    };
}

// ── 충돌 먼지: 법선 방향으로 퍼지는 흙색 입자 ──
inline EmitterConfig impactDustConfig(const core::math::Vec3f& hitPoint,
                                       const core::math::Vec3f& hitNormal) {
    return {
        .positionMin = hitPoint,             .positionMax = hitPoint,
        .velocityMin = hitNormal * 0.5f + core::math::Vec3f{-1, 0, -1},
        .velocityMax = hitNormal * 2.0f + core::math::Vec3f{ 1, 1.5f, 1},
        .colorStart  = {0.6f, 0.5f, 0.4f, 0.7f},
        .colorEnd    = {0.5f, 0.4f, 0.3f, 0.0f},
        .lifeMin = 0.3f, .lifeMax = 0.8f,
        .sizeStart = 0.06f, .sizeEnd = 0.15f,       // 먼지는 커지면서 사라짐
        .gravity = {0, -2.0f, 0},
    };
}

// ── 타겟 파편: 빨간 파편이 중력으로 낙하 ──
inline EmitterConfig targetDebrisConfig(const core::math::Vec3f& hitPoint,
                                         const core::math::Vec3f& hitNormal) {
    return {
        .positionMin = hitPoint,             .positionMax = hitPoint,
        .velocityMin = hitNormal * 1.0f + core::math::Vec3f{-2, 0, -2},
        .velocityMax = hitNormal * 4.0f + core::math::Vec3f{ 2, 3,  2},
        .colorStart  = {0.9f, 0.2f, 0.1f, 1.0f},
        .colorEnd    = {0.4f, 0.1f, 0.0f, 0.0f},
        .lifeMin = 0.4f, .lifeMax = 1.2f,
        .sizeStart = 0.04f, .sizeEnd = 0.01f,
        .gravity = {0, -9.8f, 0},                   // 강한 중력
    };
}

} // namespace gazeshot::engine
```

### Step 5: ParticleRenderer (빌보드 + 인스턴싱)

```hpp
// engine/include/gazeshot/engine/ParticleRenderer.hpp
#pragma once
#include <gazeshot/engine/ParticlePool.hpp>
#include <gazeshot/renderer/Renderer.hpp>
#include <gazeshot/renderer/ShaderProgram.hpp>
#include <gazeshot/core/math/Mat4.hpp>
#include <vector>

namespace gazeshot::engine {

class ParticleRenderer {
public:
    void init(renderer::Renderer& r) {
        float quadVerts[] = {  // position(3) + texcoord(2)
            -0.5f,-0.5f,0, 0,0,  0.5f,-0.5f,0, 1,0,
             0.5f, 0.5f,0, 1,1, -0.5f, 0.5f,0, 0,1,
        };
        core::u32 idx[] = {0,1,2, 2,3,0};

        vao_ = r.createVertexArray();
        r.bindVertexArray(vao_);
        quadVBO_ = r.createVertexBuffer(quadVerts, sizeof(quadVerts),
                                         renderer::BufferUsage::Static);
        quadIBO_ = r.createIndexBuffer(idx, 6);
        r.setVertexLayout({{"aOffset", renderer::AttribType::Float3},
                           {"aTexCoord", renderer::AttribType::Float2}});

        // 인스턴스 버퍼: pos(3)+color(4)+size(1) = 8 floats per particle
        instanceVBO_ = r.createVertexBuffer(nullptr, MAX_PARTICLES * 32,
                                             renderer::BufferUsage::Stream);
    }

    void render(renderer::Renderer& r, renderer::ShaderProgram& shader,
                const ParticlePool& pool, const core::math::Mat4f& view,
                const core::math::Mat4f& proj, bool additiveBlend = false) {
        if (pool.activeCount() == 0) return;

        // View Matrix에서 카메라 right/up 추출 (빌보드 핵심)
        core::math::Vec3f camR{view[0][0], view[1][0], view[2][0]};
        core::math::Vec3f camU{view[0][1], view[1][1], view[2][1]};

        shader.bind();
        shader.setMat4("uView", view);
        shader.setMat4("uProjection", proj);
        shader.setVec3("uCameraRight", camR);
        shader.setVec3("uCameraUp", camU);

        r.setDepthTest(true);
        r.setDepthWrite(false);
        r.setBlendEnabled(true);
        r.setBlendFunc(renderer::BlendFactor::SrcAlpha,
            additiveBlend ? renderer::BlendFactor::One
                          : renderer::BlendFactor::OneMinusSrcAlpha);

        // 인스턴스 데이터 업로드
        buf_.clear();
        for (size_t i = 0; i < pool.activeCount(); ++i) {
            const auto& p = pool[i];
            buf_.insert(buf_.end(), {p.position.x, p.position.y, p.position.z,
                p.color.x, p.color.y, p.color.z, p.color.w, p.size});
        }
        instanceVBO_->bind();
        instanceVBO_->updateData(buf_.data(),
            static_cast<core::u32>(buf_.size() * sizeof(float)));

        r.bindVertexArray(vao_);
        r.drawIndexedInstanced(6, static_cast<core::u32>(pool.activeCount()));
        r.setDepthWrite(true);
        r.setBlendEnabled(false);
    }

private:
    static constexpr size_t MAX_PARTICLES = 2048;
    core::u32 vao_ = 0;
    std::unique_ptr<renderer::VertexBuffer> quadVBO_, instanceVBO_;
    std::unique_ptr<renderer::IndexBuffer>  quadIBO_;
    std::vector<float> buf_;
};

} // namespace gazeshot::engine
```

### Step 6: 파티클 셰이더

```glsl
// shaders/particle.vert

layout(location = 0) in vec3 aOffset;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aParticlePos;
layout(location = 3) in vec4 aParticleColor;
layout(location = 4) in float aParticleSize;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    vec3 worldPos = aParticlePos
                  + uCameraRight * aOffset.x * aParticleSize
                  + uCameraUp    * aOffset.y * aParticleSize;

    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
    vTexCoord = aTexCoord;
    vColor = aParticleColor;
}
```

```glsl
// shaders/particle.frag

out vec4 FragColor;
in vec2 vTexCoord;
in vec4 vColor;

void main() {
    // 원형 파티클: 중심에서 멀수록 투명
    vec2 center = vTexCoord - vec2(0.5);
    float dist = length(center) * 2.0;
    float alpha = 1.0 - smoothstep(0.6, 1.0, dist);
    FragColor = vec4(vColor.rgb, vColor.a * alpha);
}
```

### Step 7: 사격 시스템 연동

```cpp
// game/src/main.cpp (Ch.19 추가 부분)

struct App {
    // ... (기존 멤버들)
    engine::ParticleEmitter muzzleEmitter{256};   // 머즐 플래시 전용
    engine::ParticleEmitter impactEmitter{512};   // 먼지/파편 공용
    engine::ParticleRenderer particleRenderer;
};

void shoot(App& app) {
    auto [origin, direction] = app.camera.aimRay();
    core::Ray ray{origin, direction};

    // 머즐 플래시 (매 사격마다)
    app.muzzleEmitter.emit(engine::muzzleFlashConfig(origin, direction), 20);

    if (auto hit = sceneRaycast(ray, app.scene)) {
        auto* entity = app.scene.findEntityById(hit->entityId);
        if (entity && entity->name().starts_with("target_")) {
            app.impactEmitter.emit(
                engine::targetDebrisConfig(hit->point, hit->normal), 30);
        } else {
            app.impactEmitter.emit(
                engine::impactDustConfig(hit->point, hit->normal), 15);
        }
        app.tracer = {origin, hit->point, 0.3f};
    } else {
        app.tracer = {origin, origin + direction * 100.0f, 0.3f};
    }
}

void update(App& app, core::f32 dt) {
    app.muzzleEmitter.update(dt);
    app.impactEmitter.update(dt);
}

void render(App& app, core::f32 alpha) {
    // ... (불투명 오브젝트 렌더링 후)
    auto& shader = *app.particleShader;
    app.particleRenderer.render(*app.renderer, shader,
        app.muzzleEmitter.pool(), view, proj, /*additive=*/true);
    app.particleRenderer.render(*app.renderer, shader,
        app.impactEmitter.pool(), view, proj, /*additive=*/false);
}
```

---

## 3. C++ 학습 포인트

### 오브젝트 풀 패턴 (Object Pool)

파티클처럼 빈번하게 생성/소멸하는 객체에 `new`/`delete`를 쓰면
매 프레임 수백 번의 힙 할당이 발생한다:

```cpp
class ParticlePool {
    std::vector<Particle> pool_;   // 고정 크기, 사전 할당
    size_t activeCount_ = 0;
    // spawn: pool_[activeCount_++] 반환
    // kill:  swap with last + activeCount_--
};
// 장점: 동적 할당 0회, 연속 메모리(캐시 친화적), 생성/소멸 O(1)
```

### placement new

사전 할당된 메모리에 객체를 직접 구성하는 기법:

```cpp
#include <new>
alignas(Particle) std::byte buffer[sizeof(Particle) * 100];
Particle* p = new (buffer + sizeof(Particle) * idx) Particle{};
p->position = {1, 2, 3};
p->~Particle();  // 소멸자만 호출 (delete 쓰면 안 됨)
```

이 프로젝트에서는 `std::vector`가 메모리를 관리하므로 직접 쓸 일은 없지만,
커스텀 할당자의 핵심 기법이다.

### std::pmr (Polymorphic Memory Resources)

C++17 표준 커스텀 할당자 인터페이스:

```cpp
#include <memory_resource>

std::byte stackBuf[1024 * 64];  // 64KB 스택 arena
std::pmr::monotonic_buffer_resource arena(
    stackBuf, sizeof(stackBuf), std::pmr::null_memory_resource());

std::pmr::vector<Particle> particles(&arena);
particles.reserve(500);

arena.release();  // 매 프레임: 모든 할당을 O(1)로 해제
```

`monotonic_buffer_resource`는 개별 해제를 추적하지 않아
"프레임 끝에 일괄 폐기"하는 파티클에 최적이다.

### std::ranges로 활성 파티클 처리

```cpp
#include <ranges>
#include <span>

auto active = std::span(pool.activeBegin(), pool.capacity())
            | std::views::take(pool.activeCount());

for (auto& p : active) {
    p.position = p.position + p.velocity * dt;
}

// 파이프라인 체이닝: 필터 + 변환 (lazy, 추가 할당 없음)
auto renderData = std::span(pool.activeBegin(), pool.activeCount())
    | std::views::filter([](const Particle& p) { return p.size > 0.001f; })
    | std::views::transform([](const Particle& p) {
          return RenderData{p.position, p.color, p.size};
      });
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 머즐 플래시 | 사격 시 총구 위치에 짧고 밝은 노란 파티클 |
| 가산 블렌딩 | 머즐 플래시가 겹칠수록 밝아짐 |
| 충돌 먼지 | 장애물 피격 시 흙색 파티클이 법선 방향으로 퍼짐 |
| 타겟 파편 | 타겟 피격 시 빨간 파편이 중력으로 낙하 |
| 빌보딩 | 카메라를 돌려도 파티클이 항상 정면을 향함 |
| 페이드아웃 | 파티클이 수명 종료 시 투명하게 사라짐 |
| 60fps 유지 | 수백 파티클 동시 존재 시에도 프레임 드랍 없음 |
| 풀 고갈 | 풀이 가득 차면 새 파티클이 무시됨 (크래시 없음) |

---

## 블로그 데모 아이디어

1. **이펙트 비교 GIF**: 머즐 플래시 / 충돌 먼지 / 타겟 파편 세 가지 효과
2. **블렌딩 모드 비교**: 가산 블렌딩 vs 알파 블렌딩 나란히 비교
3. **오브젝트 풀 다이어그램**: swap-and-pop의 단계별 시각화
4. **빌보딩 원리**: 카메라 회전 시 파티클이 따라 돌아가는 GIF
5. **성능 그래프**: `new`/`delete` vs 오브젝트 풀의 프레임 타임 비교

---

## 다음 챕터 예고

**Chapter 20: 포스트 프로세싱**

프레임버퍼에 씬을 렌더링한 뒤, 화면 전체에 후처리 효과를 적용한다.
데모: 비네팅, 색수차, 블룸 효과로 스나이퍼 스코프의 몰입감을 높인다.
