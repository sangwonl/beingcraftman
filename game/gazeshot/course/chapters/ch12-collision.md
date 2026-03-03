# Chapter 12: 충돌 감지 (Collision Detection)

## 데모 미리보기

```
┌─────────────────────────────────────────────────────┐
│              ╭────────────╮                          │
│              │             │                          │
│              │  ◎ ┃  ◎    │  ← 기둥(AABB) + 타겟(구)  │
│              │     ＋      │                          │
│              │             │                          │
│              ╰────────────╯                          │
│                                                     │
│  Click → Ray 발사                                    │
│  ├─ Ray가 기둥 AABB에 t=12.3에서 교차                 │
│  ├─ Ray가 타겟 Sphere에 t=28.7에서 교차               │
│  └─ 12.3 < 28.7 → 기둥이 더 가까움 → "BLOCKED!"      │
│                                                     │
│  콘솔:                                               │
│  [Collision] BLOCKED by obstacle_2 at t=12.3         │
│  머리 이동 후 → [Collision] HIT! target_4 +100pts     │
└─────────────────────────────────────────────────────┘
```

- **데모**: Ch.11의 레이캐스트를 체계적인 충돌 시스템으로 업그레이드
- **바운딩 볼륨**: 메시 데이터에서 자동 생성된 AABB, BoundingSphere
- **C++20 concepts**: `Intersectable` concept로 제네릭 충돌 검사
- 블로그에 "충돌 감지 파이프라인" 다이어그램과 "concept vs virtual" 비교 포함 가능

---

## 학습 목표

1. AABB, BoundingSphere 바운딩 볼륨을 메시 데이터에서 자동 생성한다
2. Ray-AABB, Ray-Sphere, Ray-Plane 교차 검사를 체계적으로 구현한다
3. 가장 가까운 교차점 선택과 장애물 차단 로직을 구현한다
4. **C++20 concepts**로 제네릭 충돌 검사를 설계하고, virtual 함수와의 차이를 이해한다

---

## 1. 배경 지식

### 바운딩 볼륨과 Broad/Narrow Phase

```
실제 메시 형태      AABB           BoundingSphere
   ◇◇◇◇           ┌────────┐        ╭──╮
  ◇    ◇◇         │        │       ╭╯    ╰╮
 ◇◇◇    ◇         │        │       ╰╮    ╭╯
  ◇◇◇◇◇◇          └────────┘        ╰──╯

전체 엔티티 (100개)
  → [Broad Phase] 바운딩 볼륨으로 빠르게 필터링 → 후보 (3개)
    → [Narrow Phase] 정밀 검사 (이 프로젝트에서는 생략)
      → 최종 교차점 (1개)
```

### Ray의 t 파라미터와 Epsilon

```
P(t) = origin + t * direction

t = 0     → origin        t = 12.3 → 기둥 교차
t = 28.7  → 타겟 교차     t가 작을수록 가까운 교차점

부동소수점 문제: t ≈ 0 일 때 자기 자신과 교차 판정
해결: EPSILON = 0.001f (1mm) — 이보다 작은 t는 무시
```

---

## 2. 구현 가이드

### Step 1: 바운딩 볼륨 타입

```hpp
// core/include/gazeshot/core/BoundingVolume.hpp
#pragma once
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/Types.hpp>
#include <algorithm>

namespace gazeshot::core {

struct AABB {
    math::Vec3f min;
    math::Vec3f max;

    [[nodiscard]] constexpr math::Vec3f center() const {
        return (min + max) * 0.5f;
    }
    [[nodiscard]] constexpr math::Vec3f halfExtents() const {
        return (max - min) * 0.5f;
    }
    [[nodiscard]] constexpr AABB merged(const AABB& other) const {
        return {
            .min = {std::min(min.x, other.min.x), std::min(min.y, other.min.y),
                    std::min(min.z, other.min.z)},
            .max = {std::max(max.x, other.max.x), std::max(max.y, other.max.y),
                    std::max(max.z, other.max.z)},
        };
    }
};

struct BoundingSphere {
    math::Vec3f center;
    f32 radius = 0.0f;
};

} // namespace gazeshot::core
```

### Step 2: 메시 데이터에서 바운딩 볼륨 자동 생성

