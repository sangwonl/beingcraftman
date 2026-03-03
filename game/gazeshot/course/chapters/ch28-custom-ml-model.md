# Chapter 28: (보너스) 직접 시선 추적 모델 만들기

## 데모 미리보기

```
┌─────────────────────────────────────────────────────────────────┐
│  ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌────────┐│
│  │  DATA     │     │  TRAIN   │     │  EXPORT  │     │  GAME  ││
│  │ ┌──┐ ┌──┐│     │ PyTorch  │     │  .onnx   │     │  C++   ││
│  │ │👁│ │👁││ ──→ │ CNN      │ ──→ │  model   │ ──→ │  ONNX  ││
│  │ └──┘ └──┘│     │ training │     │  file    │     │Runtime ││
│  │ eye crop │     │ loss:0.02│     │ 2.1 MB   │     │ 3.2ms  ││
│  │ + label  │     │ epoch:50 │     │          │     │/infer  ││
│  └──────────┘     └──────────┘     └──────────┘     └────────┘│
│                                                                 │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ 비교:  MediaPipe 시선 추정  →  평균 오차 4.2도               ││
│  │        직접 학습 모델       →  평균 오차 2.8도 (캘리브 후)    ││
│  │        추론 속도            →  3.2ms/frame (ONNX Runtime)    ││
│  └─────────────────────────────────────────────────────────────┘│
│  Console: "GazeML: model loaded, input: 64x64x1, output: 2"    │
└─────────────────────────────────────────────────────────────────┘
```

- **데모**: 직접 학습한 CNN 모델로 시선을 추정하여 레티클 이동
- **파이프라인**: 캘리브레이션 데이터 수집 → Python 학습 → ONNX 변환 → C++ 추론
- **비교**: 기존 MediaPipe 기반 추정 vs 직접 학습 모델의 정확도/속도 비교
- 블로그에 "데이터 수집부터 게임 연동까지 전체 ML 파이프라인" 다이어그램 포함 가능

---

## 학습 목표

1. CNN(합성곱 신경망)의 기본 구조를 이해하고 시선 추정 모델을 설계한다
2. 캘리브레이션 데이터와 공개 데이터셋으로 학습 파이프라인을 구축한다
3. PyTorch 학습 → ONNX 변환 → C++ 추론의 전체 흐름을 구현한다
4. ONNX Runtime의 C API를 RAII로 안전하게 래핑하고 `std::span`으로 텐서를 전달한다

---

## 1. 배경 지식

### CNN 기초: 왜 이미지에 합성곱인가?

```
전결합(FC) 신경망: 64x64 이미지 = 4096 입력 → 히든 1024 = 419만 파라미터!
합성곱(Conv) 신경망: 작은 필터가 슬라이딩 → 파라미터 공유, 위치 불변

  ┌─────────────────────────────────────────┐
  │ Input      Conv1      Pool1     Conv2   │
  │ 64x64x1 → 62x62x16 → 31x31x16 → ...   │
  │ ...  Pool2     Flatten    FC      Output │
  │ ... → 6x6x64 → 2304 → 128 → 2         │
  │                              (gaze_x,y) │
  └─────────────────────────────────────────┘

계층적 특징 추출: 에지 → 텍스처 → 동공 → 시선 방향
```

### 전이 학습 (Transfer Learning)

```
문제: 캘리브레이션 9점 × 30프레임 = 270개 → CNN 학습에 부족

해결: 전이 학습
  ┌───────────────────────────────────────┐
  │ 사전 학습: MPIIGaze 21만장            │
  │ → "눈 이미지에서 시선을 추정하는 법"   │
  │   을 일반적으로 학습                   │
  └───────────────┬───────────────────────┘
  ┌───────────────▼───────────────────────┐
  │ 미세 조정: 캘리브레이션 데이터         │
  │ → Conv 레이어 고정 (freeze)           │
  │ → FC 레이어만 재학습 → 개인 맞춤      │
  └───────────────────────────────────────┘
```

### ONNX: 학습과 추론의 다리

