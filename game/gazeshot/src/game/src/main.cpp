#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/platform/Window.hpp>
#include <gazeshot/renderer/Renderer.hpp>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <emscripten.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <cmath>
#include <cstdio>

using namespace gazeshot;
using namespace gazeshot::core::math;
using namespace gazeshot::core::math::literals;

const char* VERT_SRC = R"(
layout(location = 0) in vec3 aPos;
uniform mat4 uTransform;
void main() {
  gl_Position = uTransform * vec4(aPos, 1.0);
}
)";
const char* FRAG_SRC = R"(
out vec4 FragColor;
void main() {
  FragColor = vec4(0.95, 0.55, 0.15, 1.0);
}
)";

Mat4f rotateZ(float radians) {
  float c = std::cos(radians);
  float s = std::sin(radians);
  Mat4f m = Mat4f::identity();
  // row-major: m[row][col] = 수학 표기 그대로
  // [ c  -s  0  0 ]
  // [ s   c  0  0 ]
  // [ 0   0  1  0 ]
  // [ 0   0  0  1 ]
  m[0][0] = c;
  m[0][1] = -s;
  m[1][0] = s;
  m[1][1] = c;
  return m;
}

struct App {
  platform::Window window;
  std::unique_ptr<renderer::Renderer> renderer;
  std::unique_ptr<renderer::ShaderProgram> shader;
  std::unique_ptr<renderer::VertexBuffer> vbo;
  std::unique_ptr<renderer::IndexBuffer> ibo;
  core::u32 vao = 0;
  core::f32 time = 0.0f;
};

unsigned int compileShader(unsigned int type, const char* src) {
  unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  int success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[512];
    glGetShaderInfoLog(shader, 512, nullptr, log);
    std::fprintf(stderr, "Shader compilation failed: %s\n", log);
  }

  return shader;
}

void init(App& app) {
  app.renderer = renderer::createRenderer();
  app.renderer->init();
  app.renderer->setDepthTest(true);

  app.shader = app.renderer->createShaderProgram(VERT_SRC, FRAG_SRC);

  // clang-format off
  core::f32 vertices[] = {
    // front (z = +0.5)
    -0.5f, -0.5f,  0.5f,  // 0: front bottom-left
     0.5f, -0.5f,  0.5f,  // 1: front bottom-right
     0.5f,  0.5f,  0.5f,  // 2: front top-right
    -0.5f,  0.5f,  0.5f,  // 3: front top-left
    // back (z = -0.5)
    -0.5f, -0.5f, -0.5f,  // 4: back bottom-left
     0.5f, -0.5f, -0.5f,  // 5: back bottom-right
     0.5f,  0.5f, -0.5f,  // 6: back top-right
    -0.5f,  0.5f, -0.5f,  // 7: back top-left
  };
  // clang-format on
  core::u32 indices[] = {
      0, 1, 2, 2, 3, 0,  // front
      4, 5, 6, 6, 7, 4,  // back
      4, 0, 3, 3, 7, 4,  // left
      1, 5, 6, 6, 2, 1,  // right
      3, 2, 6, 6, 7, 3,  // top
      4, 5, 1, 1, 0, 4,  // bottom
  };

  app.vao = app.renderer->createVertexArray();
  app.renderer->bindVertexArray(app.vao);
  app.vbo = app.renderer->createVertexBuffer(
      vertices, sizeof(vertices), renderer::BufferUsage::Static);
  app.ibo = app.renderer->createIndexBuffer(indices, 36);
  app.renderer->setVertexLayout({{"aPos", renderer::AttribType::Float3}});
}

void oneFrame(void* arg) {
  auto* app = static_cast<App*>(arg);
  app->window.pollEvents();

  if (app->window.shouldClose()) {
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    return;
  }

  app->time += 1.0f / 60.0f;

  app->renderer->clear({0.12f, 0.12f, 0.15f, 1.0f});
  app->renderer->setViewport(0, 0, app->window.width(), app->window.height());

  Mat4f model = rotateY(app->time) * rotateX(app->time * 0.7f);

  Mat4f view = lookAt(
      Vec3f{0, 0, 3},  // 카메라 위치
      Vec3f{0, 0, 0},  // 바라보는 곳
      Vec3f{0, 1, 0}   // 월드 업 벡터
  );

  core::f32 aspect = static_cast<float>(app->window.width()) /
                     static_cast<float>(app->window.height());

  Mat4f proj = perspective(45.0_deg, aspect, 0.1f, 100.0f);

  Mat4f mvp = proj * view * model;

  app->shader->bind();
  app->shader->setMat4("uTransform", mvp);
  app->renderer->bindVertexArray(app->vao);
  app->renderer->drawIndexed(36);

  app->window.swapBuffers();
}

int main(int argc, char* argv[]) {
  App app{
      .window = gazeshot::platform::Window{gazeshot::platform::WindowConfig{
          .title = "Gazeshot", .width = 1280, .height = 720}}};

  init(app);

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(oneFrame, &app, 0, true);
#else
  while (!app.window.shouldClose()) {
    oneFrame(&app);
  }
#endif

  return 0;
}