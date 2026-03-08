# Chapter 05: 윈도우, 입력, 게임 루프

## 데모 미리보기

```
┌─────────────────────────────────────┐
│                                     │
│        ◆────◆  ← 마우스 드래그로     │
│       ╱    ╱│    큐브 회전 제어       │
│      ◆────◆ │                       │
│      │    │╱   Space: 색상 변경      │
│      ◆────◆    R: 회전/위치 리셋     │
│                ↑↓←→: 큐브 이동       │
│  FPS: 60  |  Update: 60Hz fixed     │
│  Mouse: (640, 360)                  │
└─────────────────────────────────────┘
```

- **데모**: 마우스 드래그로 큐브 자유 회전, 방향키로 이동, Space로 색상 변경
- **콘솔 출력**: FPS, 마우스 좌표 실시간 표시
- **핵심**: 입력 처리와 물리 업데이트 분리로 키 입력 누락 방지
- 블로그에 인터랙티브 데모 GIF, 고정 시간 스텝 다이어그램 포함 가능

---

## 학습 목표

1. SDL3 이벤트를 `std::variant`로 래핑한 이벤트 시스템을 구현한다
2. 키보드/마우스 상태를 폴링 + 이벤트 이중 방식으로 관리한다
3. 고정 시간 스텝(fixed timestep) 게임 루프를 구현한다
4. `std::variant`, `std::visit`, lambda, `std::function`, `std::chrono`를 실습한다
5. Emscripten 호환 게임 루프 구조를 확립한다

---

## 1. 배경 지식

### 왜 고정 시간 스텝인가?

```
가변 스텝 (나쁜):
60fps: update(0.016)  → 물체가 1.6 이동
30fps: update(0.033)  → 물체가 3.3 이동  ← 다른 결과!

고정 스텝 (좋은):
어떤 fps든: update(0.02) × N번 → 동일한 물리 결과
```

고정 시간 스텝이 필요한 이유:
- **물리 안정성**: 충돌 감지가 프레임률에 독립적
- **재현성**: 같은 입력 → 같은 결과
- **네트워크**: 리플레이, 동기화 가능

### 게임 루프 구조

```
accumulator = 0
previousTime = now()

매 프레임:
    currentTime = now()
    frameTime = currentTime - previousTime
    previousTime = currentTime
    accumulator += frameTime

    while (accumulator >= FIXED_DT):
        update(FIXED_DT)
        accumulator -= FIXED_DT

    alpha = accumulator / FIXED_DT
    render(alpha)    ← alpha로 보간하면 더 부드러움
```

### `std::variant` 이벤트 시스템

전통적인 방법 (enum + union):
```cpp
// C 스타일: 타입 안전하지 않음
struct Event { enum Type { KEY, MOUSE } type; union { KeyData; MouseData; }; };
```

모던 C++ 방법:
```cpp
// 타입 안전한 합 타입 (tagged union)
using Event = std::variant<KeyEvent, MouseMoveEvent, MouseButtonEvent, WindowEvent>;
```

---

## 2. 구현 가이드

### Step 1: 이벤트 타입 정의 (core 레이어)

> **설계 원칙**: Event는 플랫폼 독립적이므로 `core`에 정의합니다. 이렇게 하면 `platform`이 `engine`에 의존하지 않아 순환 의존성이 발생하지 않습니다.

```hpp
// core/include/gazeshot/core/Event.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <variant>

namespace gazeshot::core {

// ── 키보드 ──
enum class KeyAction : u8 { Pressed, Released };

struct KeyEvent {
    i32 scancode;       // SDL scancode (0-511 범위, keycode 아님!)
    KeyAction action;
    bool repeat;        // 키 반복 여부
};

// ── 마우스 이동 ──
struct MouseMoveEvent {
    f32 x, y;     // 현재 위치
    f32 dx, dy;   // 이전 대비 변화량
};

// ── 마우스 버튼 ──
enum class MouseButton : u8 { Left, Middle, Right };

struct MouseButtonEvent {
    MouseButton button;
    KeyAction action;   // Pressed / Released
    f32 x, y;           // 클릭 위치
};

// ── 윈도우 ──
struct WindowResizeEvent {
    i32 width, height;
};

struct WindowCloseEvent {};

// ── 통합 이벤트 타입 ──
using Event = std::variant<
    KeyEvent,
    MouseMoveEvent,
    MouseButtonEvent,
    WindowResizeEvent,
    WindowCloseEvent
>;

// ── overloaded 헬퍼 (C++17) ──
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

// C++17에서는 deduction guide 필요, C++20에서는 불필요
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

} // namespace gazeshot::core
```

