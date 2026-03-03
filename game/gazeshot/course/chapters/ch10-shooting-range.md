# Chapter 10: 사격장 씬 구성

## 데모 미리보기

```
┌─────────────────────────────────────────────────────┐
│                   사격장 레이아웃 (탑뷰)              │
│                                                     │
│   10m (근거리)    30m (중거리)      60m (원거리)      │
│                                                     │
│    ◎  ◎  ◎     ┃ ◎  ◎ ┃ ◎     ┃ ◎ ┃ ◎ ┃◎        │
│                 ┃      ┃       ┃   ┃   ┃          │
│                 기둥   기둥     기둥 기둥 기둥        │
│                                                     │
│                  ● ← 플레이어                        │
├─────────────────────────────────────────────────────┤
│              ╭────────────╮                          │
│              │ 스코프 뷰   │                          │
│              │  ◎ ┃       │  ← 기둥 뒤에 타겟 보임     │
│              │     ◎  ＋  │     WASD로 엿보기!        │
│              ╰────────────╯                          │
│  Targets: 0/9 hit  |  Zoom: 4.0x                    │
└─────────────────────────────────────────────────────┘
```

- **데모**: 완전한 사격장 — 9개 타겟, 기둥 장애물, 3단계 거리
- **패럴랙스**: WASD로 머리를 움직여 기둥 뒤 타겟 발견
- **거리감**: 원근 투영으로 먼 타겟이 자연스럽게 작게 보임
- 블로그에 "사격장 레이아웃 설계"와 "패럴랙스로 엿보기" GIF 포함 가능

---

## 학습 목표

1. 게임 레벨을 데이터로 설계하고 코드로 구현한다
2. 타겟과 장애물의 배치 전략을 이해한다
3. 패럴랙스 효과가 실제로 동작하는 것을 확인한다
4. designated initializers, `constexpr` 배열, `enum class`를 실습한다

---

## 1. 배경 지식

### 레벨 설계 원칙

```
1. 난이도 그래디언트: 가까운 것 → 먼 것
2. 장애물 밀도: 근거리 0개, 중거리 1~2개, 원거리 2~3개
3. 패럴랙스 유도: 장애물이 타겟을 "거의" 가리게 배치
   → 약간의 머리 이동으로 발견 가능하도록
4. 스케일: 미터 단위 (사람 높이 1.7m, 타겟 직경 0.5m)
```

### 월드 스케일

```
1 유닛 = 1 미터

플레이어: y=1.6 (눈 높이)
타겟:     직경 0.5m, 중심 높이 1.5m
기둥:     반지름 0.15m, 높이 3m
바닥:     y=0
```

---

## 2. 구현 가이드

### Step 1: 레벨 데이터 정의

```hpp
// game/include/gazeshot/game/LevelData.hpp

#pragma once

#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/Types.hpp>

#include <array>

namespace gazeshot::game {

enum class Distance : core::u8 { Near, Mid, Far };

struct TargetDef {
    core::math::Vec3f position;
    core::f32 radius;
    Distance distance;
    core::u32 basePoints;
    bool partiallyHidden;  // 장애물에 부분 가려짐
};

struct ObstacleDef {
    core::math::Vec3f position;
    core::f32 radius;
    core::f32 height;
};

// ── 레벨 데이터 (constexpr) ──

constexpr core::f32 NEAR_Z  = -10.0f;
constexpr core::f32 MID_Z   = -30.0f;
constexpr core::f32 FAR_Z   = -60.0f;

constexpr std::array<TargetDef, 9> TARGETS = {{
    // ── 근거리 (10m) — 장애물 없음, 큰 타겟 ──
    {{ -2.0f, 1.5f, NEAR_Z}, 0.35f, Distance::Near, 50,  false},
    {{  0.0f, 1.5f, NEAR_Z}, 0.35f, Distance::Near, 50,  false},
    {{  2.0f, 1.5f, NEAR_Z}, 0.35f, Distance::Near, 50,  false},

    // ── 중거리 (30m) — 기둥 1~2개, 중간 타겟 ──
    {{ -2.5f, 1.5f, MID_Z},  0.30f, Distance::Mid,  100, false},
    {{  0.0f, 1.5f, MID_Z},  0.30f, Distance::Mid,  100, true },  // 기둥에 가려짐
    {{  2.5f, 1.5f, MID_Z},  0.30f, Distance::Mid,  100, false},

    // ── 원거리 (60m) — 기둥 2~3개, 작은 타겟 ──
    {{ -2.0f, 1.5f, FAR_Z},  0.25f, Distance::Far,  200, true },  // 가려짐
    {{  0.5f, 1.5f, FAR_Z},  0.25f, Distance::Far,  200, true },  // 가려짐
    {{  3.0f, 1.5f, FAR_Z},  0.25f, Distance::Far,  200, false},
}};

constexpr std::array<ObstacleDef, 5> OBSTACLES = {{
    // 중거리 기둥
    {{ -0.3f, 1.5f, MID_Z + 2.0f},  0.15f, 3.0f},  // 타겟 4번 앞
    {{  1.8f, 1.5f, MID_Z + 1.5f},  0.15f, 3.0f},

    // 원거리 기둥
    {{ -1.5f, 1.5f, FAR_Z + 3.0f},  0.15f, 3.0f},  // 타겟 6번 앞
    {{  0.8f, 1.5f, FAR_Z + 4.0f},  0.15f, 3.0f},  // 타겟 7번 앞
    {{  1.2f, 1.5f, FAR_Z + 2.5f},  0.12f, 3.0f},
}};

} // namespace gazeshot::game
```

