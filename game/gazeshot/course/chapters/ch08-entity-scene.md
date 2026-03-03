# Chapter 08: 엔티티와 씬 관리

## 데모 미리보기

```
┌─────────────────────────────────────────────┐
│                                             │
│   ● ●    ■         ▌                        │
│     ●      ■   ●     ▌    ← 12개 오브젝트   │
│        ●              ▌                     │
│   ▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬  ← 바닥           │
│                                             │
│  Tab: 다음 오브젝트 선택 (노란 아웃라인)       │
│  Delete: 선택 오브젝트 비활성화               │
│  Entity count: 12 (active: 10)              │
└─────────────────────────────────────────────┘
```

- **데모**: 12개 엔티티가 씬에 배치, Tab으로 순회 선택, Delete로 비활성화
- **콘솔**: 활성 엔티티 수, 선택된 엔티티 이름 표시
- 블로그에 "게임 오브젝트 관리의 설계 선택지" 다이어그램 포함 가능

---

## 학습 목표

1. Transform, Entity, Scene 클래스를 설계한다
2. 소유권 모델 (`unique_ptr`, 참조 반환)을 이해한다
3. 활성/비활성 엔티티 관리, 선택 시스템을 구현한다
4. `std::optional`, Rule of Zero, `std::ranges::views::filter`를 실습한다

---

## 1. 배경 지식

### 게임 오브젝트 관리 패턴들

```
1. 상속 기반 (전통적)
   GameObject → Enemy, Bullet, Target ...
   문제: 다중 상속, 깊은 계층, 유연성 부족

2. ECS (Entity Component System)
   Entity = ID, Component = 데이터, System = 로직
   장점: 유연, 캐시 친화적
   단점: 이 프로젝트에는 과도한 복잡성

3. 단순 엔티티 (우리의 선택) ★
   Entity = Transform + Mesh + Material + 상태
   장점: 이해하기 쉬움, 필요한 만큼만
   단점: 유연성 제한 (이 프로젝트에서는 충분)
```

### Rule of Zero

```cpp
class Scene {
    std::vector<std::unique_ptr<Entity>> entities_;
    // 소멸자, 복사, 이동을 직접 안 써도 된다!
    // unique_ptr이 알아서 Entity를 delete한다
    // vector가 알아서 메모리를 해제한다
};
// → 컴파일러 생성 소멸자가 올바르게 동작
```

스마트 포인터를 쓰면 직접 소멸자를 쓸 필요가 없다.
이것이 **Rule of Zero**: "0개의 특수 멤버 함수를 직접 정의하라."

---

## 2. 구현 가이드

### Step 1: Transform

```hpp
// engine/include/gazeshot/engine/Transform.hpp

#pragma once

#include <gazeshot/core/math/Math.hpp>

namespace gazeshot::engine {

struct Transform {
    core::math::Vec3f position{0, 0, 0};
    core::math::Quatf rotation{};           // 단위 쿼터니언 = 회전 없음
    core::math::Vec3f scale{1, 1, 1};

    [[nodiscard]] core::math::Mat4f modelMatrix() const {
        using namespace core::math;
        return translate(position)
             * rotation.toMat4()
             * math::scale(scale);
    }
};

} // namespace gazeshot::engine
```

### Step 2: Entity

```hpp
// engine/include/gazeshot/engine/Entity.hpp

#pragma once

#include <gazeshot/engine/Transform.hpp>
#include <gazeshot/engine/Mesh.hpp>
#include <gazeshot/engine/Material.hpp>
#include <gazeshot/core/Types.hpp>

#include <string>
#include <optional>

namespace gazeshot::engine {

class Entity {
public:
    explicit Entity(std::string name) : name_(std::move(name)) {}

    // ── 이름 ──
    const std::string& name() const { return name_; }

    // ── Transform ──
    Transform& transform() { return transform_; }
    const Transform& transform() const { return transform_; }

    // ── Mesh (없을 수 있음 — 빈 엔티티) ──
    void setMesh(Mesh* mesh) { mesh_ = mesh; }
    Mesh* mesh() const { return mesh_; }
    bool hasMesh() const { return mesh_ != nullptr; }

    // ── Material ──
    void setMaterial(const Material& mat) { material_ = mat; }
    const Material& material() const { return material_; }
    Material& material() { return material_; }

    // ── 활성 상태 ──
    bool isActive() const { return active_; }
    void setActive(bool active) { active_ = active; }

    // ── ID (씬에서 부여) ──
    core::u32 id() const { return id_; }

private:
    friend class Scene;  // Scene이 id_ 설정
    core::u32 id_ = 0;
    std::string name_;
    Transform transform_;
    Mesh* mesh_ = nullptr;  // non-owning (메시는 리소스 매니저가 관리)
    Material material_;
    bool active_ = true;
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: `std::optional`**

```cpp
// 씬에서 엔티티를 검색할 때:
std::optional<std::reference_wrapper<Entity>> findEntity(std::string_view name);

// 사용:
if (auto found = scene.findEntity("target_01")) {
    found->get().setActive(false);   // reference_wrapper → .get() 필요
}

