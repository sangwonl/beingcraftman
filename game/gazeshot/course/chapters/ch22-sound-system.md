# Chapter 22: 사운드 시스템

## 데모 미리보기

```
┌─────────────────────────────────────────────────────┐
│                  사운드 이벤트 흐름                    │
│                                                     │
│  [Game Thread]              [Audio Thread]           │
│                                                     │
│  클릭! ─→ shouldPlayShot    ──→  ♪ BANG!            │
│           .store(true)          (shot.wav 믹싱)      │
│                                                     │
│  피격! ─→ shouldPlayHit     ──→  ♪ PING!            │
│           .store(true)          (hit.wav 믹싱)       │
│                                                     │
│  빈탄창 → shouldPlayEmpty   ──→  ♪ CLICK            │
│           .store(true)          (empty.wav 믹싱)     │
│                                                     │
│  항상  ─→ ambientPlaying    ──→  ♪ 휘이이이~ (wind)  │
│           == true               (ambient.wav 루프)   │
│                                                     │
│  ┌─ Simple Mixer ────────────────────┐              │
│  │  ch0: shot    ████░░░░░░ vol=0.8  │              │
│  │  ch1: hit     ░░░░░░░░░░ vol=0.0  │              │
│  │  ch2: empty   ░░░░░░░░░░ vol=0.0  │              │
│  │  ch3: ambient ██████████ vol=0.3  │              │
│  │  ────────────────────────────────  │              │
│  │  output: mix & clamp → speakers   │              │
│  └───────────────────────────────────┘              │
└─────────────────────────────────────────────────────┘
```

- **데모**: 사격하면 총성, 피격하면 금속 타격음, 빈 탄창이면 찰칵 소리
- **배경**: 바람 소리가 사격장 분위기를 연출
- **WASM**: 브라우저에서 첫 클릭 후 소리가 활성화됨
- 블로그에 "오디오 스레드와 게임 스레드의 lock-free 통신" 다이어그램 포함 가능

---

## 학습 목표

1. SDL3 오디오 시스템을 초기화하고 WAV 파일을 로드한다
2. 오디오 콜백에서 여러 사운드를 동시에 믹싱하는 간단한 믹서를 구현한다
3. 게임 이벤트(사격, 피격, 빈 탄창)에 사운드를 연결한다
4. 거리 기반 볼륨 감쇄를 구현한다
5. `std::atomic`, lock-free 기초, C 호환 콜백 패턴을 실습한다

---

## 1. 배경 지식

### 디지털 오디오 기초

```
PCM (Pulse Code Modulation):
  - 샘플레이트: 초당 샘플 수 (44100Hz, 48000Hz 등)
  - 비트 깊이: 샘플당 비트 수 (16bit = -32768 ~ +32767)
  - 채널: 모노(1), 스테레오(2)

WAV 파일 구조:
  ┌──────────┬──────────┬──────────────────────┐
  │ RIFF 헤더 │ fmt 청크  │ data 청크 (PCM 샘플) │
  └──────────┴──────────┴──────────────────────┘

1초 분량 = 샘플레이트 × 채널 수 × (비트깊이 / 8)
예: 48000 × 2 × 2 = 192,000 bytes/sec
```

### SDL3 오디오 모델

SDL3는 SDL2와 달리 **오디오 스트림(AudioStream)** 중심으로 API가 재설계되었다.

```
┌────────────┐    put    ┌────────────────┐    bind   ┌──────────────┐
│ WAV 데이터  │ ───────→ │ SDL_AudioStream │ ───────→ │ Audio Device  │
│ (PCM 버퍼)  │          │ (포맷 변환 포함) │          │ (스피커 출력)  │
└────────────┘          └────────────────┘          └──────────────┘
```

- **콜백**: 오디오 디바이스가 데이터를 요청할 때마다 콜백 함수 호출. 저지연, 정밀 제어.
- **스트림(push)**: 게임 측에서 데이터를 밀어넣으면 SDL이 디바이스로 전달. 간편함.

이 챕터에서는 **콜백 방식**을 사용한다. 사운드 믹싱을 직접 제어해야 하기 때문이다.

### 오디오 스레드와 실시간 제약

