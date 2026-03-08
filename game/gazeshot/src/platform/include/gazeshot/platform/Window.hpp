#pragma once

#include <gazeshot/core/Event.hpp>
#include <gazeshot/core/Types.hpp>
#include <string>
#include <string_view>
#include <vector>

struct SDL_Window;
struct SDL_GLContextState;
using SDL_GLContext = SDL_GLContextState*;

using namespace gazeshot::core;

namespace gazeshot::platform {

struct WindowConfig {
  std::string title = "GazeShot";
  i32 width = 1280;
  i32 height = 720;
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
  std::vector<Event> pollEvents();
  void swapBuffers();

  i32 width() const { return width_; }
  i32 height() const { return height_; }

 private:
  SDL_Window* window_ = nullptr;
  SDL_GLContext context_ = nullptr;
  i32 width_ = 0;
  i32 height_ = 0;
  bool closed_ = false;
};

}  // namespace gazeshot::platform