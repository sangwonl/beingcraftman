# Chapter 23: 시선 추적 기술 개관

## 데모 미리보기

이 챕터를 마치면 시선 추적 기술의 전체 파이프라인을 이해하고,
프로토타입 환경에서 MediaPipe로 얼굴을 감지하는 것까지 확인한다.

```
┌──────────────────────────────────────────────────────────────────┐
│                    Eye Tracking Pipeline                         │
│                                                                  │
│  ┌──────────┐    ┌────────────┐    ┌────────────┐    ┌────────┐ │
│  │  Webcam   │───→│ Face Mesh  │───→│ Head Pose  │───→│  rear  │ │
│  │  Frame    │    │ 468 points │    │ (PnP)      │    │ sight  │ │
│  └──────────┘    └─────┬──────┘    └────────────┘    │ offset │ │
│                        │                              └────────┘ │
│                        ▼                                         │
│                  ┌────────────┐    ┌────────────┐    ┌────────┐ │
│                  │ Eye Region │───→│ Gaze       │───→│ front  │ │
│                  │ Crop       │    │ Estimation │    │ sight  │ │
│                  └────────────┘    └────────────┘    │ dir    │ │
│                                                      └────────┘ │
│                                                                  │
│  headOffset + gazeDirection → SniperCamera (Ch.09)               │
└──────────────────────────────────────────────────────────────────┘
```

- **목표**: Milestone 1-2에서 키보드(가늠자) + 마우스(가늠쇠)로 동작하던 입력을 시선 추적으로 교체하기 위한 기술 조사
- **결과물**: 기술 조사 정리, 라이브러리 선택, MediaPipe 기반 얼굴 감지 프로토타입
- 블로그에 "시선 추적 기술 비교표 + 파이프라인 다이어그램" 포함 가능

---

## 학습 목표

1. 시선 추적 기술의 두 가지 접근(하드웨어 vs 소프트웨어)을 이해한다
2. 우리 게임에 필요한 데이터(headOffset, gazeDirection)를 명확히 정의한다
3. 주요 라이브러리를 비교하고 프로젝트에 적합한 것을 선택한다
4. appearance-based와 model-based 시선 추적의 차이를 이해한다
5. PnP(Perspective-n-Point) 알고리즘의 원리를 파악한다
6. MediaPipe Face Mesh 프로토타입을 브라우저에서 실행한다

---

## 1. 배경 지식

### 시선 추적 기술 분류

시선 추적은 크게 두 가지 접근으로 나뉜다.

```
┌─────────────────────────────────────────────────────────────┐
│               Eye Tracking Approaches                       │
├────────────────────────┬────────────────────────────────────┤
│  Hardware-based        │  Software (Webcam)-based           │
│                        │                                    │
│  - 적외선 LED 조명      │  - 일반 웹캠만 사용                 │
│  - 전용 카메라 센서     │  - 컴퓨터 비전 + 딥러닝             │
│  - 높은 정확도          │  - 정확도 중~하                     │
│  - 높은 비용            │  - 무료/저비용                      │
│                        │                                    │
│  예) Tobii Eye Tracker │  예) MediaPipe, OpenFace           │
│      Pupil Labs        │      dlib, GazeML                  │
└────────────────────────┴────────────────────────────────────┘
```

**하드웨어 기반**: IR LED + 전용 카메라로 동공 반사점 추적. 정확도 0.5~1도.
Tobii Eye Tracker 5 기준 약 25만원, 전용 SDK 필요.

**소프트웨어(웹캠) 기반**: 일반 웹캠 + 딥러닝으로 시선 추정. 정확도 3~5도.
추가 하드웨어 불필요, WASM 브라우저에서도 동작 가능.

### 우리 게임에 필요한 데이터

Chapter 09에서 설계한 `SniperCamera`를 떠올려보자:

```
키보드/마우스 (Milestone 1-2):
  WASD     → headOffset   → 카메라 위치(가늠자)
  Mouse    → gazePoint    → 레티클 방향(가늠쇠)

시선 추적 (Milestone 3):
  얼굴 위치 → headOffset   → 카메라 위치(가늠자)
  시선 방향 → gazePoint    → 레티클 방향(가늠쇠)
```

즉, 시선 추적 시스템은 두 가지 데이터를 제공해야 한다:

1. **얼굴 위치/방향** → `moveHead()` 대체. 정확도 낮아도 됨 (cm 단위)
2. **시선 방향** → `moveGaze()` 대체. 높을수록 좋지만 타겟 크기로 보정 가능