```
오디오 디바이스는 일정 주기(~5ms)마다 버퍼를 요청한다.
콜백이 제시간에 완료되지 않으면 → 오디오 끊김(underrun)

오디오 콜백 내에서 금지:
  ✗ 메모리 할당 (new, malloc)     ✗ 파일 I/O
  ✗ mutex lock                   ✗ printf, 로깅
허용:
  ✓ 미리 할당된 버퍼 읽기/쓰기     ✓ std::atomic 연산
  ✓ 단순 산술/믹싱
```

이것이 `std::atomic`이 필요한 이유다. 게임 스레드와 오디오 스레드가 mutex 없이 통신해야 한다.

---

## 2. 구현 가이드

### Step 1: SoundClip — WAV 데이터 컨테이너

```hpp
// engine/include/gazeshot/engine/SoundClip.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <vector>

namespace gazeshot::engine {

struct SoundClip {
    std::vector<core::u8> data;       // PCM 샘플 데이터
    core::u32 sampleRate    = 0;
    core::u16 channels      = 0;
    core::u16 bitsPerSample = 0;
    core::u32 lengthBytes   = 0;

    [[nodiscard]] float durationSeconds() const {
        if (sampleRate == 0 || channels == 0 || bitsPerSample == 0) return 0.0f;
        core::u32 bytesPerSec = sampleRate * channels * (bitsPerSample / 8);
        return static_cast<float>(lengthBytes) / static_cast<float>(bytesPerSec);
    }
};

} // namespace gazeshot::engine
```

### Step 2: WAV 로딩 — SDL_LoadWAV 활용

```cpp
// engine/src/AudioLoader.cpp
#include <gazeshot/engine/SoundClip.hpp>
#include <SDL3/SDL.h>
#include <optional>
#include <string_view>
#include <cstdio>

namespace gazeshot::engine {

[[nodiscard]] std::optional<SoundClip> loadWAV(std::string_view path) {
    SDL_AudioSpec spec{};
    Uint8* wavBuffer = nullptr;
    Uint32 wavLength = 0;

    // SDL_LoadWAV는 내부적으로 RIFF/WAV 헤더를 파싱한다
    if (!SDL_LoadWAV(std::string(path).c_str(), &spec, &wavBuffer, &wavLength)) {
        std::printf("[Audio] Failed to load WAV: %s — %s\n",
                    path.data(), SDL_GetError());
        return std::nullopt;
    }

    SoundClip clip;
    clip.sampleRate    = static_cast<core::u32>(spec.freq);
    clip.channels      = static_cast<core::u16>(spec.channels);
    clip.bitsPerSample = static_cast<core::u16>(SDL_AUDIO_BITSIZE(spec.format));
    clip.lengthBytes   = wavLength;
    clip.data.assign(wavBuffer, wavBuffer + wavLength);
    SDL_free(wavBuffer);

    std::printf("[Audio] Loaded: %s (%.1fs, %uHz, %uch)\n",
                path.data(), clip.durationSeconds(),
                clip.sampleRate, clip.channels);
    return clip;
}

} // namespace gazeshot::engine
```

> **WAV 직접 파싱**: SDL_LoadWAV 내부 동작을 이해하고 싶다면 RIFF 헤더를 직접 읽어볼 수 있다. 그러나 SDL이 포맷 변환까지 처리해주므로 프로덕션에서는 SDL_LoadWAV가 실용적이다.

### Step 3: MixerChannel과 AudioSystem

```hpp
// engine/include/gazeshot/engine/AudioSystem.hpp
#pragma once
#include <gazeshot/engine/SoundClip.hpp>
#include <gazeshot/core/Types.hpp>
#include <array>
#include <atomic>

using SDL_AudioDeviceID = unsigned int;

namespace gazeshot::engine {

struct MixerChannel {
    const SoundClip* clip = nullptr;
    std::atomic<bool> trigger{false};  // 게임 스레드 → 오디오 스레드 신호
    core::u32 position = 0;            // 현재 재생 위치 (바이트)
    float volume  = 1.0f;
    bool playing  = false;
    bool loop     = false;
};

class AudioSystem {
public:
    static constexpr core::u32 MAX_CHANNELS = 8;
    static constexpr core::u32 SAMPLE_RATE  = 48000;

    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool init();
    void shutdown();

    void assignClip(core::u32 channel, const SoundClip* clip,
                    float volume = 1.0f, bool loop = false);
    void play(core::u32 channel);
    void stop(core::u32 channel);
    void setVolume(core::u32 channel, float vol);
    [[nodiscard]] bool isPlaying(core::u32 channel) const;

private:
    static void audioCallback(void* userdata, unsigned char* stream, int len);
    void mixAudio(unsigned char* stream, int len);

    std::array<MixerChannel, MAX_CHANNELS> channels_{};
    SDL_AudioDeviceID deviceId_ = 0;
    std::atomic<bool> initialized_{false};
};

} // namespace gazeshot::engine
```

