# Chapter 01: 프로젝트 아키텍처와 빌드 시스템

## 데모 미리보기

이 챕터를 마치면 다음을 얻는다:

```
┌─────────────────────────────────────┐
│          GazeShot Engine            │
│                                     │
│    (SDL3 윈도우, 틸 색 배경)          │
│                                     │
│    ESC를 누르면 종료                  │
│                                     │
└─────────────────────────────────────┘
```

- **Desktop**: SDL3 윈도우가 열리고 틸(teal) 색 배경이 보인다
- **Web**: 브라우저에서 동일한 화면이 canvas 위에 렌더링된다
- **테스트**: `ctest`로 doctest 기반 테스트가 통과한다

블로그에 Desktop 스크린샷 + 브라우저 스크린샷을 나란히 올릴 수 있다.

---

## 학습 목표

1. 게임 엔진 프로젝트의 레이어 구조를 직접 설계한다
2. CMake로 모듈별 라이브러리를 구성하고 의존성을 관리한다
3. SDL3를 FetchContent로 가져와 윈도우를 띄운다
4. Desktop과 WASM 듀얼 빌드를 구성한다
5. doctest를 통합하여 첫 테스트를 작성한다
6. C++20 namespace, 헤더/소스 분리, forward declaration을 실습한다

---

## 1. 배경 지식

### 게임 엔진의 레이어 구조

게임 엔진은 일반적으로 아래에서 위로 쌓는 레이어 구조를 따른다.
아래 레이어는 위 레이어를 알지 못한다 (단방향 의존).

```
┌─────────────────────────────────────┐
│  Game Layer                         │  ← 스나이퍼 게임 고유 로직
│  (SniperCamera, ShootingRange,      │
│   ScoreSystem, GazeTracker)         │
├─────────────────────────────────────┤
│  Engine Layer                       │  ← 장르 무관한 엔진 기능
│  (Scene, Entity, ResourceManager,   │
│   ParticleSystem)                   │
├─────────────────────────────────────┤
│  Renderer Layer                     │  ← 그래픽스 API 추상화
│  (Renderer interface,               │
│   OpenGL ES 3.0 backend)            │
├─────────────────────────────────────┤
│  Platform Layer                     │  ← OS/브라우저 추상화
│  (Window, Input, Audio — via SDL3)  │
├─────────────────────────────────────┤
│  Core Layer                         │  ← 기반 유틸리티
│  (Math, Types, Logger, Timer)       │
└─────────────────────────────────────┘
```

**핵심 규칙**: 화살표는 아래로만 향한다.
- Game은 Engine을 include할 수 있다
- Engine은 Renderer를 include할 수 있다
- Core는 아무것도 include하지 않는다 (자체 완결)

### CMake 모듈 구조

각 레이어를 CMake static library 타겟으로 만든다:

```cmake
add_library(gazeshot_core STATIC ...)
add_library(gazeshot_renderer STATIC ...)
target_link_libraries(gazeshot_renderer PUBLIC gazeshot_core)  # 의존 방향
```

이렇게 하면:
- 컴파일 단위가 분리되어 변경 시 전체 재빌드를 피한다
- 의존성 위반 시 링크 에러로 즉시 발견된다
- 테스트에서 필요한 레이어만 링크할 수 있다

### SDL3를 선택하는 이유

GLFW 대비 SDL3의 장점은 코스 아웃라인에서 다뤘다.
여기서는 실무적인 차이 하나만 강조한다:

```cpp
// GLFW: 윈도우만 제공, 나머지는 직접 조합
glfwInit();                  // 윈도우
// + miniaudio / OpenAL      // 오디오는 별도
// + 직접 구현               // 카메라(웹캠)는 별도

// SDL3: 하나의 초기화로 통합
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_CAMERA);
```

### C++20 namespace 설계

중첩 네임스페이스로 모듈 경계를 코드에서도 명확히 한다:

```cpp
// C++17 이전
namespace gazeshot { namespace core { namespace math { /* ... */ } } }

// C++17 이후: 중첩 네임스페이스 선언
namespace gazeshot::core::math {
    struct Vec3 { float x, y, z; };
}
```

네임스페이스 규칙:
- `gazeshot::core` — 수학, 타입, 유틸리티
- `gazeshot::platform` — SDL3 래퍼
- `gazeshot::renderer` — 렌더링 추상화
- `gazeshot::engine` — 씬, 엔티티, 리소스
- `gazeshot::game` — 게임 고유 로직

---

## 2. 디렉토리 구조

먼저 전체 구조를 만든다. 이 챕터에서 실제로 내용을 채우는 파일에 `✏️`를 표시했다.

```
gazeshot/
├── CMakeLists.txt              ✏️  루트 CMake
├── cmake/
│   └── Dependencies.cmake      ✏️  FetchContent 설정
├── core/
│   ├── CMakeLists.txt          ✏️
│   └── include/
│       └── gazeshot/
│           └── core/
│               └── Types.hpp   ✏️  기본 타입 정의
├── platform/
│   ├── CMakeLists.txt          ✏️
│   ├── include/
│   │   └── gazeshot/
│   │       └── platform/
│   │           └── Window.hpp  ✏️  윈도우 클래스 선언
│   └── src/
│       └── Window.cpp          ✏️  SDL3 구현
├── renderer/
│   ├── CMakeLists.txt          ✏️  (빈 스켈레톤)
│   └── include/
│       └── gazeshot/
│           └── renderer/
│               └── .gitkeep
├── engine/
│   ├── CMakeLists.txt          ✏️  (빈 스켈레톤)
│   └── include/
│       └── gazeshot/
│           └── engine/
│               └── .gitkeep
├── game/
│   ├── CMakeLists.txt          ✏️
│   └── src/
│       └── main.cpp            ✏️  엔트리포인트
├── tests/
│   ├── CMakeLists.txt          ✏️
│   └── test_types.cpp          ✏️  첫 번째 테스트
└── web/
    └── shell.html              ✏️  WASM 호스트 페이지
```

**include 경로 규칙**: `#include <gazeshot/core/Types.hpp>` 형태를 쓴다.
이렇게 하면 어떤 모듈의 헤더인지 경로에서 바로 보인다.

```
mkdir -p gazeshot/{cmake,core/{include/gazeshot/core,src},platform/{include/gazeshot/platform,src},renderer/{include/gazeshot/renderer},engine/{include/gazeshot/engine},game/src,tests,web}
```

---

## 3. 구현 가이드

### Step 1: 루트 CMakeLists.txt

```cmake
# gazeshot/CMakeLists.txt

cmake_minimum_required(VERSION 3.24)
project(gazeshot VERSION 0.1.0 LANGUAGES CXX)

# ── C++20 표준 ──
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)     # GNU 확장 비활성 → 이식성

# ── 빌드 타입 기본값 ──
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ── 공통 컴파일 옵션 ──
add_compile_options(-Wall -Wextra -Wpedantic)

# ── 외부 의존성 ──
include(cmake/Dependencies.cmake)

# ── 모듈 (아래에서 위로, 의존 순서) ──
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(renderer)
add_subdirectory(engine)
add_subdirectory(game)

# ── 테스트 ──
enable_testing()
add_subdirectory(tests)
```

**포인트**:
- `CMAKE_CXX_EXTENSIONS OFF`: GCC/Clang의 비표준 확장 사용을 막아서 Emscripten 호환성을 보장한다
- 모듈 순서는 의존성 순서와 일치시킨다 (core → platform → renderer → engine → game)

### Step 2: 외부 의존성 (FetchContent)