// 더 간단한 대안: 포인터 반환
Entity* findEntity(std::string_view name);
if (auto* e = scene.findEntity("target_01")) {
    e->setActive(false);
}
```

이 프로젝트에서는 포인터 반환이 더 실용적이다.
`std::optional<T&>`은 C++26까지 공식 지원이 아니므로,
"없을 수 있는 참조"는 포인터가 관용적이다.

### Step 3: Scene

```hpp
// engine/include/gazeshot/engine/Scene.hpp

#pragma once

#include <gazeshot/engine/Entity.hpp>
#include <gazeshot/engine/Light.hpp>
#include <gazeshot/renderer/Renderer.hpp>

#include <vector>
#include <memory>
#include <string_view>
#include <algorithm>
#include <ranges>

namespace gazeshot::engine {

class Scene {
public:
    // ── 엔티티 생성 ──
    Entity& createEntity(std::string name) {
        auto& e = entities_.emplace_back(
            std::make_unique<Entity>(std::move(name))
        );
        e->id_ = nextId_++;
        return *e;
    }

    // ── 엔티티 검색 ──
    Entity* findEntity(std::string_view name) {
        auto it = std::ranges::find_if(entities_,
            [name](const auto& e) { return e->name() == name; }
        );
        return (it != entities_.end()) ? it->get() : nullptr;
    }

    Entity* findEntityById(core::u32 id) {
        auto it = std::ranges::find_if(entities_,
            [id](const auto& e) { return e->id() == id; }
        );
        return (it != entities_.end()) ? it->get() : nullptr;
    }

    // ── 활성 엔티티 순회 (C++20 ranges) ──
    auto activeEntities() {
        return entities_
            | std::views::transform([](auto& ptr) -> Entity& { return *ptr; })
            | std::views::filter(&Entity::isActive);
    }

    // ── 전체 엔티티 ──
    core::u32 entityCount() const {
        return static_cast<core::u32>(entities_.size());
    }

    core::u32 activeEntityCount() const {
        return static_cast<core::u32>(
            std::ranges::count_if(entities_,
                [](const auto& e) { return e->isActive(); })
        );
    }

    // ── 렌더링 ──
    void render(renderer::Renderer& r, renderer::ShaderProgram& shader,
                const core::math::Mat4f& view, const core::math::Mat4f& proj,
                const core::math::Vec3f& viewPos) {
        for (auto& entity : activeEntities()) {
            if (!entity.hasMesh()) continue;

            auto model = entity.transform().modelMatrix();
            shader.setMat4("uModel", model);
            shader.setMat4("uView", view);
            shader.setMat4("uProjection", proj);

            // normal matrix
            auto normalMat = transpose(inverse(model));
            // setMat3 생략 — 이전 챕터에서 다룬 방식으로

            auto& mat = entity.material();
            shader.setVec3("uMatAmbient", mat.ambient);
            shader.setVec3("uMatDiffuse", mat.diffuse);
            shader.setVec3("uMatSpecular", mat.specular);
            shader.setFloat("uMatShininess", mat.shininess);

            entity.mesh()->draw(r);
        }
    }