```
PyTorch 모델 (.pt)
    │  torch.onnx.export()
    ▼
ONNX 모델 (.onnx)    ← 범용 교환 포맷
    │  OrtSession::Run()
    ▼
C++ 추론 결과

ONNX Runtime: Microsoft 주도, CPU/GPU/WASM 다양한 백엔드 지원
```

---

## 2. 구현 가이드

### Step 1: 모델 아키텍처 (Python)

```python
# ml/model.py
import torch
import torch.nn as nn

class GazeEstimationCNN(nn.Module):
    """입력: 1x64x64 눈 이미지, 출력: (gaze_x, gaze_y)"""
    def __init__(self, num_outputs: int = 2):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv2d(1, 16, 3), nn.BatchNorm2d(16), nn.ReLU(), nn.MaxPool2d(2),
            nn.Conv2d(16, 32, 3), nn.BatchNorm2d(32), nn.ReLU(), nn.MaxPool2d(2),
            nn.Conv2d(32, 64, 3), nn.BatchNorm2d(64), nn.ReLU(), nn.MaxPool2d(2),
        )  # → 64x6x6
        self.regressor = nn.Sequential(
            nn.Flatten(),
            nn.Linear(64 * 6 * 6, 128), nn.ReLU(), nn.Dropout(0.3),
            nn.Linear(128, num_outputs),
        )

    def forward(self, x):
        return self.regressor(self.features(x))
# 총 파라미터: ~31만 → 모델 파일: ~1.2 MB, 추론: ~2-4ms (CPU)
```

### Step 2: 학습 파이프라인 (Python)

```python
# ml/train.py
import torch, torch.nn as nn, torch.optim as optim
from torch.utils.data import DataLoader, random_split, Dataset

class GazeDataset(Dataset):
    """캘리브레이션에서 수집한 (눈 이미지, 시선 좌표) 쌍
    data/images/: 64x64 흑백 PNG, data/labels.csv: image,gaze_x,gaze_y
    __getitem__: 이미지 → 0~1 정규화 → (1,64,64) 텐서"""

def train():
    model = GazeEstimationCNN().to(device)

    # 전이 학습: 사전 학습된 Conv 레이어 로딩 + freeze
    try:
        state = torch.load("pretrained/gaze_mpiigaze.pt")
        model.features.load_state_dict(state["features"])
        for p in model.features.parameters(): p.requires_grad = False
    except FileNotFoundError:
        pass  # 처음부터 학습

    criterion = nn.MSELoss()
    optimizer = optim.Adam(filter(lambda p: p.requires_grad, model.parameters()),
                           lr=1e-3, weight_decay=1e-4)

    for epoch in range(100):
        model.train()
        for images, labels in train_loader:
            optimizer.zero_grad()
            loss = criterion(model(images), labels)
            loss.backward(); optimizer.step()

        model.eval()
        val_loss = evaluate(model, val_loader, criterion)
        if val_loss < best_val_loss:
            torch.save(model.state_dict(), "best_gaze_model.pt")
```

### Step 3: ONNX 변환 (Python)

```python
# ml/export_onnx.py
import torch
from model import GazeEstimationCNN

model = GazeEstimationCNN()
model.load_state_dict(torch.load("best_gaze_model.pt"))
model.eval()

torch.onnx.export(
    model, torch.randn(1, 1, 64, 64), "gaze_model.onnx",
    input_names=["eye_image"], output_names=["gaze_vector"],
    dynamic_axes={"eye_image": {0: "batch"}, "gaze_vector": {0: "batch"}},
    opset_version=17,
)
```

### Step 4: ONNX Runtime C++ RAII 래퍼

