# Chapter 09: 스나이퍼 카메라 시스템

## 데모 미리보기

```
┌─────────────────────────────────────────────┐
│              ╭────────────╮                  │
│          ╭───│            │───╮              │
│         │    │   스코프    │    │             │
│         │    │    뷰      │    │             │
│         │    │     ＋     │    │  ← 레티클   │
│         │    │            │    │   (마우스)   │
│          ╰───│            │───╯              │
│              ╰────────────╯                  │
│  WASD: 시점(가늠자) 이동                      │
│  Mouse: 레티클(가늠쇠) 이동                   │
│  Scroll: 스코프 줌                           │
└─────────────────────────────────────────────┘
```

- **데모**: 스코프 원형 마스크를 통해 씬을 바라봄
- **가늠자**: WASD로 카메라 위치를 미세하게 이동 (머리 움직임 시뮬레이션)
- **가늠쇠**: 마우스로 스코프 내 레티클(십자선) 위치 제어
- **줌**: 마우스 휠로 스코프 배율 변경
- 블로그에 "가늠자-가늠쇠 원리의 게임 구현" 다이어그램 포함 가능

---

## 학습 목표

1. 스나이퍼 카메라의 가늠자(rear sight)/가늠쇠(front sight) 분리 모델을 구현한다
2. 시차(parallax) 효과가 자연스럽게 발생하는 것을 확인한다
3. FOV로 스코프 배율을 제어한다
4. 상속 vs 합성, `std::clamp`, `std::lerp`를 실습한다

---

## 1. 배경 지식

### 가늠자-가늠쇠 모델

실제 스나이퍼 조준 원리를 게임에 매핑:

```
실제 사격:
[눈 위치] ──→ [가늠자(뒷 구멍)] ──→ [가늠쇠(앞 침)] ──→ [타겟]
 (고정)        (총에 고정)          (총에 고정)

우리 게임:
[얼굴 위치] ──→ [카메라 위치] ──→ [레티클 위치] ──→ [타겟]
 (키보드/         (headOffset)     (gazePoint)
  추후 얼굴추적)

조준선 = 카메라 위치에서 레티클 방향으로 나가는 Ray
```

핵심 통찰: **카메라 위치와 조준 방향이 분리**되어 있다.

- 일반 FPS: 카메라 방향 = 조준 방향 (동일)
- 우리 게임: 카메라 위치(가늠자) ≠ 조준 방향(가늠쇠)

### 시차(Parallax) 효과

카메라 위치를 옮기면 가까운 물체/먼 물체의 상대적 위치가 달라진다:

```
카메라 중앙:              카메라 왼쪽으로 이동:
    ┃  ← 장애물            │  ← 장애물
    ┃                      │
    ┃ ★ (숨겨진 타겟)        │    ★ (보인다!)
    ┃                      │
    ●  카메라              ●  카메라

가까운 장애물은 많이 이동, 먼 타겟은 조금 이동 → 뒤가 보인다
```

이 효과는 perspective 투영 + 카메라 이동으로 **자동 발생**한다.
별도 로직 불필요!

### FOV와 스코프 배율

```
FOV 60° = 1x 배율 (기본 시야)
FOV 30° = 2x 배율 (2배 확대)
FOV 15° = 4x 배율 (4배 확대)
FOV 10° = 6x 배율 (6배 확대)

배율 = tan(baseFOV/2) / tan(currentFOV/2)
```

---

## 2. 구현 가이드

### Step 1: SniperCamera 클래스

