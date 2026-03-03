# Chapter 25: 시선 방향 추적 (Gaze Direction Tracking)

## 데모 미리보기

```
┌───────────────────────────────────────────────────────────────┐
│                                                               │
│  [웹캠 프레임]         [눈 랜드마크 분석]       [게임 화면]     │
│                                                               │
│   ┌─────────┐         ┌─────────────┐    ╭────────────╮      │
│   │  (^_^)  │  ───→   │  L: ⊙──→    │    │            │      │
│   │  /   \  │         │  R: ⊙──→    │    │     ＋ ←gazePoint │
│   └─────────┘         │             │    │            │      │
│                       │ EAR_L: 0.31 │    ╰────────────╯      │
│                       │ EAR_R: 0.29 │                        │
│                       └─────────────┘                        │
│                                                               │
│  얼굴 랜드마크 ──→ 눈 영역 추출 ──→ 동공 위치 검출             │
│       (Ch.24)          │                    │                 │
│                        ▼                    ▼                 │
│                    EAR 계산            시선 벡터 추정          │
│                        │                    │                 │
│                        ▼                    ▼                 │
│                   깜빡임 감지 ──→      gazePoint 업데이트      │
│                    (사격 트리거)      (레티클 이동, Ch.09)      │
│                                                               │
│  Gaze: (0.12, -0.05) | EAR: 0.30 | Blink: -- | Filter: MA10 │
└───────────────────────────────────────────────────────────────┘
```

- **데모**: 눈을 움직이면 스코프 내 레티클(가늠쇠)이 따라 이동
- **깜빡임**: 양쪽 눈 동시 깜빡임으로 사격, 한쪽만 깜빡이면 재장전
- **필터링**: RingBuffer 기반 이동 평균으로 떨림(jitter) 제거
- 블로그에 "EAR 그래프 + 깜빡임 감지 타이밍" 다이어그램 포함 가능

---

## 학습 목표

1. 얼굴 랜드마크에서 눈 영역을 추출하고 동공 위치를 검출한다
2. Eye Aspect Ratio(EAR)를 계산하여 눈 개폐 상태를 판별한다
3. 동공 오프셋 기반 시선 방향을 추정하고 gazePoint로 변환한다
4. 깜빡임 감지와 디바운싱으로 사격 트리거를 구현한다
5. **ring buffer**(`std::array` 기반 순환 버퍼)와 non-type template parameter를 실습한다

---

## 1. 배경 지식

### 눈 랜드마크 구조

MediaPipe Face Mesh의 468개 랜드마크 중 눈 주변 6개로 개폐와 동공 위치를 분석한다:

```
      p1          p2
       ╲        ╱
  p0 ── ⊙ ── p3     ← p0: inner corner, p3: outer corner
       ╱        ╲
      p5          p4     p1,p2: upper eyelid / p4,p5: lower eyelid

MediaPipe 인덱스:
  왼쪽 눈: 33, 160, 158, 133, 153, 144  |  Iris: 468
  오른쪽:  362, 385, 387, 263, 373, 380  |  Iris: 473
```

### Eye Aspect Ratio (EAR)

```
          |p1 - p5| + |p2 - p4|
  EAR = ─────────────────────────
              2 * |p0 - p3|

  EAR 값     상태          의미
  0.25~0.35  눈 뜸         정상 상태
  0.15~0.25  반쯤 감김     졸음 또는 전환
  < 0.15     눈 감김       깜빡임 감지

  EAR 그래프:
  0.30 ┤────╱╲──╱────╲──────────╱╲──╱────╲──
  0.20 ┤╱                ╲  ╱                ╲
  0.15 ┤──────────────────╲╱──────── threshold
  0.05 ┤                   ▼ blink detected!
       └──────────────────────────────────→ t
```

### 시선 방향 추정

```
시선 벡터 계산:
  eyeCenter = (p0 + p3) / 2
  pupilOffset = pupil - eyeCenter
  gazeX = pupilOffset.x / eyeWidth    ← 정규화 (-1 ~ +1)
  gazeY = pupilOffset.y / eyeHeight
```

한계: 웹캠 기반 동공 추정은 +-5~10도 정도의 정확도.
정밀한 시선 추적이 필요하면 Ch.28의 CNN 모델이나 전용 하드웨어를 사용한다.

### 깜빡임 감지와 디바운싱

