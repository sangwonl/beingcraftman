# Chapter 06: 프로시저럴 메시 생성

## 데모 미리보기

```
┌─────────────────────────────────────────────┐
│                                             │
│   ■ Box      ● Sphere    ▌Cylinder   ▬ Plane│
│                                             │
│   법선을 색으로 시각화:                        │
│   R = |nx|   G = |ny|   B = |nz|            │
│                                             │
│   마우스 드래그로 전체 회전                     │
└─────────────────────────────────────────────┘
```

- **데모**: 4가지 프로시저럴 도형이 나란히 배치, 법선 벡터를 RGB 색상으로 시각화
- **인터랙션**: 마우스 드래그로 전체 회전, 휠로 줌
- 블로그에 "코드 몇 줄로 구를 생성하는 수학" 다이어그램 포함 가능

---

## 학습 목표

1. 박스, 구, 실린더, 평면의 정점/인덱스를 코드로 생성한다
2. 각 도형의 수학적 유도를 이해한다
3. face normal vs vertex normal의 차이를 파악한다
4. `Mesh` 클래스로 정점 데이터 + 렌더링을 캡슐화한다
5. `std::vector` 메모리 모델, `std::span`, `emplace_back`, structured bindings를 실습한다

---

## 1. 배경 지식

### 왜 프로시저럴 메시인가?

Ch.03~05에서 큐브 정점을 하드코딩했다. 문제:
- 구나 실린더는 수십~수백 개의 정점이 필요
- 해상도(세그먼트 수)를 런타임에 조절할 수 없다
- 법선, 텍스처 좌표를 수동으로 계산해야 한다

프로시저럴 생성:
```cpp
auto sphere = MeshGen::sphere(1.0f, 32, 16);  // 반지름, 세그먼트, 링
// → 528개 정점, 2880개 인덱스 자동 생성
```

### 정점 데이터 구조

```cpp
struct Vertex {
    Vec3f position;
    Vec3f normal;
    Vec2f texCoord;
};
// sizeof(Vertex) = 32 bytes (12 + 12 + 8)
```

### 법선 벡터

법선(normal)은 표면에 수직인 단위 벡터다.

```
Face normal:   삼각형 전체에 하나     → flat shading (각진 모양)
Vertex normal: 정점마다 하나          → smooth shading (부드러운 모양)
               (인접 면의 법선 평균)
```

구의 경우 vertex normal = normalize(position) (중심이 원점이면).

### 와인딩 순서 (Winding Order)

```
반시계 방향 (CCW) = 앞면     시계 방향 (CW) = 뒷면
     v0                      v0
    ╱  ╲                    ╱  ╲
   v1───v2                 v2───v1

glEnable(GL_CULL_FACE) → 뒷면을 렌더링하지 않음 (성능 최적화)
```

---

## 2. 설계

```
core/include/gazeshot/core/Vertex.hpp    ← Vertex 구조체
engine/include/gazeshot/engine/Mesh.hpp  ← Mesh 클래스
engine/include/gazeshot/engine/MeshGen.hpp ← 도형 생성 함수들
```

```cpp
// Mesh 클래스 개요
class Mesh {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    // GPU 리소스 (VBO, IBO, VAO)
    void upload(Renderer& renderer);  // GPU로 전송
    void draw(Renderer& renderer);    // 렌더링
};
```

---

## 3. 구현 가이드

### Step 1: Vertex 구조체

```hpp
// core/include/gazeshot/core/Vertex.hpp

#pragma once

#include <gazeshot/core/math/Vec2.hpp>
#include <gazeshot/core/math/Vec3.hpp>

namespace gazeshot::core {

struct Vertex {
    math::Vec3f position;
    math::Vec3f normal;
    math::Vec2f texCoord;
};

static_assert(sizeof(Vertex) == 32, "Vertex must be 32 bytes for GPU alignment");

} // namespace gazeshot::core
```

### Step 2: Mesh 클래스