### 주요 라이브러리 비교

| 라이브러리 | 얼굴 위치 | 시선 방향 | 무료 | WASM 호환 | 비고 |
|-----------|----------|----------|------|----------|------|
| MediaPipe Face Mesh | O | △ | O | O | 468 랜드마크, Google |
| OpenFace | O | O | O | X | 학술용, 높은 정확도 |
| dlib | O | X | O | △ | 68 랜드마크, C++ 네이티브 |
| Tobii SDK | O | O | △ | X | 전용 하드웨어 필요 |
| GazeML / MPIIGaze | X | O | O | △ | 시선 방향만 특화 |

**기호 설명**:
- O = 지원, △ = 제한적 지원 또는 추가 작업 필요, X = 미지원

각 라이브러리의 특징:

- **MediaPipe Face Mesh**: Google 제작. 468개 3D 랜드마크를 실시간 추적.
  브라우저에서 TensorFlow.js로 네이티브 실행. 30fps 이상 성능.
  시선 방향은 직접 제공하지 않지만, 눈 랜드마크에서 추정 가능.

- **OpenFace**: 학술 기반(CMU). 얼굴 위치 + 시선 + 표정(AU)까지 높은 정확도.
  C++ 네이티브지만 의존성이 많고(OpenCV, dlib, Boost), WASM 빌드 비현실적.

- **dlib**: 순수 C++, 의존성 최소. 68개 랜드마크로 head pose 가능.
  시선 방향은 미지원, 눈 영역 디테일 부족.

```
MediaPipe Face Mesh — 468개 3D 랜드마크:

            ·  ·  ·  ·  ·
         ·                 ·
       ·    ◎           ◎    ·    ← 눈 (각 16개 점)
       ·                      ·
         ·     △              ·    ← 코
           ·   ────────    ·       ← 입
             ·  ·  ·  ·  ·
```

### 접근 전략: 왜 MediaPipe인가

프로젝트 제약: WASM 호환 필수, 무료, 얼굴+시선 모두 필요, 실시간 성능.
이 조건에서 MediaPipe가 최선이다:

```
Phase 1: MediaPipe Face Mesh + 간단한 시선 추정
  ├── 얼굴 위치/방향: 468 랜드마크에서 PnP로 추출
  ├── 시선 방향: 눈 랜드마크(홍채 중심)에서 추정
  └── WASM: 브라우저에서 네이티브 동작

Phase 2 (선택): 커스텀 CNN 모델 학습 (Ch.28)
  └── 눈 이미지 → 시선 벡터 직접 예측 (ONNX Runtime)
```

### 시선 추적의 두 가지 방법론

```
Appearance-based (외형 기반):
  눈 이미지 → [CNN 모델] → 시선 벡터
  장점: 기하학적 모델링 불필요 (end-to-end)
  단점: 학습 데이터에 의존, 환경 변화에 취약
  대표: GazeML, MPIIGaze

Model-based (모델 기반):
  랜드마크 → [3D 모델 피팅] → [눈 중심 + 동공] → 시선 벡터
  장점: 물리적으로 해석 가능, 적은 데이터로 동작
  단점: 기하학적 모델 정확도에 의존
  대표: OpenFace, dlib + PnP
```

**우리 프로젝트의 선택**: Phase 1에서는 **model-based** 접근을 사용한다.
MediaPipe 랜드마크 → PnP로 head pose → 눈 랜드마크에서 시선 추정.
이 방식이 이해하기 쉽고, 수학적으로 명확하며, 튜닝이 직관적이다.

### PnP (Perspective-n-Point) 알고리즘

얼굴 랜드마크에서 머리의 3D 위치/방향을 구하는 핵심 알고리즘이다.

```
PnP 문제: 3D 점의 알려진 좌표 + 2D 투영 관측 → 카메라 위치/방향 역산

표준 3D 얼굴 모델 (대략적 좌표, cm):      MediaPipe 인덱스:
  코끝:       (  0.0,   0.0,   0.0)         #1
  턱:         (  0.0,  -3.3,  -0.7)         #152
  왼눈 끝:    ( -2.3,   2.5,  -1.3)         #33
  오른눈 끝:  (  2.3,   2.5,  -1.3)         #263
  왼입꼬리:   ( -1.5,  -1.5,  -0.6)         #61
  오른입꼬리: (  1.5,  -1.5,  -0.6)         #291

solvePnP(3D점, 2D점) → rotation + translation
→ 머리의 위치(headOffset)와 방향(yaw, pitch, roll)
```

