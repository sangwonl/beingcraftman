# Chapter 24: 웹캠 입력과 얼굴 추적

## 데모 미리보기

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐   │
│   │  ◕‿◕         │     │  468 landmarks│     │   ╭────────╮ │   │
│   │   웹캠 입력   │ ──→ │  얼굴 메시    │ ──→ │   │  ＋    │ │   │
│   │   (30fps)    │     │  추출         │     │   │  스코프 │ │   │
│   │   640x480    │     │  + solvePnP   │     │   ╰────────╯ │   │
│   └──────────────┘     └──────────────┘     └──────────────┘   │
│                                                                 │
│       capture thread       process thread       game thread     │
│       (std::jthread)       (landmarks→pose)     (headOffset)    │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│  Face: detected | Yaw: -5.2° Pitch: 3.1° | Head: (0.08, 0.03) │
│  FPS: 60 (game) / 30 (camera) | Latency: 12ms | Kalman: ON     │
└─────────────────────────────────────────────────────────────────┘
```

- **데모**: 웹캠으로 얼굴을 인식하고, 머리 움직임이 SniperCamera의 headOffset으로 반영
- **분리 스레드**: 캡처/처리를 별도 스레드에서 수행하여 게임 루프 60fps 유지
- **Kalman 필터**: 노이즈 제거로 떨림 없는 부드러운 제어
- 블로그에 "웹캠 → 얼굴 메시 → 게임 제어" 파이프라인 다이어그램 포함 가능

---

## 학습 목표

1. 웹캠 입력 파이프라인을 Desktop(SDL3/OpenCV)과 WASM(getUserMedia) 환경에서 구성한다
2. MediaPipe Face Mesh의 468개 랜드마크에서 머리 위치/방향을 추출한다
3. solvePnP 알고리즘으로 머리의 3D 자세(pose)를 추정한다
4. Kalman 필터로 노이즈를 제거하고 부드러운 좌표 변환을 구현한다
5. `std::jthread`와 `std::stop_token`을 활용한 안전한 스레드 관리를 실습한다

---

## 1. 배경 지식

### 웹캠 캡처 파이프라인

Desktop과 WASM에서 카메라 접근 방식이 다르다:

```
Desktop 경로:
  SDL3 Camera API (SDL_OpenCamera)
    또는 OpenCV (cv::VideoCapture)
    → BGR/RGB 프레임 → 처리 스레드로 전달

WASM 경로:
  navigator.mediaDevices.getUserMedia()
    → <video> 요소 → Canvas drawImage
    → getImageData() → SharedArrayBuffer → C++ (Emscripten)

공통: 캡처는 반드시 별도 스레드(또는 Web Worker)에서!
      게임 루프(60fps)를 카메라(30fps)가 블로킹하면 안 된다.
```

### 얼굴 랜드마크와 핵심 포인트

MediaPipe Face Mesh는 468개의 3D 랜드마크를 제공한다:

```
468개 랜드마크 중 핵심 6개 (solvePnP용):

      33 ← 코 끝          정면 기준 3D 모델 좌표 (mm):
     263 ← 턱                코 끝:       ( 0.0,    0.0,    0.0  )
     61  ← 왼쪽 입꼬리       턱:          ( 0.0,  -63.6, -12.5  )
    291  ← 오른쪽 입꼬리     왼쪽 입꼬리: (-43.3,  -32.7,  -26.0 )
    199  ← 왼쪽 눈 바깥     오른쪽 입꼬리:( 43.3,  -32.7,  -26.0 )
    168  ← 미간             왼쪽 눈:     (-28.9,   32.7,  -24.1 )
                            미간:        ( 0.0,    18.0,  -15.0 )
```

### solvePnP와 Head Pose Estimation

```
PnP (Perspective-n-Point) 문제:
  "2D 이미지 좌표와 대응하는 3D 모델 좌표가 주어졌을 때,
   카메라(또는 물체)의 3D 자세(위치 + 방향)를 구하라"