**C++ 학습 포인트: designated initializers + constexpr 배열**

```cpp
constexpr std::array<TargetDef, 9> TARGETS = {{
    {{ -2.0f, 1.5f, -10.0f}, 0.35f, Distance::Near, 50, false},
    //  position              radius  distance        pts hidden
}};
```

이 데이터는:
- **컴파일 타임에 확정** → 런타임 초기화 비용 0
- **타입 안전** → `Distance::Near` (enum class)
- **선언적** → 코드를 읽으면 레벨 배치가 보인다

### Step 2: Target 엔티티

```hpp
// game/include/gazeshot/game/Target.hpp

#pragma once

#include <gazeshot/engine/Entity.hpp>
#include <gazeshot/game/LevelData.hpp>

namespace gazeshot::game {

// 타겟 상태를 Entity에 추가 데이터로 관리
struct TargetState {
    Distance distance = Distance::Near;
    core::u32 basePoints = 0;
    bool hit = false;
    core::f32 hitAnimTimer = 0.0f;  // 피격 애니메이션
};

} // namespace gazeshot::game
```

### Step 3: 씬 구성

```cpp
// game/src/ShootingRange.cpp

#include <gazeshot/game/LevelData.hpp>
#include <gazeshot/engine/Scene.hpp>
#include <gazeshot/engine/MeshGen.hpp>

using namespace gazeshot;
using namespace core::math;

void buildShootingRange(engine::Scene& scene, App& app) {
    // ── 바닥 ──
    auto& floor = scene.createEntity("floor");
    floor.setMesh(&app.planeMesh);
    floor.transform().position = {0, 0, -35};
    floor.transform().scale = {20, 1, 80};
    floor.material() = {
        .ambient  = {0.15f, 0.15f, 0.12f},
        .diffuse  = {0.3f, 0.3f, 0.25f},
        .specular = {0.1f, 0.1f, 0.1f},
        .shininess = 4.0f,
    };

    // ── 타겟 ──
    for (core::u32 i = 0; i < TARGETS.size(); ++i) {
        auto& def = TARGETS[i];
        auto name = std::string("target_") + std::to_string(i);
        auto& entity = scene.createEntity(name);
        entity.setMesh(&app.sphereMesh);
        entity.transform().position = def.position;
        entity.transform().scale = Vec3f(def.radius * 2.0f);

        // 거리별 색상
        switch (def.distance) {
            case Distance::Near:
                entity.material().diffuse = {0.2f, 0.8f, 0.3f};  // 초록
                break;
            case Distance::Mid:
                entity.material().diffuse = {0.8f, 0.7f, 0.1f};  // 노랑
                break;
            case Distance::Far:
                entity.material().diffuse = {0.9f, 0.2f, 0.2f};  // 빨강
                break;
        }
        entity.material().specular = {0.8f, 0.8f, 0.8f};
        entity.material().shininess = 64.0f;
    }

    // ── 장애물 (기둥) ──
    for (core::u32 i = 0; i < OBSTACLES.size(); ++i) {
        auto& def = OBSTACLES[i];
        auto name = std::string("obstacle_") + std::to_string(i);
        auto& entity = scene.createEntity(name);
        entity.setMesh(&app.cylinderMesh);
        entity.transform().position = def.position;
        entity.transform().scale = {def.radius * 2, def.height, def.radius * 2};
        entity.material() = {
            .ambient  = {0.1f, 0.1f, 0.1f},
            .diffuse  = {0.35f, 0.3f, 0.25f},
            .specular = {0.2f, 0.2f, 0.2f},
            .shininess = 8.0f,
        };
    }

    // ── 거리 표시 기둥 (좌우 경계) ──
    for (float x : {-5.0f, 5.0f}) {
        for (float z : {NEAR_Z, MID_Z, FAR_Z}) {
            auto& post = scene.createEntity("post");
            post.setMesh(&app.cylinderMesh);
            post.transform().position = {x, 1.0f, z};
            post.transform().scale = {0.1f, 2.0f, 0.1f};
            post.material().diffuse = {0.5f, 0.5f, 0.5f};
        }
    }
}
```