```hpp
// engine/include/gazeshot/engine/Mesh.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/Vertex.hpp>
#include <gazeshot/renderer/Renderer.hpp>

#include <vector>
#include <span>
#include <memory>

namespace gazeshot::engine {

class Mesh {
public:
    Mesh() = default;
    Mesh(std::vector<core::Vertex> vertices, std::vector<core::u32> indices)
        : vertices_(std::move(vertices))
        , indices_(std::move(indices)) {}

    // ── GPU에 업로드 ──
    void upload(renderer::Renderer& r) {
        vao_ = r.createVertexArray();
        r.bindVertexArray(vao_);

        vbo_ = r.createVertexBuffer(
            vertices_.data(),
            static_cast<core::u32>(vertices_.size() * sizeof(core::Vertex)),
            renderer::BufferUsage::Static
        );

        ibo_ = r.createIndexBuffer(
            indices_.data(),
            static_cast<core::u32>(indices_.size())
        );

        r.setVertexLayout({
            {"aPosition", renderer::AttribType::Float3},
            {"aNormal",   renderer::AttribType::Float3},
            {"aTexCoord", renderer::AttribType::Float2},
        });
    }

    // ── 그리기 ──
    void draw(renderer::Renderer& r) const {
        r.bindVertexArray(vao_);
        r.drawIndexed(static_cast<core::u32>(indices_.size()));
    }

    // ── 접근자 ──
    std::span<const core::Vertex> vertices() const { return vertices_; }
    std::span<const core::u32> indices() const { return indices_; }
    core::u32 vertexCount() const { return static_cast<core::u32>(vertices_.size()); }
    core::u32 indexCount() const { return static_cast<core::u32>(indices_.size()); }

private:
    std::vector<core::Vertex> vertices_;
    std::vector<core::u32> indices_;

    core::u32 vao_ = 0;
    std::unique_ptr<renderer::VertexBuffer> vbo_;
    std::unique_ptr<renderer::IndexBuffer> ibo_;
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: `std::span`**

```cpp
std::span<const core::Vertex> vertices() const { return vertices_; }
```

`std::span`은 "소유하지 않는 연속 메모리 뷰"다:
- `std::vector`의 데이터를 참조하지만 복사하지 않는다
- 크기 정보를 함께 가지고 있다 (포인터 + 크기)
- 배열, vector, C 배열 어디서든 만들 수 있다

```cpp
void processVertices(std::span<const Vertex> verts) {
    for (auto& [pos, normal, uv] : verts) {  // structured binding!
        // ...
    }
}

std::vector<Vertex> v = {...};
processVertices(v);          // vector → span 자동 변환
Vertex arr[10] = {...};
processVertices(arr);        // C 배열 → span 자동 변환
```

### Step 3: 도형 생성기

```hpp
// engine/include/gazeshot/engine/MeshGen.hpp

#pragma once

#include <gazeshot/engine/Mesh.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <cmath>
#include <numbers>   // C++20: std::numbers::pi_v

