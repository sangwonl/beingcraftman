# Chapter 13: 패럴랙스와 엿보기 메카닉

## 데모 미리보기

```
┌─────────────────────────────────────────────────────────┐
│                  탑뷰 디버그 (F2)                        │
│                                                         │
│   obstacle_1 ┃         ★ target_4                       │
│              ┃        ╱                                  │
│              ┃      ╱   ← 시선(LOS)                     │
│              ┃    ╱                                      │
│              ┃  ╱                                        │
│           ●──╱── headOffset →                           │
│           카메라                                         │
│                                                         │
├─────────────────────────────────────────────────────────┤
│              ╭────────────╮                              │
│          ╭┄┄┄│            │╮  ← 엿보기 시 가장자리 변화    │
│         ┆    │  ◎  ★  ＋  │ ┆                            │
│         ┆    │     ┃      │ ┆    Q/E: 좌우 엿보기         │
│          ╰┄┄┄│            │╯     R/F: 상하 엿보기         │
│              ╰────────────╯                              │
│  Peek: (0.12, 0.00) | Visible: 6/9 | LOS: clear        │
└─────────────────────────────────────────────────────────┘
```

- **데모**: Q/E로 머리를 좌우로 기울여 장애물 뒤 타겟을 발견
- **디버그 탑뷰**: F2로 카메라-장애물-타겟의 기하학적 관계를 시각화
- **스코프 피드백**: 엿보기 시 스코프 가장자리가 한쪽으로 치우침 (시각적 단서)
- 블로그에 "패럴랙스 수학"과 "엿보기 전/후 비교 GIF" 포함 가능

---

## 학습 목표

1. 패럴랙스(시차) 효과의 수학적 원리를 이해하고 게임 메카닉으로 활용한다
2. 엿보기(peek) 시스템을 구현하여 장애물 뒤 타겟을 발견하는 게임플레이를 만든다
3. 부드러운 보간(easing)과 복귀 동작을 구현한다
4. 고차 함수(higher-order functions)와 `std::function`, `std::invoke`를 실습한다

---

## 1. 배경 지식

### 패럴랙스 원리: 왜 가까운 것이 더 많이 움직이는가?

카메라(가늠자)를 옆으로 옮기면, 가까운 물체와 먼 물체의 화면상 이동량이 다르다:

```
카메라가 Δx만큼 이동했을 때 화면상 이동량:

  화면 이동량 ∝ Δx / distance

  가까운 장애물 (d=5m):  Δx / 5  = 큰 이동
  먼 타겟 (d=30m):       Δx / 30 = 작은 이동

  → 가까운 장애물이 더 많이 밀려남 → 뒤에 숨겨진 타겟이 드러남!

수학적으로:
  카메라 (cx, cy, cz), 물체 (px, py, pz)
  screenX = focal * (px - cx) / (pz - cz)

  cx가 Δcx만큼 변하면:
    ΔscreenX = focal * (-Δcx) / (pz - cz)

  즉 (pz - cz)가 작을수록 (가까울수록) 화면 이동이 크다.
```

### 엿보기 메카닉 설계

```
게임플레이 흐름:
1. 스코프로 사격장을 바라봄 → 일부 타겟이 기둥에 가려짐
2. Q/E키로 머리를 좌우로 기울임 (peek)
3. 패럴랙스에 의해 가까운 기둥이 밀려나고 뒤의 타겟이 드러남
4. 드러난 타겟을 조준하여 사격!

시각적 피드백:
- 엿보기 중: 스코프 가장자리가 엿보기 반대쪽으로 어두워짐
  (실제 스코프에서 눈을 옮기면 가장자리가 어두워지는 현상)
- 복귀 시: 부드러운 easing으로 자연스럽게 원위치
```

### 가시성 판정

카메라 → 타겟 방향으로 Ray를 쏘아 중간 장애물 교차를 확인하는 **Raycast 방식**을 사용한다.
GPU 기반 Occlusion Query(`GL_ANY_SAMPLES_PASSED`)도 가능하지만, 1프레임 지연과
CPU-GPU 동기화 비용이 있다. 구체 타겟 + 실린더 장애물인 이 프로젝트에는 raycast가 충분하다.

---

## 2. 구현 가이드

### Step 1: PeekController 클래스