### Step 4: AudioSystem 구현

```cpp
// engine/src/AudioSystem.cpp
#include <gazeshot/engine/AudioSystem.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace gazeshot::engine {

AudioSystem::AudioSystem() = default;
AudioSystem::~AudioSystem() { shutdown(); }

bool AudioSystem::init() {
    if (initialized_.load()) return true;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::printf("[Audio] SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq     = static_cast<int>(SAMPLE_RATE);
    desired.format   = SDL_AUDIO_S16;   // 16bit signed
    desired.channels = 2;               // 스테레오

    // SDL3: SDL_OpenAudioDevice + SDL_SetAudioPostmixCallback
    deviceId_ = SDL_OpenAudioDevice(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (deviceId_ == 0) {
        std::printf("[Audio] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    // postmix 콜백 등록 — 디바이스가 출력 직전에 호출
    SDL_SetAudioPostmixCallback(deviceId_, AudioSystem::audioCallback, this);
    SDL_ResumeAudioDevice(deviceId_);

    initialized_.store(true);
    std::printf("[Audio] Initialized: %uHz, stereo, S16\n", SAMPLE_RATE);
    return true;
}

void AudioSystem::shutdown() {
    if (!initialized_.exchange(false)) return;
    if (deviceId_ != 0) { SDL_CloseAudioDevice(deviceId_); deviceId_ = 0; }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioSystem::assignClip(core::u32 ch, const SoundClip* clip,
                              float volume, bool loop) {
    if (ch >= MAX_CHANNELS) return;
    channels_[ch] = {clip, {false}, 0, volume, false, loop};
}

void AudioSystem::play(core::u32 ch) {
    if (ch >= MAX_CHANNELS) return;
    channels_[ch].trigger.store(true, std::memory_order_release);
}

void AudioSystem::stop(core::u32 ch) {
    if (ch >= MAX_CHANNELS) return;
    channels_[ch].playing = false;
    channels_[ch].position = 0;
}

void AudioSystem::setVolume(core::u32 ch, float vol) {
    if (ch >= MAX_CHANNELS) return;
    channels_[ch].volume = std::clamp(vol, 0.0f, 1.0f);
}

bool AudioSystem::isPlaying(core::u32 ch) const {
    return ch < MAX_CHANNELS && channels_[ch].playing;
}

// C 호환 정적 콜백 — userdata로 this를 전달받아 인스턴스 메서드 호출
void AudioSystem::audioCallback(void* userdata, unsigned char* stream, int len) {
    static_cast<AudioSystem*>(userdata)->mixAudio(stream, len);
}

// ── 믹싱 로직 (오디오 스레드에서 실행) ──
void AudioSystem::mixAudio(unsigned char* stream, int len) {
    std::memset(stream, 0, static_cast<size_t>(len));

    auto* output = reinterpret_cast<core::i16*>(stream);
    core::u32 sampleCount = static_cast<core::u32>(len) / sizeof(core::i16);

    for (auto& ch : channels_) {
        if (!ch.clip) continue;

        // 트리거 확인 (lock-free: exchange로 원자적 읽기+초기화)
        if (ch.trigger.exchange(false, std::memory_order_acquire)) {
            ch.playing  = true;
            ch.position = 0;
        }
        if (!ch.playing) continue;

        auto* src = reinterpret_cast<const core::i16*>(ch.clip->data.data());
        core::u32 srcSamples = ch.clip->lengthBytes / sizeof(core::i16);

        for (core::u32 i = 0; i < sampleCount; ++i) {
            if (ch.position >= srcSamples) {
                if (ch.loop) { ch.position = 0; }
                else { ch.playing = false; break; }
            }

            // 믹싱: 기존 출력에 채널 샘플을 더하고 클램핑
            float mixed = static_cast<float>(output[i])
                        + static_cast<float>(src[ch.position]) * ch.volume;
            output[i] = static_cast<core::i16>(
                std::clamp(mixed, -32768.0f, 32767.0f));
            ++ch.position;
        }
    }
}

} // namespace gazeshot::engine
```

