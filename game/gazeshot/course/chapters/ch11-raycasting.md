# Chapter 11: 레이캐스팅과 탄도학

## 데모 미리보기

```
┌─────────────────────────────────────────────┐
│              ╭────────────╮                  │
│              │             │                  │
│              │  ◎──────── │─── 트레이서 궤적  │
│              │     ＋      │                  │
│              │             │                  │
│              ╰────────────╯                  │
│                                             │
│  Click: 사격 → 빨간 트레이서 라인 표시         │
│  Hit: "target_2 hit! +100pts" 콘솔 출력       │
│  Miss: 트레이서가 허공으로 사라짐              │
└─────────────────────────────────────────────┘
```

- **데모**: 클릭으로 사격, 레티클 → 타겟 방향으로 트레이서 라인 표시
- **피격**: 타겟 색상 변경 + 콘솔에 점수 출력
- 블로그에 "레이-구 교차 수학 유도" 다이어그램 포함 가능

---

## 학습 목표

1. Ray 구조체와 레이-구, 레이-AABB 교차 검사를 구현한다
2. 화면 좌표 → 월드 좌표 역변환(unproject)을 이해한다
3. 가늠자-가늠쇠 기반 조준선을 Ray로 변환한다
4. `std::optional<HitResult>`, `[[nodiscard]]`, structured bindings를 실습한다

---

## 1. 배경 지식

### 레이-구 교차 (Ray-Sphere Intersection)

```
Ray:    P(t) = origin + t * direction    (t ≥ 0)
Sphere: |P - center|² = radius²

대입하면 t에 대한 이차방정식:
  a = dot(d, d)           = 1 (direction이 정규화되었으면)
  b = 2 * dot(d, oc)      (oc = origin - center)
  c = dot(oc, oc) - r²
  판별식 = b² - 4ac

  판별식 < 0  → 교차 없음
  판별식 = 0  → 접선 (1점)
  판별식 > 0  → 관통 (2점), t = (-b ± √판별식) / 2a
```

---

## 2. 구현 가이드

### Step 1: Ray와 HitResult

```hpp
// core/include/gazeshot/core/Ray.hpp
#pragma once
#include <gazeshot/core/math/Vec3.hpp>

namespace gazeshot::core {

struct Ray {
    math::Vec3f origin;
    math::Vec3f direction;  // 반드시 정규화

    [[nodiscard]] constexpr math::Vec3f at(f32 t) const {
        return origin + direction * t;
    }
};

struct HitResult {
    math::Vec3f point;       // 교차점
    math::Vec3f normal;      // 교차점의 법선
    f32 distance;            // origin에서 교차점까지 거리
    u32 entityId = 0;        // 맞은 엔티티
};

} // namespace gazeshot::core
```

### Step 2: 교차 검사

```hpp
// core/include/gazeshot/core/Intersect.hpp
#pragma once
#include <gazeshot/core/Ray.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <optional>
#include <cmath>
#include <algorithm>

namespace gazeshot::core {

// ── 레이-구 교차 ──
[[nodiscard]] inline std::optional<HitResult>
intersectSphere(const Ray& ray, const math::Vec3f& center, f32 radius) {
    auto oc = ray.origin - center;
    f32 a = math::dot(ray.direction, ray.direction);
    f32 halfB = math::dot(oc, ray.direction);
    f32 c = math::dot(oc, oc) - radius * radius;
    f32 discriminant = halfB * halfB - a * c;

    if (discriminant < 0) return std::nullopt;

    f32 sqrtD = std::sqrt(discriminant);
    f32 t = (-halfB - sqrtD) / a;
    if (t < 0.001f) {
        t = (-halfB + sqrtD) / a;
        if (t < 0.001f) return std::nullopt;
    }

    auto point = ray.at(t);
    auto normal = math::normalize(point - center);
    return HitResult{point, normal, t, 0};
}

// ── 레이-AABB 교차 (Slab method) ──
struct AABB {
    math::Vec3f min, max;
};

[[nodiscard]] inline std::optional<HitResult>
intersectAABB(const Ray& ray, const AABB& box) {
    f32 tmin = 0.001f, tmax = 1e30f;
    for (int i = 0; i < 3; ++i) {
        f32 invD = 1.0f / ray.direction[i];
        f32 t0 = (box.min[i] - ray.origin[i]) * invD;
        f32 t1 = (box.max[i] - ray.origin[i]) * invD;
        if (invD < 0) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin) return std::nullopt;
    }
    auto point = ray.at(tmin);
    return HitResult{point, {}, tmin, 0};
}

} // namespace gazeshot::core
```

### Step 3: 씬 레이캐스트

```cpp
// 씬에서 가장 가까운 교차점 찾기
std::optional<HitResult> sceneRaycast(const Ray& ray, Scene& scene) {
    std::optional<HitResult> closest;

    for (auto& entity : scene.activeEntities()) {
        if (!entity.hasMesh()) continue;
        auto& pos = entity.transform().position;
        auto& scl = entity.transform().scale;
        f32 radius = std::max({scl.x, scl.y, scl.z}) * 0.5f;

        auto hit = intersectSphere(ray, pos, radius);
        if (hit && (!closest || hit->distance < closest->distance)) {
            hit->entityId = entity.id();
            closest = hit;
        }
    }
    return closest;
}
```

### Step 4: 사격 연동

```cpp
void shoot(App& app) {
    auto [origin, direction] = app.camera.aimRay();
    Ray ray{origin, direction};

    if (auto hit = sceneRaycast(ray, app.scene)) {
        auto* entity = app.scene.findEntityById(hit->entityId);
        if (entity && entity->name().starts_with("target_")) {
            entity->material().diffuse = {1, 1, 1};  // 피격 플래시
            std::printf("HIT: %s at dist %.1fm\n",
                entity->name().c_str(), hit->distance);
        }
        app.tracer = {origin, hit->point, 0.3f};  // 트레이서 라인
    } else {
        app.tracer = {origin, origin + direction * 100.0f, 0.3f};
    }
}
```

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 사격 가능 | 클릭 시 트레이서 라인 표시 |
| 타겟 피격 | 레티클이 타겟 위일 때 클릭 → 색상 변경 |
| 장애물 차단 | 기둥 뒤 타겟은 기둥에 먼저 맞음 |
| 빗나감 | 빈 곳 클릭 → 트레이서가 멀리 사라짐 |

---

## 블로그 데모 아이디어

1. **레이-구 교차 다이어그램**: 판별식 시각화
2. **트레이서 GIF**: 사격 시 빨간 선이 타겟까지
3. **장애물 차단**: 기둥에 맞는 트레이서 vs 패럴랙스로 비켜서 타겟 피격

---

## 다음 챕터 예고

**Chapter 12: 충돌 감지** — AABB, BoundingSphere를 체계화하고 concepts로 제네릭화한다.