```
디바운싱 전략:
  1. EAR < threshold → 깜빡임 시작 감지
  2. 최소 지속 시간 (150ms) 확인 → 자연 깜빡임 필터링
  3. 최대 지속 시간 (800ms) 초과 → 단순 눈 감기로 무시
  4. EAR > threshold 복귀 → 깜빡임 종료, 사격 트리거
  5. 쿨다운 (300ms) → 연속 오발 방지
```

---

## 2. 구현 가이드

### Step 1: RingBuffer — 시선 히스토리용 순환 버퍼

```hpp
// game/include/gazeshot/game/tracking/RingBuffer.hpp

#pragma once

#include <gazeshot/core/math/Vec2.hpp>
#include <gazeshot/core/Types.hpp>

#include <array>
#include <algorithm>

namespace gazeshot::game::tracking {

template<typename T, std::size_t N>
class RingBuffer {
    static_assert(N > 0, "RingBuffer size must be positive");

public:
    void push(T value) {
        data_[head_ % N] = value;
        ++head_;
        count_ = std::min(count_ + 1, N);
    }

    // 이동 평균 — 시선 데이터의 고주파 노이즈를 제거하는 가장 간단한 필터
    [[nodiscard]] T average() const {
        if (count_ == 0) return T{};
        T sum{};
        for (std::size_t i = 0; i < count_; ++i) {
            sum = sum + at(i);
        }
        return sum * (1.0f / static_cast<float>(count_));
    }

    [[nodiscard]] T latest() const {
        if (count_ == 0) return T{};
        return data_[(head_ - 1) % N];
    }

    [[nodiscard]] T at(std::size_t i) const {
        if (i >= count_) return T{};
        return data_[(head_ - 1 - i) % N];
    }

    [[nodiscard]] std::size_t size() const { return count_; }
    [[nodiscard]] bool empty() const { return count_ == 0; }
    [[nodiscard]] bool full() const { return count_ == N; }
    [[nodiscard]] constexpr std::size_t capacity() const { return N; }
    void clear() { head_ = 0; count_ = 0; }

private:
    std::array<T, N> data_{};
    std::size_t head_ = 0;
    std::size_t count_ = 0;
};

} // namespace gazeshot::game::tracking
```

**C++ 학습 포인트: non-type template parameter (`size_t N`)**

```cpp
template<typename T, std::size_t N>
class RingBuffer {
    std::array<T, N> data_;  // 컴파일 타임에 크기 결정 → 스택 할당
};

// 서로 다른 N은 서로 다른 타입을 만든다:
RingBuffer<Vec2f, 10> gazeHistory;   // 10프레임 이동 평균
RingBuffer<float, 30> earHistory;    // 30프레임 EAR 히스토리
// gazeHistory와 earHistory는 다른 타입 → 서로 대입 불가

// 런타임 크기(std::vector)와의 차이:
//   std::array → 스택 할당, 캐시 친화적, 동적 확장 불가
//   std::vector → 힙 할당, 캐시 미스 가능, 동적 확장 가능
//   고정 크기 버퍼에는 std::array가 성능 면에서 우월

// C++20 확장: 부동소수점도 non-type parameter로 사용 가능
template<float Threshold>
bool isAbove(float val) { return val > Threshold; }
auto result = isAbove<0.15f>(ear);
```

### Step 2: EyeAnalyzer — 눈 영역 분석