    // ── 광원 ──
    DirectionalLight& light() { return light_; }

private:
    std::vector<std::unique_ptr<Entity>> entities_;
    core::u32 nextId_ = 1;
    DirectionalLight light_;
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: `std::ranges::views::filter` (C++20)**

```cpp
auto activeEntities() {
    return entities_
        | std::views::transform([](auto& ptr) -> Entity& { return *ptr; })
        | std::views::filter(&Entity::isActive);
}

// 사용:
for (auto& entity : scene.activeEntities()) {
    // 활성 엔티티만 순회 — 별도 컨테이너 없이, 지연 평가(lazy)
}
```

`std::views::filter`는:
- 새로운 컨테이너를 만들지 않는다 (메모리 0)
- 순회할 때만 조건을 검사한다 (lazy)
- 파이프(`|`) 문법으로 체이닝 가능

### Step 4: 데모 — 씬 구성과 선택 시스템

```cpp
// game/src/main.cpp (Ch.08)

void initScene(App& app) {
    auto& scene = app.scene;

    // 메시 생성 (공유)
    app.boxMesh = MeshGen::box();
    app.sphereMesh = MeshGen::sphere(0.5f);
    app.cylinderMesh = MeshGen::cylinder(0.3f, 0.8f);
    app.boxMesh.upload(*app.renderer);
    app.sphereMesh.upload(*app.renderer);
    app.cylinderMesh.upload(*app.renderer);

    // 바닥
    auto& floor = scene.createEntity("floor");
    floor.setMesh(&app.planeMesh);
    floor.transform().position = {0, -1, 0};
    floor.transform().scale = {10, 1, 10};
    floor.material().diffuse = {0.3f, 0.3f, 0.3f};

    // 오브젝트 배치
    auto spawn = [&](const char* name, Mesh* mesh, Vec3f pos, Vec3f color) {
        auto& e = scene.createEntity(name);
        e.setMesh(mesh);
        e.transform().position = pos;
        e.material().diffuse = color;
    };

    spawn("box_1",      &app.boxMesh,      {-3, 0, 0}, {0.8f, 0.2f, 0.2f});
    spawn("box_2",      &app.boxMesh,      {-3, 0, 3}, {0.6f, 0.1f, 0.1f});
    spawn("sphere_1",   &app.sphereMesh,   {-1, 0, 0}, {0.2f, 0.6f, 0.8f});
    spawn("sphere_2",   &app.sphereMesh,   {-1, 0, 3}, {0.1f, 0.4f, 0.6f});
    spawn("sphere_3",   &app.sphereMesh,   { 0, 0, 1}, {0.2f, 0.8f, 0.4f});
    spawn("cylinder_1", &app.cylinderMesh, { 1, 0, 0}, {0.7f, 0.7f, 0.2f});
    spawn("cylinder_2", &app.cylinderMesh, { 1, 0, 3}, {0.5f, 0.5f, 0.1f});
    spawn("cylinder_3", &app.cylinderMesh, { 3, 0, 0}, {0.8f, 0.5f, 0.2f});
    spawn("cylinder_4", &app.cylinderMesh, { 3, 0, 3}, {0.6f, 0.4f, 0.1f});
    spawn("sphere_4",   &app.sphereMesh,   { 0, 0,-2}, {0.5f, 0.2f, 0.7f});
    spawn("box_3",      &app.boxMesh,      { 2, 0,-2}, {0.3f, 0.7f, 0.3f});
}

void update(App& app, f32 dt) {
    // Tab: 다음 엔티티 선택
    if (app.input.isKeyPressed(SDLK_TAB)) {
        app.selectedIndex = (app.selectedIndex + 1) % app.scene.entityCount();
        auto* entity = app.scene.findEntityById(app.selectedIndex + 1);
        if (entity) {
            std::printf("Selected: %s\n", entity->name().c_str());
        }
    }

    // Delete: 선택 엔티티 비활성화
    if (app.input.isKeyPressed(SDLK_DELETE) || app.input.isKeyPressed(SDLK_BACKSPACE)) {
        auto* entity = app.scene.findEntityById(app.selectedIndex + 1);
        if (entity) {
            entity->setActive(false);
            std::printf("Deactivated: %s | Active: %d/%d\n",
                entity->name().c_str(),
                app.scene.activeEntityCount(),
                app.scene.entityCount());
        }
    }
}

void render(App& app, f32 alpha) {
    app.renderer->clear({0.1f, 0.1f, 0.12f, 1.0f});
    app.renderer->setDepthTest(true);

    // 광원 설정 (셰이더에 한 번)
    auto& light = app.scene.light();
    app.shader->bind();
    app.shader->setVec3("uLightDir", normalize(light.direction));
    // ... ambient, diffuse, specular 설정

    // 씬 전체 렌더링 (한 줄!)
    app.scene.render(*app.renderer, *app.shader, view, proj, cameraPos);

    // 선택된 엔티티 하이라이트 (두 번째 패스 — 와이어프레임)
    auto* selected = app.scene.findEntityById(app.selectedIndex + 1);
    if (selected && selected->isActive() && selected->hasMesh()) {
        // 와이어프레임 모드로 약간 크게 그려서 아웃라인 효과
        // (실제로는 스텐실 기반이 더 좋지만, 간이 구현)
        app.outlineShader->bind();
        auto model = selected->transform().modelMatrix()
                   * core::math::scale(Vec3f(1.05f));
        app.outlineShader->setMat4("uMVP", proj * view * model);
        selected->mesh()->draw(*app.renderer);
    }
}
```

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 12개 오브젝트 | 바닥 + 11개 오브젝트가 보임 |
| Tab 선택 | Tab 누를 때마다 다른 오브젝트에 하이라이트 |
| Delete 비활성화 | Delete 후 오브젝트 사라짐 |
| 활성 카운트 | 콘솔에 정확한 active/total 표시 |
| 조명 적용 | Ch.07의 Phong 조명이 모든 오브젝트에 적용 |
| 메모리 | 프로그램 종료 시 리소스 정리 (leak 없음) |

---

## 4. 블로그 데모 아이디어

1. **씬 스크린샷**: 12개 오브젝트 + 선택 하이라이트
2. **Before/After**: main.cpp에서 개별 draw → `scene.render()` 한 줄
3. **소유권 다이어그램**: Scene → unique_ptr<Entity>, Entity → Mesh* (non-owning)
4. **ranges 코드**: `activeEntities()` 체이닝의 우아함
5. **인터랙션 GIF**: Tab으로 순회 선택, Delete로 제거

---

## 다음 챕터 예고

**Chapter 09: 스나이퍼 카메라 시스템**

1인칭 고정 카메라 + 가늠자/가늠쇠 분리 + 스코프 뷰를 구현한다.
데모: 키보드로 시점(가늠자) 이동, 마우스로 레티클(가늠쇠) 이동. 스코프를 통해 씬을 바라본다.