입력:  2D 이미지 포인트 6개 + 3D 모델 포인트 6개 + 카메라 내부 파라미터
출력:  rvec (Rodrigues 회전 벡터) + tvec (이동 벡터)

Rodrigues 변환:
  회전 벡터 (3x1) ↔ 회전 행렬 (3x3)
  벡터의 방향 = 회전축, 크기 = 회전 각도 (라디안)

  rvec = [rx, ry, rz]
  → yaw ≈ ry (좌우), pitch ≈ rx (위아래), roll ≈ rz (갸웃)

  회전 행렬: R = I + sin(θ)[k]× + (1-cos(θ))[k]×²
  여기서 k = rvec/|rvec|, θ = |rvec|
```

### Kalman 필터: 왜 필요한가?

```
랜드마크 검출기의 출력은 프레임마다 떨린다:

  원본 (noise):   ▓▒▓░▓▒░▓▒▓░▒▓
  Kalman 적용 후: ▓▓▓▓▓▒▒▒░░░░░  ← 부드러운 곡선

Kalman 필터의 핵심:
  1. 예측 (Predict): 이전값 + 속도 * dt
  2. 보정 (Update):  예측값 + K * (측정값 - 예측값)

  K = Kalman Gain (0~1)
    K → 0: 부드럽지만 반응 느림 (예측 신뢰)
    K → 1: 빠르지만 떨림 (측정 신뢰)
```

### 좌표계 변환

```
카메라 좌표 → 게임 좌표:

  headOffset.x = -normalize(yaw, -30°, +30°) * maxHeadOffsetX
  headOffset.y =  normalize(pitch, -20°, +20°) * maxHeadOffsetY

캘리브레이션:
  사용자가 정면을 바라볼 때의 yaw/pitch를 기준점(0, 0)으로 설정
  → 상대적 움직임만 게임에 반영
```

---

## 2. 구현 가이드

### Step 1: Kalman 필터

```hpp
// game/include/gazeshot/game/tracking/KalmanFilter1D.hpp

#pragma once

#include <gazeshot/core/Types.hpp>

namespace gazeshot::game::tracking {

// 1D Kalman Filter: position + velocity 상태 모델
class KalmanFilter1D {
public:
    struct Config {
        core::f32 processNoise = 1e-4f;     // Q
        core::f32 measurementNoise = 1e-2f;  // R
    };

    explicit KalmanFilter1D(const Config& config = {})
        : q_(config.processNoise), r_(config.measurementNoise) {}

    core::f32 update(core::f32 measurement, core::f32 dt) {
        x_ += v_ * dt;                           // Predict
        p_ += q_;
        core::f32 k = p_ / (p_ + r_);            // Kalman Gain
        core::f32 innov = measurement - x_;
        x_ += k * innov;                          // Update
        if (dt > 0.0001f) v_ += (k * innov) / dt * 0.1f;
        p_ = (1.0f - k) * p_;
        return x_;
    }

    void reset(core::f32 value = 0.0f) { x_ = value; v_ = 0.0f; p_ = 1.0f; }
    [[nodiscard]] core::f32 value() const { return x_; }

private:
    core::f32 x_ = 0, v_ = 0, p_ = 1, q_, r_;
};

} // namespace gazeshot::game::tracking
```

### Step 2: Head Pose Estimator

```hpp
// game/include/gazeshot/game/tracking/HeadPoseEstimator.hpp

#pragma once

#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/core/Types.hpp>
#include <array>
#include <cmath>

namespace gazeshot::game::tracking {

struct Landmark2D { core::f32 x, y; };
struct HeadPose { core::f32 yaw, pitch, roll; };  // 도 단위

// 3D 모델 포인트 (표준 얼굴 기준, mm 단위)
inline constexpr std::array<core::math::Vec3f, 6> FACE_MODEL_POINTS = {{
    { 0.0f,    0.0f,    0.0f   },   // 코 끝 (원점)
    { 0.0f,  -63.6f, -12.5f   },   // 턱
    {-43.3f, -32.7f, -26.0f   },   // 왼쪽 입꼬리
    { 43.3f, -32.7f, -26.0f   },   // 오른쪽 입꼬리
    {-28.9f,  32.7f, -24.1f   },   // 왼쪽 눈 바깥
    { 0.0f,   18.0f, -15.0f   },   // 미간
}};

// MediaPipe 랜드마크 인덱스
inline constexpr std::array<int, 6> LANDMARK_INDICES = {
    33, 263, 61, 291, 199, 168
};

class HeadPoseEstimator {
public:
    struct Config {
        core::f32 imageWidth = 640.0f;
        core::f32 imageHeight = 480.0f;
        core::f32 focalLength = 640.0f;  // ≈ imageWidth (웹캠 FOV ~60°)
    };

