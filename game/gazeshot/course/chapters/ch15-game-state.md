# Chapter 15: 게임 스테이트와 점수 시스템

## 데모 미리보기

```
┌───────────────────────────────────────────────────┐
│                  [Ready Screen]                    │
│            ╔════════════════════════╗              │
│            ║   PROJECT GAZESHOT     ║              │
│            ║   Press SPACE to Start ║              │
│            ╚════════════════════════╝              │
│  SPACE ──→                                        │
├───────────────────────────────────────────────────┤
│                  [Playing Screen]                  │
│   ◎  ◎  ◎     ┃ ◎  ◎ ┃ ◎     ┃ ◎ ┃ ◎ ┃◎       │
│              ╭────────────╮                        │
│              │     ＋      │  Score: 0350          │
│              ╰────────────╯  Time: 42.3s          │
│                              Streak: x3  Hits: 5/9│
│  모든 타겟 클리어 or 타이머 종료 ──→                 │
├───────────────────────────────────────────────────┤
│                  [Result Screen]                   │
│            ╔════════════════════════╗              │
│            ║   Score:  1,250 pts    ║              │
│            ║   Hits: 7/9  Time: 38s ║              │
│            ║   Best Streak: x4     ║              │
│            ║   Press R to Retry    ║              │
│            ╚════════════════════════╝              │
│  R ──→ [Ready]                                    │
└───────────────────────────────────────────────────┘
```

- **데모**: Ready → Playing → Result 세 단계 게임 흐름 완성
- **점수**: 거리 + 정확도 + 시간 + 연속 명중 복합 보너스
- 블로그에 "상태 머신 다이어그램"과 "점수 산출 공식" 도해 포함 가능

---

## 학습 목표

1. `std::variant` 기반 유한 상태 머신으로 게임 흐름을 관리한다
2. Ch.05의 `overloaded` 패턴을 상태별 update/render 분기에 적용한다
3. 거리, 정확도, 시간, 연속 명중을 복합 반영하는 점수 시스템을 설계한다
4. `std::chrono`로 게임 타이머와 경과 시간을 정밀하게 측정한다

---

## 1. 배경 지식

### 게임 상태 머신

```
[Ready] ──── SPACE 키 ────→ [Playing]
                               │
                    ┌──────────┴──────────┐
                    │                     │
              모든 타겟 클리어        타이머 종료
                    └──────────┬──────────┘
                               ▼
                           [Result] ── R 키 ──→ [Ready]
```

전통적인 `enum + switch`는 상태별 데이터가 불분명하다:
```cpp
enum State { READY, PLAYING, RESULT };
float timer;       // Playing에서만 유효
int finalScore;    // Result에서만 유효
```

모던 C++에서는 `std::variant`가 이 문제를 **타입 안전하게** 해결한다.

### 점수 설계 원칙

```
총점 = 기본 점수(거리) + 정확도 보너스 + 시간 보너스 + 스트릭 보너스

1. 거리 기반:  Near 50pts / Mid 100pts / Far 200pts
2. 정확도:    basePoints × (1 - hitDist/radius) × 0.5
3. 시간:      남은시간 × 10pts (전부 클리어 시에만)
4. 스트릭:    streak 1: ×1.0 / 2: ×1.2 / 3: ×1.5 / 4+: ×2.0
```

---

## 2. 구현 가이드

### Step 1: 게임 상태 타입 정의

```hpp
// game/include/gazeshot/game/GameState.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <variant>
#include <chrono>

namespace gazeshot::game {

struct ReadyState {
    core::f32 blinkTimer = 0.0f;  // "Press SPACE" 깜빡임용
};

struct PlayingState {
    core::f32 timer = 60.0f;
    core::i32 score = 0;
    core::i32 hits = 0;
    core::i32 streak = 0;
    core::i32 bestStreak = 0;
    core::i32 shotsFired = 0;

    using Clock = std::chrono::steady_clock;
    Clock::time_point startTime;
};

struct ResultState {
    core::i32 finalScore = 0;
    core::f32 totalTime = 0.0f;
    core::i32 totalHits = 0;
    core::i32 totalShots = 0;
    core::i32 bestStreak = 0;
    core::f32 displayTimer = 0.0f;  // 등장 애니메이션용
};

using GameState = std::variant<ReadyState, PlayingState, ResultState>;

} // namespace gazeshot::game
```