```hpp
// game/include/gazeshot/game/SniperCamera.hpp

#pragma once

#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/core/Types.hpp>

#include <algorithm>  // std::clamp
#include <cmath>

namespace gazeshot::game {

class SniperCamera {
public:
    // ── 설정 ──
    struct Config {
        core::math::Vec3f basePosition{0.0f, 1.6f, 0.0f};  // 사격 위치 (서있는 높이)
        core::math::Vec3f lookDirection{0.0f, 0.0f, -1.0f}; // 기본 바라보는 방향

        // 가늠자 (headOffset) 제한
        core::f32 maxHeadOffsetX = 0.15f;  // 좌우 ±15cm
        core::f32 maxHeadOffsetY = 0.10f;  // 상하 ±10cm
        core::f32 headSpeed = 0.5f;        // 초당 이동 속도
        core::f32 headReturnSpeed = 3.0f;  // 복귀 속도

        // 가늠쇠 (gazePoint) 제한
        core::f32 maxGazeX = 0.8f;   // 스코프 반지름 비율 내
        core::f32 maxGazeY = 0.8f;
        core::f32 gazeSensitivity = 0.003f;

        // 스코프
        core::f32 baseFOV = 60.0f;   // 기본 FOV (도)
        core::f32 minFOV = 5.0f;     // 최대 줌
        core::f32 maxFOV = 60.0f;    // 최소 줌 (기본)
        core::f32 zoomSpeed = 2.0f;
    };

    explicit SniperCamera(const Config& config = {})
        : config_(config)
        , fov_(config.baseFOV) {}

    // ── 가늠자 조작 (키보드 → 추후 얼굴 추적) ──
    void moveHead(core::f32 dx, core::f32 dy, core::f32 dt) {
        targetHeadOffset_.x += dx * config_.headSpeed * dt;
        targetHeadOffset_.y += dy * config_.headSpeed * dt;

        // 범위 제한
        targetHeadOffset_.x = std::clamp(targetHeadOffset_.x,
            -config_.maxHeadOffsetX, config_.maxHeadOffsetX);
        targetHeadOffset_.y = std::clamp(targetHeadOffset_.y,
            -config_.maxHeadOffsetY, config_.maxHeadOffsetY);
    }

    // 가늠자 복귀 (키를 뗐을 때)
    void returnHead(core::f32 dt) {
        targetHeadOffset_ = core::math::lerp(
            targetHeadOffset_,
            core::math::Vec2f{0, 0},
            config_.headReturnSpeed * dt
        );
    }

    // ── 가늠쇠 조작 (마우스 → 추후 시선 추적) ──
    void moveGaze(core::f32 dx, core::f32 dy) {
        gazePoint_.x += dx * config_.gazeSensitivity;
        gazePoint_.y -= dy * config_.gazeSensitivity;  // Y 반전

        // 스코프 원 안에 제한
        core::f32 distSq = gazePoint_.x * gazePoint_.x
                         + gazePoint_.y * gazePoint_.y;
        core::f32 maxR = config_.maxGazeX;
        if (distSq > maxR * maxR) {
            core::f32 dist = std::sqrt(distSq);
            gazePoint_.x *= maxR / dist;
            gazePoint_.y *= maxR / dist;
        }
    }

    // ── 줌 ──
    void zoom(core::f32 delta) {
        fov_ -= delta * config_.zoomSpeed;
        fov_ = std::clamp(fov_, config_.minFOV, config_.maxFOV);
    }

    // ── 매 프레임 업데이트 ──
    void update(core::f32 dt) {
        // 부드러운 보간
        headOffset_ = core::math::lerp(headOffset_, targetHeadOffset_, 8.0f * dt);
    }

    // ── 뷰 행렬 계산 ──
    [[nodiscard]] core::math::Mat4f viewMatrix() const {
        using namespace core::math;

        Vec3f cameraPos = config_.basePosition
            + Vec3f{headOffset_.x, headOffset_.y, 0.0f};

        // 카메라가 바라보는 점: 기본 방향 + 가늠쇠 오프셋
        Vec3f lookTarget = cameraPos + config_.lookDirection * 100.0f
            + Vec3f{gazePoint_.x * 5.0f, gazePoint_.y * 5.0f, 0.0f};

        return lookAt(cameraPos, lookTarget, Vec3f{0, 1, 0});
    }

    // ── 투영 행렬 ──
    [[nodiscard]] core::math::Mat4f projectionMatrix(core::f32 aspect) const {
        using namespace core::math;
        return perspective(radians(fov_), aspect, 0.1f, 500.0f);
    }

    // ── 조준선 Ray (사격 시 사용) ──
    struct AimRay {
        core::math::Vec3f origin;
        core::math::Vec3f direction;
    };

    [[nodiscard]] AimRay aimRay() const {
        using namespace core::math;

        Vec3f origin = config_.basePosition
            + Vec3f{headOffset_.x, headOffset_.y, 0.0f};

        // 가늠쇠가 가리키는 방향
        Vec3f target = origin + config_.lookDirection * 100.0f
            + Vec3f{gazePoint_.x * 5.0f, gazePoint_.y * 5.0f, 0.0f};

        return { origin, normalize(target - origin) };
    }

    // ── 접근자 ──
    core::math::Vec2f headOffset() const { return headOffset_; }
    core::math::Vec2f gazePoint() const { return gazePoint_; }
    core::f32 fov() const { return fov_; }
    core::f32 magnification() const {
        using namespace core::math;
        return std::tan(radians(config_.baseFOV / 2.0f))
             / std::tan(radians(fov_ / 2.0f));
    }
    const core::math::Vec3f& position() const { return config_.basePosition; }

private:
    Config config_;
    core::math::Vec2f headOffset_{};        // 현재 보간된 값
    core::math::Vec2f targetHeadOffset_{};  // 목표값
    core::math::Vec2f gazePoint_{};         // 스코프 내 레티클 위치 (-1~1)
    core::f32 fov_;
};

} // namespace gazeshot::game
```

