# Chapter 05: 윈도우, 입력, 게임 루프

## 데모 미리보기

```
┌─────────────────────────────────────┐
│                                     │
│        ◆────◆  ← 마우스 드래그로     │
│       ╱    ╱│    큐브 회전 제어       │
│      ◆────◆ │                       │
│      │    │╱   Space: 색상 변경      │
│      ◆────◆    R: 회전 리셋          │
│                                     │
│  FPS: 60  |  Update: 50Hz fixed     │
│  Mouse: (640, 360)                  │
└─────────────────────────────────────┘
```

- **데모**: 마우스 드래그로 큐브 자유 회전, 키보드로 색상/리셋 제어
- **콘솔 출력**: FPS, 업데이트 레이트, 마우스 좌표 실시간 표시
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

### Step 1: 이벤트 타입 정의

```hpp
// engine/include/gazeshot/engine/Event.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <variant>

namespace gazeshot::engine {

// ── 키보드 ──
enum class KeyAction : core::u8 { Pressed, Released };

struct KeyEvent {
    core::i32 key;      // SDL keycode
    KeyAction action;
    bool repeat;        // 키 반복 여부
};

// ── 마우스 이동 ──
struct MouseMoveEvent {
    core::f32 x, y;     // 현재 위치
    core::f32 dx, dy;   // 이전 대비 변화량
};

// ── 마우스 버튼 ──
enum class MouseButton : core::u8 { Left, Middle, Right };

struct MouseButtonEvent {
    MouseButton button;
    KeyAction action;   // Pressed / Released
    core::f32 x, y;     // 클릭 위치
};

// ── 윈도우 ──
struct WindowResizeEvent {
    core::i32 width, height;
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

} // namespace gazeshot::engine
```

**C++ 학습 포인트: overloaded 패턴**

```cpp
// 여러 lambda를 하나의 visitor로 합치는 트릭
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
```

이것이 하는 일:
1. 여러 lambda 타입을 상속받는다
2. `using Ts::operator()...` 로 모든 `operator()` 를 현재 스코프로 가져온다
3. `std::visit`가 variant의 실제 타입에 맞는 lambda를 호출한다

### Step 2: Input 시스템

```hpp
// engine/include/gazeshot/engine/Input.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec2.hpp>
#include <gazeshot/engine/Event.hpp>

#include <array>
#include <functional>
#include <vector>

namespace gazeshot::engine {

class Input {
public:
    // ── 키보드 상태 쿼리 (폴링) ──
    bool isKeyPressed(core::i32 key) const;   // 이번 프레임에 눌림
    bool isKeyHeld(core::i32 key) const;      // 현재 눌려있음
    bool isKeyReleased(core::i32 key) const;  // 이번 프레임에 뗌

    // ── 마우스 상태 쿼리 (폴링) ──
    core::math::Vec2f mousePosition() const { return mousePos_; }
    core::math::Vec2f mouseDelta() const { return mouseDelta_; }
    bool isMouseButtonHeld(MouseButton btn) const;

    // ── 이벤트 처리 (프레임 시작 시 호출) ──
    void processEvent(const Event& event);
    void endFrame();  // pressed/released 상태 리셋

    // ── 이벤트 핸들러 등록 (옵저버 패턴) ──
    using EventHandler = std::function<void(const Event&)>;
    void addHandler(EventHandler handler) { handlers_.push_back(std::move(handler)); }

private:
    static constexpr core::usize MAX_KEYS = 512;

    struct KeyState {
        bool current  = false;
        bool previous = false;
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

bool Input::isKeyPressed(core::i32 key) const {
    if (key < 0 || key >= static_cast<core::i32>(MAX_KEYS)) return false;
    return keys_[key].current && !keys_[key].previous;
}

bool Input::isKeyHeld(core::i32 key) const {
    if (key < 0 || key >= static_cast<core::i32>(MAX_KEYS)) return false;
    return keys_[key].current;
}

bool Input::isKeyReleased(core::i32 key) const {
    if (key < 0 || key >= static_cast<core::i32>(MAX_KEYS)) return false;
    return !keys_[key].current && keys_[key].previous;
}

bool Input::isMouseButtonHeld(MouseButton btn) const {
    return mouseButtons_[static_cast<core::u8>(btn)];
}

void Input::processEvent(const Event& event) {
    std::visit(overloaded{
        [this](const KeyEvent& e) {
            if (e.key >= 0 && e.key < static_cast<core::i32>(MAX_KEYS)) {
                keys_[e.key].current = (e.action == KeyAction::Pressed);
            }
        },
        [this](const MouseMoveEvent& e) {
            mouseDelta_ = {e.dx, e.dy};
            mousePos_ = {e.x, e.y};
        },
        [this](const MouseButtonEvent& e) {
            mouseButtons_[static_cast<core::u8>(e.button)] =
                (e.action == KeyAction::Pressed);
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
    }
    mouseDelta_ = {0.0f, 0.0f};
}

} // namespace gazeshot::engine
```

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