각 상태가 **자신만의 데이터**를 갖는다. `PlayingState`의 `timer`는 Ready나 Result에서는 존재하지 않는다.

### Step 2: 점수 계산

```hpp
// game/include/gazeshot/game/ScoreSystem.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/Ray.hpp>
#include <gazeshot/game/LevelData.hpp>
#include <algorithm>

namespace gazeshot::game {

struct ScoreBreakdown {
    core::i32 basePoints = 0;
    core::i32 accuracyBonus = 0;
    core::f32 streakMultiplier = 1.0f;
    core::i32 total = 0;
};

[[nodiscard]] constexpr core::f32 streakMultiplier(core::i32 streak) {
    if (streak <= 1) return 1.0f;
    if (streak == 2) return 1.2f;
    if (streak == 3) return 1.5f;
    return 2.0f;
}

[[nodiscard]] inline ScoreBreakdown calculateHitScore(
    const TargetDef& target, const core::HitResult& hit, core::i32 currentStreak
) {
    ScoreBreakdown result;
    result.basePoints = static_cast<core::i32>(target.basePoints);

    core::f32 hitDistance = core::math::length(hit.point - target.position);
    core::f32 accuracy = std::clamp(1.0f - (hitDistance / target.radius), 0.0f, 1.0f);
    result.accuracyBonus = static_cast<core::i32>(
        static_cast<core::f32>(result.basePoints) * accuracy * 0.5f
    );

    result.streakMultiplier = streakMultiplier(currentStreak);
    result.total = static_cast<core::i32>(
        static_cast<core::f32>(result.basePoints + result.accuracyBonus)
        * result.streakMultiplier
    );
    return result;
}

[[nodiscard]] constexpr core::i32 calculateTimeBonus(core::f32 remainingTime) {
    return (remainingTime <= 0.0f) ? 0 : static_cast<core::i32>(remainingTime * 10.0f);
}

} // namespace gazeshot::game
```

### Step 3: 상태 전이 로직

```cpp
// game/include/gazeshot/game/GameFlow.hpp
#pragma once
#include <gazeshot/game/GameState.hpp>
#include <gazeshot/game/ScoreSystem.hpp>
#include <gazeshot/game/LevelData.hpp>
#include <gazeshot/engine/Event.hpp>
#include <array>
#include <cstdio>

namespace gazeshot::game {

using engine::overloaded;  // Ch.05에서 정의한 헬퍼 재사용

class GameFlow {
public:
    GameFlow() : state_(ReadyState{}) {}

    void resetTargets() { targetHit_.fill(false); }
    bool isTargetHit(core::u32 i) const { return i < targetHit_.size() && targetHit_[i]; }
    void markTargetHit(core::u32 i) { if (i < targetHit_.size()) targetHit_[i] = true; }

    bool allTargetsCleared() const {
        for (auto h : targetHit_) if (!h) return false;
        return true;
    }

    const GameState& state() const { return state_; }
    GameState& state() { return state_; }

    // Ready → Playing
    void startGame() {
        resetTargets();
        PlayingState playing{};
        playing.startTime = PlayingState::Clock::now();
        state_ = playing;
    }

    // Playing → Result
    void endGame(const PlayingState& playing, bool allCleared) {
        using Duration = std::chrono::duration<core::f32>;
        core::f32 elapsed = Duration(PlayingState::Clock::now() - playing.startTime).count();

        ResultState result;
        result.finalScore = playing.score;
        result.totalTime = elapsed;
        result.totalHits = playing.hits;
        result.totalShots = playing.shotsFired;
        result.bestStreak = playing.bestStreak;

        if (allCleared) {
            result.finalScore += calculateTimeBonus(playing.timer);
        }
        state_ = result;
    }

    // Result → Ready
    void restartGame() { state_ = ReadyState{}; }

private:
    GameState state_;
    std::array<bool, TARGETS.size()> targetHit_{};
};

} // namespace gazeshot::game
```

### Step 4: 상태별 업데이트

