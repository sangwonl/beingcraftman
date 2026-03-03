# Chapter 27: 통합과 튜닝

## 데모 미리보기

```
┌───────────────────────────────────────────────────────────┐
│  ┌─────────────────────────────────────────────────────┐  │
│  │            ╭──────────────────╮                      │  │
│  │         ╭──│      ★ target    │──╮                   │  │
│  │        │   │        ＋        │   │  ← gaze+mouse   │  │
│  │         ╰──│                  │──╯                   │  │
│  │            ╰──────────────────╯                      │  │
│  │  Ammo: 08/20    Score: 1250    Targets: 7/9          │  │
│  └─────────────────────────────────────────────────────┘  │
│  ┌── Performance ──────────────────────────────────────┐  │
│  │ Mode: [KB/Mouse] [Eye Track] [Hybrid ✓]            │  │
│  │ Capture 8ms → Track 12ms → Predict 0.3ms = 20.3ms  │  │
│  │ Difficulty: [Easy] [Normal ✓] [Hard]                │  │
│  └─────────────────────────────────────────────────────┘  │
│  ┌── Input Pipeline ──────────────────────────────────┐  │
│  │ Keyboard/Mouse ──┐                                  │  │
│  │                   ├→ InputProvider → SniperCamera    │  │
│  │ Eye Tracking ─────┘  (strategy)                     │  │
│  └─────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────┘
```

- **데모**: 모든 시스템(카메라, 렌더링, 사격, HUD, 사운드, 시선 추적, 캘리브레이션)이 통합
- **입력 전환**: 키보드/마우스, 시선 추적, 하이브리드 모드를 런타임에 전환
- **성능 오버레이**: 파이프라인 각 단계의 지연 시간을 실시간 표시
- 블로그에 "end-to-end latency 파이프라인" 다이어그램 포함 가능

---

## 학습 목표

1. Strategy 패턴 기반 `InputProvider` 추상화로 입력 소스를 교체 가능하게 만든다
2. 하이브리드 모드(시선 + 마우스 보정)를 구현한다
3. 파이프라인 지연 시간을 측정하고 프로파일링 도구를 만든다
4. 예측 알고리즘(선형 외삽, 칼만 필터)으로 체감 레이턴시를 줄인다
5. 시선 추적 정확도에 맞춘 게임 밸런스를 튜닝한다
6. `[[likely]]`/`[[unlikely]]`, 프로파일링 매크로, 캐시 친화적 데이터 배치를 실습한다

---

## 1. 배경 지식

### 통합의 핵심 과제

```
1. 입력 소스 교체
   Ch.09 SniperCamera는 키보드/마우스만 받았다.
   Ch.24~26의 시선 추적도 받아야 한다 → InputProvider 추상화

2. 레이턴시
   [캡처 ~8ms] → [추적 ~12ms] → [변환 ~1ms] → [렌더링 ~16ms] = ~37ms
   → 예측(prediction)으로 보상

3. 게임 밸런스
   마우스: 정확도 높음 → 타겟 작아도 됨
   시선: 정확도 낮음 → 타겟 크기/관용도 조정 필요
```

### 레이턴시 파이프라인

```
[Camera Capture ~8ms] → [Face/Gaze Track ~12ms] → [Prediction ~0.3ms] → [Render ~16ms]
= 총 ~37ms (예측 없이) → ~20ms (예측 적용 시 체감)
```

### 예측 알고리즘

- **선형 외삽**: `pos(t+dt) = pos(t) + vel(t)*dt` -- 빠르지만 급변 시 오버슈팅
- **칼만 필터**: 측정값과 예측값을 최적 결합 -- 노이즈에 강하지만 튜닝 필요

### 스무딩 vs 반응성

스무딩(alpha=0.1)은 떨림이 없지만 지연, 반응성(alpha=0.9)은 즉각 반응하지만 떨림. 우리 게임(스나이퍼 정밀 조준): alpha=0.4~0.6이 적합.

---

## 2. 구현 가이드

### Step 1: InputProvider 인터페이스 + InputData

```hpp
// game/include/gazeshot/game/InputProvider.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec2.hpp>
#include <string_view>

namespace gazeshot::game {

struct InputData {
    core::math::Vec2f headDelta{};    // 가늠자 이동량
    core::math::Vec2f gazeDelta{};    // 가늠쇠 이동량
    core::f32 zoomDelta = 0.0f;
    bool shootTriggered = false;
    bool reloadTriggered = false;
};

class InputProvider {
public:
    virtual ~InputProvider() = default;
    virtual InputData poll(core::f32 dt) = 0;
    virtual std::string_view name() const = 0;
    virtual bool isAvailable() const = 0;
};

} // namespace gazeshot::game
```