### Step 3: Window 이벤트 연동 개선

```cpp
// platform/src/Window.cpp  (pollEvents 수정)

#include <gazeshot/engine/Event.hpp>
#include <vector>

// Window 클래스에 추가:
std::vector<engine::Event> Window::pollEvents() {
    std::vector<engine::Event> events;
    SDL_Event sdlEvent;

    while (SDL_PollEvent(&sdlEvent)) {
        switch (sdlEvent.type) {
        case SDL_EVENT_QUIT:
            closed_ = true;
            events.push_back(engine::WindowCloseEvent{});
            break;

        case SDL_EVENT_KEY_DOWN:
            events.push_back(engine::KeyEvent{
                .key = sdlEvent.key.key,
                .action = engine::KeyAction::Pressed,
                .repeat = sdlEvent.key.repeat
            });
            if (sdlEvent.key.key == SDLK_ESCAPE) closed_ = true;
            break;

        case SDL_EVENT_KEY_UP:
            events.push_back(engine::KeyEvent{
                .key = sdlEvent.key.key,
                .action = engine::KeyAction::Released,
                .repeat = false
            });
            break;

        case SDL_EVENT_MOUSE_MOTION:
            events.push_back(engine::MouseMoveEvent{
                .x  = sdlEvent.motion.x,
                .y  = sdlEvent.motion.y,
                .dx = sdlEvent.motion.xrel,
                .dy = sdlEvent.motion.yrel
            });
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            engine::MouseButton btn = engine::MouseButton::Left;
            if (sdlEvent.button.button == SDL_BUTTON_RIGHT)
                btn = engine::MouseButton::Right;
            else if (sdlEvent.button.button == SDL_BUTTON_MIDDLE)
                btn = engine::MouseButton::Middle;

            events.push_back(engine::MouseButtonEvent{
                .button = btn,
                .action = (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                    ? engine::KeyAction::Pressed : engine::KeyAction::Released,
                .x = sdlEvent.button.x,
                .y = sdlEvent.button.y
            });
            break;
        }

        case SDL_EVENT_WINDOW_RESIZED:
            width_  = sdlEvent.window.data1;
            height_ = sdlEvent.window.data2;
            events.push_back(engine::WindowResizeEvent{
                .width = width_, .height = height_
            });
            break;

        default: break;
        }
    }
    return events;
}
```

### Step 4: 고정 시간 스텝 게임 루프

```hpp
// engine/include/gazeshot/engine/GameLoop.hpp

#pragma once

#include <gazeshot/core/Types.hpp>
#include <chrono>

namespace gazeshot::engine {

class GameClock {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::duration<core::f32>;  // 초 단위 float

    static constexpr core::f32 FIXED_DT = 1.0f / 50.0f;  // 50Hz 물리
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

### Step 5: 데모 — 인터랙티브 큐브

```cpp
// game/src/main.cpp  (Ch.05)

