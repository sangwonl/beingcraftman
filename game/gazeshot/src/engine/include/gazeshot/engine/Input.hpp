#pragma once

#include <array>
#include <functional>
#include <gazeshot/core/Event.hpp>
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec2.hpp>
#include <vector>

using namespace gazeshot::core;
using namespace gazeshot::core::math;

namespace gazeshot::engine {

class Input {
 public:
  bool isKeyPressed(i32 scancode) const;
  bool isKeyHeld(i32 scancode) const;
  bool isKeyReleased(i32 scancode) const;

  Vec2f mousePosition() const { return mousePos_; }
  Vec2f mouseDelta() const { return mouseDelta_; }
  bool isMouseButtonHeld(MouseButton button) const;

  void processEvent(const Event& event);
  void endFrame();  // 프레임 끝에 previous 업데이트

  using EventHandler = std::function<void(const Event&)>;
  void addHandler(EventHandler handler) {
    handlers_.push_back(std::move(handler));
  }

 private:
  static constexpr usize MAX_KEYS = 512;

  struct KeyState {
    bool current = false;
    bool previous = false;
    mutable bool pressed = false;   // 눌렸는가 (isKeyPressed()에서 소비)
    mutable bool released = false;  // 뗐는가 (isKeyReleased()에서 소비)
  };

  std::array<KeyState, MAX_KEYS> keys_{};
  std::array<bool, 3> mouseButtons_{};
  Vec2f mousePos_{};
  Vec2f mouseDelta_{};
  std::vector<EventHandler> handlers_;
};

}  // namespace gazeshot::engine