```hpp
// game/include/gazeshot/game/tracking/ml/OrtSession.hpp

#pragma once
#include <onnxruntime_c_api.h>
#include <memory>
#include <string>
#include <stdexcept>

namespace gazeshot::game::tracking::ml {

// ── 커스텀 Deleter: C API의 해제 함수를 호출 ──
struct OrtEnvDeleter {
    void operator()(OrtEnv* p) const { if (p) OrtReleaseEnv(p); }
};
struct OrtSessionOptionsDeleter {
    void operator()(OrtSessionOptions* p) const { if (p) OrtReleaseSessionOptions(p); }
};
struct OrtSessionDeleter {
    void operator()(OrtSession* p) const { if (p) OrtReleaseSession(p); }
};
struct OrtMemoryInfoDeleter {
    void operator()(OrtMemoryInfo* p) const { if (p) OrtReleaseMemoryInfo(p); }
};
struct OrtValueDeleter {
    void operator()(OrtValue* p) const { if (p) OrtReleaseValue(p); }
};

// ── unique_ptr 타입 별칭 ──
using OrtEnvPtr            = std::unique_ptr<OrtEnv, OrtEnvDeleter>;
using OrtSessionOptionsPtr = std::unique_ptr<OrtSessionOptions, OrtSessionOptionsDeleter>;
using OrtSessionPtr        = std::unique_ptr<OrtSession, OrtSessionDeleter>;
using OrtMemoryInfoPtr     = std::unique_ptr<OrtMemoryInfo, OrtMemoryInfoDeleter>;
using OrtValuePtr          = std::unique_ptr<OrtValue, OrtValueDeleter>;

inline void checkOrtStatus(const OrtApi* api, OrtStatus* status) {
    if (status != nullptr) {
        const char* msg = api->GetErrorMessage(status);
        std::string errorMsg(msg);
        api->ReleaseStatus(status);
        throw std::runtime_error("ONNX Runtime error: " + errorMsg);
    }
}

} // namespace gazeshot::game::tracking::ml
```

**C++ 학습 포인트: C 라이브러리 RAII 래핑**

```cpp
// 패턴: unique_ptr + 커스텀 Deleter로 C API를 감싸기
struct OrtSessionDeleter {
    void operator()(OrtSession* s) { OrtReleaseSession(s); }
};
using OrtSessionPtr = std::unique_ptr<OrtSession, OrtSessionDeleter>;

// 사용:
OrtSession* raw = nullptr;
api->CreateSession(env, path, opts, &raw);
OrtSessionPtr session(raw);  // RAII 관리 시작
// → 스코프 이탈 시 자동 해제, 예외 발생 시에도 안전

// 동일 패턴 적용 가능: SQLite, SDL, OpenSSL 등 모든 C 라이브러리
// Empty Base Optimization: Deleter가 비어있으면 sizeof 증가 없음
```

### Step 5: 추론 엔진