```hpp
// core/include/gazeshot/core/BoundingVolumeGen.hpp
#pragma once
#include <gazeshot/core/BoundingVolume.hpp>
#include <limits>
#include <span>
#include <cmath>

namespace gazeshot::core {

[[nodiscard]] inline AABB computeAABB(std::span<const math::Vec3f> vertices) {
    constexpr f32 INF = std::numeric_limits<f32>::max();
    AABB result{.min = {INF, INF, INF}, .max = {-INF, -INF, -INF}};
    for (const auto& v : vertices) {
        result.min.x = std::min(result.min.x, v.x);
        result.min.y = std::min(result.min.y, v.y);
        result.min.z = std::min(result.min.z, v.z);
        result.max.x = std::max(result.max.x, v.x);
        result.max.y = std::max(result.max.y, v.y);
        result.max.z = std::max(result.max.z, v.z);
    }
    return result;
}

// Ritter's 근사: AABB 중심 → 가장 먼 정점까지 거리 = 반지름
[[nodiscard]] inline BoundingSphere computeBoundingSphere(
    std::span<const math::Vec3f> vertices)
{
    if (vertices.empty()) return {{0, 0, 0}, 0};
    auto aabb = computeAABB(vertices);
    auto center = aabb.center();
    f32 maxDistSq = 0.0f;
    for (const auto& v : vertices) {
        auto diff = v - center;
        maxDistSq = std::max(maxDistSq, math::dot(diff, diff));
    }
    return {center, std::sqrt(maxDistSq)};
}

} // namespace gazeshot::core
```

**C++ 포인트: `std::span` — 어떤 연속 컨테이너든 통일된 방식으로 받는 비소유 뷰 (C++20)**

### Step 3: 교차 검사 함수 체계화

Ch.11에서 `intersectSphere()`, `intersectAABB()`를 개별 구현했다.
이제 **함수 오버로딩**으로 통일된 `intersect()` 이름 아래 체계화한다.

```hpp
// core/include/gazeshot/core/Intersect.hpp
#pragma once
#include <gazeshot/core/Ray.hpp>
#include <gazeshot/core/BoundingVolume.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <optional>
#include <cmath>
#include <algorithm>

namespace gazeshot::core {

inline constexpr f32 RAY_EPSILON = 0.001f;

// ── 레이-구 교차 ──
[[nodiscard]] inline std::optional<HitResult>
intersect(const Ray& ray, const BoundingSphere& sphere) {
    auto oc = ray.origin - sphere.center;
    f32 a = math::dot(ray.direction, ray.direction);
    f32 halfB = math::dot(oc, ray.direction);
    f32 c = math::dot(oc, oc) - sphere.radius * sphere.radius;
    f32 discriminant = halfB * halfB - a * c;
    if (discriminant < 0.0f) return std::nullopt;

    f32 sqrtD = std::sqrt(discriminant);
    f32 t = (-halfB - sqrtD) / a;
    if (t < RAY_EPSILON) {
        t = (-halfB + sqrtD) / a;
        if (t < RAY_EPSILON) return std::nullopt;
    }
    auto point = ray.at(t);
    return HitResult{point, math::normalize(point - sphere.center), t, 0};
}

// ── 레이-AABB 교차 (Slab Method) ──
//
//   tmin = max(t0_x, t0_y, t0_z)  ← 가장 늦게 들어가는 축
//   tmax = min(t1_x, t1_y, t1_z)  ← 가장 빨리 나오는 축
//   tmin < tmax → 교차!
//
[[nodiscard]] inline std::optional<HitResult>
intersect(const Ray& ray, const AABB& box) {
    f32 tmin = RAY_EPSILON;
    f32 tmax = std::numeric_limits<f32>::max();
    for (int i = 0; i < 3; ++i) {
        f32 invD = 1.0f / ray.direction[i];
        f32 t0 = (box.min[i] - ray.origin[i]) * invD;
        f32 t1 = (box.max[i] - ray.origin[i]) * invD;
        if (invD < 0.0f) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin) return std::nullopt;
    }
    auto point = ray.at(tmin);
    // 법선: 교차면 판별 (축별 거리 비율이 가장 큰 축)
    auto diff = point - box.center();
    auto half = box.halfExtents();
    math::Vec3f normal{0, 0, 0};
    f32 maxRatio = 0.0f;
    for (int i = 0; i < 3; ++i) {
        f32 ratio = std::abs(diff[i]) / half[i];
        if (ratio > maxRatio) {
            maxRatio = ratio;
            normal = {0, 0, 0};
            normal[i] = (diff[i] > 0.0f) ? 1.0f : -1.0f;
        }
    }
    return HitResult{point, normal, tmin, 0};
}

// ── 레이-평면 교차 ──
struct Plane {
    math::Vec3f normal;
    f32 distance;  // dot(normal, pointOnPlane)
};

[[nodiscard]] inline std::optional<HitResult>
intersect(const Ray& ray, const Plane& plane) {
    f32 denom = math::dot(plane.normal, ray.direction);
    if (std::abs(denom) < 1e-6f) return std::nullopt;
    f32 t = (plane.distance - math::dot(plane.normal, ray.origin)) / denom;
    if (t < RAY_EPSILON) return std::nullopt;
    auto point = ray.at(t);
    auto normal = (denom < 0.0f) ? plane.normal : -plane.normal;
    return HitResult{point, normal, t, 0};
}

} // namespace gazeshot::core
```