### Step 2: 키보드/마우스 + 시선 추적 제공자

```hpp
// game/include/gazeshot/game/KeyboardMouseProvider.hpp
namespace gazeshot::game {

class KeyboardMouseProvider final : public InputProvider {
public:
    explicit KeyboardMouseProvider(engine::Input& input) : input_(input) {}

    InputData poll(core::f32 /*dt*/) override {
        InputData data;
        if (input_.isKeyHeld(KeyCode::A)) data.headDelta.x -= 1.0f;
        if (input_.isKeyHeld(KeyCode::D)) data.headDelta.x += 1.0f;
        if (input_.isKeyHeld(KeyCode::W)) data.headDelta.y += 1.0f;
        if (input_.isKeyHeld(KeyCode::S)) data.headDelta.y -= 1.0f;
        auto md = input_.mouseDelta();
        data.gazeDelta = {md.x, md.y};
        data.zoomDelta = input_.scrollDelta();
        data.shootTriggered = input_.isMousePressed(MouseButton::Left);
        data.reloadTriggered = input_.isKeyPressed(KeyCode::R);
        return data;
    }

    std::string_view name() const override { return "Keyboard/Mouse"; }
    bool isAvailable() const override { return true; }
private:
    engine::Input& input_;
};

} // namespace gazeshot::game
```

```hpp
// game/include/gazeshot/game/EyeTrackingProvider.hpp
namespace gazeshot::game {

class EyeTrackingProvider final : public InputProvider {
public:
    EyeTrackingProvider(tracking::FaceTracker& face,
                        tracking::GazeTracker& gaze,
                        tracking::Calibration& calib)
        : face_(face), gaze_(gaze), calib_(calib) {}

    InputData poll(core::f32 /*dt*/) override {
        InputData data;
        // Ch.24: 얼굴 위치 → 가늠자
        if (auto hp = face_.latestHeadPose()) [[likely]] {
            auto cal = calib_.mapHeadPose(*hp);
            data.headDelta = cal - prevHead_;
            prevHead_ = cal;
        }
        // Ch.25: 시선 방향 → 가늠쇠
        if (auto gp = gaze_.latestGazePoint()) [[likely]] {
            auto cal = calib_.mapGazePoint(*gp);
            data.gazeDelta = cal - prevGaze_;
            prevGaze_ = cal;
        }
        data.shootTriggered = gaze_.blinkDetected();  // 깜빡임 → 사격
        return data;
    }

    std::string_view name() const override { return "Eye Tracking"; }
    bool isAvailable() const override {
        return face_.isRunning() && gaze_.isRunning();
    }
private:
    tracking::FaceTracker& face_;
    tracking::GazeTracker& gaze_;
    tracking::Calibration& calib_;
    core::math::Vec2f prevHead_{}, prevGaze_{};
};

} // namespace gazeshot::game
```

### Step 3: 하이브리드 제공자 (시선 + 마우스 보정)

```hpp
// game/include/gazeshot/game/HybridProvider.hpp
namespace gazeshot::game {

class HybridProvider final : public InputProvider {
public:
    struct Config {
        core::f32 gazeWeight = 0.7f;
        core::f32 mouseWeight = 0.3f;
        core::f32 correctionRadius = 0.1f;  // 마우스 보정 최대 반경
    };

    HybridProvider(std::unique_ptr<InputProvider> gaze,
                   std::unique_ptr<InputProvider> mouse,
                   const Config& cfg = {})
        : gaze_(std::move(gaze)), mouse_(std::move(mouse)), cfg_(cfg) {}

    InputData poll(core::f32 dt) override {
        auto g = gaze_->poll(dt);
        auto m = mouse_->poll(dt);
        InputData r;
        r.headDelta = g.headDelta;          // 가늠자: 시선 추적
        r.gazeDelta = g.gazeDelta * cfg_.gazeWeight
                    + clampLength(m.gazeDelta, cfg_.correctionRadius)
                      * cfg_.mouseWeight;   // 가늠쇠: 블렌딩
        r.zoomDelta = m.zoomDelta;
        r.shootTriggered = g.shootTriggered || m.shootTriggered;
        r.reloadTriggered = m.reloadTriggered;
        return r;
    }

    std::string_view name() const override { return "Hybrid (Gaze+Mouse)"; }
    bool isAvailable() const override { return gaze_->isAvailable(); }

private:
    static core::math::Vec2f clampLength(core::math::Vec2f v, core::f32 max) {
        core::f32 len = v.length();
        return (len > max) ? v * (max / len) : v;
    }
    std::unique_ptr<InputProvider> gaze_, mouse_;
    Config cfg_;
};

} // namespace gazeshot::game
```