```hpp
// game/include/gazeshot/game/tracking/ml/GazeInference.hpp

#pragma once
#include <gazeshot/game/tracking/ml/OrtSession.hpp>
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec2.hpp>
#include <span>
#include <vector>
#include <array>
#include <chrono>

namespace gazeshot::game::tracking::ml {

class GazeInference {
public:
    struct Config {
        std::string modelPath = "assets/models/gaze_model.onnx";
        core::u32 inputWidth = 64, inputHeight = 64, inputChannels = 1;
        core::u32 outputSize = 2;
        core::u32 numThreads = 2;
    };

    explicit GazeInference(const Config& config = {}) : config_(config) {}
    GazeInference(GazeInference&&) noexcept = default;
    GazeInference(const GazeInference&) = delete;

    void init() {
        api_ = OrtGetApiBase()->GetApi(ORT_API_VERSION);

        OrtEnv* rawEnv = nullptr;
        checkOrtStatus(api_, api_->CreateEnv(
            ORT_LOGGING_LEVEL_WARNING, "GazeShot", &rawEnv));
        env_.reset(rawEnv);

        OrtSessionOptions* rawOpts = nullptr;
        checkOrtStatus(api_, api_->CreateSessionOptions(&rawOpts));
        sessionOpts_.reset(rawOpts);
        api_->SetIntraOpNumThreads(sessionOpts_.get(), config_.numThreads);
        api_->SetSessionGraphOptimizationLevel(sessionOpts_.get(), ORT_ENABLE_ALL);

        OrtSession* rawSession = nullptr;
        checkOrtStatus(api_, api_->CreateSession(
            env_.get(), config_.modelPath.c_str(),
            sessionOpts_.get(), &rawSession));
        session_.reset(rawSession);

        OrtMemoryInfo* rawMem = nullptr;
        checkOrtStatus(api_, api_->CreateCpuMemoryInfo(
            OrtArenaAllocator, OrtMemTypeDefault, &rawMem));
        memoryInfo_.reset(rawMem);
    }

    // ── std::span으로 입력을 복사 없이 전달 ──
    core::math::Vec2f infer(std::span<const float> eyeImage) {
        auto t0 = std::chrono::steady_clock::now();

        std::array<int64_t, 4> shape = {
            1, static_cast<int64_t>(config_.inputChannels),
            static_cast<int64_t>(config_.inputHeight),
            static_cast<int64_t>(config_.inputWidth),
        };

        OrtValue* rawInput = nullptr;
        checkOrtStatus(api_, api_->CreateTensorWithDataAsOrtValue(
            memoryInfo_.get(), const_cast<float*>(eyeImage.data()),
            eyeImage.size() * sizeof(float),
            shape.data(), shape.size(),
            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &rawInput));
        OrtValuePtr inputTensor(rawInput);

        const char* inNames[]  = {"eye_image"};
        const char* outNames[] = {"gaze_vector"};

        OrtValue* rawOutput = nullptr;
        checkOrtStatus(api_, api_->Run(session_.get(), nullptr,
            inNames, &rawInput, 1, outNames, 1, &rawOutput));
        OrtValuePtr outputTensor(rawOutput);

        float* data = nullptr;
        checkOrtStatus(api_, api_->GetTensorMutableData(
            rawOutput, reinterpret_cast<void**>(&data)));

        lastInferMs_ = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        return {data[0], data[1]};
    }

    // ── span 기반 오버로드 (출력도 span) ──
    void infer(std::span<const float> input, std::span<float> output) {
        auto r = infer(input);
        if (output.size() >= 2) { output[0] = r.x; output[1] = r.y; }
    }

    [[nodiscard]] core::f32 lastInferTimeMs() const { return lastInferMs_; }

private:
    Config config_;
    const OrtApi* api_ = nullptr;
    OrtEnvPtr env_;
    OrtSessionOptionsPtr sessionOpts_;
    OrtSessionPtr session_;
    OrtMemoryInfoPtr memoryInfo_;
    core::f32 lastInferMs_ = 0.0f;
};

} // namespace gazeshot::game::tracking::ml
```

**C++ 학습 포인트: `std::span`으로 텐서 데이터 전달**

```cpp
void infer(std::span<const float> input, std::span<float> output);

// 어떤 컨테이너든 복사 없이 전달:
std::vector<float> pixels(4096);    infer(pixels, result);  // vector
std::array<float, 4096> pixels;     infer(pixels, result);  // array
float pixels[4096];                 infer(pixels, result);  // C 배열

// span은 "안전한 포인터+크기"를 C API에 전달하는 중간 다리 역할
// ONNX Runtime C API가 요구하는 raw 포인터: input.data(), input.size()
```

### Step 6: 게임 루프 통합

```cpp
// game/src/main.cpp (Ch.28 — ML 시선 추적 통합)

#include <gazeshot/game/tracking/ml/GazeInference.hpp>
using namespace gazeshot::game::tracking::ml;

struct App {
    // ... 기존 멤버 (Ch.27) ...
    GazeInference mlInference;
    bool useMLTracker = false;
};

void init(App& app) {
    // ML 추적기 초기화 (모델 파일이 있을 때만)
    try {
        GazeInference::Config cfg;
        cfg.modelPath = "assets/models/gaze_model.onnx";
        app.mlInference = GazeInference(cfg);
        app.mlInference.init();
        app.useMLTracker = true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ML tracker unavailable: %s\n", e.what());
        app.useMLTracker = false;  // 기존 랜드마크 기반으로 fallback
    }
}

void update(App& app, core::f32 dt) {
    core::math::Vec2f gazeDir;
    if (app.useMLTracker) {
        // 눈 이미지 전처리 → CNN 추론
        auto eyePixels = preprocessEye(app.faceTracker.leftEyeRegion());
        gazeDir = app.mlInference.infer(eyePixels);
    } else {
        gazeDir = app.gazeTracker.estimate();  // Ch.25 방식
    }
    app.camera.setGazeDirection(gazeDir);

    // F5: ML/랜드마크 기반 전환
    if (app.input.isKeyPressed(SDLK_F5))
        app.useMLTracker = !app.useMLTracker;
}
```

