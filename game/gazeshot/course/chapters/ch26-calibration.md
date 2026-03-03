# Chapter 26: 캘리브레이션 시스템

## 데모 미리보기

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│     ○               ○               ○                    │
│                                                          │
│                  ╔═══════════════╗                        │
│     ○           ║   여기를       ║    ○                   │
│                 ║  바라보세요     ║                        │
│                  ╚═══════════════╝                        │
│                                                          │
│     ○               ●               ○   ← 9-point grid  │
│                     ↑                                    │
│              현재 캘리브레이션 포인트 (활성)                 │
│                                                          │
│  ■■■■■■■■■■■■■■■■□□□□□□□□□□□□□□□  ← 진행률 바 (5/9)      │
│  [ESC] 취소  |  [R] 재시작  |  [SPACE] 포인트 확인         │
│  Drift: 0.02  |  Accuracy: 87%  |  State: CALIBRATING    │
└──────────────────────────────────────────────────────────┘
```

- **데모**: 9개의 화면 포인트를 순서대로 바라보며 시선-화면 매핑을 학습
- **실시간 피드백**: 드리프트 값, 정확도, 진행률을 HUD(Ch.14)에 표시
- **단축키**: R로 재캘리브레이션, ESC로 취소
- 블로그에 "최소자승법으로 시선 매핑" 수학 유도와 "캘리브레이션 UX 설계" 포함 가능

---

## 학습 목표

1. 9-point 캘리브레이션 절차를 설계하고 시선-화면 매핑 함수를 학습한다
2. 최소자승법(least squares)으로 다항식 매핑 계수를 피팅한다
3. 호모그래피 변환의 원리를 이해한다
4. 드리프트 감지와 런타임 보정 메커니즘을 구현한다
5. `std::array` 캘리브레이션 저장, `constexpr` 그리드, 행렬 수학을 실습한다

---

## 1. 배경 지식

### 왜 캘리브레이션이 필요한가?

Ch.25에서 시선 방향을 추적했지만, 시선 값 → 화면 좌표 변환은 사용자마다 다르다 (모니터 거리, 카메라 각도, 눈 형태, 자세). 캘리브레이션은 이 개인차를 학습하는 과정이다.

### 최소자승법 (Least Squares)

```
2차 다항식 매핑:
  screenX = a0 + a1*gX + a2*gY + a3*gX² + a4*gY² + a5*gX*gY

행렬로 표현: A * c = s
  A = | 1  gX₁  gY₁  gX₁²  gY₁²  gX₁*gY₁ |   (N×6, N=9)
      | 1  gX₂  gY₂  gX₂²  gY₂²  gX₂*gY₂ |
      | ...                                   |
  c = | a0  a1  a2  a3  a4  a5 |ᵀ              (계수 벡터)

최소자승 해: c = (AᵀA)⁻¹ * Aᵀs
  → Ch.02에서 만든 행렬 역행렬 함수를 활용!
```

### 호모그래피 (Homography)

```
시선 평면 → 화면 평면의 3×3 투영 변환:
  screenX = (h00*gX + h01*gY + h02) / (h20*gX + h21*gY + 1)
  screenY = (h10*gX + h11*gY + h12) / (h20*gX + h21*gY + 1)

8개 미지수 → 4쌍 이상의 대응점 필요 (9점이면 과결정)

호모그래피 vs 다항식:
  호모그래피: 기하학적 의미 명확 (평면 투영)
  다항식:     유연, 비선형 왜곡 포착 가능
  이 프로젝트: 2차 다항식을 기본으로 사용
```

### 드리프트와 런타임 보정

자세 변화, 조명 변화, 피로 등으로 매핑이 점차 어긋나는 현상. 대응: 오프셋 누적 평균으로 자동 보정 + R키로 재캘리브레이션.

---

## 2. 구현 가이드

### Step 1: 캘리브레이션 데이터 구조

```hpp
// game/include/gazeshot/game/tracking/CalibrationData.hpp
#pragma once
#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/core/Types.hpp>
#include <array>