```hpp
// game/include/gazeshot/game/PeekController.hpp

#pragma once

#include <gazeshot/game/SniperCamera.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/core/Types.hpp>

#include <functional>
#include <cmath>

namespace gazeshot::game {

// ── 이징 함수 타입 ──
using EaseFunc = std::function<float(float)>;

// ── 기본 이징 함수들 ──
inline EaseFunc easeLinear    = [](float t) { return t; };
inline EaseFunc easeOutQuad   = [](float t) { return t * (2.0f - t); };
inline EaseFunc easeOutCubic  = [](float t) {
    float u = 1.0f - t; return 1.0f - u * u * u;
};

class PeekController {
public:
    struct Config {
        core::f32 maxPeekX = 0.15f;   // 좌우 ±15cm
        core::f32 maxPeekY = 0.10f;   // 상하 ±10cm
        core::f32 peekSpeed = 0.4f;
        core::f32 returnSpeed = 2.5f;
        EaseFunc peekEase = easeOutQuad;
        EaseFunc returnEase = easeOutCubic;
    };

    explicit PeekController(const Config& config = {})
        : config_(config) {}

    // ── 엿보기 입력 ──
    void peekHorizontal(core::f32 direction, core::f32 dt) {
        targetPeek_.x += direction * config_.peekSpeed * dt;
        targetPeek_.x = std::clamp(targetPeek_.x,
            -config_.maxPeekX, config_.maxPeekX);
        peeking_ = true;
    }

    void peekVertical(core::f32 direction, core::f32 dt) {
        targetPeek_.y += direction * config_.peekSpeed * dt;
        targetPeek_.y = std::clamp(targetPeek_.y,
            -config_.maxPeekY, config_.maxPeekY);
        peeking_ = true;
    }

    // ── 복귀 (키를 뗐을 때) ──
    void returnToCenter(core::f32 dt) {
        if (!peeking_) return;
        core::f32 dist = std::sqrt(
            targetPeek_.x * targetPeek_.x + targetPeek_.y * targetPeek_.y);
        if (dist < 0.001f) {
            targetPeek_ = {0.0f, 0.0f};
            peeking_ = false;
            return;
        }
        core::f32 easedT = std::invoke(config_.returnEase,
            std::min(config_.returnSpeed * dt, 1.0f));
        targetPeek_.x = core::math::lerp(targetPeek_.x, 0.0f, easedT);
        targetPeek_.y = core::math::lerp(targetPeek_.y, 0.0f, easedT);
    }

    // ── 매 프레임 업데이트: SniperCamera에 적용 ──
    void update(SniperCamera& camera, core::f32 dt) {
        core::f32 easedT = std::invoke(config_.peekEase,
            std::min(8.0f * dt, 1.0f));
        currentPeek_.x = core::math::lerp(currentPeek_.x, targetPeek_.x, easedT);
        currentPeek_.y = core::math::lerp(currentPeek_.y, targetPeek_.y, easedT);
        camera.applyHeadOffset(currentPeek_);
    }

    // ── 접근자 ──
    [[nodiscard]] bool isPeeking() const { return peeking_; }
    [[nodiscard]] core::math::Vec2f currentPeek() const { return currentPeek_; }

    // 엿보기 강도 (0~1): 스코프 가장자리 효과에 사용
    [[nodiscard]] core::f32 peekIntensity() const {
        core::f32 maxOff = std::max(config_.maxPeekX, config_.maxPeekY);
        core::f32 cur = std::sqrt(
            currentPeek_.x * currentPeek_.x + currentPeek_.y * currentPeek_.y);
        return std::clamp(cur / maxOff, 0.0f, 1.0f);
    }

    // 엿보기 방향 (정규화): 스코프 비네트 방향에 사용
    [[nodiscard]] core::math::Vec2f peekDirection() const {
        core::f32 len = std::sqrt(
            currentPeek_.x * currentPeek_.x + currentPeek_.y * currentPeek_.y);
        if (len < 0.001f) return {0.0f, 0.0f};
        return {currentPeek_.x / len, currentPeek_.y / len};
    }

    void setPeekEase(EaseFunc func) { config_.peekEase = std::move(func); }
    void setReturnEase(EaseFunc func) { config_.returnEase = std::move(func); }

private:
    Config config_;
    core::math::Vec2f targetPeek_{};
    core::math::Vec2f currentPeek_{};
    bool peeking_ = false;
};

} // namespace gazeshot::game
```

### Step 2: SniperCamera에 applyHeadOffset 추가