---

## 3. C++ 학습 포인트 정리

### C 라이브러리 RAII 래핑 — `unique_ptr` + 커스텀 Deleter

```cpp
// 1. Deleter 정의 → 2. using 별칭 → 3. raw 포인터를 감싸기

struct OrtSessionDeleter {
    void operator()(OrtSession* s) const { if (s) OrtReleaseSession(s); }
};
using OrtSessionPtr = std::unique_ptr<OrtSession, OrtSessionDeleter>;

OrtSession* raw = nullptr;
api->CreateSession(env, path, opts, &raw);
OrtSessionPtr session(raw);
// → 스코프 이탈/예외 시 자동 해제. 크기 오버헤드 없음 (EBO).
```

### `std::span`으로 제로카피 텐서 I/O

```cpp
void infer(std::span<const float> input, std::span<float> output);
// vector, array, C 배열 모두 복사 없이 전달 가능
// .data() → raw 포인터, .size() → 개수, .size_bytes() → 바이트
```

### Python-학습 → C++-추론 인터페이스

```cpp
// Python: 모델 설계/학습/내보내기 (.onnx)
// C++: 모델 로딩/전처리/추론/후처리/게임 통합
// 주의: 양쪽 전처리가 정확히 일치해야 함 (정규화, 채널 순서 등)
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| Python 학습 | `train.py` → val_loss 수렴 |
| ONNX 변환 | `gaze_model.onnx` 생성, onnx.checker 통과 |
| C++ 모델 로딩 | "GazeML: model loaded" 콘솔 출력 |
| 추론 실행 | 더미 입력 → Vec2f 정상 반환 |
| 추론 속도 | CPU에서 5ms 이하 |
| RAII 안전성 | 모델 파일 없을 때 → 예외 → fallback, 비정상 종료 없음 |
| 추적기 전환 | F5 키로 ML/랜드마크 기반 전환 |
| 시선 정확도 | 캘리브레이션 점에서 평균 오차 3도 이내 |
| 메모리 누수 | 종료 시 모든 ORT 리소스 해제 (Valgrind) |

---

## 블로그 데모 아이디어

1. **전체 파이프라인 다이어그램**: 데이터 수집 → 학습 → ONNX → C++ 추론 → 게임
2. **CNN 아키텍처 시각화**: 눈 이미지 → 레이어별 특징 맵 → 시선 벡터
3. **성능 비교표**: MediaPipe 기반 vs 직접 학습 모델의 정확도/속도
4. **RAII 래핑 Before/After**: C API 직접 호출 vs unique_ptr 래핑
5. **전이 학습 효과**: 처음부터 학습 vs 사전 학습 fine-tuning 비교

---

## 코스 완성!

28개 챕터로 구성된 **Project GazeShot** 코스를 모두 마쳤다. 빈 프로젝트에서 시작하여 시선 추적 3D 스나이퍼 게임을 완성하기까지의 여정을 돌아보자.

### Milestone 1: 플레이 가능한 프로토타입 (Ch.01 ~ 15)

```
Ch.01  프로젝트 아키텍처     ─ namespace, CMake 듀얼 빌드
Ch.02  벡터와 행렬           ─ class template, operator overloading, constexpr
Ch.03  변환과 투영           ─ template specialization, static_assert
Ch.04  렌더링 추상화         ─ virtual/override, unique_ptr, RAII, move semantics
Ch.05  입력과 게임 루프      ─ std::variant/visit, lambda, std::chrono
Ch.06  프로시저럴 메시       ─ std::vector, std::span, emplace_back
Ch.07  라이팅               ─ std::array, alignas
Ch.08  엔티티와 씬          ─ unique_ptr 소유권, std::optional, Rule of Zero
Ch.09  스나이퍼 카메라       ─ 합성 패턴, std::clamp, std::lerp
Ch.10  사격장 구성           ─ designated initializers, constexpr 배열
Ch.11  레이캐스팅           ─ std::optional<HitResult>, [[nodiscard]]
Ch.12  충돌 감지            ─ C++20 concepts, requires
Ch.13  패럴랙스 엿보기      ─ std::function, std::invoke, 고차 함수
Ch.14  HUD와 스코프         ─ string_view, std::format, std::span
Ch.15  게임 스테이트        ─ variant 상태 머신, overloaded 패턴