### Step 4: SniperCamera에 InputProvider 연동

Ch.09의 `SniperCamera`에 `InputProvider`를 주입한다. 핵심 변경 부분만:

```hpp
// game/include/gazeshot/game/SniperCamera.hpp (Ch.27 수정 부분)
class SniperCamera {
public:
    // ── Ch.27 추가 ──
    void setInputProvider(std::unique_ptr<InputProvider> provider) {
        inputProvider_ = std::move(provider);
    }

    void update(core::f32 dt) {
        if (inputProvider_ && inputProvider_->isAvailable()) {
            auto input = inputProvider_->poll(dt);
            if (input.headDelta.x != 0 || input.headDelta.y != 0)
                moveHead(input.headDelta.x, input.headDelta.y, dt);
            else
                returnHead(dt);
            moveGaze(input.gazeDelta.x, input.gazeDelta.y);
            if (input.zoomDelta != 0) zoom(input.zoomDelta);
            lastInput_ = input;
        }
        headOffset_ = core::math::lerp(headOffset_, targetHeadOffset_, 8.0f * dt);
    }

    const InputData& lastInput() const { return lastInput_; }

    // ... Ch.09 기존 메서드들(moveHead, moveGaze, viewMatrix 등) 그대로 ...

private:
    // ... Ch.09 기존 멤버 + Ch.27 추가:
    std::unique_ptr<InputProvider> inputProvider_;
    InputData lastInput_;
};
```

### Step 5: 프로파일링 도구

```hpp
// core/include/gazeshot/core/Profiler.hpp
namespace gazeshot::core {

struct ScopeTimer {
    const char* name;
    std::chrono::steady_clock::time_point start;

    explicit ScopeTimer(const char* n)
        : name(n), start(std::chrono::steady_clock::now()) {}
    ~ScopeTimer() {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        std::printf("[PROFILE] %s: %.2f ms\n", name, us / 1000.0);
    }
    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;
};

#define PROFILE_SCOPE(name) \
    ::gazeshot::core::ScopeTimer _profTimer##__LINE__(name)

// 릴리즈에서 제거:
// #ifdef GAZESHOT_PROFILE ... #else #define PROFILE_SCOPE(name) ((void)0) #endif

} // namespace gazeshot::core
```

### Step 6: 칼만 필터 예측기

```hpp
// game/include/gazeshot/game/GazePredictor.hpp
namespace gazeshot::game {

class KalmanPredictor {
public:
    struct Config {
        core::f32 processNoise = 0.01f;       // Q
        core::f32 measurementNoise = 0.1f;    // R
        core::f32 initialUncertainty = 1.0f;
    };

    explicit KalmanPredictor(const Config& c = {})
        : cfg_(c), p_(c.initialUncertainty) {}

    core::math::Vec2f update(core::math::Vec2f measurement, core::f32 dt) {
        // 예측
        auto posPred = state_ + velocity_ * dt;
        auto pPred = p_ + cfg_.processNoise;
        // 보정
        auto k = pPred / (pPred + cfg_.measurementNoise);
        state_ = posPred + (measurement - posPred) * k;
        if (dt > 0.0001f) velocity_ = (state_ - prevState_) / dt;
        prevState_ = state_;
        p_ = (1.0f - k) * pPred;
        return state_;
    }

    // 레이턴시만큼 앞서 예측
    core::math::Vec2f predictAhead(core::f32 seconds) const {
        return state_ + velocity_ * seconds;
    }

    void reset() { state_ = {}; velocity_ = {}; prevState_ = {}; p_ = cfg_.initialUncertainty; }

private:
    Config cfg_;
    core::math::Vec2f state_{}, velocity_{}, prevState_{};
    core::f32 p_;
};

} // namespace gazeshot::game
```

### Step 7: 난이도 설정