```hpp
// SniperCamera에 추가 (Ch.09 확장)

class SniperCamera {
public:
    // ... 기존 코드 ...

    // PeekController가 직접 headOffset을 설정
    void applyHeadOffset(core::math::Vec2f offset) {
        targetHeadOffset_ = offset;
    }

    // headOffset 정규화 크기 (0~1)
    [[nodiscard]] core::f32 headOffsetMagnitude() const {
        core::f32 mx = config_.maxHeadOffsetX;
        core::f32 my = config_.maxHeadOffsetY;
        if (mx < 0.001f && my < 0.001f) return 0.0f;
        return std::sqrt(
            (headOffset_.x / mx) * (headOffset_.x / mx) +
            (headOffset_.y / my) * (headOffset_.y / my)
        ) / std::sqrt(2.0f);
    }
};
```

### Step 3: 가시성 판정 (VisibilityChecker)

```hpp
// game/include/gazeshot/game/VisibilityChecker.hpp

#pragma once

#include <gazeshot/core/Ray.hpp>
#include <gazeshot/core/Intersect.hpp>
#include <gazeshot/game/LevelData.hpp>

#include <vector>

namespace gazeshot::game {

struct VisibilityResult {
    core::u32 targetIndex;
    bool visible;
    core::f32 distance;
    core::u32 blockingObstacle;  // 없으면 UINT32_MAX
};

class VisibilityChecker {
public:
    [[nodiscard]] std::vector<VisibilityResult>
    checkAll(const core::math::Vec3f& cameraPos) const {
        std::vector<VisibilityResult> results;
        results.reserve(TARGETS.size());
        for (core::u32 i = 0; i < TARGETS.size(); ++i)
            results.push_back(checkTarget(cameraPos, i));
        return results;
    }

    [[nodiscard]] VisibilityResult
    checkTarget(const core::math::Vec3f& cameraPos, core::u32 idx) const {
        auto& target = TARGETS[idx];
        auto toTarget = target.position - cameraPos;
        core::f32 targetDist = core::math::length(toTarget);
        core::Ray ray{cameraPos, core::math::normalize(toTarget)};

        for (core::u32 j = 0; j < OBSTACLES.size(); ++j) {
            auto& obs = OBSTACLES[j];
            core::AABB obsBox{
                {obs.position.x - obs.radius, obs.position.y - obs.height * 0.5f,
                 obs.position.z - obs.radius},
                {obs.position.x + obs.radius, obs.position.y + obs.height * 0.5f,
                 obs.position.z + obs.radius}
            };
            auto hit = core::intersectAABB(ray, obsBox);
            if (hit && hit->distance < targetDist)
                return {idx, false, targetDist, j};
        }
        return {idx, true, targetDist, UINT32_MAX};
    }

    [[nodiscard]] core::u32
    visibleCount(const core::math::Vec3f& cameraPos) const {
        core::u32 count = 0;
        for (core::u32 i = 0; i < TARGETS.size(); ++i)
            if (checkTarget(cameraPos, i).visible) ++count;
        return count;
    }
};

} // namespace gazeshot::game
```

### Step 4: 스코프 가장자리 피드백 셰이더

```glsl
// scope_peek.frag — 엿보기 시 비대칭 비네트 효과

uniform vec2 uPeekDirection;   // 엿보기 방향 (정규화)
uniform float uPeekIntensity;  // 엿보기 강도 (0~1)
uniform float uScopeRadius;

void main() {
    vec3 color = ambient + diffuse + specular;  // 기존 Phong 결과

    vec2 screenUV = gl_FragCoord.xy / vec2(uScreenSize) * 2.0 - 1.0;
    float dist = length(screenUV);

    if (dist > uScopeRadius) {
        color *= 0.05;  // 스코프 밖
    } else {
        // 엿보기 반대쪽이 더 어두워짐
        float edgeFactor = dist / uScopeRadius;
        float peekDot = dot(normalize(screenUV), -uPeekDirection);
        float peekDarken = max(peekDot * uPeekIntensity * edgeFactor * edgeFactor, 0.0);

        float vignette = 1.0 - edgeFactor * edgeFactor * 0.3 - peekDarken * 0.5;
        color *= clamp(vignette, 0.1, 1.0);
    }

    FragColor = vec4(color, 1.0);
}
```

### Step 5: 디버그 탑뷰 시각화