```cmake
# gazeshot/cmake/Dependencies.cmake

include(FetchContent)

# ── SDL3 ──
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.8     # 안정 릴리즈 태그 사용
    GIT_SHALLOW    TRUE
)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# ── doctest (테스트 전용) ──
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)
```

> **GIT_SHALLOW TRUE**: 히스토리 전체를 받지 않아 초기 빌드가 빠르다.
> SDL3의 태그는 실제 최신 안정 릴리즈로 바꿔 쓴다.
> `cmake -B build` 시 자동으로 다운로드된다.

### Step 3: Core 모듈

Core는 다른 모듈에 의존하지 않는 자체 완결 레이어다.

```hpp
// gazeshot/core/include/gazeshot/core/Types.hpp

#pragma once

#include <cstdint>
#include <cstddef>

namespace gazeshot::core {

// ── 고정 크기 정수 별칭 ──
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;

} // namespace gazeshot::core
```

**왜 별칭을 만드는가?**
- `std::uint32_t`를 매번 쓰면 노이즈가 많다
- `u32`, `f32` 같은 짧은 이름은 그래픽스/게임 코드에서 관례적이다
- Rust의 타입 이름과도 비슷하여 가독성이 좋다

```cmake
# gazeshot/core/CMakeLists.txt

add_library(gazeshot_core INTERFACE)  # 헤더 온리 → INTERFACE

target_include_directories(gazeshot_core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

**INTERFACE 라이브러리**: 소스 파일이 없고 헤더만 있을 때 사용한다.
`target_link_libraries(X gazeshot_core)` 하면 include 경로가 자동으로 전파된다.

### Step 4: Platform 모듈 — Window 클래스

이 프로젝트의 첫 번째 "진짜" 코드다. SDL3 윈도우를 RAII로 감싼다.

```hpp
// gazeshot/platform/include/gazeshot/platform/Window.hpp

#pragma once

#include <gazeshot/core/Types.hpp>

#include <string>
#include <string_view>

// ── forward declaration ──
// SDL 헤더를 여기서 include하지 않는다.
// Window.hpp를 include하는 쪽이 SDL에 의존하지 않도록.
struct SDL_Window;
typedef void* SDL_GLContext;

namespace gazeshot::platform {

struct WindowConfig {
    std::string title = "GazeShot";
    core::i32 width   = 1280;
    core::i32 height  = 720;
};

class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    // ── 복사 금지, 이동 허용 ──
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    // ── 매 프레임 ──
    bool shouldClose() const;
    void pollEvents();
    void swapBuffers();

    // ── 접근자 ──
    core::i32 width() const { return width_; }
    core::i32 height() const { return height_; }

private:
    SDL_Window*  window_  = nullptr;
    SDL_GLContext context_ = nullptr;
    core::i32    width_   = 0;
    core::i32    height_  = 0;
    bool         closed_  = false;
};

} // namespace gazeshot::platform
```

**C++ 학습 포인트: forward declaration**

`struct SDL_Window;` 한 줄로 SDL 헤더 전체를 include하지 않는다.
이 헤더를 include하는 모든 파일이 SDL에 의존하지 않게 된다.
실제 SDL 함수 호출은 `.cpp` 파일에서만 한다.

**C++ 학습 포인트: 복사 금지, 이동 허용**

윈도우는 OS 리소스(핸들)를 가지고 있다. 복사하면 이중 해제 위험이 있으므로
`delete`로 막고, 이동만 허용한다.

```
Window a(config);
Window b = a;            // ❌ 컴파일 에러 (복사 금지)
Window c = std::move(a); // ✅ a의 핸들을 c로 이전, a는 빈 상태
```

이제 구현:

```cpp
// gazeshot/platform/src/Window.cpp

#include <gazeshot/platform/Window.hpp>

#include <SDL3/SDL.h>

#include <stdexcept>
#include <utility>   // std::exchange

