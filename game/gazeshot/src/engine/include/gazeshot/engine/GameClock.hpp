#pragma once

#include <chrono>
#include <gazeshot/core/Types.hpp>

using namespace gazeshot::core;

namespace gazeshot::engine {

class GameClock {
 public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Duration = std::chrono::duration<f32>;

  static constexpr f32 FIXED_DT = 1.0f / 60.0f;  // 60 FPS 고정 시간 간격
  static constexpr f32 MAX_FRAME_TIME = 0.25f;   // 프레임당 최대 시간 (250ms)

  GameClock() : previousTime_(Clock::now()) {}

  struct FrameResult {
    i32 updateCount;  // 이번 프레임에서 업데이트가 몇 번 호출되어야 하는지
    f32 alpha;        // 다음 프레임과의 보간값 (0.0 ~ 1.0)
    f32 frameTime;    // 이번 프레임의 실제 시간 (초)
  };

  FrameResult tick() {
    auto currentTime = Clock::now();
    f32 frameTime = Duration(currentTime - previousTime_).count();
    previousTime_ = currentTime;

    // 프레임당 최대 시간 제한, 한 프레임이 너무 길면 자른다
    if (frameTime > MAX_FRAME_TIME) {
      frameTime = MAX_FRAME_TIME;
    }

    accumulator_ += frameTime;

    i32 updateCount = 0;
    while (accumulator_ >= FIXED_DT) {
      accumulator_ -= FIXED_DT;
      updateCount++;
    }

    return {
        .updateCount = updateCount,
        .alpha = accumulator_ / FIXED_DT,
        .frameTime = frameTime
    };
  }

  f32 fps() const { return fps_; }
  void updateFPS(f32 frameTime) {
    frameCount_++;
    fpsAccumulator_ += frameTime;
    if (fpsAccumulator_ >= 1.0f) {
      fps_ = static_cast<f32>(frameCount_) / fpsAccumulator_;
      frameCount_ = 0;
      fpsAccumulator_ = 0.0f;
    }
  }

 private:
  TimePoint previousTime_;
  f32 accumulator_ = 0.0f;
  f32 fps_ = 0.0f;
  i32 frameCount_ = 0;
  f32 fpsAccumulator_ = 0.0f;
};

}  // namespace gazeshot::engine