```cpp
// game/src/GameUpdate.cpp
#include <gazeshot/game/GameFlow.hpp>
#include <gazeshot/engine/Input.hpp>

namespace gazeshot::game {

void handleShot(GameFlow& flow, PlayingState& playing,
                const std::optional<core::HitResult>& hit, core::u32 targetIndex) {
    playing.shotsFired++;

    if (hit && targetIndex < TARGETS.size() && !flow.isTargetHit(targetIndex)) {
        playing.streak++;
        playing.hits++;
        playing.bestStreak = std::max(playing.bestStreak, playing.streak);

        auto breakdown = calculateHitScore(TARGETS[targetIndex], *hit, playing.streak);
        playing.score += breakdown.total;
        flow.markTargetHit(targetIndex);

        std::printf("[Score] target_%u: base=%d acc=+%d streak=x%.1f => +%d (total: %d)\n",
            targetIndex, breakdown.basePoints, breakdown.accuracyBonus,
            breakdown.streakMultiplier, breakdown.total, playing.score);
    } else {
        playing.streak = 0;  // 빗나감 — 스트릭 리셋
    }
}

void updateGameState(GameFlow& flow, engine::Input& input, core::f32 dt) {
    std::visit(overloaded{
        [&](ReadyState& ready) {
            ready.blinkTimer += dt;
            if (ready.blinkTimer > 1.0f) ready.blinkTimer = 0.0f;
            if (input.isKeyPressed(SDLK_SPACE)) flow.startGame();
        },
        [&](PlayingState& playing) {
            playing.timer -= dt;
            if (playing.timer <= 0.0f) {
                playing.timer = 0.0f;
                flow.endGame(playing, false);
                return;
            }
            if (flow.allTargetsCleared()) {
                flow.endGame(playing, true);
                return;
            }
            // 사격은 마우스 클릭 이벤트 + 레이캐스트 결과와 함께 handleShot()에서 처리
        },
        [&](ResultState& result) {
            result.displayTimer += dt;
            if (input.isKeyPressed(SDLK_R)) flow.restartGame();
        }
    }, flow.state());
}

} // namespace gazeshot::game
```

### Step 5: 상태별 렌더링

```cpp
// game/src/GameRender.cpp
#include <gazeshot/game/GameFlow.hpp>
#include <gazeshot/engine/HUD.hpp>
#include <string>

namespace gazeshot::game {

void renderGameState(const GameFlow& flow, engine::HUD& hud) {
    std::visit(overloaded{
        [&](const ReadyState& s) {
            hud.drawCenteredText(0.3f, "PROJECT GAZESHOT", 2.0f);
            hud.drawCenteredText(0.45f, "Sniper Shooting Range", 1.0f);
            if (s.blinkTimer < 0.7f) {
                hud.drawCenteredText(0.65f, "Press SPACE to Start", 1.2f);
            }
        },
        [&](const PlayingState& s) {
            hud.drawText({0.02f, 0.05f},
                std::string("Score: ") + std::to_string(s.score), 1.2f);

            core::i32 sec = static_cast<core::i32>(s.timer);
            core::i32 tenth = static_cast<core::i32>((s.timer - sec) * 10.0f);
            hud.drawText({0.78f, 0.05f},
                std::string("Time: ") + std::to_string(sec) + "." + std::to_string(tenth), 1.2f);

            hud.drawText({0.02f, 0.92f},
                std::string("Hits: ") + std::to_string(s.hits) + " / 9", 1.0f);

            if (s.streak >= 2) {
                hud.drawText({0.78f, 0.92f},
                    std::string("Streak: x") + std::to_string(s.streak), 1.0f);
            }
        },
        [&](const ResultState& s) {
            hud.drawCenteredText(0.15f, "RESULT", 2.5f);
            hud.drawCenteredText(0.35f,
                std::string("Score: ") + std::to_string(s.finalScore) + " pts", 1.5f);
            hud.drawCenteredText(0.45f,
                std::string("Hits: ") + std::to_string(s.totalHits) + " / 9", 1.0f);

            core::i32 acc = (s.totalShots > 0)
                ? static_cast<core::i32>(static_cast<core::f32>(s.totalHits) /
                  static_cast<core::f32>(s.totalShots) * 100.0f) : 0;
            hud.drawCenteredText(0.53f,
                std::string("Accuracy: ") + std::to_string(acc) + "%", 1.0f);
            hud.drawCenteredText(0.61f,
                std::string("Best Streak: x") + std::to_string(s.bestStreak), 1.0f);

            if (s.displayTimer > 1.0f) {
                hud.drawCenteredText(0.80f, "Press R to Retry", 1.2f);
            }
        }
    }, flow.state());
}

} // namespace gazeshot::game
```