이 알고리즘은 Ch.24에서 실제로 구현한다. 여기서는 원리만 이해한다.

### WASM 고려 사항

```
Desktop:  SDL3 Camera → MediaPipe C++ (또는 OpenCV) → 랜드마크 → 게임 루프
Browser:  getUserMedia → MediaPipe JS (TF.js, WASM 가속) → JS→C++ 인터페이스
```

브라우저에서는 MediaPipe가 JS 라이브러리로 네이티브 동작한다.
C++ 포팅 없이 JS 측에서 추적하고, 결과만 WASM에 넘기면 된다.

---

## 2. 구현 가이드

이 챕터는 서베이 중심이므로 코딩 분량이 적다.
하지만 다음 챕터를 위한 프로토타입 환경을 미리 준비한다.

### Step 1: tracking 네임스페이스 스켈레톤

시선 추적 모듈의 인터페이스를 먼저 설계한다.
아직 구현은 하지 않고, 어떤 데이터를 주고받을지 정의한다.

```hpp
// game/include/gazeshot/game/tracking/TrackingData.hpp

#pragma once

#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/core/Types.hpp>

namespace gazeshot::game::tracking {

// ── 시선 추적 시스템이 제공하는 데이터 ──
struct TrackingData {
    // 얼굴 위치 오프셋 (카메라 기준, 정규화된 좌표)
    // → SniperCamera::moveHead() 대체
    core::math::Vec3f headPosition{0.0f, 0.0f, 0.0f};

    // 머리 방향 (yaw, pitch, roll in radians)
    core::math::Vec3f headRotation{0.0f, 0.0f, 0.0f};

    // 시선 방향 (화면 좌표 기준, -1 ~ 1)
    // → SniperCamera::moveGaze() 대체
    core::math::Vec2f gazePoint{0.0f, 0.0f};

    // 눈 개폐 비율 (0 = 감김, 1 = 완전 열림)
    core::f32 leftEyeOpenness  = 1.0f;
    core::f32 rightEyeOpenness = 1.0f;

    // 깜빡임 감지 (사격 트리거로 사용)
    bool blinkDetected = false;

    // 추적 품질 (0 = 추적 불가, 1 = 최상)
    core::f32 confidence = 0.0f;

    // 타임스탬프 (ms)
    core::f64 timestamp = 0.0;
};

// ── 추적 시스템의 추상 인터페이스 ──
// Ch.24~25에서 구현체를 만든다
class ITracker {
public:
    virtual ~ITracker() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    // 매 프레임 호출: 최신 추적 데이터를 반환
    virtual TrackingData getLatestData() const = 0;

    // 추적 중인지 여부
    virtual bool isTracking() const = 0;

    // 캘리브레이션 (Ch.26에서 구현)
    virtual void startCalibration() {}
    virtual void resetCalibration() {}
};

} // namespace gazeshot::game::tracking
```

### Step 2: SniperCamera에 TrackingData 연결점 준비

기존 `SniperCamera`(Ch.09)에 시선 추적 데이터를 받는 메서드를 추가한다.

```hpp
// game/include/gazeshot/game/SniperCamera.hpp에 추가

enum class InputSource : core::u8 {
    KeyboardMouse,   // Milestone 1-2
    EyeTracking,     // Milestone 3
    Hybrid           // 시선 + 마우스 보정
};

// SniperCamera 클래스 내부:
void applyTrackingData(const tracking::TrackingData& data, core::f32 dt) {
    if (data.confidence < 0.3f) return;  // 추적 품질이 낮으면 무시

    // 머리 위치 → 가늠자 오프셋
    targetHeadOffset_.x = std::clamp(
        data.headPosition.x * config_.headTrackingScale,
        -config_.maxHeadOffsetX, config_.maxHeadOffsetX);
    targetHeadOffset_.y = std::clamp(
        data.headPosition.y * config_.headTrackingScale,
        -config_.maxHeadOffsetY, config_.maxHeadOffsetY);

    // 시선 방향 → 가늠쇠
    gazePoint_.x = std::clamp(data.gazePoint.x, -config_.maxGazeX, config_.maxGazeX);
    gazePoint_.y = std::clamp(data.gazePoint.y, -config_.maxGazeY, config_.maxGazeY);
}
```