    explicit HeadPoseEstimator(const Config& config = {})
        : config_(config) {}

    // 6개 핵심 랜드마크로 머리 자세 추정 (간소화 버전)
    // 정밀 버전은 OpenCV cv::solvePnP 사용
    HeadPose estimate(const std::array<Landmark2D, 6>& lm) {
        // Yaw: 코 끝의 좌우 편차 (입꼬리 중심 기준)
        core::f32 mouthCenterX = (lm[2].x + lm[3].x) / 2.0f;
        core::f32 mouthWidth = std::abs(lm[3].x - lm[2].x);
        core::f32 yaw = (lm[0].x - mouthCenterX) / std::max(mouthWidth, 1.0f) * 60.0f;

        // Pitch: 코-미간 vs 코-턱 거리 비율
        core::f32 noseToBridge = lm[5].y - lm[0].y;
        core::f32 noseToChin = lm[1].y - lm[0].y;
        core::f32 pitch = (noseToBridge / std::max(std::abs(noseToChin), 1.0f) - 0.5f) * 60.0f;

        // Roll: 눈-입꼬리 기울기
        core::f32 roll = std::atan2(lm[4].y - lm[3].y, lm[4].x - lm[3].x)
                       * 180.0f / 3.14159f + 90.0f;

        return {yaw, pitch, roll};
    }

private:
    Config config_;
};

} // namespace gazeshot::game::tracking
```

**참고**: 정밀한 solvePnP는 OpenCV의 `cv::solvePnP`를 사용한다.
WASM에서는 JavaScript의 MediaPipe가 직접 pose를 제공하므로,
위 코드는 OpenCV 없이도 동작하는 간소화 버전이다.

### Step 3: FaceTracker (캡처 + 처리 스레드)

```hpp
// game/include/gazeshot/game/tracking/FaceTracker.hpp

#pragma once

#include <gazeshot/game/tracking/HeadPoseEstimator.hpp>
#include <gazeshot/game/tracking/KalmanFilter1D.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/core/Types.hpp>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <optional>

namespace gazeshot::game::tracking {

struct FrameData {
    std::vector<core::u8> pixels;
    core::u32 width = 0, height = 0;
    bool valid = false;
};

struct TrackingResult {
    HeadPose pose;
    core::math::Vec2f headOffset;
    bool faceDetected = false;
    core::f32 latencyMs = 0.0f;
};

class FaceTracker {
public:
    struct Config {
        core::f32 maxYawDeg = 30.0f;       core::f32 maxPitchDeg = 20.0f;
        core::f32 maxHeadOffsetX = 0.15f;  core::f32 maxHeadOffsetY = 0.10f;
        core::f32 processNoise = 1e-4f;    core::f32 measureNoise = 1e-2f;
    };

    explicit FaceTracker(const Config& config = {})
        : config_(config)
        , kalmanYaw_({config.processNoise, config.measureNoise})
        , kalmanPitch_({config.processNoise, config.measureNoise}) {}

    ~FaceTracker() = default;  // jthread 소멸자가 자동 stop + join
    FaceTracker(const FaceTracker&) = delete;
    FaceTracker& operator=(const FaceTracker&) = delete;

    void start() {
        captureThread_ = std::jthread([this](std::stop_token st) { captureLoop(st); });
        processThread_ = std::jthread([this](std::stop_token st) { processLoop(st); });
    }