### Step 4: C++20 Concepts로 제네릭 충돌 시스템

```hpp
// engine/include/gazeshot/engine/CollisionSystem.hpp
#pragma once
#include <gazeshot/core/Ray.hpp>
#include <gazeshot/core/Intersect.hpp>
#include <gazeshot/core/BoundingVolume.hpp>
#include <concepts>
#include <optional>
#include <vector>

namespace gazeshot::engine {

// ════════════════════════════════════════════
//  C++20 Concept: Intersectable
// ════════════════════════════════════════════

template<typename Shape>
concept Intersectable = requires(const Shape& s, const core::Ray& r) {
    { intersect(r, s) } -> std::same_as<std::optional<core::HitResult>>;
};

static_assert(Intersectable<core::AABB>);
static_assert(Intersectable<core::BoundingSphere>);
static_assert(Intersectable<core::Plane>);

// ── 제네릭 교차 검사: Intersectable을 만족하는 어떤 Shape든 받는다 ──
template<Intersectable Shape>
[[nodiscard]] std::optional<core::HitResult>
testIntersection(const core::Ray& ray, const Shape& shape, core::u32 entityId) {
    auto hit = intersect(ray, shape);
    if (hit) hit->entityId = entityId;
    return hit;
}

// ════════════════════════════════════════════
//  Collider & 응답 타입
// ════════════════════════════════════════════

enum class ColliderType : core::u8 { AABB, Sphere };

struct Collider {
    ColliderType type = ColliderType::Sphere;
    core::AABB aabb;
    core::BoundingSphere sphere;
    bool isObstacle = false;
};

enum class HitType : core::u8 { TargetHit, ObstacleBlock, Miss };

struct CollisionResult {
    HitType type = HitType::Miss;
    std::optional<core::HitResult> hit;
    core::u32 entityId = 0;
};

// ════════════════════════════════════════════
//  CollisionSystem
// ════════════════════════════════════════════

class CollisionSystem {
public:
    void addCollider(core::u32 entityId, const Collider& collider) {
        colliders_.push_back({entityId, collider});
    }

    void removeCollider(core::u32 entityId) {
        std::erase_if(colliders_, [entityId](const auto& e) {
            return e.entityId == entityId;
        });
    }

    void updateWorldBounds(core::u32 entityId,
                           const core::math::Vec3f& position,
                           const core::math::Vec3f& scale) {
        for (auto& entry : colliders_) {
            if (entry.entityId != entityId) continue;
            if (entry.collider.type == ColliderType::AABB) {
                auto half = entry.collider.aabb.halfExtents();
                half = {half.x * scale.x, half.y * scale.y, half.z * scale.z};
                entry.worldAABB = {position - half, position + half};
            } else {
                f32 s = std::max({scale.x, scale.y, scale.z});
                entry.worldSphere = {position, entry.collider.sphere.radius * s};
            }
            break;
        }
    }

    // ── 핵심: 가장 가까운 교차점 선택 ──
    [[nodiscard]] CollisionResult raycast(const core::Ray& ray) const {
        std::optional<core::HitResult> closestHit;
        core::u32 closestId = 0;
        bool closestIsObstacle = false;

        for (const auto& entry : colliders_) {
            std::optional<core::HitResult> hit;
            if (entry.collider.type == ColliderType::AABB)
                hit = testIntersection(ray, entry.worldAABB, entry.entityId);
            else
                hit = testIntersection(ray, entry.worldSphere, entry.entityId);

            if (hit && (!closestHit || hit->distance < closestHit->distance)) {
                closestHit = hit;
                closestId = entry.entityId;
                closestIsObstacle = entry.collider.isObstacle;
            }
        }

        if (!closestHit) return {HitType::Miss, std::nullopt, 0};
        if (closestIsObstacle) return {HitType::ObstacleBlock, closestHit, closestId};
        return {HitType::TargetHit, closestHit, closestId};
    }

    [[nodiscard]] core::u32 colliderCount() const {
        return static_cast<core::u32>(colliders_.size());
    }

private:
    struct ColliderEntry {
        core::u32 entityId;
        Collider collider;
        core::AABB worldAABB{};
        core::BoundingSphere worldSphere{};
    };
    std::vector<ColliderEntry> colliders_;
};

} // namespace gazeshot::engine
```