namespace gazeshot::engine {

namespace MeshGen {

// ────────────────────────────────────────
// 평면 (Plane)
// ────────────────────────────────────────
// XZ 평면, Y=0, 법선은 +Y
//
//  (-w/2, 0, -d/2) ──── (w/2, 0, -d/2)
//       │                     │
//       │      subdivs...     │
//       │                     │
//  (-w/2, 0, d/2)  ──── (w/2, 0, d/2)

inline Mesh plane(core::f32 width, core::f32 depth, core::u32 subdivs = 1) {
    using namespace core;
    using namespace core::math;

    std::vector<Vertex> verts;
    std::vector<u32> idxs;

    u32 cols = subdivs + 1;
    u32 rows = subdivs + 1;
    verts.reserve(cols * rows);

    for (u32 r = 0; r < rows; ++r) {
        for (u32 c = 0; c < cols; ++c) {
            f32 u = static_cast<f32>(c) / static_cast<f32>(subdivs);
            f32 v = static_cast<f32>(r) / static_cast<f32>(subdivs);

            verts.push_back({
                .position = { (u - 0.5f) * width, 0.0f, (v - 0.5f) * depth },
                .normal   = { 0.0f, 1.0f, 0.0f },
                .texCoord = { u, v },
            });
        }
    }

    for (u32 r = 0; r < subdivs; ++r) {
        for (u32 c = 0; c < subdivs; ++c) {
            u32 tl = r * cols + c;
            u32 tr = tl + 1;
            u32 bl = tl + cols;
            u32 br = bl + 1;
            // CCW winding
            idxs.insert(idxs.end(), {tl, bl, tr, tr, bl, br});
        }
    }

    return Mesh(std::move(verts), std::move(idxs));
}

// ────────────────────────────────────────
// 박스 (Box)
// ────────────────────────────────────────
// 6면 × 4정점 = 24정점 (법선이 면마다 다르므로 정점 공유 불가)
// 6면 × 2삼각형 × 3 = 36 인덱스

inline Mesh box(core::f32 w = 1, core::f32 h = 1, core::f32 d = 1) {
    using namespace core;
    using namespace core::math;

    f32 hw = w/2, hh = h/2, hd = d/2;
    std::vector<Vertex> v;
    std::vector<u32> idx;
    v.reserve(24);
    idx.reserve(36);

    auto addFace = [&](Vec3f p0, Vec3f p1, Vec3f p2, Vec3f p3, Vec3f n) {
        u32 base = static_cast<u32>(v.size());
        v.push_back({p0, n, {0, 0}});
        v.push_back({p1, n, {1, 0}});
        v.push_back({p2, n, {1, 1}});
        v.push_back({p3, n, {0, 1}});
        idx.insert(idx.end(), {base, base+1, base+2, base, base+2, base+3});
    };

    // 앞 (+Z)
    addFace({-hw,-hh, hd}, { hw,-hh, hd}, { hw, hh, hd}, {-hw, hh, hd}, {0,0,1});
    // 뒤 (-Z)
    addFace({ hw,-hh,-hd}, {-hw,-hh,-hd}, {-hw, hh,-hd}, { hw, hh,-hd}, {0,0,-1});
    // 우 (+X)
    addFace({ hw,-hh, hd}, { hw,-hh,-hd}, { hw, hh,-hd}, { hw, hh, hd}, {1,0,0});
    // 좌 (-X)
    addFace({-hw,-hh,-hd}, {-hw,-hh, hd}, {-hw, hh, hd}, {-hw, hh,-hd}, {-1,0,0});
    // 상 (+Y)
    addFace({-hw, hh, hd}, { hw, hh, hd}, { hw, hh,-hd}, {-hw, hh,-hd}, {0,1,0});
    // 하 (-Y)
    addFace({-hw,-hh,-hd}, { hw,-hh,-hd}, { hw,-hh, hd}, {-hw,-hh, hd}, {0,-1,0});

    return Mesh(std::move(v), std::move(idx));
}

// ────────────────────────────────────────
// 구 (Sphere)
// ────────────────────────────────────────
// 구면 좌표계 (theta, phi) → 직교 좌표계 (x, y, z)
//
// theta: 0 ~ π (위에서 아래, 위도)
// phi:   0 ~ 2π (한 바퀴, 경도)
//
// x = r * sin(theta) * cos(phi)
// y = r * cos(theta)
// z = r * sin(theta) * sin(phi)

inline Mesh sphere(core::f32 radius, core::u32 segments = 32, core::u32 rings = 16) {
    using namespace core;
    using namespace core::math;
    constexpr f32 PI = std::numbers::pi_v<f32>;

    std::vector<Vertex> verts;
    std::vector<u32> idxs;
    verts.reserve((rings + 1) * (segments + 1));

    for (u32 ring = 0; ring <= rings; ++ring) {
        f32 theta = static_cast<f32>(ring) * PI / static_cast<f32>(rings);
        f32 sinT = std::sin(theta);
        f32 cosT = std::cos(theta);

        for (u32 seg = 0; seg <= segments; ++seg) {
            f32 phi = static_cast<f32>(seg) * 2.0f * PI / static_cast<f32>(segments);
            f32 sinP = std::sin(phi);
            f32 cosP = std::cos(phi);

            Vec3f normal{sinT * cosP, cosT, sinT * sinP};
            Vec3f position = normal * radius;
            Vec2f uv{
                static_cast<f32>(seg) / static_cast<f32>(segments),
                static_cast<f32>(ring) / static_cast<f32>(rings)
            };

            verts.push_back({position, normal, uv});
        }
    }

    // 인덱스: 쿼드를 두 개의 삼각형으로
    for (u32 ring = 0; ring < rings; ++ring) {
        for (u32 seg = 0; seg < segments; ++seg) {
            u32 curr = ring * (segments + 1) + seg;
            u32 next = curr + segments + 1;

            idxs.insert(idxs.end(), {curr, next, curr + 1});
            idxs.insert(idxs.end(), {curr + 1, next, next + 1});
        }
    }

    return Mesh(std::move(verts), std::move(idxs));
}

// ────────────────────────────────────────
// 실린더 (Cylinder)
// ────────────────────────────────────────
// 옆면: 원의 파라메트릭 방정식으로 스트립 생성
// 상면/하면: 부채꼴

inline Mesh cylinder(core::f32 radius, core::f32 height,
                     core::u32 segments = 32) {
    using namespace core;
    using namespace core::math;
    constexpr f32 PI = std::numbers::pi_v<f32>;

    std::vector<Vertex> verts;
    std::vector<u32> idxs;
    f32 hh = height / 2.0f;

    // ── 옆면 ──
    for (u32 i = 0; i <= segments; ++i) {
        f32 angle = static_cast<f32>(i) * 2.0f * PI / static_cast<f32>(segments);
        f32 c = std::cos(angle), s = std::sin(angle);
        f32 u = static_cast<f32>(i) / static_cast<f32>(segments);

        Vec3f normal{c, 0, s};
        // 상단
        verts.push_back({{c*radius, hh, s*radius}, normal, {u, 1}});
        // 하단
        verts.push_back({{c*radius, -hh, s*radius}, normal, {u, 0}});
    }

    for (u32 i = 0; i < segments; ++i) {
        u32 top = i * 2;
        u32 bot = top + 1;
        idxs.insert(idxs.end(), {top, bot, top + 2});
        idxs.insert(idxs.end(), {bot, bot + 2, top + 2});
    }

    // ── 상면 (cap) ──
    u32 topCenter = static_cast<u32>(verts.size());
    verts.push_back({{0, hh, 0}, {0, 1, 0}, {0.5f, 0.5f}});

    for (u32 i = 0; i <= segments; ++i) {
        f32 angle = static_cast<f32>(i) * 2.0f * PI / static_cast<f32>(segments);
        f32 c = std::cos(angle), s = std::sin(angle);
        verts.push_back({
            {c*radius, hh, s*radius},
            {0, 1, 0},
            {c*0.5f + 0.5f, s*0.5f + 0.5f}
        });
    }
    for (u32 i = 0; i < segments; ++i) {
        idxs.insert(idxs.end(), {topCenter, topCenter + i + 1, topCenter + i + 2});
    }

    // ── 하면 (cap) ──
    u32 botCenter = static_cast<u32>(verts.size());
    verts.push_back({{0, -hh, 0}, {0, -1, 0}, {0.5f, 0.5f}});

    for (u32 i = 0; i <= segments; ++i) {
        f32 angle = static_cast<f32>(i) * 2.0f * PI / static_cast<f32>(segments);
        f32 c = std::cos(angle), s = std::sin(angle);
        verts.push_back({
            {c*radius, -hh, s*radius},
            {0, -1, 0},
            {c*0.5f + 0.5f, s*0.5f + 0.5f}
        });
    }
    for (u32 i = 0; i < segments; ++i) {
        idxs.insert(idxs.end(), {botCenter, botCenter + i + 2, botCenter + i + 1});
    }

    return Mesh(std::move(verts), std::move(idxs));
}

} // namespace MeshGen
} // namespace gazeshot::engine
```

**C++ 학습 포인트: `std::vector` 메모리 모델**

```cpp
std::vector<Vertex> verts;
verts.reserve(528);          // 메모리 미리 확보 (재할당 0회)
verts.push_back({...});      // 복사 삽입
verts.emplace_back(pos, n, uv); // 제자리 생성 (복사 없음)
```

`reserve` vs 그냥 `push_back`:
- reserve 없이: push_back할 때마다 capacity 초과 시 재할당 + 전체 복사
- reserve 후: 재할당 없이 바로 삽입 → 구 528개 정점에서 큰 차이

**C++ 학습 포인트: structured bindings**

```cpp
for (auto& [position, normal, texCoord] : mesh.vertices()) {
    // position, normal, texCoord를 직접 접근
    std::printf("pos: (%.2f, %.2f, %.2f)\n", position.x, position.y, position.z);
}
```

### Step 4: 법선 시각화 셰이더

법선 벡터를 색으로 변환하면 도형이 올바른지 시각적으로 확인할 수 있다:

```glsl
// vertex shader
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMVP;
out vec3 vNormal;