```hpp
// game/include/gazeshot/game/tracking/EyeAnalyzer.hpp

#pragma once

#include <gazeshot/core/math/Vec2.hpp>
#include <gazeshot/core/Types.hpp>

#include <array>
#include <cmath>
#include <algorithm>

namespace gazeshot::game::tracking {

struct EyeLandmarkIndices {
    static constexpr std::array<int, 6> left  = {33, 160, 158, 133, 153, 144};
    static constexpr int leftIris = 468;
    static constexpr std::array<int, 6> right = {362, 385, 387, 263, 373, 380};
    static constexpr int rightIris = 473;
};

struct EyeState {
    core::f32 ear = 0.0f;
    core::math::Vec2f pupilOffset{};
    bool open = true;
};

struct GazeResult {
    EyeState leftEye, rightEye;
    core::math::Vec2f gazeDirection{};
    core::f32 averageEAR = 0.0f;
};

class EyeAnalyzer {
public:
    struct Config {
        core::f32 earThreshold = 0.20f;
        core::f32 gazeSensitivity = 2.0f;
        core::f32 maxGazeOffset = 0.8f;
    };

    explicit EyeAnalyzer(const Config& config = {}) : config_(config) {}

    [[nodiscard]] static core::f32 computeEAR(
        const core::math::Vec2f& p0, const core::math::Vec2f& p1,
        const core::math::Vec2f& p2, const core::math::Vec2f& p3,
        const core::math::Vec2f& p4, const core::math::Vec2f& p5)
    {
        core::f32 vertical1 = distance(p1, p5);
        core::f32 vertical2 = distance(p2, p4);
        core::f32 horizontal = distance(p0, p3);
        if (horizontal < 1e-6f) return 0.0f;
        return (vertical1 + vertical2) / (2.0f * horizontal);
    }

    [[nodiscard]] static core::math::Vec2f computePupilOffset(
        const core::math::Vec2f& inner, const core::math::Vec2f& outer,
        const core::math::Vec2f& upper, const core::math::Vec2f& lower,
        const core::math::Vec2f& pupil)
    {
        auto eyeCenterX = (inner.x + outer.x) * 0.5f;
        auto eyeCenterY = (upper.y + lower.y) * 0.5f;
        core::f32 eyeW = std::abs(outer.x - inner.x);
        core::f32 eyeH = std::abs(upper.y - lower.y);
        if (eyeW < 1e-6f || eyeH < 1e-6f) return {};

        return {
            std::clamp((pupil.x - eyeCenterX) / (eyeW * 0.5f), -1.0f, 1.0f),
            std::clamp((pupil.y - eyeCenterY) / (eyeH * 0.5f), -1.0f, 1.0f)
        };
    }

    [[nodiscard]] EyeState analyzeEye(
        const core::math::Vec2f& p0, const core::math::Vec2f& p1,
        const core::math::Vec2f& p2, const core::math::Vec2f& p3,
        const core::math::Vec2f& p4, const core::math::Vec2f& p5,
        const core::math::Vec2f& pupil) const
    {
        core::f32 ear = computeEAR(p0, p1, p2, p3, p4, p5);
        auto offset = computePupilOffset(p0, p3, p1, p5, pupil);
        return { ear, offset * config_.gazeSensitivity, ear > config_.earThreshold };
    }

    [[nodiscard]] GazeResult computeGaze(const EyeState& left, const EyeState& right) const {
        auto gazeDir = (left.pupilOffset + right.pupilOffset) * 0.5f;
        core::f32 len = std::sqrt(gazeDir.x * gazeDir.x + gazeDir.y * gazeDir.y);
        if (len > config_.maxGazeOffset) gazeDir = gazeDir * (config_.maxGazeOffset / len);
        return { left, right, gazeDir, (left.ear + right.ear) * 0.5f };
    }

private:
    Config config_;

    [[nodiscard]] static core::f32 distance(const core::math::Vec2f& a, const core::math::Vec2f& b) {
        auto dx = a.x - b.x; auto dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

} // namespace gazeshot::game::tracking
```

### Step 3: BlinkDetector — 깜빡임 감지 + 디바운싱