```cpp
// game/src/DebugView.cpp

namespace gazeshot::game {

struct DebugLine {
    core::math::Vec3f start, end, color;
};

std::vector<DebugLine> buildDebugLines(
    const core::math::Vec3f& cameraPos,
    const VisibilityChecker& checker)
{
    std::vector<DebugLine> lines;
    auto results = checker.checkAll(cameraPos);

    for (auto& r : results) {
        auto& target = TARGETS[r.targetIndex];
        if (r.visible) {
            // 시선 통과: 초록
            lines.push_back({cameraPos, target.position, {0.2f, 0.9f, 0.3f}});
        } else {
            // 차단: 빨강(카메라→장애물) + 회색(장애물→타겟)
            auto& obs = OBSTACLES[r.blockingObstacle];
            lines.push_back({cameraPos, obs.position, {0.9f, 0.2f, 0.2f}});
            lines.push_back({obs.position, target.position, {0.5f, 0.5f, 0.5f}});
        }
    }

    // headOffset 벡터 (노란 선)
    core::math::Vec3f basePos{0.0f, 1.6f, cameraPos.z};
    lines.push_back({basePos, cameraPos, {1.0f, 1.0f, 0.2f}});

    return lines;
}

} // namespace gazeshot::game
```

### Step 6: 게임 루프 통합

```cpp
// game/src/main.cpp (Ch.13)

#include <gazeshot/game/PeekController.hpp>
#include <gazeshot/game/VisibilityChecker.hpp>

using namespace gazeshot;
using namespace gazeshot::game;

struct App {
    SniperCamera camera;
    PeekController peekCtrl;
    VisibilityChecker visChecker;
    bool debugTopView = false;
    // ... 기존 멤버 ...
};

void update(App& app, core::f32 dt) {
    auto& peek = app.peekCtrl;

    // ── 엿보기 입력 (Q/E: 좌우, R/F: 상하) ──
    bool peekInput = false;
    if (app.input.isKeyHeld(SDLK_q)) { peek.peekHorizontal(-1, dt); peekInput = true; }
    if (app.input.isKeyHeld(SDLK_e)) { peek.peekHorizontal( 1, dt); peekInput = true; }
    if (app.input.isKeyHeld(SDLK_r)) { peek.peekVertical( 1, dt);   peekInput = true; }
    if (app.input.isKeyHeld(SDLK_f)) { peek.peekVertical(-1, dt);   peekInput = true; }
    if (!peekInput) peek.returnToCenter(dt);

    peek.update(app.camera, dt);
    app.camera.update(dt);

    // 가늠쇠 (마우스) — 기존과 동일
    auto mouseDelta = app.input.mouseDelta();
    app.camera.moveGaze(mouseDelta.x, mouseDelta.y);

    // 디버그 뷰 토글
    if (app.input.isKeyPressed(SDLK_F2)) app.debugTopView = !app.debugTopView;

    // 가시성 디버그 출력
    static core::f32 logTimer = 0;
    logTimer += dt;
    if (logTimer >= 0.5f) {
        auto ho = app.camera.headOffset();
        auto cameraPos = app.camera.position() + core::math::Vec3f{ho.x, ho.y, 0};
        std::printf("Peek: (%.2f, %.2f) | Visible: %u/%zu | %s\n",
            ho.x, ho.y, app.visChecker.visibleCount(cameraPos),
            TARGETS.size(), peek.isPeeking() ? "PEEKING" : "center");
        logTimer = 0;
    }
}

void render(App& app, core::f32 alpha) {
    auto& cam = app.camera;
    core::f32 aspect = static_cast<core::f32>(app.window.width())
                     / static_cast<core::f32>(app.window.height());

    if (app.debugTopView) {
        // 탑뷰: 직교 투영으로 전체 레이아웃 + 시선 라인
        using namespace core::math;
        auto view = lookAt({0, 50, -35}, {0, 0, -35}, {0, 0, -1});
        auto proj = ortho(-15.0f, 15.0f, -45.0f, 15.0f, 0.1f, 100.0f);
        app.scene.render(*app.renderer, *app.shader, view, proj, {0, 50, -35});

        auto ho = cam.headOffset();
        auto debugLines = buildDebugLines(
            cam.position() + Vec3f{ho.x, ho.y, 0}, app.visChecker);
        renderDebugLines(debugLines, *app.renderer, *app.lineShader, view, proj);
    } else {
        // 일반 스코프 뷰 + 엿보기 시각 피드백
        auto view = cam.viewMatrix();
        auto proj = cam.projectionMatrix(aspect);
        auto ho = cam.headOffset();
        app.scene.render(*app.renderer, *app.shader, view, proj,
                         cam.position() + core::math::Vec3f{ho.x, ho.y, 0});

        app.shader->setVec2("uPeekDirection", app.peekCtrl.peekDirection());
        app.shader->setFloat("uPeekIntensity", app.peekCtrl.peekIntensity());
    }
}
```