```hpp
// game/include/gazeshot/game/DifficultySettings.hpp
namespace gazeshot::game {

enum class Difficulty : core::u8 { Easy = 0, Normal = 1, Hard = 2, Count = 3 };

struct DifficultyConfig {
    std::string_view name;
    core::f32 targetScaleMultiplier;   // 타겟 크기 배율
    core::f32 hitToleranceRadius;      // 피격 관용 반경
    core::f32 roundTimeSeconds;        // 제한 시간
    core::f32 targetVisibleSeconds;    // 노출 시간 (0=무제한)
    core::f32 scoreMultiplier;         // 점수 배율
    core::f32 gazeSmoothing;           // 스무딩 강도
    core::f32 predictionAheadMs;       // 예측 선행 시간
};

inline constexpr std::array<DifficultyConfig, 3> DIFFICULTY_TABLE {{
    { .name="Easy",   .targetScaleMultiplier=1.5f, .hitToleranceRadius=0.8f,
      .roundTimeSeconds=90.0f,  .targetVisibleSeconds=0.0f,
      .scoreMultiplier=0.5f, .gazeSmoothing=0.3f, .predictionAheadMs=30.0f },
    { .name="Normal", .targetScaleMultiplier=1.0f, .hitToleranceRadius=0.5f,
      .roundTimeSeconds=60.0f,  .targetVisibleSeconds=0.0f,
      .scoreMultiplier=1.0f, .gazeSmoothing=0.5f, .predictionAheadMs=20.0f },
    { .name="Hard",   .targetScaleMultiplier=0.7f, .hitToleranceRadius=0.3f,
      .roundTimeSeconds=45.0f,  .targetVisibleSeconds=3.0f,
      .scoreMultiplier=2.0f, .gazeSmoothing=0.7f, .predictionAheadMs=15.0f },
}};

inline const DifficultyConfig& getDifficulty(Difficulty d) {
    return DIFFICULTY_TABLE[static_cast<core::u32>(d)];
}

} // namespace gazeshot::game
```

### Step 8: 전체 통합 게임 루프

```cpp
// game/src/Game.cpp (Ch.27 핵심 부분)

void switchInputMode(Game& g) {
    using M = Game::InputMode;
    switch (g.inputMode) {
    case M::KeyboardMouse:
        g.inputMode = M::EyeTracking;
        g.camera.setInputProvider(
            std::make_unique<game::EyeTrackingProvider>(
                g.faceTracker, g.gazeTracker, g.calibration));
        break;
    case M::EyeTracking:
        g.inputMode = M::Hybrid;
        g.camera.setInputProvider(
            std::make_unique<game::HybridProvider>(
                std::make_unique<game::EyeTrackingProvider>(
                    g.faceTracker, g.gazeTracker, g.calibration),
                std::make_unique<game::KeyboardMouseProvider>(
                    g.window.input())));
        break;
    case M::Hybrid:
        g.inputMode = M::KeyboardMouse;
        g.camera.setInputProvider(
            std::make_unique<game::KeyboardMouseProvider>(g.window.input()));
        break;
    }
}

void update(Game& g, core::f32 dt) {
    auto& diff = game::getDifficulty(g.currentDifficulty);

    if (g.window.input().isKeyPressed(KeyCode::Tab)) switchInputMode(g);

    { PROFILE_SCOPE("Camera.Update"); g.camera.update(dt); }

    // 시선 예측 (시선 추적 모드일 때)
    if (g.inputMode != Game::InputMode::KeyboardMouse) {
        g.gazePredictor.update(g.camera.gazePoint(), dt);
        g.headPredictor.update(g.camera.headOffset(), dt);
        core::f32 ahead = diff.predictionAheadMs / 1000.0f;
        auto predicted = g.gazePredictor.predictAhead(ahead);
        // predicted 결과를 카메라에 반영
    }

    // 사격 처리
    if (g.camera.lastInput().shootTriggered) [[unlikely]] {
        auto hit = castRay(g.camera.aimRay(), g.scene);
        if (hit.has_value()) [[likely]] {
            if (hit->distanceFromCenter <= diff.hitToleranceRadius)
                processHit(g, *hit, diff);
        }
    }
}
```

---

## 3. C++ 학습 포인트 정리

### `[[likely]]` / `[[unlikely]]` (C++20) -- 분기 예측 힌트

```cpp
// 시선 데이터는 대부분 존재 → likely
if (headPose.has_value()) [[likely]] {
    processHeadPose(*headPose);
}

// 사격은 드물게 발생 → unlikely
if (lastInput.shootTriggered) [[unlikely]] {
    auto ray = camera.aimRay();
    auto hit = castRay(ray, scene);
}
```