```hpp
// game/include/gazeshot/game/tracking/BlinkDetector.hpp

#pragma once

#include <gazeshot/core/Types.hpp>

namespace gazeshot::game::tracking {

enum class BlinkEvent : core::u8 {
    None,           // 깜빡임 없음
    BothEyesBlink,  // 양쪽 동시 → 사격
    LeftEyeOnly,    // 왼쪽만 → 재장전 또는 취소
    RightEyeOnly,   // 오른쪽만 → 재장전 또는 취소
};

class BlinkDetector {
public:
    struct Config {
        core::f32 earThreshold = 0.20f;
        core::f32 minBlinkDuration = 0.15f;   // 최소 지속 — 자연 깜빡임 필터
        core::f32 maxBlinkDuration = 0.80f;   // 최대 지속 — 단순 눈 감기 무시
        core::f32 cooldown = 0.30f;           // 연속 감지 방지 쿨다운
        core::f32 simultaneousThreshold = 0.05f;  // 양쪽 동시 판정 시간차
    };

    explicit BlinkDetector(const Config& config = {}) : config_(config) {}

    [[nodiscard]] BlinkEvent update(core::f32 leftEAR, core::f32 rightEAR, core::f32 dt) {
        if (cooldownTimer_ > 0.0f) { cooldownTimer_ -= dt; return BlinkEvent::None; }

        bool leftClosed = leftEAR < config_.earThreshold;
        bool rightClosed = rightEAR < config_.earThreshold;
        BlinkEvent result = BlinkEvent::None;

        // 왼쪽 눈 추적
        if (leftClosed && !leftWasClosed_) leftCloseTime_ = 0.0f;
        if (leftClosed) leftCloseTime_ += dt;
        if (!leftClosed && leftWasClosed_) {
            leftBlinkCompleted_ = isValidBlink(leftCloseTime_);
            leftCompletedTime_ = 0.0f;
        }

        // 오른쪽 눈 추적
        if (rightClosed && !rightWasClosed_) rightCloseTime_ = 0.0f;
        if (rightClosed) rightCloseTime_ += dt;
        if (!rightClosed && rightWasClosed_) {
            rightBlinkCompleted_ = isValidBlink(rightCloseTime_);
            rightCompletedTime_ = 0.0f;
        }

        // 양쪽 동시 깜빡임 판정
        if (leftBlinkCompleted_ && rightBlinkCompleted_) {
            if (std::abs(leftCompletedTime_ - rightCompletedTime_)
                < config_.simultaneousThreshold) {
                result = BlinkEvent::BothEyesBlink;
                resetBlinks();
                cooldownTimer_ = config_.cooldown;
            }
        }

        // 한쪽만 깜빡임 — 동시 판정 대기 시간이 지나면 확정
        if (leftBlinkCompleted_) {
            leftCompletedTime_ += dt;
            if (leftCompletedTime_ > config_.simultaneousThreshold * 2.0f) {
                result = BlinkEvent::LeftEyeOnly;
                resetBlinks(); cooldownTimer_ = config_.cooldown;
            }
        }
        if (rightBlinkCompleted_) {
            rightCompletedTime_ += dt;
            if (rightCompletedTime_ > config_.simultaneousThreshold * 2.0f) {
                result = BlinkEvent::RightEyeOnly;
                resetBlinks(); cooldownTimer_ = config_.cooldown;
            }
        }

        leftWasClosed_ = leftClosed;
        rightWasClosed_ = rightClosed;
        return result;
    }

    [[nodiscard]] bool isInCooldown() const { return cooldownTimer_ > 0.0f; }

private:
    Config config_;
    bool leftWasClosed_ = false, rightWasClosed_ = false;
    core::f32 leftCloseTime_ = 0.0f, rightCloseTime_ = 0.0f;
    bool leftBlinkCompleted_ = false, rightBlinkCompleted_ = false;
    core::f32 leftCompletedTime_ = 0.0f, rightCompletedTime_ = 0.0f;
    core::f32 cooldownTimer_ = 0.0f;

    [[nodiscard]] bool isValidBlink(core::f32 dur) const {
        return dur >= config_.minBlinkDuration && dur <= config_.maxBlinkDuration;
    }
    void resetBlinks() {
        leftBlinkCompleted_ = rightBlinkCompleted_ = false;
        leftCompletedTime_ = rightCompletedTime_ = 0.0f;
    }
};

} // namespace gazeshot::game::tracking
```

### Step 4: GazeTracker — 통합 시선 추적기

```hpp
// game/include/gazeshot/game/tracking/GazeTracker.hpp

#pragma once

#include <gazeshot/game/tracking/RingBuffer.hpp>
#include <gazeshot/game/tracking/EyeAnalyzer.hpp>
#include <gazeshot/game/tracking/BlinkDetector.hpp>

#include <array>

namespace gazeshot::game::tracking {

class GazeTracker {
public:
    struct Config {
        EyeAnalyzer::Config eyeConfig{};
        BlinkDetector::Config blinkConfig{};
        core::f32 smoothFactor = 0.7f;  // 0 = 이동 평균만, 1 = 현재 값만
    };

    explicit GazeTracker(const Config& config = {})
        : config_(config)
        , eyeAnalyzer_(config.eyeConfig)
        , blinkDetector_(config.blinkConfig) {}

    struct FrameResult {
        core::math::Vec2f gazePoint;
        BlinkEvent blinkEvent;
        core::f32 averageEAR;
        bool valid;
    };

    [[nodiscard]] FrameResult processFrame(
        const std::array<core::math::Vec2f, 6>& leftEyePts,
        const std::array<core::math::Vec2f, 6>& rightEyePts,
        const core::math::Vec2f& leftPupil,
        const core::math::Vec2f& rightPupil,
        core::f32 dt)
    {
        // 1. 눈 상태 분석
        auto leftEye = eyeAnalyzer_.analyzeEye(
            leftEyePts[0], leftEyePts[1], leftEyePts[2],
            leftEyePts[3], leftEyePts[4], leftEyePts[5], leftPupil);
        auto rightEye = eyeAnalyzer_.analyzeEye(
            rightEyePts[0], rightEyePts[1], rightEyePts[2],
            rightEyePts[3], rightEyePts[4], rightEyePts[5], rightPupil);

        // 2. 시선 통합
        auto gazeResult = eyeAnalyzer_.computeGaze(leftEye, rightEye);

        // 3. 이동 평균 필터
        gazeHistory_.push(gazeResult.gazeDirection);
        earHistory_.push(gazeResult.averageEAR);

        auto smoothed = gazeHistory_.average();
        auto current = gazeResult.gazeDirection;
        auto filtered = core::math::Vec2f{
            smoothed.x * (1.0f - config_.smoothFactor) + current.x * config_.smoothFactor,
            smoothed.y * (1.0f - config_.smoothFactor) + current.y * config_.smoothFactor,
        };

        // 4. 깜빡임 감지
        auto blink = blinkDetector_.update(leftEye.ear, rightEye.ear, dt);

        return { filtered, blink, gazeResult.averageEAR, true };
    }

    [[nodiscard]] core::math::Vec2f latestGaze() const { return gazeHistory_.latest(); }
    [[nodiscard]] core::f32 latestEAR() const { return earHistory_.latest(); }
    void reset() { gazeHistory_.clear(); earHistory_.clear(); }

private:
    Config config_;
    EyeAnalyzer eyeAnalyzer_;
    BlinkDetector blinkDetector_;
    RingBuffer<core::math::Vec2f, 10> gazeHistory_;  // 최근 10프레임 평균
    RingBuffer<core::f32, 30> earHistory_;            // 최근 30프레임 EAR
};

} // namespace gazeshot::game::tracking
```