void main() {
    gl_Position = uMVP * vec4(aPosition, 1.0);
    vNormal = aNormal;
}

// fragment shader
in vec3 vNormal;
out vec4 FragColor;

void main() {
    // 법선을 색으로: (-1,1) → (0,1)
    vec3 color = abs(vNormal);
    FragColor = vec4(color, 1.0);
}
```

법선 시각화 의미:
- **빨강 강한 면** = X축 방향 (좌/우)
- **초록 강한 면** = Y축 방향 (상/하)
- **파랑 강한 면** = Z축 방향 (앞/뒤)

### Step 5: 데모 코드

```cpp
// game/src/main.cpp (Ch.06)

void init(App& app) {
    // 4가지 도형 생성
    app.meshes[0] = MeshGen::box(1, 1, 1);
    app.meshes[1] = MeshGen::sphere(0.6f, 32, 16);
    app.meshes[2] = MeshGen::cylinder(0.4f, 1.2f, 32);
    app.meshes[3] = MeshGen::plane(1.5f, 1.5f, 4);

    for (auto& mesh : app.meshes) {
        mesh.upload(*app.renderer);
    }

    // 법선 시각화 셰이더
    app.shader = app.renderer->createShaderProgram(normalVertSrc, normalFragSrc);
}

void render(App& app, core::f32 alpha) {
    app.renderer->clear({0.1f, 0.1f, 0.12f, 1.0f});
    app.renderer->setDepthTest(true);

    float aspect = (float)app.window.width() / (float)app.window.height();
    Mat4f view = lookAt(Vec3f{0, 2, 6}, Vec3f{0, 0, 0}, Vec3f{0, 1, 0});
    Mat4f proj = perspective(45.0_deg, aspect, 0.1f, 100.0f);

    // 4개 도형을 X축으로 나란히 배치
    float positions[] = {-3.0f, -1.0f, 1.0f, 3.0f};

    for (int i = 0; i < 4; ++i) {
        Mat4f model = translate(Vec3f{positions[i], 0, 0})
                    * rotateY(app.cubeRotation.y)
                    * rotateX(app.cubeRotation.x);
        Mat4f mvp = proj * view * model;

        app.shader->bind();
        app.shader->setMat4("uMVP", mvp);
        app.meshes[i].draw(*app.renderer);
    }

    app.window.swapBuffers();
}
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 4가지 도형 | 박스, 구, 실린더, 평면이 모두 보임 |
| 법선 색상 | 박스의 각 면이 R/G/B로 구분됨 |
| 구 부드러움 | 구의 색이 부드럽게 그래디언트됨 |
| 실린더 캡 | 상하면이 닫혀 있음 |
| 와인딩 | face culling 켜도 구멍 없음 |
| WASM 동작 | 브라우저에서 동일 |

---

## 5. 블로그 데모 아이디어

1. **법선 시각화 스크린샷**: 4가지 도형의 RGB 법선 색상
2. **구면 좌표계 다이어그램**: theta, phi → x, y, z 변환 그림
3. **세그먼트 비교**: segments=4 vs 16 vs 64 구 → 로우폴리에서 스무스까지
4. **와이어프레임 모드**: `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` 토글
5. **정점 카운트 표시**: "구 32x16 = 528 vertices, 2880 indices"

---

## 다음 챕터 예고

**Chapter 07: 라이팅 기초**

Phong 라이팅 모델을 셰이더에 구현한다.
데모: 법선 색상 대신 실제 조명이 적용된 4가지 도형 — 햇빛에 비춰진 것처럼 보인다.