**중요: scancode vs keycode**

SDL3에서:
- **keycode** (`SDLK_A`, `SDLK_SPACE` 등): bit 30이 설정된 큰 값 (1억 범위)
- **scancode** (`SDL_SCANCODE_A`, `SDL_SCANCODE_SPACE` 등): 0-511 범위

배열 기반 입력 시스템은 `scancode`를 써야 합니다 (512칸 배열로 충분).
`keycode`를 배열 인덱스로 쓰면 메모리 오버플로우가 발생합니다.

**C++ 학습 포인트: overloaded 패턴 (고급)**

```cpp
// 여러 lambda를 하나의 visitor로 합치는 트릭
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;  // CTAD 힌트 (C++17)
```

### 왜 필요한가?

`std::variant`를 방문할 때 타입별로 다른 동작을 하려면 오버로드된 callable이 필요합니다:

```cpp
std::variant<int, float, std::string> v = "hello";

// ❌ 단일 lambda는 모든 타입을 auto로 받아야 함 (타입 구분 힘듦)
std::visit([](auto x) { /* int인지 string인지 구분 불가 */ }, v);

// ✅ overloaded로 타입별 처리
std::visit(overloaded{
    [](int x) { std::cout << "int: " << x; },
    [](float x) { std::cout << "float: " << x; },
    [](const std::string& s) { std::cout << "str: " << s; }
}, v);
```

### 동작 원리

**1. Variadic Inheritance (다중 상속)**

```cpp
template <class... Ts>
struct overloaded : Ts... {  // 모든 lambda 타입을 동시에 상속
```

예시:
```cpp
auto l1 = [](int x) { return x + 1; };    // Lambda1 타입
auto l2 = [](float x) { return x * 2; };  // Lambda2 타입

// overloaded<Lambda1, Lambda2>는 이렇게 확장됨:
struct overloaded : Lambda1, Lambda2 {
    // Lambda1과 Lambda2를 둘 다 상속
};
```

**2. Pack Expansion으로 operator() 가져오기**

```cpp
using Ts::operator()...;  // 모든 부모의 operator()를 오버로드 집합으로
```

이것은 다음처럼 확장됩니다:
```cpp
using Lambda1::operator();  // operator()(int)
using Lambda2::operator();  // operator()(float)
// → 두 operator()가 오버로드된 하나의 함수 집합처럼 동작
```

**3. Deduction Guide**

```cpp
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
```

이게 없으면:
```cpp
// ❌ 타입을 명시해야 함 (너무 복잡)
auto v = overloaded<decltype(lambda1), decltype(lambda2)>{lambda1, lambda2};

// ✅ Deduction guide 덕분에 타입 자동 추론 (C++17)
auto v = overloaded{lambda1, lambda2};
```

**참고**: C++20에서는 aggregate initialization으로 자동 추론되므로 deduction guide가 불필요합니다.

### 전체 과정 예시

```cpp
using Event = std::variant<KeyEvent, MouseMoveEvent>;
Event e = MouseMoveEvent{100.0f, 200.0f, 5.0f, 10.0f};

std::visit(overloaded{
    [](const KeyEvent& k) { std::cout << "Key: " << k.scancode; },
    [](const MouseMoveEvent& m) { std::cout << "Mouse: " << m.x << "," << m.y; }
}, e);

// 컴파일러가 하는 일:
// 1. 두 lambda의 타입을 L1, L2로 추론
// 2. struct overloaded : L1, L2 생성 (두 operator() 오버로드)
// 3. variant가 실제로 MouseMoveEvent를 담고 있음을 확인
// 4. operator()(const MouseMoveEvent&) 호출 → 두 번째 lambda 실행
```

### 핵심 개념

| 문법 | 역할 |
|------|------|
| `struct overloaded : Ts...` | 모든 lambda 타입을 **다중 상속** |
| `using Ts::operator()...` | 모든 `operator()`를 **오버로드 집합**으로 노출 |
| `overloaded(Ts...) -> overloaded<Ts...>` | **CTAD** 힌트 (C++17) |