### Step 5: SniperCamera 연동 + 게임 루프 통합

```cpp
// game/src/main.cpp (Ch.25 업데이트)

#include <gazeshot/game/tracking/GazeTracker.hpp>
#include <gazeshot/game/SniperCamera.hpp>

using namespace gazeshot;
using namespace gazeshot::game::tracking;

struct App {
    game::SniperCamera camera;
    GazeTracker gazeTracker;
    bool useGazeInput = true;
    // ... 기존 멤버 (Ch.24의 faceTracker 등) ...
};

void update(App& app, core::f32 dt) {
    auto& landmarks = app.faceTracker.latestLandmarks();

    if (app.useGazeInput && landmarks.valid) {
        // 눈 랜드마크 추출
        std::array<core::math::Vec2f, 6> leftEyePts, rightEyePts;
        for (int i = 0; i < 6; ++i) {
            leftEyePts[i]  = landmarks.points[EyeLandmarkIndices::left[i]];
            rightEyePts[i] = landmarks.points[EyeLandmarkIndices::right[i]];
        }
        auto leftPupil  = landmarks.points[EyeLandmarkIndices::leftIris];
        auto rightPupil = landmarks.points[EyeLandmarkIndices::rightIris];

        auto result = app.gazeTracker.processFrame(
            leftEyePts, rightEyePts, leftPupil, rightPupil, dt);

        if (result.valid) {
            app.camera.setGazePoint(result.gazePoint);

            switch (result.blinkEvent) {
                case BlinkEvent::BothEyesBlink:
                    shoot(app);
                    std::printf("[Gaze] BLINK -> SHOOT!\n");
                    break;
                case BlinkEvent::LeftEyeOnly:
                case BlinkEvent::RightEyeOnly:
                    reload(app);
                    std::printf("[Gaze] WINK -> RELOAD\n");
                    break;
                case BlinkEvent::None: break;
            }
        }
    } else {
        // 마우스 입력 폴백
        auto mouseDelta = app.input.mouseDelta();
        app.camera.moveGaze(mouseDelta.x, mouseDelta.y);
        if (app.input.isMouseButtonPressed(0)) shoot(app);
    }

    if (app.input.isKeyPressed(SDLK_TAB)) {
        app.useGazeInput = !app.useGazeInput;
        std::printf("[Input] Mode: %s\n", app.useGazeInput ? "GAZE" : "MOUSE");
    }

    app.camera.update(dt);

    // 디버그 출력
    static core::f32 logTimer = 0;
    logTimer += dt;
    if (logTimer >= 0.5f) {
        auto gaze = app.gazeTracker.latestGaze();
        auto ear = app.gazeTracker.latestEAR();
        std::printf("Gaze: (%.2f, %.2f) | EAR: %.2f | Mode: %s\n",
            gaze.x, gaze.y, ear, app.useGazeInput ? "GAZE" : "MOUSE");
        logTimer = 0;
    }
}
```

