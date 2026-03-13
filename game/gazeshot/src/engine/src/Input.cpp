#include <gazeshot/engine/Input.hpp>

using namespace gazeshot::core;
using namespace gazeshot::core::math;

namespace gazeshot::engine {

bool Input::isKeyPressed(i32 scancode) const {
  if (scancode < 0 || scancode >= MAX_KEYS) return false;
  return keys_[scancode].pressed;
}

bool Input::isKeyHeld(i32 scancode) const {
  if (scancode < 0 || scancode >= MAX_KEYS) return false;
  return keys_[scancode].current;
}

bool Input::isKeyReleased(i32 scancode) const {
  if (scancode < 0 || scancode >= MAX_KEYS) return false;
  return keys_[scancode].released;
}

bool Input::isMouseButtonHeld(MouseButton button) const {
  return mouseButtons_[static_cast<u8>(button)];
}

void Input::processEvent(const Event& event) {
  std::visit(
      overloaded{
          [this](const KeyEvent& e) {
            if (e.scancode < 0 || e.scancode >= MAX_KEYS) return;
            auto& key = keys_[e.scancode];

            if (e.action == KeyAction::Pressed) {
              bool wasPressed = key.current;
              key.current = true;

              // 처음 눌렀을 때만 (repeat 아님)
              if (!wasPressed && !e.repeat) {
                key.pressed = true;
              }
            } else {
              key.current = false;
              key.released = true;
            }

            // log key state
            printf(
                "Key %d: current=%d, pressed=%d, released=%d\n",
                e.scancode,
                key.current,
                key.pressed,
                key.released
            );
          },
          [this](const MouseMoveEvent& e) {
            mouseDelta_ = {e.dx, e.dy};
            mousePos_ = {e.x, e.y};
          },
          [this](const MouseButtonEvent& e) {
            mouseButtons_[static_cast<u8>(e.button)] =
                (e.action == KeyAction::Pressed);
          },
          [](auto&&) {}
      },
      event
  );

  // 외부 핸들러 호출
  for (const auto& handler : handlers_) {
    handler(event);
  }
}

void Input::endFrame() {
  for (auto& key : keys_) {
    key.previous = key.current;
    key.pressed = false;  // 프레임 끝에 초기화 (다음 프레임을 위해)
    key.released = false;
  }
  mouseDelta_ = {0.0f, 0.0f};
}

}  // namespace gazeshot::engine