이 패턴은 **"lambda 오버로딩"**이라 불리며, 타입 안전한 `std::variant` 처리의 핵심 이디엄입니다

### Step 2: Input 시스템

```hpp
// engine/include/gazeshot/engine/Input.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec2.hpp>
#include <gazeshot/core/Event.hpp>  // ✅ core::Event 사용

#include <array>
#include <functional>
#include <vector>

namespace gazeshot::engine {

class Input {
public:
    // ── 키보드 상태 쿼리 (폴링) ──
    bool isKeyPressed(core::i32 scancode) const;   // 이번 프레임에 눌림
    bool isKeyHeld(core::i32 scancode) const;      // 현재 눌려있음
    bool isKeyReleased(core::i32 scancode) const;  // 이번 프레임에 뗌

    // ── 마우스 상태 쿼리 (폴링) ──
    core::math::Vec2f mousePosition() const { return mousePos_; }
    core::math::Vec2f mouseDelta() const { return mouseDelta_; }
    bool isMouseButtonHeld(core::MouseButton btn) const;

    // ── 이벤트 처리 (프레임 시작 시 호출) ──
    void processEvent(const core::Event& event);
    void endFrame();  // pressed/released 상태 리셋

    // ── 이벤트 핸들러 등록 (옵저버 패턴) ──
    using EventHandler = std::function<void(const core::Event&)>;
    void addHandler(EventHandler handler) { handlers_.push_back(std::move(handler)); }

private:
    static constexpr core::usize MAX_KEYS = 512;

    struct KeyState {
        bool current  = false;
        bool previous = false;
        mutable bool pressed  = false;   // 이번 프레임에 처음 눌림 (프레임 내 유지)
        mutable bool released = false;   // 이번 프레임에 뗌 (프레임 내 유지)
    };

    std::array<KeyState, MAX_KEYS> keys_{};
    std::array<bool, 3> mouseButtons_{};
    core::math::Vec2f mousePos_{};
    core::math::Vec2f mouseDelta_{};
    std::vector<EventHandler> handlers_;
};

} // namespace gazeshot::engine
```

```cpp
// engine/src/Input.cpp

#include <gazeshot/engine/Input.hpp>

namespace gazeshot::engine {

bool Input::isKeyPressed(core::i32 scancode) const {
    if (scancode < 0 || scancode >= static_cast<core::i32>(MAX_KEYS)) return false;
    return keys_[scancode].pressed;  // ✅ 프레임 내내 유지
}

bool Input::isKeyHeld(core::i32 scancode) const {
    if (scancode < 0 || scancode >= static_cast<core::i32>(MAX_KEYS)) return false;
    return keys_[scancode].current;
}

bool Input::isKeyReleased(core::i32 scancode) const {
    if (scancode < 0 || scancode >= static_cast<core::i32>(MAX_KEYS)) return false;
    return keys_[scancode].released;  // ✅ 프레임 내내 유지
}

bool Input::isMouseButtonHeld(core::MouseButton btn) const {
    return mouseButtons_[static_cast<core::u8>(btn)];
}

void Input::processEvent(const core::Event& event) {
    std::visit(core::overloaded{
        [this](const core::KeyEvent& e) {
            if (e.scancode < 0 || e.scancode >= static_cast<core::i32>(MAX_KEYS)) return;
            auto& key = keys_[e.scancode];

            if (e.action == core::KeyAction::Pressed) {
                bool wasPressed = key.current;
                key.current = true;

                // 처음 눌렀을 때만 (repeat 아님)
                if (!wasPressed && !e.repeat) {
                    key.pressed = true;  // ✅ 프레임 끝까지 유지됨
                }
            } else {
                key.current = false;
                key.released = true;  // ✅ 프레임 끝까지 유지됨
            }
        },
        [this](const core::MouseMoveEvent& e) {
            mouseDelta_ = {e.dx, e.dy};
            mousePos_ = {e.x, e.y};
        },
        [this](const core::MouseButtonEvent& e) {
            mouseButtons_[static_cast<core::u8>(e.button)] =
                (e.action == core::KeyAction::Pressed);
        },
        [](auto&) {}  // 나머지 이벤트 무시
    }, event);

    // 등록된 핸들러에도 전달
    for (auto& handler : handlers_) {
        handler(event);
    }
}

void Input::endFrame() {
    for (auto& key : keys_) {
        key.previous = key.current;
        key.pressed = false;   // ✅ 프레임 끝에 초기화
        key.released = false;  // ✅ 프레임 끝에 초기화
    }
    mouseDelta_ = {0.0f, 0.0f};
}

} // namespace gazeshot::engine
```