SniperCamera에 `setGazePoint` 메서드를 추가한다 (Ch.09 확장):

```hpp
class SniperCamera {
public:
    // ... 기존 코드 ...

    // 시선 추적에서 직접 gazePoint 설정 (Ch.25)
    void setGazePoint(core::math::Vec2f point) {
        core::f32 distSq = point.x * point.x + point.y * point.y;
        core::f32 maxR = config_.maxGazeX;
        if (distSq > maxR * maxR) {
            core::f32 dist = std::sqrt(distSq);
            point.x *= maxR / dist;
            point.y *= maxR / dist;
        }
        gazePoint_ = point;
    }
};
```

**C++ 학습 포인트: 신호 처리 패턴을 C++ 제네릭으로**

```cpp
// RingBuffer는 "어떤 타입이든" 이동 평균을 구할 수 있다.
// 단, T가 operator+와 operator*(float)를 지원해야 한다.
RingBuffer<float, 10> earFilter;      // float의 이동 평균
RingBuffer<Vec2f, 10> gazeFilter;     // Vec2f의 이동 평균
RingBuffer<Vec3f, 20> positionFilter; // Vec3f의 이동 평균

// concept로 이 제약을 명시할 수도 있다 (Ch.12 참조):
template<typename T>
concept Averageable = requires(T a, T b, float s) {
    { a + b } -> std::same_as<T>;
    { a * s } -> std::same_as<T>;
};

template<Averageable T, std::size_t N>
class RingBuffer { /* ... */ };
// → + 또는 * 가 없는 타입을 넣으면 명확한 컴파일 오류
```

```
필터 비교:

단순 이동 평균:  가중치 [1, 1, 1, ...]     안정적이나 반응 느림
가중 이동 평균:  가중치 [10, 9, 8, ...]     최신 데이터 비중 높음
지수 이동 평균:  가중치 [1.0, 0.8, 0.64, ..]  과거로 갈수록 기하급수적 감소

이 프로젝트에서는 지수 이동 평균이 가장 적합하다:
최신 시선 위치에 빠르게 반응하면서 과거 데이터로 안정성을 확보한다.
```

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| EAR 계산 | 콘솔에 EAR 값이 0.25~0.35 범위 (정상 눈 뜸) |
| 눈 감김 감지 | 눈을 감으면 EAR이 0.15 이하로 떨어짐 |
| 시선 이동 | 눈을 좌우/상하로 움직이면 gazePoint 변화 |
| 레티클 연동 | gazePoint가 스코프 내 레티클 위치에 반영 |
| 이동 평균 필터 | 시선이 떨리지 않고 부드럽게 이동 |
| 양쪽 깜빡임 | 양쪽 동시 감으면 사격 트리거 |
| 한쪽 깜빡임 | 한쪽만 감으면 재장전 트리거 |
| 디바운싱 | 자연스러운 깜빡임(100ms 미만)은 무시 |
| 쿨다운 | 사격 후 300ms 내 재사격 불가 |
| 입력 전환 | TAB으로 시선/마우스 모드 전환 |
| 시선 범위 제한 | gazePoint가 스코프 원 밖으로 나가지 않음 |
| 디버그 출력 | Gaze, EAR, Mode 실시간 콘솔 표시 |

---

## 4. 블로그 데모 아이디어

1. **EAR 실시간 그래프**: 눈 깜빡임 시 EAR 급락하는 그래프 + threshold 라인
2. **시선 추적 히트맵**: 화면 어디를 주시하는지 가우시안 히트맵 오버레이
3. **깜빡임 타임라인**: 자연 깜빡임(무시) vs 의도적 깜빡임(사격) 비교
4. **필터 비교 GIF**: 필터 없음(떨림) vs 이동 평균(부드러움) 나란히
5. **RingBuffer 시각화**: 순환 버퍼에 데이터가 들어가고 오래된 것이 덮어쓰이는 애니메이션
6. **눈 랜드마크 오버레이**: 웹캠 위에 6개 포인트 + 동공 + EAR 값 표시

---

## 다음 챕터 예고

**Chapter 26: 캘리브레이션 시스템**

시선 추적의 정확도는 사용자마다, 환경마다 다르다.
화면 9점을 순서대로 바라보며 시선-화면 매핑 함수를 학습하고,
드리프트 보정과 민감도 조절 UI를 구현한다.
데모: 캘리브레이션 화면에서 점을 따라 바라보면 정확도가 향상되고,
이후 게임 플레이 시 레티클이 시선을 정확히 따라간다.