### Step 6: 메인 루프 통합

```cpp
// game/src/main.cpp  (Ch.15)
using namespace gazeshot;
using namespace gazeshot::game;

void update(App& app, core::f32 dt) {
    updateGameState(app.gameFlow, app.input, dt);

    // Playing 상태에서만 카메라/사격 처리
    if (auto* playing = std::get_if<PlayingState>(&app.gameFlow.state())) {
        app.camera.update(app.input, dt);

        if (app.input.isMouseButtonPressed(engine::MouseButton::Left)) {
            auto [origin, direction] = app.camera.aimRay();
            core::Ray ray{origin, direction};
            auto hit = sceneRaycast(ray, app.scene);

            core::u32 targetIndex = 0xFFFFFFFF;
            if (hit) {
                auto* entity = app.scene.findEntityById(hit->entityId);
                if (entity && entity->name().starts_with("target_")) {
                    targetIndex = static_cast<core::u32>(std::stoi(entity->name().substr(7)));
                }
            }
            handleShot(app.gameFlow, *playing, hit, targetIndex);
        }
    }
}

void render(App& app, core::f32 alpha) {
    app.renderer->clear({0.05f, 0.05f, 0.08f, 1.0f});

    if (std::holds_alternative<PlayingState>(app.gameFlow.state())) {
        auto view = app.camera.viewMatrix();
        auto proj = app.camera.projectionMatrix(
            static_cast<core::f32>(app.window.width()) /
            static_cast<core::f32>(app.window.height()));
        app.scene.render(*app.renderer, view, proj);
        app.hud.renderScope(app.camera.scopeZoom());
    }

    renderGameState(app.gameFlow, app.hud);
    app.window.swapBuffers();
}
```

### Step 7: 사운드 피드백 (선택 사항)

본격적인 사운드 시스템은 Ch.22에서 구현한다. 여기서는 SDL3 Audio API로 최소한의 효과음만 추가할 수 있다:

```cpp
// 간이 구현 — SDL_OpenAudioDevice + SDL_LoadWAV + SDL_CreateAudioStream
// RAII로 감싸고 play() 메서드만 외부에 노출
// handleShot() 내부에서 hit 여부에 따라 hitSound.play() / shotSound.play() 호출
```

---

## 3. C++ 학습 포인트

### `std::variant` 기반 상태 머신

Ch.05에서 이벤트 타입으로 첫 등장한 `std::variant`를 게임 상태에 적용한다:

```cpp
// Ch.05 (이벤트): "어떤 일이 일어났는가?"
using Event = std::variant<KeyEvent, MouseMoveEvent, MouseButtonEvent>;

// Ch.15 (상태): "지금 게임이 어떤 단계인가?"
using GameState = std::variant<ReadyState, PlayingState, ResultState>;
```

| 특성 | enum + switch | std::variant |
|------|-------------|-------------|
| 상태별 데이터 | 전역 변수로 흩어짐 | 상태 구조체에 캡슐화 |
| 새 상태 추가 | 런타임 버그 가능 | 컴파일 에러로 누락 방지 |
| 타입 안전성 | 낮음 | 높음 |

### `std::visit` + `overloaded`의 두 가지 용법

```cpp
// update — 상태를 변경 (mutable reference)
std::visit(overloaded{
    [&](ReadyState& s)   { s.blinkTimer += dt; },
    [&](PlayingState& s) { s.timer -= dt; },
    [&](ResultState& s)  { s.displayTimer += dt; }
}, state);

// render — 상태를 읽기만 (const reference)
std::visit(overloaded{
    [&](const ReadyState& s)   { renderReadyScreen(s); },
    [&](const PlayingState& s) { renderPlayingHUD(s); },
    [&](const ResultState& s)  { renderResultScreen(s); }
}, state);
```

새 상태를 추가하면 모든 `std::visit` 호출에서 처리하지 않으면 **컴파일 에러**가 발생한다.

### `std::holds_alternative`와 `std::get_if`

```cpp
// 특정 상태인지 확인
if (std::holds_alternative<PlayingState>(state)) { renderScene(); }

// 안전한 데이터 접근 (nullptr 반환 가능)
if (auto* playing = std::get_if<PlayingState>(&state)) {
    std::printf("Timer: %.1f\n", playing->timer);
}
```