#include <gazeshot/platform/Window.hpp>
#include <gazeshot/renderer/Renderer.hpp>
#include <gazeshot/engine/Event.hpp>
#include <gazeshot/engine/Input.hpp>
#include <gazeshot/engine/GameLoop.hpp>
#include <gazeshot/core/math/Math.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
    Vec3f cubeRotation{0, 0, 0};
    Vec3f targetRotation{0, 0, 0};
    Vec4f clearColor{0.12f, 0.12f, 0.15f, 1.0f};
    int colorIndex = 0;
    bool isDragging = false;
};

// ── 고정 스텝 업데이트 (물리/로직) ──
void update(App& app, core::f32 dt) {
    // 마우스 드래그로 회전
    if (app.isDragging) {
        auto delta = app.input.mouseDelta();
        app.targetRotation.y += delta.x * 0.01f;
        app.targetRotation.x += delta.y * 0.01f;
    }

    // 부드러운 보간
    app.cubeRotation = lerp(app.cubeRotation, app.targetRotation, 10.0f * dt);

    // R 키: 리셋
    if (app.input.isKeyPressed(SDLK_R)) {
        app.targetRotation = Vec3f{0, 0, 0};
    }

    // Space: 색상 변경
    if (app.input.isKeyPressed(SDLK_SPACE)) {
        constexpr Vec4f colors[] = {
            {0.12f, 0.12f, 0.15f, 1.0f},  // 다크
            {0.15f, 0.08f, 0.08f, 1.0f},  // 레드
            {0.08f, 0.15f, 0.08f, 1.0f},  // 그린
            {0.08f, 0.08f, 0.15f, 1.0f},  // 블루
        };
        app.colorIndex = (app.colorIndex + 1) % 4;
        app.clearColor = colors[app.colorIndex];
    }

    // 마우스 버튼
    app.isDragging = app.input.isMouseButtonHeld(engine::MouseButton::Left);
}

// ── 렌더링 ──
void render(App& app, core::f32 alpha) {
    app.renderer->clear(app.clearColor);

    // 보간된 회전 (alpha는 현재 미사용, 추후 물리 보간에 사용)
    Mat4f model = rotateY(app.cubeRotation.y) * rotateX(app.cubeRotation.x);
    Mat4f view = lookAt(Vec3f{0,0,3}, Vec3f{0,0,0}, Vec3f{0,1,0});
    float aspect = (float)app.window.width() / (float)app.window.height();
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

    // 2. 고정 스텝 업데이트
    auto frame = app->clock.tick();
    for (core::i32 i = 0; i < frame.updateCount; ++i) {
        update(*app, engine::GameClock::FIXED_DT);
    }

    // 3. 렌더링
    render(*app, frame.alpha);

    // 4. 프레임 마무리
    app->input.endFrame();
    app->clock.updateFPS(frame.frameTime);

    // 5. 디버그 출력 (1초마다)
    static float logTimer = 0;
    logTimer += frame.frameTime;
    if (logTimer >= 1.0f) {
        auto pos = app->input.mousePosition();
        std::printf("FPS: %.0f | Mouse: (%.0f, %.0f)\n",
                    app->clock.fps(), pos.x, pos.y);
        logTimer = 0;
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
| 키보드 입력 | Space로 색상 전환, R로 리셋 |
| FPS 표시 | 콘솔에 FPS 출력 |
| 고정 스텝 | 윈도우 리사이즈 중에도 물리 안정 |
| WASM 호환 | 브라우저에서 동일 동작 |
| 부드러운 보간 | lerp로 큐브가 부드럽게 따라옴 |

---

## 5. 블로그 데모 아이디어

1. **인터랙티브 GIF**: 마우스 드래그 → 큐브 회전, Space → 색상 변경
2. **고정 시간 스텝 다이어그램**: accumulator, FIXED_DT, alpha 관계 시각화
3. **FPS 그래프**: 안정적인 60fps + 50Hz 업데이트
4. **코드**: `std::visit + overloaded` 패턴의 우아함 강조
5. **나선형 지옥 시연**: MAX_FRAME_TIME 제거 → 프레임 폭주 → 다시 추가하여 해결
6. **WASM 데모**: 블로그에 임베딩하여 독자가 직접 조작

---

## 6. Phase A 완성!

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
