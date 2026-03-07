#pragma once

#include <gazeshot/core/Types.hpp>
#include <string>
#include <string_view>

struct SDL_Window;
struct SDL_GLContextState;
using SDL_GLContext = SDL_GLContextState*;

namespace gazeshot::platform {

struct WindowConfig {
  std::string title = "GazeShot";
  core::i32 width = 1280;
  core::i32 height = 720;
};

class Window {
 public:
  explicit Window(const WindowConfig& config);
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&& other) noexcept;
  Window& operator=(Window&& other) noexcept;

  bool shouldClose() const;
  void pollEvents();
  void swapBuffers();

  core::i32 width() const { return width_; }
  core::i32 height() const { return height_; }

 private:
  SDL_Window* window_ = nullptr;
  SDL_GLContext context_ = nullptr;
  core::i32 width_ = 0;
  core::i32 height_ = 0;
  bool closed_ = false;
};

}  // namespace gazeshot::platform