#include <gazeshot/core/math/Math.hpp>
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

#include <cmath>
#include <cstdio>

using namespace gazeshot::core::math;

#ifdef __EMSCRIPTEN__
static const char* VERT_SRC = R"(#version 300 es
precision mediump float;
layout(location = 0) in vec3 aPos;
uniform mat4 uTransform;
void main() {
  gl_Position = uTransform * vec4(aPos, 1.0);
}
)";
static const char* FRAG_SRC = R"(#version 300 es
precision mediump float;
out vec4 FragColor;
void main() {
  FragColor = vec4(0.95, 0.55, 0.15, 1.0);
}
)";
#else
static const char* VERT_SRC = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uTransform;
void main() {
  gl_Position = uTransform * vec4(aPos, 1.0);
}
)";
static const char* FRAG_SRC = R"(#version 330 core
out vec4 FragColor;
void main() {
  FragColor = vec4(0.95, 0.55, 0.15, 1.0);
}
)";
#endif

Mat4f rotateZ(float radians) {
  float c = std::cos(radians);
  float s = std::sin(radians);
  Mat4f m = Mat4f::identity();
  m[0][0] = c;
  m[0][1] = s;
  m[1][0] = -s;
  m[1][1] = c;
  return m;
}

struct App {
  gazeshot::platform::Window window;
  unsigned int shaderProgram = 0;
  unsigned int vao = 0;
  float time = 0.0f;
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

void initGL(App& app) {
  auto vs = compileShader(GL_VERTEX_SHADER, VERT_SRC);
  auto fs = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);
  app.shaderProgram = glCreateProgram();
  glAttachShader(app.shaderProgram, vs);
  glAttachShader(app.shaderProgram, fs);
  glLinkProgram(app.shaderProgram);
  glDeleteShader(vs);
  glDeleteShader(fs);

  float vertices[] = {
      0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f,
  };

  unsigned int vbo;
  glGenVertexArrays(1, &app.vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(app.vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
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

  Mat4f transform = rotateZ(app->time);

  glClearColor(0.12f, 0.12f, 0.15f, 1.0f);  // teal color
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(app->shaderProgram);

  int loc = glGetUniformLocation(app->shaderProgram, "uTransform");
  glUniformMatrix4fv(loc, 1, GL_FALSE, transform.data());

  glBindVertexArray(app->vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  app->window.swapBuffers();
}

int main(int argc, char* argv[]) {
  App app{.window = gazeshot::platform::Window{gazeshot::platform::WindowConfig{
              .title = "Gazeshot", .width = 1280, .height = 720}}};

  initGL(app);

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(oneFrame, &app, 0, true);
#else
  while (!app.window.shouldClose()) {
    oneFrame(&app);
  }
#endif

  return 0;
}