**C++ 학습 포인트: pressed/released vs current/previous**

```
프레임 N (키를 막 누름):
  1. processEvent() → current = true, pressed = true
  2. isKeyPressed() → pressed(T) → true ✅
  3. isKeyHeld() → current(T) → true ✅
  4. endFrame() → pressed = false, previous = current (F → T)

프레임 N+1 (키를 계속 누르고 있음):
  1. processEvent() → (이벤트 없음, current는 true 유지)
  2. isKeyPressed() → pressed(F) → false ❌
  3. isKeyHeld() → current(T) → true ✅
  4. endFrame() → pressed = false, previous = current (T → T)

프레임 N+2 (키를 뗌):
  1. processEvent() → current = false, released = true
  2. isKeyReleased() → released(T) → true ✅
  3. isKeyHeld() → current(F) → false ❌
  4. endFrame() → released = false, previous = current (T → F)
```

**핵심**:
- `pressed`/`released`: **이벤트 발생 시점**에만 true, 프레임 끝까지 유지
- `current`/`previous`: **상태 추적**용 (Held 판정)
- 프레임 내내 `pressed`가 유지되므로 **고정 스텝 업데이트가 0번이어도 입력 누락 없음**

**C++ 학습 포인트: `std::function`**

```cpp
using EventHandler = std::function<void(const Event&)>;

// 어떤 callable이든 저장 가능:
input.addHandler([](const Event& e) { /* lambda */ });
input.addHandler(myFreeFunction);
input.addHandler(std::bind(&MyClass::onEvent, &obj, std::placeholders::_1));
```

`std::function`은 type erasure 패턴의 표준 라이브러리 구현이다.
어떤 callable이든 동일한 타입으로 저장할 수 있게 해준다.

### Step 3: Window 이벤트 연동 (SDL → core 변환)

> **핵심**: `platform` 레이어가 SDL 타입을 `core::Event`로 변환합니다. 이렇게 하면 상위 레이어(`engine`, `game`)는 SDL을 전혀 몰라도 됩니다.

```cpp
// platform/include/gazeshot/platform/Window.hpp  (헤더에 선언 추가)

#include <gazeshot/core/Event.hpp>  // ✅ core::Event 사용

namespace gazeshot::platform {

class Window {
public:
    // ...
    std::vector<core::Event> pollEvents();  // ✅ core::Event 반환
    // ...
};

} // namespace gazeshot::platform
```

```cpp
// platform/src/Window.cpp  (SDL → core 변환 로직)

#include <gazeshot/platform/Window.hpp>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>  // SDL_SCANCODE_* 상수
#include <vector>

namespace gazeshot::platform {

std::vector<core::Event> Window::pollEvents() {
    std::vector<core::Event> events;
    SDL_Event sdlEvent;

    while (SDL_PollEvent(&sdlEvent)) {
        switch (sdlEvent.type) {
        case SDL_EVENT_QUIT:
            closed_ = true;
            events.push_back(core::WindowCloseEvent{});
            break;

        case SDL_EVENT_KEY_DOWN:
            events.push_back(core::KeyEvent{
                .scancode = static_cast<core::i32>(sdlEvent.key.scancode),  // ✅ scancode
                .action = core::KeyAction::Pressed,
                .repeat = sdlEvent.key.repeat
            });
            // Escape로 윈도우 닫기
            if (sdlEvent.key.scancode == SDL_SCANCODE_ESCAPE) {
                closed_ = true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            events.push_back(core::KeyEvent{
                .scancode = static_cast<core::i32>(sdlEvent.key.scancode),  // ✅ scancode
                .action = core::KeyAction::Released,
                .repeat = false
            });
            break;

        case SDL_EVENT_MOUSE_MOTION:
            events.push_back(core::MouseMoveEvent{
                .x  = sdlEvent.motion.x,
                .y  = sdlEvent.motion.y,
                .dx = sdlEvent.motion.xrel,
                .dy = sdlEvent.motion.yrel
            });
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            core::MouseButton btn = core::MouseButton::Left;
            if (sdlEvent.button.button == SDL_BUTTON_RIGHT)
                btn = core::MouseButton::Right;
            else if (sdlEvent.button.button == SDL_BUTTON_MIDDLE)
                btn = core::MouseButton::Middle;

            events.push_back(core::MouseButtonEvent{
                .button = btn,
                .action = (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                    ? core::KeyAction::Pressed : core::KeyAction::Released,
                .x = sdlEvent.button.x,
                .y = sdlEvent.button.y
            });
            break;
        }

        case SDL_EVENT_WINDOW_RESIZED:
            width_  = sdlEvent.window.data1;
            height_ = sdlEvent.window.data2;
            events.push_back(core::WindowResizeEvent{
                .width = width_, .height = height_
            });
            break;

        default: break;
        }
    }
    return events;
}

} // namespace gazeshot::platform
```