    void stop() {
        captureThread_.request_stop();
        processThread_.request_stop();
        frameReady_.notify_all();
    }

    void calibrate() {
        std::lock_guard lock(resultMutex_);
        calibrationYaw_ = rawPose_.yaw;
        calibrationPitch_ = rawPose_.pitch;
        kalmanYaw_.reset(0.0f);
        kalmanPitch_.reset(0.0f);
    }

    [[nodiscard]] TrackingResult latestResult() const {
        std::lock_guard lock(resultMutex_);
        return latestResult_;
    }

private:
    void captureLoop(std::stop_token stoken) {
        initCamera();
        while (!stoken.stop_requested()) {
            FrameData frame = captureFrame();
            if (!frame.valid) continue;
            { std::lock_guard lock(frameMutex_); pendingFrame_ = std::move(frame); }
            frameReady_.notify_one();
        }
        releaseCamera();
    }

    void processLoop(std::stop_token stoken) {
        HeadPoseEstimator estimator;
        while (!stoken.stop_requested()) {
            FrameData frame;
            {   // 새 프레임 대기
                std::unique_lock lock(frameMutex_);
                frameReady_.wait(lock, stoken, [this] { return pendingFrame_.valid; });
                if (stoken.stop_requested()) break;
                frame = std::move(pendingFrame_);
                pendingFrame_.valid = false;
            }
            auto landmarks = detectLandmarks(frame);
            if (!landmarks) continue;

            auto pose = estimator.estimate(*landmarks);
            rawPose_ = pose;

            // 캘리브레이션 + Kalman 필터 + 게임 좌표 변환
            core::f32 dt = 1.0f / 30.0f;
            core::f32 yaw = kalmanYaw_.update(pose.yaw - calibrationYaw_, dt);
            core::f32 pitch = kalmanPitch_.update(pose.pitch - calibrationPitch_, dt);
            core::f32 headX = -std::clamp(yaw / config_.maxYawDeg, -1.0f, 1.0f)
                              * config_.maxHeadOffsetX;
            core::f32 headY = std::clamp(pitch / config_.maxPitchDeg, -1.0f, 1.0f)
                              * config_.maxHeadOffsetY;

            { std::lock_guard lock(resultMutex_);
              latestResult_ = { {yaw, pitch, pose.roll}, {headX, headY}, true }; }
        }
    }

    // 플랫폼별 stub (Desktop: SDL/OpenCV, WASM: JS interop)
    void initCamera()   { /* SDL_OpenCamera() 또는 cv::VideoCapture(0) */ }
    FrameData captureFrame() { return {}; }
    void releaseCamera() {}
    std::optional<std::array<Landmark2D, 6>>
    detectLandmarks(const FrameData&) { return std::nullopt; }

    Config config_;
    std::mutex frameMutex_;
    std::condition_variable_any frameReady_;
    FrameData pendingFrame_;
    mutable std::mutex resultMutex_;
    TrackingResult latestResult_;
    HeadPose rawPose_{};
    core::f32 calibrationYaw_ = 0.0f, calibrationPitch_ = 0.0f;
    KalmanFilter1D kalmanYaw_, kalmanPitch_;
    std::jthread captureThread_;   // 반드시 마지막에 선언 (초기화 순서 보장)
    std::jthread processThread_;
};

} // namespace gazeshot::game::tracking
```

### Step 4: SniperCamera 연동

Ch.09의 SniperCamera에 얼굴 추적 입력을 연결한다:

```cpp
// game/src/main.cpp (Ch.24)

#include <gazeshot/game/tracking/FaceTracker.hpp>
#include <gazeshot/game/SniperCamera.hpp>

using namespace gazeshot;
using namespace gazeshot::game;

struct App {
    SniperCamera camera;
    tracking::FaceTracker faceTracker;
    bool trackingEnabled = false;
    // ... 기존 멤버 ...
};

void init(App& app) {
    app.faceTracker.start();
    app.trackingEnabled = true;
    std::printf("[FaceTracker] Started. Press C to calibrate.\n");
}

