#pragma once

#include <gazeshot/core/Types.hpp>
#include <variant>

namespace gazeshot::core {

enum class KeyAction : u8 { Pressed, Released };

struct KeyEvent {
  i32 scancode;  // SDL scancode 호환이지만 SDL 의존은 아님
  KeyAction action;
  bool repeat;
};

struct MouseMoveEvent {
  f32 x, y;    // 현재 위치
  f32 dx, dy;  // 이전 프레임과의 위치 차이
};

enum class MouseButton : u8 { Left, Middle, Right };

struct MouseButtonEvent {
  MouseButton button;
  KeyAction action;
  f32 x, y;  // 버튼 이벤트가 발생한 위치
};

struct WindowResizeEvent {
  i32 width, height;
};

struct WindowCloseEvent {};

using Event = std::variant<
    KeyEvent,
    MouseMoveEvent,
    MouseButtonEvent,
    WindowResizeEvent,
    WindowCloseEvent>;

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace gazeshot::core