**설계 장점**:
- ✅ `engine`과 `game` 레이어는 SDL을 전혀 모름
- ✅ SDL → GLFW 교체 시 `platform` 내부만 수정
- ✅ 순환 의존성 없음: `core ← platform ← renderer ← engine`

### Step 4: 고정 시간 스텝 게임 루프

```hpp
// engine/include/gazeshot/engine/GameClock.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <chrono>

namespace gazeshot::engine {

class GameClock {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::duration<core::f32>;  // 초 단위 float

    static constexpr core::f32 FIXED_DT = 1.0f / 60.0f;  // 60Hz 물리
    static constexpr core::f32 MAX_FRAME_TIME = 0.25f;    // 나선형 지옥 방지

    GameClock() : previousTime_(Clock::now()) {}

    struct FrameResult {
        core::i32 updateCount;  // 이번 프레임에 update() 몇 번 호출
        core::f32 alpha;        // 렌더 보간 계수 [0, 1)
        core::f32 frameTime;    // 이번 프레임 실제 소요 시간
    };

    FrameResult tick() {
        auto currentTime = Clock::now();
        core::f32 frameTime = Duration(currentTime - previousTime_).count();
        previousTime_ = currentTime;

        // 나선형 지옥 방지: 한 프레임이 너무 길면 자른다
        if (frameTime > MAX_FRAME_TIME) {
            frameTime = MAX_FRAME_TIME;
        }

        accumulator_ += frameTime;

        core::i32 updateCount = 0;
        while (accumulator_ >= FIXED_DT) {
            accumulator_ -= FIXED_DT;
            ++updateCount;
        }

        return {
            .updateCount = updateCount,
            .alpha = accumulator_ / FIXED_DT,
            .frameTime = frameTime,
        };
    }

    // FPS 계산
    core::f32 fps() const { return fps_; }
    void updateFPS(core::f32 frameTime) {
        frameCount_++;
        fpsAccumulator_ += frameTime;
        if (fpsAccumulator_ >= 1.0f) {
            fps_ = static_cast<core::f32>(frameCount_) / fpsAccumulator_;
            frameCount_ = 0;
            fpsAccumulator_ = 0.0f;
        }
    }

private:
    TimePoint previousTime_;
    core::f32 accumulator_ = 0.0f;
    core::f32 fps_ = 0.0f;
    core::i32 frameCount_ = 0;
    core::f32 fpsAccumulator_ = 0.0f;
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: `std::chrono`**

```cpp
using Clock = std::chrono::steady_clock;     // 단조 증가 시계 (시간 조정에 영향 안 받음)
using Duration = std::chrono::duration<float>; // 초 단위 float

auto start = Clock::now();
// ... 작업 ...
float elapsed = Duration(Clock::now() - start).count();  // 초 단위
```

왜 `steady_clock`인가?
- `system_clock`: NTP 동기화 등으로 시간이 뒤로 갈 수 있다 → 게임 루프에 부적합
- `steady_clock`: 절대 뒤로 가지 않는다 → 경과 시간 측정에 적합

### Step 5: 입력 처리와 물리 업데이트 분리 (핵심!)

> **중요**: 입력 처리는 **프레임마다 1번** 실행되어야 하고, 물리 업데이트는 **고정 스텝**으로 0~N번 실행됩니다. 이 둘을 분리하지 않으면 키 입력이 씹힙니다!

**문제 상황**:

```cpp
// ❌ 잘못된 방법: update 안에서 입력 처리
void update(App& app, f32 dt) {
    if (app.input.isKeyPressed(SDL_SCANCODE_SPACE)) {  // 입력 처리
        // ...
    }
    // 물리 업데이트 ...
}

