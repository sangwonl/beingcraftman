#include <SDL3/SDL.h>

#include <gazeshot/platform/Window.hpp>
#include <stdexcept>
#include <utility>

namespace gazeshot::platform {

Window::Window(const WindowConfig& config)
    : width_(config.width), height_(config.height) {
  // SDL 초기화
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
  }

  // GL 컨텍스트 요청
  // - WASM: WebGL2 매핑을 위해 OpenGL ES 3.0
  // - Native: 데스크톱 OpenGL 3.3 Core
#ifdef __EMSCRIPTEN__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#if defined(__APPLE__)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
#endif
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  // 윈도우 생성
  window_ = SDL_CreateWindow(config.title.c_str(), width_, height_,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window_) {
    throw std::runtime_error(std::string("SDL_CreateWindow failed: ") +
                             SDL_GetError());
  }

  // GL 컨텍스트 생성
  context_ = SDL_GL_CreateContext(window_);
  if (!context_) {
    throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") +
                             SDL_GetError());
  }

  // VSync 활성화
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

// 이동 대입 연산자
Window& Window::operator=(Window&& other) noexcept {
  if (this != &other) {
    // 기존 리소스 해제
    if (context_) {
      SDL_GL_DestroyContext(context_);
    }
    if (window_) {
      SDL_DestroyWindow(window_);
    }

    // 리소스 이동
    window_ = std::exchange(other.window_, nullptr);
    context_ = std::exchange(other.context_, nullptr);
    width_ = other.width_;
    height_ = other.height_;
    closed_ = other.closed_;
  }
  return *this;
}

bool Window::shouldClose() const { return closed_; }

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
        width_ = event.window.data1;
        height_ = event.window.data2;
        break;
      default:
        break;
    }
  }
}

void Window::swapBuffers() { SDL_GL_SwapWindow(window_); }

}  // namespace gazeshot::platform