**C++ 학습 포인트: 상속 vs 합성**

```cpp
// 상속 (피함):
class SniperCamera : public Camera { ... };
// 문제: Camera의 자유 이동 메서드가 노출됨

// 합성 (선택):
class SniperCamera {
    // Camera를 내부에 갖지도 않음
    // viewMatrix()를 직접 계산
    // 스나이퍼 전용 인터페이스만 노출
};
```

`SniperCamera`는 일반 카메라와 동작이 완전히 다르므로
(고정 위치, 제한된 이동, 분리된 조준), 상속보다 독립 구현이 맞다.

**C++ 학습 포인트: `std::clamp`**

```cpp
headOffset_.x = std::clamp(headOffset_.x, -0.15f, 0.15f);
// = std::max(-0.15f, std::min(headOffset_.x, 0.15f))
// 한 줄로 범위 제한
```

### Step 2: 입력 연동

```cpp
// game/src/main.cpp (Ch.09)

void update(App& app, f32 dt) {
    auto& cam = app.camera;

    // ── 가늠자 (WASD) ──
    bool headMoving = false;
    if (app.input.isKeyHeld(SDLK_A)) { cam.moveHead(-1, 0, dt); headMoving = true; }
    if (app.input.isKeyHeld(SDLK_D)) { cam.moveHead( 1, 0, dt); headMoving = true; }
    if (app.input.isKeyHeld(SDLK_W)) { cam.moveHead(0,  1, dt); headMoving = true; }
    if (app.input.isKeyHeld(SDLK_S)) { cam.moveHead(0, -1, dt); headMoving = true; }
    if (!headMoving) {
        cam.returnHead(dt);  // 키를 뗌 → 천천히 원위치
    }

    // ── 가늠쇠 (마우스 이동) ──
    auto mouseDelta = app.input.mouseDelta();
    cam.moveGaze(mouseDelta.x, mouseDelta.y);

    // ── 줌 (스크롤) ──
    // scrollDelta를 Input에서 추가 구현 필요
    // cam.zoom(scrollDelta);

    cam.update(dt);
}

void render(App& app, f32 alpha) {
    auto& cam = app.camera;
    f32 aspect = (f32)app.window.width() / (f32)app.window.height();

    Mat4f view = cam.viewMatrix();
    Mat4f proj = cam.projectionMatrix(aspect);
    Vec3f viewPos = cam.position() + Vec3f{cam.headOffset().x, cam.headOffset().y, 0};

    // 씬 렌더링
    app.scene.render(*app.renderer, *app.shader, view, proj, viewPos);

    // 콘솔 디버그
    static f32 logTimer = 0;
    logTimer += 1.0f / 60.0f;
    if (logTimer >= 0.5f) {
        auto ho = cam.headOffset();
        auto gp = cam.gazePoint();
        std::printf("Head: (%.3f, %.3f) | Gaze: (%.2f, %.2f) | Zoom: %.1fx\n",
            ho.x, ho.y, gp.x, gp.y, cam.magnification());
        logTimer = 0;
    }
}
```