namespace gazeshot::game::tracking {

static constexpr core::u32 CALIB_GRID_COLS = 3;
static constexpr core::u32 CALIB_GRID_ROWS = 3;
static constexpr core::u32 CALIB_POINT_COUNT = CALIB_GRID_COLS * CALIB_GRID_ROWS;

// constexpr 함수로 컴파일 타임에 9-point 그리드 생성
constexpr auto generateCalibrationGrid() {
    std::array<core::math::Vec2f, CALIB_POINT_COUNT> points{};
    constexpr core::f32 margin = 0.1f;
    constexpr core::f32 range = 1.0f - 2.0f * margin;

    for (core::u32 row = 0; row < CALIB_GRID_ROWS; ++row) {
        for (core::u32 col = 0; col < CALIB_GRID_COLS; ++col) {
            core::u32 idx = row * CALIB_GRID_COLS + col;
            points[idx] = {
                margin + range * static_cast<core::f32>(col)
                    / static_cast<core::f32>(CALIB_GRID_COLS - 1),
                margin + range * static_cast<core::f32>(row)
                    / static_cast<core::f32>(CALIB_GRID_ROWS - 1)
            };
        }
    }
    return points;
}

static constexpr auto CALIB_SCREEN_POINTS = generateCalibrationGrid();

struct CalibrationSample {
    core::math::Vec2f gazeRaw;     // 시선 추적기 원시 값
    core::math::Vec2f screenPos;   // 대응 화면 좌표 (0~1)
    bool collected = false;
};

struct CalibrationData {
    std::array<CalibrationSample, CALIB_POINT_COUNT> samples{};
    core::u32 collectedCount = 0;

    [[nodiscard]] bool isComplete() const { return collectedCount >= CALIB_POINT_COUNT; }
    [[nodiscard]] core::u32 currentIndex() const { return collectedCount; }
    [[nodiscard]] core::math::Vec2f currentScreenPoint() const {
        if (collectedCount >= CALIB_POINT_COUNT) return {0.5f, 0.5f};
        return CALIB_SCREEN_POINTS[collectedCount];
    }

    void collectSample(core::math::Vec2f gazeRaw) {
        if (collectedCount >= CALIB_POINT_COUNT) return;
        auto& s = samples[collectedCount];
        s.gazeRaw = gazeRaw;
        s.screenPos = CALIB_SCREEN_POINTS[collectedCount];
        s.collected = true;
        ++collectedCount;
    }

    void reset() {
        for (auto& s : samples) { s = {}; }
        collectedCount = 0;
    }
};

} // namespace gazeshot::game::tracking
```

**C++ 학습 포인트: `constexpr` 그리드 + `std::array`**

```cpp
// constexpr 함수 → 바이너리에 이미 계산된 값 포함 (런타임 비용 0)
static constexpr auto CALIB_SCREEN_POINTS = generateCalibrationGrid();

// std::array vs std::vector:
//   std::array<T, 9>: 스택, 크기가 타입에 포함, constexpr 호환
//   std::vector<T>:   힙, 런타임 크기, 의도치 않은 resize 가능
// 캘리브레이션은 "정확히 9개"가 보장되어야 하므로 std::array가 적합
```

### Step 2: 최소자승 매핑 함수

```hpp
// game/include/gazeshot/game/tracking/GazeMapper.hpp
#pragma once
#include <gazeshot/game/tracking/CalibrationData.hpp>
#include <array>
#include <cmath>