void update(App& app, core::f32 dt) {
    if (app.input.isKeyPressed(SDLK_c)) app.faceTracker.calibrate();
    if (app.input.isKeyPressed(SDLK_t)) app.trackingEnabled = !app.trackingEnabled;

    // 얼굴 추적 → headOffset (Ch.13 applyHeadOffset과 동일 인터페이스)
    if (app.trackingEnabled) {
        auto result = app.faceTracker.latestResult();
        if (result.faceDetected)
            app.camera.applyHeadOffset(result.headOffset);
    } else {
        // 추적 비활성화 시 기존 키보드 제어 (Ch.09)
        bool moving = false;
        if (app.input.isKeyHeld(SDLK_a)) { app.camera.moveHead(-1, 0, dt); moving = true; }
        if (app.input.isKeyHeld(SDLK_d)) { app.camera.moveHead( 1, 0, dt); moving = true; }
        if (!moving) app.camera.returnHead(dt);
    }

    app.camera.moveGaze(app.input.mouseDelta().x, app.input.mouseDelta().y);
    app.camera.update(dt);

    // 디버그 출력
    static core::f32 logTimer = 0;
    logTimer += dt;
    if (logTimer >= 0.5f) {
        auto r = app.faceTracker.latestResult();
        auto ho = app.camera.headOffset();
        std::printf("Face: %s | Yaw: %.1f Pitch: %.1f | Head: (%.2f, %.2f)\n",
            r.faceDetected ? "detected" : "lost", r.pose.yaw, r.pose.pitch, ho.x, ho.y);
        logTimer = 0;
    }
}
```

### Step 5: WASM 및 OpenCV 확장

**WASM**: JS에서 카메라 + MediaPipe를 처리하고 결과만 C++로 전달한다:

```cpp
// platform/src/wasm/FaceCaptureWasm.hpp
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <gazeshot/game/tracking/HeadPoseEstimator.hpp>

namespace gazeshot::game::tracking {
extern "C" {
EMSCRIPTEN_KEEPALIVE  // JS의 MediaPipe onResults → 핵심 6개 랜드마크 전달
void onFaceLandmarks(float* data, int count) {
    if (count < 12) return;
    std::array<Landmark2D, 6> lm;
    for (int i = 0; i < 6; ++i) lm[i] = {data[i*2], data[i*2+1]};
    getGlobalTracker().onLandmarksReceived(lm);
}
EMSCRIPTEN_KEEPALIVE
void onFaceLost() { getGlobalTracker().onFaceLost(); }
} // extern "C"
} // namespace
#endif
```

**OpenCV (Desktop)**: 정밀한 solvePnP 자세 추정:

```cpp
// game/src/tracking/HeadPoseOpenCV.cpp (선택적)
#ifdef GAZESHOT_USE_OPENCV
HeadPose solvePnPPose(const std::array<Landmark2D, 6>& lm, f32 w, f32 h) {
    // FACE_MODEL_POINTS → cv::Point3d, lm → cv::Point2d 변환 후
    cv::Mat cam = (cv::Mat_<double>(3,3) << w, 0, w/2, 0, w, h/2, 0, 0, 1);
    cv::Mat rvec, tvec;
    cv::solvePnP(model, image, cam, cv::Mat::zeros(4,1,CV_64F), rvec, tvec);
    // Rodrigues rvec → 오일러 각도: ry=yaw, rx=pitch, rz=roll
    return { float(rvec.at<double>(1)*180/CV_PI),
             float(rvec.at<double>(0)*180/CV_PI),
             float(rvec.at<double>(2)*180/CV_PI) };
}
#endif
```

---

## 3. C++ 학습 포인트

### `std::jthread` + `std::stop_token` (C++20)

```cpp
// std::thread (C++11): 수동 관리 — 예외 시 join 누락 → terminate!
{ std::thread t([] { /*...*/ }); t.join(); }