namespace gazeshot::platform {

Window::Window(const WindowConfig& config)
    : width_(config.width)
    , height_(config.height)
{
    // ── SDL 초기화 ──
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(
            std::string("SDL_Init failed: ") + SDL_GetError()
        );
    }

    // ── OpenGL ES 3.0 컨텍스트 요청 ──
    // Desktop에서는 3.3 Core Profile로 동작 (ES 3.0 상위호환)
    // WASM에서는 WebGL 2.0으로 매핑
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // ── 윈도우 생성 ──
    window_ = SDL_CreateWindow(
        config.title.c_str(),
        config.width,
        config.height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window_) {
        throw std::runtime_error(
            std::string("SDL_CreateWindow failed: ") + SDL_GetError()
        );
    }

    // ── GL 컨텍스트 생성 ──
    context_ = SDL_GL_CreateContext(window_);
    if (!context_) {
        throw std::runtime_error(
            std::string("SDL_GL_CreateContext failed: ") + SDL_GetError()
        );
    }

    // ── VSync 활성화 ──
    SDL_GL_SetSwapInterval(1);
}

Window::~Window() {
    if (context_) {
        SDL_GL_DestroyContext(context_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

// ── 이동 생성자 ──
Window::Window(Window&& other) noexcept
    : window_(std::exchange(other.window_, nullptr))    // other에서 가져오고 null로
    , context_(std::exchange(other.context_, nullptr))
    , width_(other.width_)
    , height_(other.height_)
    , closed_(other.closed_)
{
}

// ── 이동 대입 연산자 ──
Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        // 기존 리소스 해제
        if (context_) SDL_GL_DestroyContext(context_);
        if (window_) SDL_DestroyWindow(window_);

        // 이동
        window_  = std::exchange(other.window_, nullptr);
        context_ = std::exchange(other.context_, nullptr);
        width_   = other.width_;
        height_  = other.height_;
        closed_  = other.closed_;
    }
    return *this;
}

bool Window::shouldClose() const {
    return closed_;
}

void Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            closed_ = true;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE) {
                closed_ = true;
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            width_  = event.window.data1;
            height_ = event.window.data2;
            break;
        default:
            break;
        }
    }
}

void Window::swapBuffers() {
    SDL_GL_SwapWindow(window_);
}

} // namespace gazeshot::platform
```

**C++ 학습 포인트: `std::exchange`**

`std::exchange(other.window_, nullptr)`는 두 가지 일을 한 번에 한다:
1. `other.window_`의 현재 값을 반환한다
2. `other.window_`에 `nullptr`을 대입한다

이동 생성자에서 "가져오면서 원본을 비우는" 패턴에 딱 맞는다.

```cmake
# gazeshot/platform/CMakeLists.txt

add_library(gazeshot_platform STATIC
    src/Window.cpp
)

target_include_directories(gazeshot_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gazeshot_platform
    PUBLIC  gazeshot_core
    PRIVATE SDL3::SDL3
)
```

**PUBLIC vs PRIVATE**:
- `gazeshot_core`는 PUBLIC → platform의 헤더에서 core 타입을 노출하므로
- `SDL3::SDL3`은 PRIVATE → SDL은 `.cpp`에서만 사용, 헤더에서는 forward declaration

### Step 5: Renderer / Engine 스켈레톤

아직 내용이 없지만 빌드 시스템에 자리를 잡아둔다.

```cmake
# gazeshot/renderer/CMakeLists.txt