### Step 5: 게임 사운드 이벤트 연결

```cpp
// game/src/GameAudio.cpp
#include <gazeshot/engine/AudioSystem.hpp>

namespace gazeshot::game {

enum SoundChannel : core::u32 {
    CH_SHOT = 0, CH_HIT = 1, CH_EMPTY = 2, CH_AMBIENT = 3,
};

struct GameAudio {
    engine::AudioSystem system;
    engine::SoundClip shotClip, hitClip, emptyClip, ambientClip;

    bool init() {
        if (!system.init()) return false;

        if (auto c = engine::loadWAV("assets/sounds/shot.wav"))     shotClip    = std::move(*c);
        if (auto c = engine::loadWAV("assets/sounds/hit.wav"))      hitClip     = std::move(*c);
        if (auto c = engine::loadWAV("assets/sounds/empty.wav"))    emptyClip   = std::move(*c);
        if (auto c = engine::loadWAV("assets/sounds/wind_loop.wav"))ambientClip = std::move(*c);

        system.assignClip(CH_SHOT,    &shotClip,    0.8f, false);
        system.assignClip(CH_HIT,     &hitClip,     0.7f, false);
        system.assignClip(CH_EMPTY,   &emptyClip,   0.6f, false);
        system.assignClip(CH_AMBIENT, &ambientClip,  0.3f, true);

        system.play(CH_AMBIENT);   // 배경음 즉시 재생
        return true;
    }
};

} // namespace gazeshot::game
```

### Step 6: 사격 시스템과 연동

Ch.11 사격 로직, Ch.12 충돌 검사, Ch.15 게임 상태에서 사운드를 트리거한다.

```cpp
// game/src/ShootingSystem.cpp — 사격 처리에 사운드 추가
void shoot(App& app) {
    auto& audio = app.gameAudio;
    auto& state = std::get<PlayingState>(app.gameState);

    if (state.ammo <= 0) {
        audio.system.play(CH_EMPTY);   // 찰칵!
        return;
    }
    --state.ammo;
    audio.system.play(CH_SHOT);        // 뻥!

    auto [origin, direction] = app.camera.aimRay();
    Ray ray{origin, direction};

    if (auto hit = sceneRaycast(ray, app.scene)) {
        auto* entity = app.scene.findEntityById(hit->entityId);
        if (entity && entity->name().starts_with("target_")) {
            // 거리 기반 볼륨 감쇄
            float atten = 1.0f / (1.0f + 0.01f * hit->distance * hit->distance);
            audio.system.setVolume(CH_HIT, 0.7f * atten);
            audio.system.play(CH_HIT);             // 핑!

            entity->material().diffuse = {1, 1, 1};
            state.score += calculateScore(*hit);
        }
        app.tracer = {origin, hit->point, 0.3f};
    } else {
        app.tracer = {origin, origin + direction * 100.0f, 0.3f};
    }
}
```

### Step 7: 거리 기반 볼륨 감쇄 유틸리티

```hpp
// engine/include/gazeshot/engine/AudioUtil.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <algorithm>

namespace gazeshot::engine {

// 역제곱 감쇄 — 현실적인 소리 감쇄 근사
[[nodiscard]] inline float distanceAttenuation(
    float distance, float refDist = 1.0f, float maxDist = 100.0f) {
    if (distance <= refDist) return 1.0f;
    if (distance >= maxDist) return 0.0f;
    float a = refDist / distance;
    return std::clamp(a * a, 0.0f, 1.0f);
}

// 선형 감쇄 — 단순 버전
[[nodiscard]] inline float linearAttenuation(
    float distance, float maxDist = 80.0f) {
    return std::clamp(1.0f - distance / maxDist, 0.0f, 1.0f);
}

} // namespace gazeshot::engine
```

### Step 8: WASM 호환 처리