### Step 5: 씬 구성 시 콜라이더 등록

```cpp
// game/src/ShootingRange.cpp (Ch.12 업데이트)
#include <gazeshot/core/BoundingVolumeGen.hpp>
#include <gazeshot/engine/CollisionSystem.hpp>

void setupColliders(engine::Scene& scene, engine::CollisionSystem& collision) {
    // ── 타겟: BoundingSphere ──
    for (core::u32 i = 0; i < game::TARGETS.size(); ++i) {
        auto* entity = scene.findEntity("target_" + std::to_string(i));
        if (!entity) continue;
        auto localSphere = core::computeBoundingSphere(entity->mesh()->positions());
        engine::Collider col{.type = engine::ColliderType::Sphere,
                             .sphere = localSphere, .isObstacle = false};
        entity->setCollider(col);
        collision.addCollider(entity->id(), col);
        auto& tf = entity->transform();
        collision.updateWorldBounds(entity->id(), tf.position, tf.scale);
    }

    // ── 장애물: AABB ──
    for (core::u32 i = 0; i < game::OBSTACLES.size(); ++i) {
        auto* entity = scene.findEntity("obstacle_" + std::to_string(i));
        if (!entity) continue;
        auto localAABB = core::computeAABB(entity->mesh()->positions());
        engine::Collider col{.type = engine::ColliderType::AABB,
                             .aabb = localAABB, .isObstacle = true};
        entity->setCollider(col);
        collision.addCollider(entity->id(), col);
        auto& tf = entity->transform();
        collision.updateWorldBounds(entity->id(), tf.position, tf.scale);
    }

    std::printf("[Collision] Registered %u colliders\n", collision.colliderCount());
}
```

### Step 6: 사격 시스템과 충돌 연동

```cpp
// game/src/Shooting.cpp (Ch.12 업데이트)
void shoot(App& app) {
    auto [origin, direction] = app.camera.aimRay();
    core::Ray ray{origin, direction};
    auto result = app.collision.raycast(ray);

    switch (result.type) {
        case engine::HitType::TargetHit: {
            auto* entity = app.scene.findEntityById(result.entityId);
            if (entity) {
                entity->material().diffuse = {1, 1, 1};  // 피격 플래시
                auto& state = app.targetStates[result.entityId];
                if (!state.hit) {
                    state.hit = true;
                    app.score += state.basePoints;
                    std::printf("[Collision] HIT! %s at t=%.1f  +%upts\n",
                        entity->name().c_str(), result.hit->distance,
                        state.basePoints);
                }
            }
            app.tracer = {origin, result.hit->point, 0.3f};
            break;
        }
        case engine::HitType::ObstacleBlock: {
            auto* entity = app.scene.findEntityById(result.entityId);
            std::printf("[Collision] BLOCKED by %s at t=%.1f\n",
                entity ? entity->name().c_str() : "?", result.hit->distance);
            app.tracer = {origin, result.hit->point, 0.3f};
            break;
        }
        case engine::HitType::Miss:
            std::printf("[Collision] MISS\n");
            app.tracer = {origin, origin + direction * 200.0f, 0.3f};
            break;
    }
}
```

### Step 7: 매 프레임 월드 바운드 갱신

```cpp
// game/src/main.cpp — update()
void update(App& app, core::f32 dt) {
    // 월드 바운드 갱신 (엔티티가 움직일 수 있으므로)
    for (auto& entity : app.scene.activeEntities()) {
        if (!entity.hasCollider()) continue;
        auto& tf = entity.transform();
        app.collision.updateWorldBounds(entity.id(), tf.position, tf.scale);
    }

    if (app.input.isMouseButtonPressed(0)) shoot(app);
}
```

---

## 3. C++ 학습 포인트

### Concepts vs Virtual Functions