namespace gazeshot::game::tracking {

static constexpr core::u32 POLY_TERMS = 6;  // 1 + gX + gY + gX² + gY² + gX*gY

struct MappingCoefficients {
    std::array<core::f32, POLY_TERMS> cx{};  // screenX 계수
    std::array<core::f32, POLY_TERMS> cy{};  // screenY 계수
    bool valid = false;
};

class GazeMapper {
public:
    // 캘리브레이션 데이터로 매핑 계수 피팅
    bool fit(const CalibrationData& data) {
        if (!data.isComplete()) return false;
        constexpr core::u32 N = CALIB_POINT_COUNT;

        // A 행렬 구성: 각 행 = [1, gX, gY, gX², gY², gX*gY]
        std::array<core::f32, N * POLY_TERMS> A{};
        std::array<core::f32, N> sx{}, sy{};

        for (core::u32 i = 0; i < N; ++i) {
            core::f32 gx = data.samples[i].gazeRaw.x;
            core::f32 gy = data.samples[i].gazeRaw.y;
            A[i*POLY_TERMS+0] = 1.0f;  A[i*POLY_TERMS+1] = gx;
            A[i*POLY_TERMS+2] = gy;    A[i*POLY_TERMS+3] = gx*gx;
            A[i*POLY_TERMS+4] = gy*gy; A[i*POLY_TERMS+5] = gx*gy;
            sx[i] = data.samples[i].screenPos.x;
            sy[i] = data.samples[i].screenPos.y;
        }

        // c = (AᵀA)⁻¹ * Aᵀs  (6×6 역행렬은 가우스-조르단으로)
        std::array<core::f32, POLY_TERMS*POLY_TERMS> AtA{}, AtA_inv{};
        std::array<core::f32, POLY_TERMS> Atsx{}, Atsy{};
        matMulAtA(A, N, AtA);
        matMulAtb(A, sx, N, Atsx);
        matMulAtb(A, sy, N, Atsy);

        if (!invertNxN<POLY_TERMS>(AtA, AtA_inv)) {
            coeff_.valid = false;
            return false;
        }
        matMulMv<POLY_TERMS>(AtA_inv, Atsx, coeff_.cx);
        matMulMv<POLY_TERMS>(AtA_inv, Atsy, coeff_.cy);
        coeff_.valid = true;
        return true;
    }

    // 시선 → 화면 매핑
    [[nodiscard]] core::math::Vec2f map(core::math::Vec2f gaze) const {
        if (!coeff_.valid) return gaze;
        core::f32 gx = gaze.x, gy = gaze.y;
        std::array<core::f32, POLY_TERMS> t = {1, gx, gy, gx*gx, gy*gy, gx*gy};
        core::f32 sx = 0, sy = 0;
        for (core::u32 i = 0; i < POLY_TERMS; ++i) {
            sx += coeff_.cx[i] * t[i];
            sy += coeff_.cy[i] * t[i];
        }
        return {sx, sy};
    }

    // 드리프트 보정: 상수항(a0, b0)에 오프셋 적용
    void applyDriftCorrection(core::math::Vec2f offset) {
        if (!coeff_.valid) return;
        coeff_.cx[0] += offset.x;
        coeff_.cy[0] += offset.y;
    }

    [[nodiscard]] bool isCalibrated() const { return coeff_.valid; }

private:
    // ── 행렬 연산 헬퍼 (std::array 기반, 고정 크기) ──

    static void matMulAtA(const auto& A, core::u32 N, auto& out) {
        for (core::u32 i = 0; i < POLY_TERMS; ++i)
            for (core::u32 j = 0; j < POLY_TERMS; ++j) {
                core::f32 s = 0;
                for (core::u32 k = 0; k < N; ++k)
                    s += A[k*POLY_TERMS+i] * A[k*POLY_TERMS+j];
                out[i*POLY_TERMS+j] = s;
            }
    }

    static void matMulAtb(const auto& A, const auto& b, core::u32 N, auto& out) {
        for (core::u32 i = 0; i < POLY_TERMS; ++i) {
            core::f32 s = 0;
            for (core::u32 k = 0; k < N; ++k) s += A[k*POLY_TERMS+i] * b[k];
            out[i] = s;
        }
    }