### `std::chrono`: 정밀 타이머

```cpp
using Clock = std::chrono::steady_clock;
auto startTime = Clock::now();
core::f32 elapsed = std::chrono::duration<core::f32>(Clock::now() - startTime).count();
```

`steady_clock`: 단조 증가 보장 → NTP 동기화에 영향받지 않아 게임 타이머에 적합. Ch.05의 GameClock과 동일한 clock이다.

---

## 4. 게임 밸런스와 피드백 루프

```
정확한 사격 → 스트릭 증가 → 배율 상승 → 높은 점수
    ↑                                        │
    └──── "더 잘 하고 싶다" ← 성취감 ←────────┘
```

- **스트릭 보너스**: 연속 명중의 긴장감 유지
- **거리 보너스**: 어려운 타겟 도전 유도
- **시간 보너스**: 전체 클리어 동기 부여
- **정확도 보너스**: "대충 맞추기"보다 "정확하게 맞추기" 장려

```cpp
// 밸런스 상수를 한 곳에 모으면 튜닝이 편하다
namespace balance {
    constexpr core::f32 GAME_DURATION    = 60.0f;
    constexpr core::f32 ACCURACY_WEIGHT  = 0.5f;
    constexpr core::f32 TIME_BONUS_RATE  = 10.0f;
    constexpr std::array<core::f32, 5> STREAK_MULTIPLIERS = {
        1.0f, 1.0f, 1.2f, 1.5f, 2.0f
    };
}
```

---

## 5. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| Ready → Playing | SPACE 키 → 타이머 시작, 사격 가능 |
| Playing → Result (타이머) | 60초 경과 → 자동 결과 화면 |
| Playing → Result (클리어) | 9개 전부 명중 → 시간 보너스 포함 결과 |
| Result → Ready | R 키 → 초기 상태 |
| 점수 누적 | 명중 시 콘솔에 점수 내역 출력 |
| 스트릭 배율 | 연속 명중 시 증가, 빗나가면 리셋 |
| HUD 타이머/점수 | 실시간 갱신 |
| 상태별 입력 | Ready에서 사격 불가, Playing에서 R 무시 |

---

## 블로그 데모 아이디어

1. **상태 전이 다이어그램**: Ready → Playing → Result 플로우차트
2. **점수 분해**: "base 200 + accuracy +80 × streak ×1.5 = 420"
3. **코드 비교**: `enum + switch` vs `std::variant + visit` 상태 머신
4. **WASM 임베딩**: 블로그 독자가 직접 플레이 가능한 브라우저 데모

---

## Milestone 1 완성!

Chapter 01~15를 마치면 **완전한 플레이 가능한 프로토타입**이 완성된다.

| Phase | 챕터 | 구성요소 |
|-------|------|---------|
| **A** | Ch.01~05 | 프로젝트 구조, 수학, 렌더러, 입력/게임루프 |
| **B** | Ch.06~10 | 메시, 라이팅, 엔티티/씬, 카메라, 사격장 |
| **C** | Ch.11~15 | 레이캐스팅, 충돌, 패럴랙스, HUD, **게임 스테이트** |

이 시점에서 동작하는 것:

1. SPACE로 게임 시작
2. WASD로 머리(가늠자) 이동 — 패럴랙스로 기둥 뒤 타겟 발견
3. 마우스로 레티클(가늠쇠) 조준 → 클릭으로 사격
4. 타겟 명중 시 점수 획득 — 스트릭 보너스
5. 60초 내 전체 클리어 시 시간 보너스
6. 결과 화면에서 최종 점수, 명중률, 최고 스트릭 확인
7. R 키로 재도전

**Desktop과 브라우저(WASM) 양쪽에서 동작한다.**

---

## 다음: Milestone 2 예고

**Chapter 16: 리소스 매니저**

프로시저럴 메시로 검증한 프로토타입에 실제 에셋을 입힌다. 텍스처, 셰이더, 메시를 중앙에서 관리하는 `ResourceManager`를 설계한다. 핸들(Handle) 기반 참조로 댕글링 포인터를 방지한다.

C++ 학습: type erasure, `std::any`, CRTP, handle/generation 패턴.