브라우저는 사용자 인터랙션 없이 오디오를 자동 재생하지 않는다.

```cpp
// game/src/main.cpp — WASM 오디오 정책 대응
void handleFirstInteraction(App& app) {
    if (app.audioActivated) return;

    // SDL3는 내부적으로 AudioContext.resume()를 호출한다.
    // 배경음은 첫 인터랙션 이후에 시작하는 것이 안전하다.
    app.gameAudio.system.play(CH_AMBIENT);
    app.audioActivated = true;
    std::printf("[Audio] Activated after user interaction\n");
}

void oneFrame(void* arg) {
    auto* app = static_cast<App*>(arg);
    app->window.pollEvents();

    if (!app->audioActivated && app->input.anyInteraction()) {
        handleFirstInteraction(*app);
    }
    // ... update, render ...
}
```

> **SDL3와 Web Audio API**: Emscripten 빌드에서 SDL3는 `SDL_OpenAudioDevice`를 Web Audio API의 `AudioContext` + `AudioWorklet`로 매핑한다. 브라우저 자동 재생 정책에 의해 `AudioContext`는 `suspended` 상태로 시작하며, 사용자 제스처 이벤트 내에서 `resume()`이 호출되어야 활성화된다. SDL3가 이를 자동 처리하지만, 배경음 시작은 첫 인터랙션 이후로 지연시키는 것이 안전하다.

---

## 3. C++ 학습 포인트

### `std::atomic` — 스레드 간 안전한 상태 공유

```cpp
std::atomic<bool> shouldPlayShot{false};

// 게임 스레드 (producer)
void onShoot() {
    shouldPlayShot.store(true, std::memory_order_release);
}

// 오디오 콜백 (consumer, 다른 스레드)
void audioCallback() {
    if (shouldPlayShot.exchange(false, std::memory_order_acquire)) {
        startPlayback();
    }
}
```

**왜 `exchange`인가?**
- `load` + `store`를 따로 하면 두 호출 사이에 다른 스레드가 끼어들 수 있다
- `exchange`는 "읽고 동시에 쓰기"를 하나의 원자적 연산으로 수행한다
- 오디오 콜백이 트리거를 읽는 동시에 `false`로 리셋하므로 중복 재생을 방지한다

**memory_order 요약**:
```
relaxed:  순서 보장 없음. 카운터 등 단독 변수에 적합.
acquire:  이 load 이후의 읽기 재배치 금지. (consumer 측)
release:  이 store 이전의 쓰기 재배치 금지. (producer 측)
seq_cst:  전체 순서 보장 (기본값). 가장 안전.
```

대부분 기본값(`seq_cst`)으로 충분하다. 최적화가 필요할 때만 `acquire`/`release`를 쓴다.

### 콜백 패턴 — C 호환 함수 포인터

```cpp
// ✗ std::function은 C 콜백으로 사용 불가
std::function<void(void*, Uint8*, int)> callback;

// ✓ static 멤버 함수 + userdata 패턴
class AudioSystem {
    static void audioCallback(void* userdata, Uint8* stream, int len) {
        auto* self = static_cast<AudioSystem*>(userdata);
        self->mixAudio(stream, len);  // userdata에서 this 복원
    }
};

// 등록 시
SDL_SetAudioPostmixCallback(deviceId, AudioSystem::audioCallback, this);
//                                                                 ^^^^
//                                                           이것이 userdata
```

**패턴**: (1) `static` 멤버 함수 선언 → (2) `userdata`에 `this` 전달 → (3) 콜백 내 `static_cast`로 복원.
SDL, GLFW, OpenAL, libcurl 등 거의 모든 C 라이브러리 콜백에서 동일하게 쓰인다.

### lock-free 기초 — 왜 오디오에서 mutex를 피하는가

```
mutex 문제 시나리오:
  1. 게임 스레드가 mutex를 잡고 사운드 데이터 준비 중
  2. 오디오 콜백 호출됨 → mutex 획득 대기(block)
  3. 게임 스레드 작업 끝날 때까지 오디오 콜백 정지
  4. 오디오 버퍼가 비어서 → 끊김 (pop/click 잡음)

lock-free 해결:
  1. 게임 스레드: atomic<bool>에 true 저장 (즉시 완료)
  2. 오디오 콜백: atomic<bool>을 exchange로 읽기 (즉시 완료)
  3. 대기 시간 = 0 → 끊김 없음
```