    template<core::u32 S>
    static bool invertNxN(const std::array<core::f32, S*S>& M,
                          std::array<core::f32, S*S>& inv) {
        // 가우스-조르단 소거법: [M | I] → [I | M⁻¹]
        std::array<core::f32, S*S*2> aug{};
        for (core::u32 i = 0; i < S; ++i)
            for (core::u32 j = 0; j < S; ++j) {
                aug[i*(S*2)+j] = M[i*S+j];
                aug[i*(S*2)+S+j] = (i == j) ? 1.0f : 0.0f;
            }

        for (core::u32 c = 0; c < S; ++c) {
            // 부분 피봇팅
            core::u32 maxR = c;
            for (core::u32 r = c+1; r < S; ++r)
                if (std::abs(aug[r*(S*2)+c]) > std::abs(aug[maxR*(S*2)+c]))
                    maxR = r;
            if (std::abs(aug[maxR*(S*2)+c]) < 1e-10f) return false;
            if (maxR != c)
                for (core::u32 j = 0; j < S*2; ++j)
                    std::swap(aug[c*(S*2)+j], aug[maxR*(S*2)+j]);

            core::f32 piv = aug[c*(S*2)+c];
            for (core::u32 j = 0; j < S*2; ++j) aug[c*(S*2)+j] /= piv;
            for (core::u32 r = 0; r < S; ++r) {
                if (r == c) continue;
                core::f32 f = aug[r*(S*2)+c];
                for (core::u32 j = 0; j < S*2; ++j)
                    aug[r*(S*2)+j] -= f * aug[c*(S*2)+j];
            }
        }

        for (core::u32 i = 0; i < S; ++i)
            for (core::u32 j = 0; j < S; ++j)
                inv[i*S+j] = aug[i*(S*2)+S+j];
        return true;
    }

    template<core::u32 S>
    static void matMulMv(const std::array<core::f32, S*S>& M,
                         const std::array<core::f32, S>& v,
                         std::array<core::f32, S>& out) {
        for (core::u32 i = 0; i < S; ++i) {
            core::f32 s = 0;
            for (core::u32 j = 0; j < S; ++j) s += M[i*S+j] * v[j];
            out[i] = s;
        }
    }

    MappingCoefficients coeff_;
};

} // namespace gazeshot::game::tracking
```

### Step 3: 캘리브레이션 스테이트 (Ch.15 GameState 확장)

```hpp
// game/include/gazeshot/game/tracking/CalibrationState.hpp
#pragma once
#include <gazeshot/game/tracking/CalibrationData.hpp>
#include <gazeshot/game/tracking/GazeMapper.hpp>

namespace gazeshot::game::tracking {

// Ch.15의 GameState variant에 추가:
// using GameState = std::variant<ReadyState, CalibrationState, PlayingState, ResultState>;

enum class CalibPhase : core::u8 {
    FaceReference, GazeCollection, Computing, Validating, Complete, Failed
};

class CalibrationState {
public:
    void start() {
        phase_ = CalibPhase::FaceReference;
        data_.reset();
        timer_ = 0; accuracy_ = 0;
    }

    void update(core::math::Vec2f gazeRaw, core::f32 dt) {
        timer_ += dt;
        switch (phase_) {
        case CalibPhase::GazeCollection: {
            // 포인트 애니메이션
            auto target = data_.currentScreenPoint();
            pointPos_.x = core::math::lerp(pointPos_.x, target.x, 3.0f * dt);
            pointPos_.y = core::math::lerp(pointPos_.y, target.y, 3.0f * dt);
            // 시선 누적 (여러 프레임 평균으로 안정화)
            gazeAccum_.x += gazeRaw.x;
            gazeAccum_.y += gazeRaw.y;
            ++accumCount_;
            break;
        }
        case CalibPhase::Computing:
            if (mapper_.fit(data_)) { phase_ = CalibPhase::Validating; timer_ = 0; }
            else phase_ = CalibPhase::Failed;
            break;
        case CalibPhase::Validating:
            accuracy_ = 1.0f - mapper_.fitError();
            phase_ = CalibPhase::Complete;
            break;
        default: break;
        }
    }

    void confirmPoint(core::math::Vec2f gazeRaw) {
        if (phase_ == CalibPhase::FaceReference) {
            faceRef_ = gazeRaw;
            phase_ = CalibPhase::GazeCollection;
            pointPos_ = data_.currentScreenPoint();
            gazeAccum_ = {}; accumCount_ = 0;
            return;
        }
        if (phase_ != CalibPhase::GazeCollection) return;

        core::math::Vec2f avg = (accumCount_ > 0)
            ? core::math::Vec2f{gazeAccum_.x / accumCount_, gazeAccum_.y / accumCount_}
            : gazeRaw;
        data_.collectSample(avg);
        gazeAccum_ = {}; accumCount_ = 0;

        if (data_.isComplete()) { phase_ = CalibPhase::Computing; timer_ = 0; }
    }