### Step 4: 패럴랙스 검증

패럴랙스는 별도 코드 없이 자동으로 동작한다:

```
SniperCamera의 headOffset 변화
  → viewMatrix()에서 카메라 위치 변경
    → perspective 투영에서 가까운 물체가 더 많이 이동
      → 장애물 뒤의 타겟이 보임!
```

검증 방법:
1. 중거리 타겟 4번을 정면으로 바라봄 → 기둥에 가려져 안 보임
2. 'A' 키로 머리를 왼쪽으로 이동 → 기둥 오른쪽으로 타겟이 나타남
3. 'D' 키로 오른쪽 이동 → 기둥 왼쪽으로 타겟이 나타남

### Step 5: 거리감 강화 (안개)

선택적으로 간단한 거리 안개를 추가:

```glsl
// phong.frag에 추가

uniform float uFogStart;  // 30.0
uniform float uFogEnd;    // 80.0
uniform vec3 uFogColor;   // 배경색과 동일

void main() {
    // ... 기존 Phong 계산 ...

    // 거리 기반 안개
    float dist = length(vWorldPos - uViewPos);
    float fogFactor = clamp((uFogEnd - dist) / (uFogEnd - uFogStart), 0.0, 1.0);
    color = mix(uFogColor, color, fogFactor);

    FragColor = vec4(color, 1.0);
}
```

안개 효과:
- 근거리 타겟: 선명
- 원거리 타겟: 배경색에 섞여 흐릿 → 거리감 강화

### Step 6: 디버그 탑뷰

```cpp
// 디버그 모드: F1으로 탑뷰 토글
if (app.input.isKeyPressed(SDLK_F1)) {
    app.debugTopView = !app.debugTopView;
}

if (app.debugTopView) {
    // 위에서 내려다보는 직교 투영
    view = lookAt(Vec3f{0, 50, -35}, Vec3f{0, 0, -35}, Vec3f{0, 0, -1});
    proj = ortho(-15.0f, 15.0f, -45.0f, 15.0f, 0.1f, 100.0f);

    // 카메라 위치, 조준선, 시야각을 라인으로 표시
    // (라인 렌더링은 간이로 GL_LINES 사용)
}
```

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 9개 타겟 | 근거리 3 + 중거리 3 + 원거리 3 |
| 거리별 색상 | 근(초록), 중(노랑), 원(빨강) |
| 기둥 배치 | 5개 기둥이 일부 타겟을 가림 |
| 패럴랙스 | WASD로 가려진 타겟 발견 가능 |
| 원근감 | 원거리 타겟이 작게 보임 |
| 안개 (선택) | 원거리가 흐릿 |
| 탑뷰 (F1) | 레이아웃 전체 확인 가능 |
| 스코프 뷰 | Ch.09의 스코프+레티클이 사격장에서 동작 |

---

## 4. 블로그 데모 아이디어

1. **탑뷰 레이아웃**: F1 디버그 뷰로 전체 배치 스크린샷
2. **패럴랙스 GIF**: 머리 이동 → 기둥 뒤 타겟 발견 과정
3. **거리별 뷰 비교**: 근거리(크고 선명) vs 원거리(작고 흐릿)
4. **스코프 뷰**: 사격장을 스코프로 바라본 스크린샷
5. **레벨 데이터**: `constexpr` 배열로 레벨을 "선언적으로 정의"하는 코드

---

## 5. Phase B 완성!

Chapter 06~10을 마치면:

| 구성요소 | 상태 |
|---------|------|
| 프로시저럴 메시 | 구, 박스, 실린더, 평면 생성기 |
| 라이팅 | Phong (Ambient + Diffuse + Specular) |
| 씬 관리 | Entity + Scene + 활성/비활성 |
| 스나이퍼 카메라 | 가늠자/가늠쇠 분리, 스코프 뷰 |
| 사격장 | 9 타겟 + 5 장애물 + 패럴랙스 |

**이 시점에서 "사격장을 스코프로 바라보고, 머리를 움직여 숨겨진 타겟을 발견하는" 핵심 경험이 동작한다.**

---

## 다음: Phase C 예고

**Chapter 11: 레이캐스팅과 탄도학**

실제로 사격한다! 레티클 위치에서 Ray를 쏘아 타겟 피격을 판정한다.
데모: 마우스 클릭으로 사격, 타겟 피격 시 색상 변경 + 트레이서 궤적 시각화.