**원칙**: 오디오 스레드에서는 "기다리는" 연산을 하지 않는다. 데이터 교환은 미리 할당된 버퍼 + atomic 플래그로 수행한다.

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 오디오 초기화 | 콘솔에 "Initialized: 48000Hz, stereo, S16" 출력 |
| 사격음 | 마우스 클릭 시 총성 재생 |
| 피격음 | 타겟 명중 시 금속 타격음 재생 |
| 빈 탄창 | 탄약 0일 때 클릭 → 찰칵 소리 |
| 배경음 | 바람 소리가 루프로 계속 재생 |
| 동시 재생 | 배경음 + 총성이 겹쳐서 들림 |
| 볼륨 감쇄 | 원거리 타겟 피격 시 피격음이 작게 들림 |
| 클리핑 없음 | 여러 소리 겹칠 때 찢어지는 소리 없음 |
| WASM | 브라우저에서 첫 클릭 후 소리 재생 |
| 종료 | 프로그램 종료 시 오디오 디바이스 정상 해제 |

---

## 5. 블로그 데모 아이디어

1. **오디오 파이프라인 다이어그램**: Game Thread → atomic → Audio Thread → Mixer → Speaker
2. **파형 시각화**: 사운드 이벤트의 PCM 파형을 캔버스에 실시간 표시
3. **거리 감쇄 그래프**: 거리 vs 볼륨 곡선 (역제곱 vs 선형)
4. **WASM 데모**: 브라우저에서 직접 플레이 가능한 사격 데모 + 사운드
5. **lock-free vs mutex 비교**: 오디오 끊김 시나리오 시뮬레이션

---

## Milestone 2 완성!

Chapter 16~22를 마치면 Milestone 2 (비주얼 폴리시)가 완성된다.

| 챕터 | 구성요소 | C++ 학습 |
|------|---------|----------|
| Ch.16 리소스 매니저 | 에셋 캐싱, 핸들 시스템 | type erasure, CRTP, handle/generation |
| Ch.17 모델 로딩 | OBJ 파서, 메시 로딩 | std::from_chars, ranges 파이프라인 |
| Ch.18 텍스처/머터리얼 | 노말 맵, 멀티텍스처 | enum class, std::bitset |
| Ch.19 파티클 시스템 | 총구 화염, 착탄 먼지 | object pool, placement new, std::pmr |
| Ch.20 포스트 프로세싱 | 비네트, 화면 흔들림 | std::function 체인, 렌더 패스 |
| Ch.21 환경 렌더링 | 스카이박스, 지형 | std::filesystem |
| **Ch.22 사운드 시스템** | **사격음, 피격음, 믹서** | **std::atomic, lock-free, 콜백 패턴** |

**이 시점에서의 게임 상태**:

```
Milestone 1 (Ch.01~15): 핵심 메카닉 완성
  → 프로시저럴 도형으로 "사격하고 맞추는" 게임플레이

Milestone 2 (Ch.16~22): 비주얼 + 오디오 폴리시
  → 모델, 텍스처, 파티클, 포스트프로세싱, 환경, 사운드
  → "보기에도 듣기에도 좋은" 완성도 있는 게임 경험
```

Milestone 1에서 기본 도형으로 프로토타이핑한 사격장이, Milestone 2를 거치며 디테일 있는 3D 모델, 노말맵, 파티클 이펙트, 스카이박스, 그리고 사격감 있는 사운드까지 갖춘 완성형 게임으로 변모했다.

---

## 다음: Milestone 3 예고

**Chapter 23: 시선 추적 기술 개관**

키보드/마우스 입력을 얼굴/시선 추적으로 대체하는 여정이 시작된다.
MediaPipe, OpenFace 등 시선 추적 기술을 조사하고, 우리 게임에 필요한 두 가지 데이터를 정의한다:
- **얼굴 위치/방향** → 가늠자 (headOffset)
- **시선 방향** → 가늠쇠 (gazeDirection)

데모: 웹캠에서 얼굴을 감지하고, 머리를 움직이면 카메라가 따라가는 첫 번째 프로토타입.