### Step 3: 스코프 오버레이 (간이 버전)

스코프의 원형 마스크를 fragment shader로 구현한다:

```glsl
// 포스트 프로세스로 하는 것이 정석이지만, 간이 버전으로 fragment shader에서 처리

// phong.frag에 추가:
uniform vec2 uReticlePos;   // 가늠쇠 위치 (-1 ~ 1)
uniform float uScopeRadius; // 스코프 반지름 (화면 비율)

void main() {
    // 기존 Phong 계산...
    vec3 color = ambient + diffuse + specular;

    // 스코프 마스크: 화면 중심에서 거리 계산
    vec2 screenUV = gl_FragCoord.xy / vec2(uScreenSize) * 2.0 - 1.0;
    float dist = length(screenUV);

    // 스코프 원 밖은 어둡게
    if (dist > uScopeRadius) {
        color *= 0.05;  // 거의 검은색
    }
    // 스코프 가장자리: 비네트 효과
    else if (dist > uScopeRadius * 0.85) {
        float edge = (dist - uScopeRadius * 0.85) / (uScopeRadius * 0.15);
        color *= mix(1.0, 0.3, edge);
    }

    // 레티클 (십자선)
    vec2 reticleScreen = uReticlePos;
    float crossSize = 0.002;
    if (abs(screenUV.x - reticleScreen.x) < crossSize && abs(screenUV.y - reticleScreen.y) < 0.03
     || abs(screenUV.y - reticleScreen.y) < crossSize && abs(screenUV.x - reticleScreen.x) < 0.03) {
        color = vec3(1.0, 0.2, 0.2);  // 빨간 십자선
    }

    FragColor = vec4(color, 1.0);
}
```

> **참고**: 이 간이 방식은 모든 오브젝트의 fragment shader에서 스코프를 그린다.
> Ch.14 (HUD)와 Ch.20 (포스트 프로세싱)에서 FBO 기반으로 정리한다.

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 가늠자 이동 | WASD로 시점이 미세하게 이동 |
| 가늠자 복귀 | 키를 떼면 천천히 원위치 |
| 가늠쇠 이동 | 마우스로 레티클이 스코프 내 이동 |
| 레티클 제한 | 스코프 원 밖으로 나가지 않음 |
| 줌 | 스크롤로 FOV 변경, 배율 표시 |
| 패럴랙스 | WASD로 이동 시 가까운/먼 물체의 상대 위치 변화 |
| 스코프 마스크 | 원 밖은 어둡게 |
| 콘솔 출력 | Head, Gaze, Zoom 실시간 표시 |

---

## 4. 블로그 데모 아이디어

1. **스코프 뷰 스크린샷**: 원형 마스크 + 십자선 + 씬
2. **가늠자-가늠쇠 다이어그램**: 실제 사격 원리 → 게임 매핑
3. **패럴랙스 GIF**: WASD로 시점 이동 시 근거리/원거리 물체 상대 이동
4. **줌 비교**: 1x, 2x, 4x, 6x 배율 나란히
5. **키보드 오버레이**: WASD = 가늠자, Mouse = 가늠쇠 도식

---

## 다음 챕터 예고

**Chapter 10: 사격장 씬 구성**

9개 타겟 + 장애물이 배치된 사격장을 구성한다.
데모: 근거리/중거리/원거리 타겟이 보이고, 일부는 기둥에 가려져 있다.
가늠자를 움직이면 가려진 타겟이 보인다(패럴랙스).