### Step 3: MediaPipe 브라우저 프로토타입

MediaPipe Face Mesh가 실제로 어떻게 동작하는지 확인하는 프로토타입이다.
HTML + JavaScript로 작성하며, 브라우저에서 바로 테스트한다.

```html
<!-- web/prototype/face-mesh-test.html (핵심 부분만 발췌) -->
<script type="module">
import { FaceMesh } from
    'https://cdn.jsdelivr.net/npm/@mediapipe/face_mesh/face_mesh.js';
import { Camera } from
    'https://cdn.jsdelivr.net/npm/@mediapipe/camera_utils/camera_utils.js';

const videoEl = document.getElementById('video');
const canvasEl = document.getElementById('canvas');
const ctx = canvasEl.getContext('2d');

// ── Head Pose에 사용할 랜드마크 인덱스 ──
const NOSE_TIP = 1, CHIN = 152;
const LEFT_EYE = 33, RIGHT_EYE = 263;
const LEFT_MOUTH = 61, RIGHT_MOUTH = 291;
const LEFT_IRIS = 468, RIGHT_IRIS = 473;  // iris refinement

const faceMesh = new FaceMesh({
    locateFile: (file) =>
        `https://cdn.jsdelivr.net/npm/@mediapipe/face_mesh/${file}`
});
faceMesh.setOptions({
    maxNumFaces: 1,
    refineLandmarks: true,   // 홍채 랜드마크 활성화 (478개)
    minDetectionConfidence: 0.5,
    minTrackingConfidence: 0.5,
});

faceMesh.onResults((results) => {
    ctx.clearRect(0, 0, 640, 480);
    ctx.drawImage(results.image, 0, 0, 640, 480);
    if (!results.multiFaceLandmarks?.length) return;

    const landmarks = results.multiFaceLandmarks[0];

    // 전체 랜드마크 점 표시
    ctx.fillStyle = 'rgba(46, 140, 138, 0.4)';
    for (const lm of landmarks) {
        ctx.beginPath();
        ctx.arc(lm.x * 640, lm.y * 480, 1, 0, 2 * Math.PI);
        ctx.fill();
    }

    // PnP용 주요 포인트 강조 (빨간색)
    ctx.fillStyle = '#ff4444';
    for (const idx of [NOSE_TIP, CHIN, LEFT_EYE, RIGHT_EYE]) {
        const lm = landmarks[idx];
        ctx.beginPath();
        ctx.arc(lm.x * 640, lm.y * 480, 4, 0, 2 * Math.PI);
        ctx.fill();
    }

    // 홍채 중심 (녹색) — 시선 추정에 사용
    if (landmarks.length > 468) {
        ctx.fillStyle = '#44ff44';
        const li = landmarks[LEFT_IRIS], ri = landmarks[RIGHT_IRIS];
        ctx.beginPath(); ctx.arc(li.x*640, li.y*480, 5, 0, 2*Math.PI); ctx.fill();
        ctx.beginPath(); ctx.arc(ri.x*640, ri.y*480, 5, 0, 2*Math.PI); ctx.fill();

        // 간단한 시선 추정: 홍채 중심의 평균
        const gazeX = ((li.x + ri.x) / 2 - 0.5) * 2;
        const gazeY = ((li.y + ri.y) / 2 - 0.5) * 2;
        console.log(`Gaze: (${gazeX.toFixed(3)}, ${gazeY.toFixed(3)})`);
    }
});

const camera = new Camera(videoEl, {
    onFrame: async () => { await faceMesh.send({ image: videoEl }); },
    width: 640, height: 480,
});
camera.start();
</script>
```

```bash
cd gazeshot/web/prototype
python3 -m http.server 8080
# 브라우저에서 http://localhost:8080/face-mesh-test.html
```

**예상 결과**:
- 웹캠 영상 위에 468개의 점이 얼굴을 따라 실시간 표시
- PnP용 주요 포인트(코끝, 턱, 눈)가 빨간색으로 강조
- 홍채 중심이 녹색 원으로 표시 (iris refinement 옵션)
- 콘솔에 시선 추정 좌표 실시간 출력

### Step 4: C++ 측 더미 트래커 (테스트용)

Ch.24 구현 전까지 마우스로 시선 추적을 시뮬레이션하는 더미 트래커다.

```hpp
// game/include/gazeshot/game/tracking/DummyTracker.hpp
#pragma once
#include <gazeshot/game/tracking/TrackingData.hpp>