---

## 3. C++ 학습 포인트

### `std::function`과 고차 함수

```cpp
// EaseFunc = "float를 받아 float를 반환하는 모든 호출 가능 객체"
using EaseFunc = std::function<float(float)>;

// 람다로 정의
EaseFunc easeOutQuad = [](float t) { return t * (2.0f - t); };

// 함수를 "값"처럼 전달 — 함수형 프로그래밍의 핵심
controller.setPeekEase([](float t) { return t * t * t; });
```

`std::function`은 타입 소거로 어떤 callable이든 담을 수 있다.
힙 할당이 발생할 수 있지만, 프레임당 1~2회 호출에는 충분하다.

### `std::invoke` — 균일한 호출 인터페이스

```cpp
// 함수, 람다, 멤버 함수 포인터를 동일한 문법으로 호출
float linearEase(float t) { return t; }
auto r1 = std::invoke(linearEase, 0.5f);                   // 일반 함수
auto r2 = std::invoke([](float t){ return t*t; }, 0.5f);   // 람다

// 제네릭 코드에서 특히 유용:
template<typename F, typename... Args>
auto apply(F&& f, Args&&... args) {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}
```

### 함수형 패턴: 함수를 값으로

```cpp
// 함수 팩토리: 파라미터화된 이징 생성
auto makeEaseOutPow(float power) -> EaseFunc {
    return [power](float t) -> float {
        return 1.0f - std::pow(1.0f - t, power);
    };
}
controller.setPeekEase(makeEaseOutPow(3.0f));  // cubic
controller.setPeekEase(makeEaseOutPow(5.0f));  // quintic

// 함수 합성
auto compose(EaseFunc first, EaseFunc second) -> EaseFunc {
    return [f = std::move(first), s = std::move(second)](float t) {
        return s(f(t));
    };
}
auto composed = compose(easeLinear, easeOutQuad);
```

함수를 변수에 저장, 인자로 전달, 반환값으로 사용, 합성할 수 있다.
OOP의 `IEaseStrategy` 인터페이스 + 상속 대신,
`std::function` + 람다로 같은 목적을 훨씬 간결하게 달성한다.

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| Q/E 엿보기 | Q: 왼쪽, E: 오른쪽으로 시점 이동 |
| R/F 엿보기 | R: 위, F: 아래로 시점 이동 |
| 범위 제한 | ±15cm(좌우), ±10cm(상하) 초과 불가 |
| 부드러운 이동 | easing 적용, 즉시 이동 아님 |
| 자동 복귀 | 키를 떼면 부드럽게 중앙으로 |
| 타겟 발견 | 가려진 타겟이 엿보기로 드러남 |
| 스코프 피드백 | 가장자리 비대칭 어두워짐 |
| 탑뷰 (F2) | 시선 라인 + 차단/통과 색상 |
| 가시성 출력 | 콘솔에 Visible 타겟 수 표시 |

---

## 블로그 데모 아이디어

1. **패럴랙스 수학 다이어그램**: `ΔscreenX ∝ 1/distance` 시각화
2. **엿보기 전/후 GIF**: 기둥에 가려진 타겟이 Q/E로 드러나는 과정
3. **탑뷰 스크린샷**: 시선 라인이 장애물에 차단/통과하는 디버그 뷰
4. **스코프 비네트 비교**: 중앙 vs 엿보기 시 가장자리 변화
5. **이징 그래프**: linear, easeOutQuad, easeOutCubic 곡선 비교
6. **코드 하이라이트**: `std::function`으로 이징 전략을 교체하는 패턴

---

## 다음 챕터 예고

**Chapter 14: HUD와 스코프 오버레이**

FBO(Framebuffer Object) 기반 포스트 프로세스로 스코프 마스크를 정리하고,
HUD에 점수, 타겟 상태, 배율 정보를 텍스트로 표시한다.
데모: 스코프 뷰 + HUD 오버레이가 합성된 완성된 화면.