add_library(gazeshot_renderer INTERFACE)
target_include_directories(gazeshot_renderer
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(gazeshot_renderer INTERFACE gazeshot_core)
```

```cmake
# gazeshot/engine/CMakeLists.txt

add_library(gazeshot_engine INTERFACE)
target_include_directories(gazeshot_engine
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(gazeshot_engine INTERFACE gazeshot_renderer)
```

### Step 6: Game — 엔트리포인트

```cmake
# gazeshot/game/CMakeLists.txt

add_executable(gazeshot_game
    src/main.cpp
)

target_link_libraries(gazeshot_game
    PRIVATE
        gazeshot_platform
        gazeshot_engine
)

# ── WASM 빌드 설정 ──
if(EMSCRIPTEN)
    set_target_properties(gazeshot_game PROPERTIES
        SUFFIX ".html"
    )
    target_link_options(gazeshot_game PRIVATE
        "SHELL:-s USE_SDL=0"           # 시스템 SDL 사용 안 함 (직접 빌드한 SDL3 사용)
        "SHELL:-s MIN_WEBGL_VERSION=2"
        "SHELL:-s MAX_WEBGL_VERSION=2"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
    )
endif()
```

```cpp
// gazeshot/game/src/main.cpp

#include <gazeshot/platform/Window.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// ── 전역 상태를 피하기 위한 App 구조체 ──
struct App {
    gazeshot::platform::Window window;
};

// ── 한 프레임을 함수로 분리 (WASM 호환 핵심) ──
void oneFrame(void* arg) {
    auto* app = static_cast<App*>(arg);

    app->window.pollEvents();

    // ── OpenGL로 배경 클리어 ──
    // 아직 GLAD가 없으므로, SDL이 로드한 GL 함수를 직접 사용
    // Desktop: gl 함수는 SDL이 컨텍스트 생성 시 로드
    // WASM:    Emscripten이 WebGL 함수를 제공
#ifdef __EMSCRIPTEN__
    #include <GLES3/gl3.h>
#elif defined(__APPLE__)
    #define GL_SILENCE_DEPRECATION
    #include <OpenGL/gl3.h>
#else
    // Desktop에서는 SDL의 GL 함수 사용을 위해 헤더 필요
    // Ch.04에서 GLAD로 교체 예정
    #include <SDL3/SDL_opengl.h>
#endif

    glClearColor(0.18f, 0.55f, 0.54f, 1.0f);  // teal 색
    glClear(GL_COLOR_BUFFER_BIT);

    app->window.swapBuffers();
}

int main(int /*argc*/, char* /*argv*/[]) {
    App app{
        .window = gazeshot::platform::Window({
            .title  = "GazeShot — Chapter 01",
            .width  = 1280,
            .height = 720,
        })
    };

#ifdef __EMSCRIPTEN__
    // 브라우저에서는 제어권을 돌려줘야 한다
    // 0 = 브라우저의 requestAnimationFrame 속도
    // true = 무한 루프처럼 동작
    emscripten_set_main_loop_arg(oneFrame, &app, 0, true);
#else
    while (!app.window.shouldClose()) {
        oneFrame(&app);
    }
#endif

    return 0;
}
```

> **주의**: 위 코드에서 `#include`를 함수 내부에 넣은 것은 실제로는 좋지 않다.
> 이것은 Ch.04에서 GLAD 또는 렌더러 추상화로 정리할 예정이다.
> 지금은 "SDL3 + GL이 동작하는 것"을 확인하는 것이 목적이다.

실제로는 main.cpp를 좀 더 깔끔하게 구성한다:

```cpp
// gazeshot/game/src/main.cpp
// (실제 구현 — 위의 설명판 대신 이것을 사용)

#include <gazeshot/platform/Window.hpp>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <emscripten.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

struct App {
    gazeshot::platform::Window window;
    bool running = true;
};

void oneFrame(void* arg) {
    auto* app = static_cast<App*>(arg);

    app->window.pollEvents();

    if (app->window.shouldClose()) {
        app->running = false;
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }

    glClearColor(0.18f, 0.55f, 0.54f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    app->window.swapBuffers();
}

int main(int /*argc*/, char* /*argv*/[]) {
    App app{
        .window = gazeshot::platform::Window({
            .title  = "GazeShot — Chapter 01",
            .width  = 1280,
            .height = 720,
        }),
    };

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(oneFrame, &app, 0, true);
#else
    while (app.running) {
        oneFrame(&app);
    }
#endif

    return 0;
}
```

**C++ 학습 포인트: designated initializers (C++20)**

```cpp
App app{
    .window = Window({
        .title  = "GazeShot",   // 필드 이름을 명시
        .width  = 1280,
        .height = 720,
    }),
};
```

어떤 값이 어떤 필드에 들어가는지 코드만 보고 알 수 있다.
`Window({"GazeShot", 1280, 720})`보다 훨씬 읽기 좋다.

### Step 7: 테스트

```cmake
# gazeshot/tests/CMakeLists.txt

add_executable(gazeshot_tests
    test_types.cpp
)

target_link_libraries(gazeshot_tests
    PRIVATE
        gazeshot_core
        doctest::doctest
)

# doctest를 자체 main으로 사용
target_compile_definitions(gazeshot_tests
    PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
)

# CTest에 등록
add_test(NAME gazeshot_tests COMMAND gazeshot_tests)
```

```cpp
// gazeshot/tests/test_types.cpp

#include <doctest/doctest.h>
#include <gazeshot/core/Types.hpp>

#include <type_traits>

using namespace gazeshot::core;

TEST_CASE("타입 크기 검증") {
    CHECK(sizeof(u8)  == 1);
    CHECK(sizeof(u16) == 2);
    CHECK(sizeof(u32) == 4);
    CHECK(sizeof(u64) == 8);

    CHECK(sizeof(f32) == 4);
    CHECK(sizeof(f64) == 8);
}

TEST_CASE("타입 특성 검증") {
    // 부호 없는 정수
    CHECK(std::is_unsigned_v<u8>);
    CHECK(std::is_unsigned_v<u32>);

    // 부호 있는 정수
    CHECK(std::is_signed_v<i32>);

    // 부동소수점
    CHECK(std::is_floating_point_v<f32>);
    CHECK(std::is_floating_point_v<f64>);
}

TEST_CASE("f32 정밀도") {
    f32 a = 0.1f;
    f32 b = 0.2f;
    f32 c = a + b;

    // 부동소수점은 == 비교하면 안 된다
    // doctest::Approx가 epsilon 범위 내에서 비교해준다
    CHECK(c == doctest::Approx(0.3f));

    // 직접 epsilon 지정도 가능
    CHECK(c == doctest::Approx(0.3f).epsilon(0.0001));
}
```

**C++ 학습 포인트: `std::is_unsigned_v<T>`**

`<type_traits>`는 컴파일 타임에 타입의 속성을 검사하는 도구다.
`_v` 접미사는 `::value`를 생략한 C++17 변수 템플릿이다:

```cpp
// C++14
static_assert(std::is_unsigned<u32>::value);

// C++17 (동일)
static_assert(std::is_unsigned_v<u32>);
```

### Step 8: WASM 셸 페이지

```html
<!-- gazeshot/web/shell.html -->
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>GazeShot</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            background: #1a1a2e;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            font-family: monospace;
            color: #e0e0e0;
        }
        #canvas-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 16px;
        }
        canvas {
            border: 2px solid #16213e;
            border-radius: 4px;
        }
        #status { font-size: 14px; opacity: 0.6; }
    </style>
</head>
<body>
    <div id="canvas-container">
        <canvas id="canvas" width="1280" height="720"
                oncontextmenu="event.preventDefault()" tabindex="-1"></canvas>
        <div id="status">Loading...</div>
    </div>

    <script>
        var Module = {
            canvas: document.getElementById('canvas'),
            onRuntimeInitialized: function() {
                document.getElementById('status').textContent = 'Running';
            }
        };
    </script>
    {{{ SCRIPT }}}
</body>
</html>
```

`{{{ SCRIPT }}}`는 Emscripten이 빌드 시 JS 코드로 대체한다.

---

## 4. 빌드 및 실행

### Desktop 빌드

```bash
cd gazeshot

# 설정 (첫 실행 시 SDL3, doctest 다운로드 — 몇 분 소요)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 빌드
cmake --build build

# 실행
./build/game/gazeshot_game

# 테스트
cd build && ctest --output-on-failure
```

**예상 결과**:
- 1280x720 윈도우가 열린다
- 틸(teal) 색 배경이 보인다 (`rgb(46, 140, 138)`)
- ESC 키 또는 창 닫기로 종료
- `ctest` 3개 테스트 모두 PASS

### WASM 빌드

> Emscripten SDK(emsdk)가 설치되어 있어야 한다.
> 설치: https://emscripten.org/docs/getting_started/downloads.html

```bash
# Emscripten 환경 활성화
source ~/emsdk/emsdk_env.sh

# 설정
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release

# 빌드
cmake --build build-web

# 로컬 서버로 실행
cd build-web/game
python3 -m http.server 8080
```

브라우저에서 `http://localhost:8080/gazeshot_game.html` 을 열면
Desktop과 동일한 틸 색 화면이 canvas에 렌더링된다.

**예상 결과**:
- 브라우저 탭에 "GazeShot" 제목
- 1280x720 canvas에 틸 색 배경
- ESC 키로 루프 중단

---

## 5. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| Desktop 빌드 | `cmake --build build` 에러 없음 |
| Desktop 실행 | 틸 색 윈도우 열림, ESC로 종료 |
| WASM 빌드 | `cmake --build build-web` 에러 없음 |
| WASM 실행 | 브라우저에서 canvas 렌더링 |
| 테스트 | `ctest --output-on-failure` 전체 PASS |
| 의존성 방향 | core는 SDL include 없음 |
| 윈도우 리사이즈 | 창 크기 조절 시 크래시 없음 |

---

## 6. 트러블슈팅

### "SDL3를 찾을 수 없다"

FetchContent가 정상 동작하려면 인터넷 연결이 필요하다.
`build/_deps/sdl3-src/` 디렉토리가 있는지 확인한다.

### macOS에서 OpenGL 경고

macOS는 OpenGL을 deprecated로 취급한다. 경고가 나와도 동작에는 문제없다.
`-Wno-deprecated` 를 macOS에서만 추가할 수 있다:

```cmake
if(APPLE AND NOT EMSCRIPTEN)
    target_compile_options(gazeshot_platform PRIVATE -Wno-deprecated)
endif()
```

### WASM에서 GL 함수 못 찾음

`MIN_WEBGL_VERSION=2` 설정을 확인한다. WebGL 2.0 = OpenGL ES 3.0이다.

---

## 7. 블로그 데모 아이디어

이 챕터의 블로그 포스트에 포함할 수 있는 것:

1. **스크린샷 비교**: Desktop 윈도우 vs 브라우저 canvas 나란히
2. **프로젝트 구조 트리**: 레이어 다이어그램
3. **빌드 시간 비교**: Desktop vs WASM 초기 빌드 시간
4. **핵심 코드**: `Window` 클래스의 이동 생성자, `std::exchange` 설명
5. **테스트 결과**: doctest 출력 캡처

---

## 8. 심화 읽기

- [SDL3 Migration Guide](https://wiki.libsdl.org/SDL3/README/migration) — SDL2와 달라진 점
- [CMake FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) — 공식 문서
- [C++ Core Guidelines: I.22](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Ri-global) — 전역 상태 피하기
- [Emscripten Main Loop](https://emscripten.org/docs/porting/emscripten-runtime-environment.html) — WASM 게임 루프

---

## 다음 챕터 예고

**Chapter 02: 커스텀 수학 라이브러리 (1) — 벡터와 행렬**

GLM을 걷어내고 직접 `Vec3`, `Mat4`를 만든다.
데모: 커스텀 수학으로 회전하는 삼각형을 렌더링한다.