namespace gazeshot::game::tracking {

class DummyTracker final : public ITracker {
public:
    bool initialize() override { active_ = true; return true; }
    void shutdown() override { active_ = false; }
    TrackingData getLatestData() const override { return data_; }
    bool isTracking() const override { return active_; }

    // 외부에서 마우스 데이터를 주입하여 시선 추적 시뮬레이션
    void setSimulatedHead(core::f32 x, core::f32 y) {
        data_.headPosition.x = x;  data_.headPosition.y = y;
        data_.confidence = 0.9f;
    }
    void setSimulatedGaze(core::f32 x, core::f32 y) {
        data_.gazePoint = {x, y};
    }
    void setSimulatedBlink(bool blink) { data_.blinkDetected = blink; }

private:
    TrackingData data_;
    bool active_ = false;
};

} // namespace gazeshot::game::tracking
```

---

## 3. C++ 학습 포인트

서베이 챕터이므로 최소화한다. 앞으로 사용할 패턴만 미리 소개.

### 팩토리 패턴 + `[[fallthrough]]` (예고)

```cpp
// Ch.24~25에서 구현할 구조:
std::unique_ptr<ITracker> createTracker(InputSource source) {
    switch (source) {
    case InputSource::EyeTracking:
        return std::make_unique<MediaPipeTracker>();
    case InputSource::KeyboardMouse:
        [[fallthrough]];        // C++17: 의도적 break 생략 명시
    default:
        return std::make_unique<DummyTracker>();
    }
}
```

### enum class + underlying type

```cpp
enum class InputSource : core::u8 { KeyboardMouse, EyeTracking, Hybrid };
// u8 지정 → 1바이트, 직렬화 시 크기 명확
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 기술 이해 | 하드웨어 vs 소프트웨어 시선 추적의 차이를 설명할 수 있음 |
| 데이터 정의 | headOffset, gazeDirection이 SniperCamera에 어떻게 매핑되는지 이해 |
| 라이브러리 선택 | MediaPipe를 선택한 이유를 3가지 이상 설명 가능 |
| PnP 이해 | 6개 랜드마크로 head pose를 구하는 원리를 이해 |
| 프로토타입 | face-mesh-test.html에서 얼굴 랜드마크가 실시간 표시 |
| 인터페이스 | TrackingData, ITracker 인터페이스가 컴파일됨 |
| 더미 트래커 | DummyTracker로 SniperCamera에 시뮬레이션 데이터 전달 가능 |

---

## 5. 블로그 데모 아이디어

1. **파이프라인 다이어그램**: 웹캠 → 랜드마크 → 게임 입력 전체 흐름
2. **라이브러리 비교표**: 위의 비교표에 실제 테스트 결과 추가
3. **MediaPipe 프로토타입 스크린샷**: 468개 랜드마크가 그려진 얼굴
4. **GIF**: 머리를 움직이면 랜드마크가 따라가는 모습
5. **코드 스니펫**: TrackingData 구조체와 ITracker 인터페이스, "이것이 앞으로 채워질 빈 칸"

---

## 6. 심화 읽기

- [MediaPipe Face Mesh](https://google.github.io/mediapipe/solutions/face_mesh.html) -- Google 공식 문서
- [Head Pose Estimation using OpenCV](https://learnopencv.com/head-pose-estimation-using-opencv-and-dlib/) -- PnP 알고리즘 실습
- [Appearance-Based Gaze Estimation in the Wild](https://arxiv.org/abs/1504.02863) -- MPIIGaze 논문
- [Eye Aspect Ratio for Blink Detection](https://vision.fe.uni-lj.si/cvww2016/proceedings/papers/05.pdf) -- EAR 알고리즘 원본
- [OpenFace 2.0](https://github.com/TadasBaltrusaitis/OpenFace) -- 학술용 얼굴 분석 툴킷
- [solvePnP in OpenCV](https://docs.opencv.org/4.x/d5/d1f/calib3d_solvePnP.html) -- PnP 공식 문서

---

## 다음 챕터 예고

**Chapter 24: 웹캠 입력과 얼굴 추적**

SDL3 Camera API로 웹캠 프레임을 캡처하고,
MediaPipe(또는 dlib + OpenCV)로 얼굴 랜드마크를 추출한다.
solvePnP로 head pose를 구해 `SniperCamera::headOffset`에 연결한다.
데모: 얼굴을 움직이면 스코프 시점이 따라 움직인다.