```
┌───────────────────┬──────────────────────────────────┐
│ virtual 함수       │ C++20 concepts                   │
├───────────────────┼──────────────────────────────────┤
│ 런타임 다형성       │ 컴파일 타임 다형성                  │
│ vtable 간접 호출    │ 직접 호출 (인라인 가능)             │
│ 상속 필요           │ 상속 불필요 (duck typing)          │
│ 런타임 오류 가능     │ 컴파일 타임에 오류 감지             │
│ 오버헤드: 함수 포인터 │ 오버헤드: 없음 (zero-cost)        │
└───────────────────┴──────────────────────────────────┘
```

virtual 방식:

```cpp
struct IIntersectable {
    virtual ~IIntersectable() = default;
    virtual std::optional<HitResult> intersect(const Ray& r) const = 0;
};
struct MySphere : IIntersectable { /* 상속 강제, vtable 오버헤드 */ };
```

concept 방식:

```cpp
template<Intersectable Shape>
auto testIntersection(const Ray& ray, const Shape& shape, u32 id);
// BoundingSphere는 아무것도 상속하지 않지만,
// intersect(Ray, BoundingSphere)가 존재하므로 Intersectable을 만족.
```

충돌 감지에 concept가 적합한 이유:
1. **성능**: 매 프레임 수백 번 호출 -- vtable 간접 호출 비용 누적
2. **자연스러움**: `intersect(ray, sphere)` 자유 함수가 더 관용적
3. **확장성**: 새 도형 추가 시 `intersect()` 오버로드만 추가, 기존 코드 수정 불필요

### `requires` 절 변형

```cpp
// 방법 1: concept를 템플릿 파라미터에 직접 (가장 간결)
template<Intersectable Shape>
auto test(const Ray& r, const Shape& s);

// 방법 2: trailing requires
template<typename Shape>
auto test(const Ray& r, const Shape& s) requires Intersectable<Shape>;

// 방법 3: 여러 제약 조합
template<typename T>
    requires Intersectable<T> && std::copyable<T>
auto process(const T& shape);
```

### `static_assert` + concept = 친절한 오류

```cpp
struct ConvexHull { ... };
// intersect(Ray, ConvexHull) 구현을 깜빡하면:
static_assert(Intersectable<ConvexHull>,
    "did you forget to implement intersect()?");
// → 템플릿 오류 수십 줄 대신, 명확한 한 줄 메시지!
```

### `std::erase_if` (C++20)

```cpp
// 기존 erase-remove idiom (3줄):
colliders_.erase(
    std::remove_if(colliders_.begin(), colliders_.end(), pred),
    colliders_.end());

// C++20 (1줄):
std::erase_if(colliders_, [id](const auto& e) { return e.entityId == id; });
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 바운딩 볼륨 생성 | 콘솔에 "Registered N colliders" 출력 |
| 타겟 피격 | 레티클이 타겟 위일 때 클릭 → "HIT!" + 점수 |
| 장애물 차단 | 기둥 뒤 타겟 사격 → "BLOCKED by obstacle_N" |
| 가장 가까운 교차점 | 기둥(t=12)이 타겟(t=28)보다 가까우면 기둥에 막힘 |
| 패럴랙스로 우회 | 머리 이동 후 사격 → 기둥 회피하여 타겟 피격 |
| 빗나감 | 빈 곳 클릭 → "MISS" + 트레이서가 멀리 사라짐 |
| concept 검증 | `static_assert`가 컴파일 통과 |
| epsilon 동작 | 레이 origin 근처에서 자기 교차 없음 |

---

## 블로그 데모 아이디어

1. **충돌 파이프라인 다이어그램**: Ray 발사 → 교차 검사 → 가장 가까운 선택 → 응답
2. **t 파라미터 비교**: 기둥 t=12 vs 타겟 t=28 → 차단되는 과정
3. **concept vs virtual 비교표**: 성능, 유연성, 오류 메시지 차이
4. **Slab Method 시각화**: Ray가 x/y/z 슬랩을 통과하는 과정
5. **장애물 우회 GIF**: WASD로 머리 이동 → 기둥 뒤 타겟 발견 → 피격 성공

---

## 다음 챕터 예고

**Chapter 13: 패럴랙스와 엿보기 메카닉**

Ch.09의 `headOffset`이 Ch.12의 충돌 시스템과 만나면 진짜 게임 메카닉이 된다.
머리 이동에 따른 시야 변화, 장애물 뒤 타겟을 엿보는 전략적 포지셔닝,
조준선(aimRay) 경로가 장애물을 피하는지 실시간 판단하는 시스템을 구현한다.

데모: 고개를 기울여 기둥 사이로 타겟을 발견하고, 정확한 타이밍에 사격하여 9개 타겟을 모두 클리어한다.