    [[nodiscard]] CalibPhase phase() const { return phase_; }
    [[nodiscard]] const CalibrationData& data() const { return data_; }
    [[nodiscard]] GazeMapper& mapper() { return mapper_; }
    [[nodiscard]] const GazeMapper& mapper() const { return mapper_; }
    [[nodiscard]] core::f32 accuracy() const { return accuracy_; }
    [[nodiscard]] core::math::Vec2f pointPos() const { return pointPos_; }
    [[nodiscard]] core::f32 progress() const {
        return static_cast<core::f32>(data_.collectedCount) / CALIB_POINT_COUNT;
    }
    [[nodiscard]] bool isComplete() const { return phase_ == CalibPhase::Complete; }

private:
    CalibPhase phase_ = CalibPhase::FaceReference;
    CalibrationData data_;
    GazeMapper mapper_;
    core::math::Vec2f faceRef_{}, pointPos_{0.5f, 0.5f}, gazeAccum_{};
    core::u32 accumCount_ = 0;
    core::f32 timer_ = 0, accuracy_ = 0;
};

} // namespace gazeshot::game::tracking
```

### Step 4: 드리프트 감지기

```hpp
// game/include/gazeshot/game/tracking/DriftDetector.hpp
#pragma once
#include <gazeshot/core/math/Math.hpp>
#include <array>
#include <cmath>

namespace gazeshot::game::tracking {

class DriftDetector {
public:
    void update(core::math::Vec2f mapped, core::math::Vec2f expected, core::f32 dt) {
        timer_ += dt;
        if (timer_ < 2.0f) return;  // 2초마다 샘플링
        timer_ = 0;

        auto diff = mapped - expected;
        history_[head_] = diff;
        head_ = (head_ + 1) % MAX_HIST;
        if (count_ < MAX_HIST) ++count_;

        // 평균 드리프트 계산
        drift_ = {};
        for (core::u32 i = 0; i < count_; ++i) {
            drift_.x += history_[i].x;
            drift_.y += history_[i].y;
        }
        drift_.x /= count_; drift_.y /= count_;
        magnitude_ = std::sqrt(drift_.x*drift_.x + drift_.y*drift_.y);
    }

    [[nodiscard]] core::math::Vec2f correctionOffset() const {
        if (magnitude_ < 0.025f) return {};
        return {-drift_.x * 0.02f, -drift_.y * 0.02f};
    }

    [[nodiscard]] bool needsRecalibration() const { return magnitude_ > 0.05f; }
    [[nodiscard]] core::f32 magnitude() const { return magnitude_; }
    void reset() { count_ = head_ = 0; magnitude_ = 0; drift_ = {}; timer_ = 0; }

private:
    static constexpr core::u32 MAX_HIST = 20;
    std::array<core::math::Vec2f, MAX_HIST> history_{};
    core::u32 head_ = 0, count_ = 0;
    core::math::Vec2f drift_{};
    core::f32 magnitude_ = 0, timer_ = 0;
};

} // namespace gazeshot::game::tracking
```

### Step 5: 게임 루프 통합

```cpp
// game/src/main.cpp (Ch.26)
#include <gazeshot/game/tracking/CalibrationState.hpp>
#include <gazeshot/game/tracking/DriftDetector.hpp>
#include <gazeshot/game/SniperCamera.hpp>
#include <gazeshot/game/HudRenderer.hpp>

using namespace gazeshot;
using namespace gazeshot::game::tracking;

struct App {
    game::SniperCamera camera;
    game::HudRenderer hud;
    CalibrationState calibState;
    DriftDetector driftDetector;
    bool inCalibration = false;
    // ... 기존 멤버 ...
};

