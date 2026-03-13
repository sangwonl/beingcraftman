#pragma once

#include <array>
#include <functional>
#include <gazeshot/core/Event.hpp>
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Vec2.hpp>
#include <vector>

namespace gazeshot::engine {

class Input {
 public:
  bool isKeyPressed(core::i32 scancode) const;
  bool isKeyHeld(core::i32 scancode) const;
  bool isKeyReleased(core::i32 scancode) const;

  core::math::Vec2f mousePosition() const { return mousePos_; }
  core::math::Vec2f mouseDelta() const { return mouseDelta_; }
  bool isMouseButtonHeld(core::MouseButton button) const;

  void processEvent(const core::Event& event);
  void endFrame();  // 프레임 끝에 previous 업데이트

  using EventHandler = std::function<void(const core::Event&)>;
  void addHandler(EventHandler handler) {
    handlers_.push_back(std::move(handler));
  }

 private:
  static constexpr core::usize MAX_KEYS = 512;

  struct KeyState {
    bool current = false;
    bool previous = false;
    mutable bool pressed = false;   // 눌렸는가 (isKeyPressed()에서 소비)
    mutable bool released = false;  // 뗐는가 (isKeyReleased()에서 소비)
  };

  std::array<KeyState, MAX_KEYS> keys_{};
  std::array<bool, 3> mouseButtons_{};
  core::math::Vec2f mousePos_{};
  core::math::Vec2f mouseDelta_{};
  std::vector<EventHandler> handlers_;
};

}  // namespace gazeshot::engine