→ 결과: 키보드+마우스로 조작하는 완전한 프로토타입
```

### Milestone 2: 비주얼 폴리시 (Ch.16 ~ 22)

```
Ch.16  리소스 매니저         ─ CRTP, type erasure, handle/generation
Ch.17  모델 로딩            ─ std::from_chars, std::ranges 파이프라인
Ch.18  텍스처와 머터리얼     ─ enum class, std::bitset
Ch.19  파티클 시스템         ─ object pool, placement new, std::pmr
Ch.20  포스트 프로세싱       ─ 함수 합성, std::function 체인
Ch.21  환경 렌더링          ─ std::filesystem
Ch.22  사운드 시스템        ─ std::atomic, lock-free 기초

→ 결과: 모델, 텍스처, 이펙트, 사운드가 입혀진 게임
```

### Milestone 3: 시선 추적 통합 (Ch.23 ~ 28)

```
Ch.23  시선 추적 기술 개관   ─ 기술 조사, 라이브러리 선택
Ch.24  웹캠과 얼굴 추적     ─ std::jthread, stop_token, mutex
Ch.25  시선 방향 추적       ─ ring buffer, 신호 처리 필터링
Ch.26  캘리브레이션         ─ 최소자승법, 매핑 함수
Ch.27  통합과 튜닝          ─ [[likely]], 프로파일링, 최적화
Ch.28  직접 ML 모델 만들기  ─ C 라이브러리 RAII 래핑, std::span 텐서

→ 결과: 시선으로 조준하고 깜빡임으로 사격하는 최종 게임
```

### 이 코스에서 익힌 C++ 기법

```
기초:          namespace, class template, constexpr, virtual/override, RAII
소유권/메모리: unique_ptr, shared_ptr, optional, any, CRTP, object pool, pmr
모던 C++20:    span, format, jthread, concepts, ranges, designated initializers
함수형 패턴:   std::function, std::invoke, lambda, variant 상태 머신
시스템:        atomic, mutex, chrono, filesystem, C 라이브러리 RAII 래핑
```

### 다음에 탐구할 주제

- **렌더링**: Vulkan/WebGPU, PBR, Deferred Shading
- **엔진**: ECS, 씬 직렬화, 에디터 UI (ImGui), 네트워크 멀티플레이어
- **C++**: C++23 (std::expected, std::print), 코루틴, 모듈
- **ML/CV**: Attention 기반 시선 모델, 모델 경량화, WebGPU 추론

28개 챕터를 거치며 빈 `main()`에서 시작하여 시선으로 조작하는 3D 게임을 완성했다. 각 챕터에서 "이 기능을 만들려면 이 C++ 기법이 필요하다"는 동기를 가지고 학습했기 때문에, 문법 암기가 아닌 실전 활용 능력을 갖추게 되었을 것이다. 가장 중요한 것은 **직접 만들어본 경험**이다. "프로젝트를 설계하고, 구현하고, 개선하는" 사이클을 반복한 경험은 어떤 새로운 기술을 마주하더라도 적용할 수 있는 기반이 될 것이다.