`[[likely]]`는 폴스루 경로 배치로 분기 예측 적중률 향상, `[[unlikely]]`는 "차가운" 코드를 별도 배치하여 명령어 캐시 효율 향상. 잘못된 힌트는 역효과 -- 반드시 측정 후 사용.

### 프로파일링: RAII 기반 구간 측정

```cpp
struct ScopeTimer {
    const char* name;
    std::chrono::steady_clock::time_point start;
    explicit ScopeTimer(const char* n)
        : name(n), start(std::chrono::steady_clock::now()) {}
    ~ScopeTimer() { /* elapsed 출력 */ }
};

#define PROFILE_SCOPE(name) ScopeTimer _timer##__LINE__(name)

void update(f32 dt) {
    PROFILE_SCOPE("Update");          // _timer42("Update")로 확장
    { PROFILE_SCOPE("Physics"); }     // 중첩 가능
    { PROFILE_SCOPE("Input"); }
}
// 출력: [PROFILE] Physics: 0.45 ms
//       [PROFILE] Input: 0.12 ms
//       [PROFILE] Update: 0.73 ms
```

`__LINE__` 토큰 연결로 같은 스코프에 여러 `PROFILE_SCOPE`를 쓸 수 있다. `#ifdef`로 릴리즈 빌드에서 완전 제거 가능.

### 캐시 친화적 데이터 배치: AoS vs SoA

```cpp
// AoS: 모든 필드가 구조체에 함께 (엔티티에 적합)
struct Particle { Vec3f pos; Vec3f vel; Vec4f color; f32 life; };
std::vector<Particle> particles;
// → 위치만 업데이트해도 color, life까지 캐시에 로드 (낭비)

// SoA: 같은 필드끼리 연속 배치 (대량 처리에 적합)
struct ParticleSystem {
    std::vector<Vec3f> positions;
    std::vector<Vec3f> velocities;
    // ...
};
// → position만 연속 로드, SIMD 벡터화도 가능
```

```
AoS: [pos0|vel0|col0|life0][pos1|vel1|col1|life1] → 캐시 라인 50% 낭비
SoA: [pos0|pos1|pos2|pos3][vel0|vel1|vel2|vel3]   → 캐시 라인 100% 활용
```

**선택**: 모든 필드를 함께 접근 → AoS, 특정 필드만 대량 접근 → SoA.

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 입력 전환 | Tab키로 Keyboard/Mouse → Eye Tracking → Hybrid 순환 |
| 키보드/마우스 | 기존 Ch.09과 동일하게 동작 |
| 시선 추적 | 얼굴 이동 → headOffset, 시선 → gazePoint 반영 |
| 하이브리드 | 시선으로 대략 조준 + 마우스로 미세 보정 가능 |
| 프로파일링 | 콘솔에 각 단계 지연 시간 출력 |
| 총 레이턴시 | 33ms (1 frame @30fps) 미만 달성 |
| 예측 동작 | 시선 이동 시 레티클이 부드럽게 선행 이동 |
| 노이즈 필터 | 칼만 필터로 시선 떨림 감소 확인 |
| Easy 난이도 | 타겟 1.5배 크기, 관용도 넓음, 90초 |
| Hard 난이도 | 타겟 0.7배, 3초 후 사라짐, 45초 |
| 메모리 | 입력 모드 전환 시 메모리 누수 없음 (unique_ptr) |

---

## 블로그 데모 아이디어

1. **입력 모드 비교 영상**: 키보드/마우스 vs 시선 vs 하이브리드 화면 분할
2. **레이턴시 파이프라인 다이어그램**: 각 단계의 ms 표시 흐름도
3. **예측 시각화**: 예측 없음 vs 칼만 필터 커서 궤적 비교 GIF
4. **난이도별 타겟 비교**: Easy/Normal/Hard 스크린샷 나란히

---

## 다음 챕터 예고

**Chapter 28: (보너스) 직접 시선 추적 모델 만들기**

CNN 기반 시선 추정 모델을 직접 학습한다.
데모: 눈 영역 이미지를 입력으로 시선 벡터를 출력하는 모델을 PyTorch로 학습하고, ONNX로 변환하여 C++에서 추론한다. 기존 MediaPipe 기반 추적과 정확도/속도를 비교한다.