void oneFrame(void* arg) {
    // ...
    auto frame = clock.tick();
    for (i32 i = 0; i < frame.updateCount; ++i) {  // 0~N번
        update(*app, FIXED_DT);
    }
    // ...
}
```

**문제점**:
1. 60fps 이상으로 돌아가면 `updateCount = 0`인 프레임 발생
2. `update`가 0번 호출 → 입력 처리 안됨 → **키 입력 씹힘!**
3. `endFrame`에서 `pressed = false`로 초기화 → 영구 손실

**해결 방법**:

```cpp
// ✅ 올바른 방법: 입력 처리와 물리 분리
void handleInput(App& app) {
    // 프레임마다 무조건 1번 실행
    if (app.input.isKeyPressed(SDL_SCANCODE_SPACE)) {
        // ...
    }
}

void update(App& app, f32 dt) {
    // 물리/게임 로직만 (고정 스텝으로 0~N번)
}

void oneFrame(void* arg) {
    // 1. 이벤트 처리
    auto events = app->window.pollEvents();
    for (auto& event : events) {
        app->input.processEvent(event);
    }

    // 2. 입력 처리 (프레임마다 1번) ✅
    handleInput(*app);

    // 3. 물리 업데이트 (고정 스텝으로 0~N번)
    auto frame = app->clock.tick();
    for (i32 i = 0; i < frame.updateCount; ++i) {
        update(*app, FIXED_DT);
    }

    // 4. 렌더링
    render(*app, frame.alpha);

    // 5. 프레임 마무리
    app->input.endFrame();
}
```

**핵심 차이**:
- `handleInput()`: **프레임당 1번** (고정 스텝과 무관)
- `update()`: **고정 스텝으로 0~N번** (물리만)
- `pressed` 상태가 프레임 끝까지 유지되므로 어느 타이밍에 체크해도 OK

### Step 6: 데모 — 인터랙티브 큐브

```cpp
// game/src/main.cpp  (Ch.05)

#include <gazeshot/platform/Window.hpp>
#include <gazeshot/renderer/Renderer.hpp>
#include <gazeshot/core/Event.hpp>        // ✅ core::Event
#include <gazeshot/engine/Input.hpp>
#include <gazeshot/engine/GameClock.hpp>  // ✅ GameClock (파일명과 일치)
#include <gazeshot/core/math/Math.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <SDL3/SDL_scancode.h>  // SDL_SCANCODE_* 상수
#include <cstdio>

using namespace gazeshot;
using namespace core::math;
using namespace core::math::literals;

struct App {
    platform::Window window;
    std::unique_ptr<renderer::Renderer> renderer;
    std::unique_ptr<renderer::ShaderProgram> shader;
    // ... VBO, IBO, VAO ...

    engine::Input input;
    engine::GameClock clock;

    // 게임 상태
    Quatf rotation{};        // 현재 회전 (보간됨)
    Quatf targetRotation{};  // 목표 회전
    Vec3f position{0, 0, 0};  // 모델 위치
    Vec4f clearColor{0.12f, 0.12f, 0.15f, 1.0f};
    core::i32 colorIndex = 0;
    bool isDragging = false;
};

// ── 입력 처리 (프레임마다 1번) ──
void handleInput(App& app) {
    // R 키: 리셋
    if (app.input.isKeyPressed(SDL_SCANCODE_R)) {
        app.targetRotation = Quatf{};  // identity
        app.position = Vec3f{0, 0, 0};
    }

    // Space: 색상 변경
    if (app.input.isKeyPressed(SDL_SCANCODE_SPACE)) {
        constexpr Vec4f colors[] = {
            {0.12f, 0.12f, 0.15f, 1.0f},  // 다크
            {0.15f, 0.08f, 0.08f, 1.0f},  // 레드
            {0.08f, 0.15f, 0.08f, 1.0f},  // 그린
            {0.08f, 0.08f, 0.15f, 1.0f},  // 블루
        };
        app.colorIndex = (app.colorIndex + 1) % 4;
        app.clearColor = colors[app.colorIndex];
    }

    // 방향키: 이동 (누르고 있는 동안 계속 이동)
    constexpr core::f32 moveSpeed = 2.0f;  // 초당 2 유닛
    core::f32 dt = engine::GameClock::FIXED_DT;

    if (app.input.isKeyHeld(SDL_SCANCODE_LEFT)) {
        app.position.x -= moveSpeed * dt;
    }
    if (app.input.isKeyHeld(SDL_SCANCODE_RIGHT)) {
        app.position.x += moveSpeed * dt;
    }
    if (app.input.isKeyHeld(SDL_SCANCODE_UP)) {
        app.position.y += moveSpeed * dt;
    }
    if (app.input.isKeyHeld(SDL_SCANCODE_DOWN)) {
        app.position.y -= moveSpeed * dt;
    }

    app.isDragging = app.input.isMouseButtonHeld(core::MouseButton::Left);
}