void update(App& app, core::f32 dt) {
    core::math::Vec2f gazeRaw{};  // Ch.25 gazeTracker에서 획득

    if (app.inCalibration) {
        app.calibState.update(gazeRaw, dt);
        if (app.input.isKeyPressed(SDLK_SPACE)) app.calibState.confirmPoint(gazeRaw);
        if (app.input.isKeyPressed(SDLK_r))     app.calibState.start();
        if (app.input.isKeyPressed(SDLK_ESCAPE)) app.inCalibration = false;
        if (app.calibState.isComplete()) {
            app.inCalibration = false;
            std::printf("Calibration done! Accuracy: %.1f%%\n",
                        app.calibState.accuracy() * 100.0f);
        }
        return;
    }

    // 캘리브레이션된 매핑 적용
    core::math::Vec2f screen = gazeRaw;
    if (app.calibState.mapper().isCalibrated()) {
        screen = app.calibState.mapper().map(gazeRaw);
        app.driftDetector.update(screen, {0.5f, 0.5f}, dt);
        app.calibState.mapper().applyDriftCorrection(
            app.driftDetector.correctionOffset());
    }

    // 0~1 → -1~1 변환 후 SniperCamera(Ch.09)에 전달
    app.camera.moveGaze(screen.x * 2.0f - 1.0f, screen.y * 2.0f - 1.0f);

    // R키: 재캘리브레이션
    if (app.input.isKeyPressed(SDLK_r)) {
        app.inCalibration = true;
        app.calibState.start();
        app.driftDetector.reset();
    }
    app.camera.update(dt);
}
```

---

## 3. C++ 학습 포인트 정리

### `std::array`로 캘리브레이션 포인트 저장

```cpp
// 캘리브레이션은 항상 정확히 9개 → std::array가 적합
std::array<CalibrationSample, CALIB_POINT_COUNT> samples{};

// vs std::vector: 힙 할당, 런타임 크기, resize 가능 → 의도치 않은 변경 위험
// std::array: 스택, 컴파일 타임 크기, constexpr 호환, 크기 변경 불가
```

### `constexpr` 그리드 레이아웃

```cpp
constexpr auto generateCalibrationGrid() { /* 반복문으로 계산 */ }
static constexpr auto CALIB_SCREEN_POINTS = generateCalibrationGrid();
// → 바이너리에 직접 포함, 런타임 비용 0
// → 그리드 상수만 바꾸면 자동 재계산
```

### 커스텀 수학 라이브러리로 최소자승

```cpp
// Ch.02에서 만든 행렬 연산을 std::array 기반으로 실전 활용
// 6×6 역행렬: 가우스-조르단 소거법 + 부분 피봇팅
// GLM 없이도 최소자승법을 풀 수 있다는 것이 핵심 경험
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 그리드 표시 | 화면에 9개 포인트가 올바른 위치에 표시 |
| 포인트 진행 | SPACE 누를 때마다 다음 포인트로 이동 |
| 진행률 바 | 하단 바가 포인트 수집에 따라 증가 |
| 얼굴 기준 | 첫 단계에서 얼굴 기준 위치 설정 |
| 매핑 계산 | 9개 수집 후 매핑 계수가 자동 계산 |
| 정확도 표시 | 캘리브레이션 완료 시 정확도(%) 표시 |
| 매핑 적용 | 캘리브레이션 후 시선 값이 화면 좌표로 변환 |
| 드리프트 감지 | 시간 경과 후 드리프트 수치 표시 |
| 재캘리브레이션 | R키로 캘리브레이션 재시작 |
| 취소 | ESC로 캘리브레이션 취소 가능 |
| 상태 통합 | Ch.15의 GameState에 CalibrationState 추가 |

---

## 블로그 데모 아이디어

1. **최소자승법 수학 유도**: A, AᵀA, (AᵀA)⁻¹ 과정을 시각적으로 설명
2. **캘리브레이션 진행 GIF**: 9개 포인트를 순서대로 바라보는 과정
3. **매핑 전/후 비교**: 캘리브레이션 전(원시 시선) vs 후(매핑된 화면 좌표)
4. **드리프트 그래프**: 시간에 따른 드리프트 변화와 자동 보정 과정
5. **정확도 히트맵**: 화면 각 영역의 매핑 정확도를 색상으로 시각화
6. **코드 하이라이트**: `constexpr` 그리드 생성과 `std::array` 행렬 연산

---

## 다음 챕터 예고

**Chapter 27: 통합과 튜닝**

키보드/마우스 입력과 시선 추적을 통합하고, 레이턴시 최적화와 게임 밸런스를 조정한다.
데모: 시선 추적 + 마우스 하이브리드 모드로 플레이 가능한 최종 빌드, 프로파일링 오버레이.