// std::jthread (C++20): 소멸자가 자동으로 request_stop() + join()
{
    std::jthread t([](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            auto frame = captureFrame();
            processFrame(frame);
        }
    });
}   // 블록을 벗어나면 자동 정리 — 예외가 발생해도 안전!
```

- **RAII 원칙**: 스레드 수명이 객체 수명에 묶임
- **예외 안전**: try-catch 없이도 안전한 정리
- **협력적 취소**: `stop_token`으로 루프 종료 요청 (강제 kill이 아님)

### `std::mutex` + `std::lock_guard` + `std::condition_variable`

```cpp
std::mutex mutex_;
std::condition_variable_any cv_;
FrameData pendingFrame_;

// ── 생산자 (캡처 스레드) ──
{
    std::lock_guard lock(mutex_);   // RAII 잠금: 스코프 끝에서 자동 해제
    pendingFrame_ = std::move(frame);
}
cv_.notify_one();  // 대기 중인 스레드를 깨움

// ── 소비자 (처리 스레드) ──
{
    std::unique_lock lock(mutex_);
    // C++20: stop_token을 받는 wait — 중지 요청 시 즉시 반환
    cv_.wait(lock, stoken, [this] { return pendingFrame_.valid; });
    if (stoken.stop_requested()) return;
    auto frame = std::move(pendingFrame_);
}
// lock_guard: 단순 RAII 잠금 (가볍고 빠름)
// unique_lock: 조건 변수와 함께 사용 (수동 잠금/해제 가능)
// condition_variable_any: stop_token wait 지원 (C++20)
```

### WASM 대안: Web Worker + SharedArrayBuffer

```
WASM에서는 std::jthread를 직접 쓸 수 없다 (pthread 제한).
대안:

  Main Thread (Game Loop)
       │  postMessage / SharedArrayBuffer
       ▼
  Web Worker (Camera + MediaPipe)
       │  결과를 SharedArrayBuffer에 기록 또는 postMessage
       ▼
  Main Thread에서 결과 읽기

실전 전략: 카메라 + MediaPipe를 JS에서 처리하고,
결과(yaw, pitch)만 C++로 전달하는 것이 가장 간단하다.
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 웹캠 접근 | 카메라 LED 켜짐, 권한 요청 표시 |
| 얼굴 검출 | 콘솔에 "Face: detected" 표시 |
| Yaw/Pitch 반응 | 고개를 돌리면 콘솔 수치 변화 |
| headOffset 반영 | 스코프 뷰가 고개 방향으로 이동 |
| 캘리브레이션 (C) | 현재 자세가 중앙(0, 0)으로 리셋 |
| Kalman 필터 | 떨림 없이 부드러운 이동 |
| 범위 제한 | headOffset이 ±0.15, ±0.10 초과 안 함 |
| 키보드 전환 (T) | T 키로 추적/키보드 모드 전환 |
| 게임 FPS 유지 | 카메라 처리와 무관하게 60fps 유지 |
| 얼굴 미검출 | 카메라 가리면 마지막 위치 유지 (튀지 않음) |

---

## 블로그 데모 아이디어

1. **파이프라인 다이어그램**: 웹캠 → 랜드마크 → solvePnP → Kalman → headOffset 흐름도
2. **랜드마크 시각화 GIF**: 얼굴 위에 468개 점이 실시간으로 따라붙는 영상
3. **Kalman 필터 비교**: 필터 OFF(떨림) vs ON(부드러움) 나란히 비교 GIF
4. **Rodrigues 변환 다이어그램**: 회전 벡터 ↔ 회전 행렬 관계 시각화
5. **스레드 아키텍처**: capture / process / game thread 시퀀스 다이어그램
6. **코드 하이라이트**: `std::jthread`와 `stop_token`으로 안전한 스레드 관리

---

## 다음 챕터 예고

**Chapter 25: 시선 방향 추적**

얼굴 위치(head pose)를 넘어 **눈동자(iris)**의 방향을 추적한다.
MediaPipe Iris 랜드마크로 시선 벡터를 추출하고,
SniperCamera의 gazePoint(가늠쇠)를 눈동자로 제어한다.
데모: 얼굴로 가늠자를, 눈동자로 가늠쇠를 동시에 조작하는 완전한 시선 사격.