// ── 물리 업데이트 (고정 스텝으로 0~N번) ──
void update(App& app, core::f32 dt) {
    // 마우스 드래그로 회전
    if (app.isDragging) {
        auto delta = app.input.mouseDelta();

        // 월드 공간 축 기준으로 회전 누적 (Quaternion)
        Quatf deltaY = Quatf::fromAxisAngle(Vec3f{0, 1, 0}, delta.x * 0.01f);
        Quatf deltaX = Quatf::fromAxisAngle(Vec3f{1, 0, 0}, delta.y * 0.01f);

        app.targetRotation = deltaY * deltaX * app.targetRotation;
    }

    // 부드러운 보간 (slerp)
    app.rotation = slerp(app.rotation, app.targetRotation, 10.0f * dt);
}

// ── 렌더링 ──
void render(App& app, core::f32 alpha) {
    app.renderer->clear(app.clearColor);

    // Translation matrix 생성
    Mat4f translation = Mat4f::identity();
    translation[0][3] = app.position.x;
    translation[1][3] = app.position.y;
    translation[2][3] = app.position.z;

    // Model matrix = Translation * Rotation
    Mat4f rotation = app.rotation.toMat4();  // Quaternion → Matrix
    Mat4f model = translation * rotation;

    Mat4f view = lookAt(Vec3f{0, 0, 3}, Vec3f{0, 0, 0}, Vec3f{0, 1, 0});
    core::f32 aspect = static_cast<core::f32>(app.window.width()) /
                       static_cast<core::f32>(app.window.height());
    Mat4f mvp = perspective(45.0_deg, aspect, 0.1f, 100.0f) * view * model;

    app.shader->bind();
    app.shader->setMat4("uTransform", mvp);
    app.renderer->bindVertexArray(app.vao);
    app.renderer->drawIndexed(36);

    app.window.swapBuffers();
}

void oneFrame(void* arg) {
    auto* app = static_cast<App*>(arg);

    // 1. 이벤트 수집
    auto events = app->window.pollEvents();
    for (auto& event : events) {
        app->input.processEvent(event);
    }

    if (app->window.shouldClose()) {
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }

    // 2. 입력 처리 (프레임마다 1번) - 고정 스텝과 무관 ✅
    handleInput(*app);

    // 3. 고정 스텝 업데이트 (물리/게임 로직만)
    auto frame = app->clock.tick();
    for (core::i32 i = 0; i < frame.updateCount; ++i) {
        update(*app, engine::GameClock::FIXED_DT);
    }

    // 4. 렌더링
    render(*app, frame.alpha);

    // 5. 프레임 마무리
    app->input.endFrame();
    app->clock.updateFPS(frame.frameTime);

    // 6. 디버그 출력 (1초마다)
    static core::f32 logTimer = 0.0f;
    logTimer += frame.frameTime;
    if (logTimer >= 1.0f) {
        auto pos = app->input.mousePosition();
        std::printf("FPS: %.2f, Mouse: (%.1f, %.1f)\n",
                    app->clock.fps(), pos.x, pos.y);
        logTimer = 0.0f;
    }
}

int main(int, char*[]) {
    App app{
        .window = platform::Window({.title = "GazeShot — Ch.05 Input & GameLoop"}),
    };
    init(app);  // renderer, shader, VBO, IBO 초기화

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(oneFrame, &app, 0, true);
#else
    while (!app.window.shouldClose()) {
        oneFrame(&app);
    }
#endif
    return 0;
}
```

---

## 3. 나선형 지옥 (Spiral of Death) 이해

```
만약 MAX_FRAME_TIME 제한이 없다면:

1. 프레임이 느려짐 (0.5초 걸림)
2. accumulator = 0.5, FIXED_DT = 0.02
3. update()를 25번 호출해야 함
4. 25번의 update()가 또 시간을 잡아먹음
5. 다음 프레임은 더 느려짐 → 더 많은 update() → 무한 루프

MAX_FRAME_TIME = 0.25 제한:
- accumulator가 0.25 이상 쌓이지 않음
- 최대 12~13번의 update()로 제한
- 물리가 잠깐 느려질 수 있지만 게임이 멈추지는 않음
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 마우스 드래그 | 좌클릭 + 드래그로 큐브 회전 |
| 방향키 이동 | ↑↓←→ 키로 큐브 이동 (누르고 있으면 계속 이동) |
| 키보드 입력 | Space로 색상 전환, R로 리셋 |
| 입력 누락 없음 | 60fps 이상에서도 Space 키 100% 반응 |
| FPS 표시 | 콘솔에 FPS 출력 |
| 고정 스텝 | 윈도우 리사이즈 중에도 물리 안정 |
| WASM 호환 | 브라우저에서 동일 동작 |
| 부드러운 보간 | slerp로 큐브가 부드럽게 회전 |

---

## 5. 핵심 개념 정리

### isKeyPressed vs isKeyHeld 언제 쓰나?

| 함수 | 용도 | 예시 |
|------|------|------|
| `isKeyPressed()` | 한 번만 발동 (토글, 액션) | Space로 색상 변경, E로 문 열기, R로 재장전 |
| `isKeyHeld()` | 계속 발동 (이동, 연사) | 방향키로 이동, 마우스로 회전, Shift로 달리기 |
| `isKeyReleased()` | 키를 뗐을 때 (차지 공격) | 마우스로 힘 모으기 → 뗄 때 발사 |

### 입력 처리 분리의 중요성

```
❌ 잘못된 설계 (입력이 씹힘):
oneFrame() {
    processEvents();
    for (i = 0; i < updateCount; ++i) {  // updateCount가 0이면?
        update();  // ← 여기서 isKeyPressed 체크
    }
    endFrame();  // ← pressed = false로 초기화 → 영구 손실!
}

✅ 올바른 설계 (입력 보장):
oneFrame() {
    processEvents();
    handleInput();  // ← 프레임당 무조건 1번
    for (i = 0; i < updateCount; ++i) {
        update();  // ← 물리만
    }
    endFrame();  // ← pressed 초기화해도 이미 처리 완료
}
```

## 6. 블로그 데모 아이디어

1. **인터랙티브 GIF**: 마우스 드래그 → 큐브 회전, 방향키 → 이동, Space → 색상 변경
2. **고정 시간 스텝 다이어그램**: accumulator, FIXED_DT, updateCount 관계 시각화
3. **입력 누락 문제 시연**: update 안에 넣었을 때 vs 밖으로 뺐을 때 비교
4. **FPS 그래프**: 안정적인 60fps + 60Hz 업데이트
5. **코드**: `std::visit + overloaded` 패턴의 우아함 강조
6. **나선형 지옥 시연**: MAX_FRAME_TIME 제거 → 프레임 폭주 → 다시 추가하여 해결
7. **WASM 데모**: 블로그에 임베딩하여 독자가 직접 조작

---

## 7. Phase A 완성!

Chapter 01~05를 마치면 다음이 완성된다:

| 구성요소 | 상태 |
|---------|------|
| 프로젝트 구조 | CMake 모듈별 분리, 듀얼 빌드 |
| 수학 라이브러리 | Vec2/3/4, Mat4, Quat, Transform |
| 렌더러 | 추상 인터페이스 + OpenGL 백엔드 |
| 윈도우/입력 | SDL3 래핑, 이벤트 시스템, 폴링 |
| 게임 루프 | 고정 시간 스텝, FPS 측정 |
| 테스트 | doctest, CTest 통합 |
| 크로스 플랫폼 | Desktop + WASM |

---

## 다음: Phase B 예고

**Chapter 06: 프로시저럴 메시 생성**

구, 박스, 실린더, 평면을 코드로 생성한다. 법선 벡터를 계산한다.
데모: 4가지 도형이 조명 없이 나란히 렌더링된다 (색으로 법선 시각화).
