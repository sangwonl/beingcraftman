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

// clang-format off
float vertices[] = {
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

unsigned int indices[] = {
    0, 1, 2, 2, 3, 0,  // front
    4, 5, 6, 6, 7, 4,  // back
    4, 0, 3, 3, 7, 4,  // left
    1, 5, 6, 6, 2, 1,  // right
    3, 2, 6, 6, 7, 3,  // top
    4, 5, 1, 1, 0, 4,  // bottom
};

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

  unsigned int vbo, ebo;
  glGenVertexArrays(1, &app.vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);
  glBindVertexArray(app.vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  glEnable(GL_DEPTH_TEST);
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

  using namespace gazeshot::core::math;
  using namespace gazeshot::core::math::literals;

  app->time += 1.0f / 60.0f;

  Mat4f model = rotateY(app->time) * rotateX(app->time * 0.7f);

  Mat4f view = lookAt(Vec3f{0, 0, 3},  // 카메라 위치
                      Vec3f{0, 0, 0},  // 바라보는 곳
                      Vec3f{0, 1, 0}   // 월드 업 벡터
  );

  float aspect = static_cast<float>(app->window.width()) /
                 static_cast<float>(app->window.height());

  Mat4f proj = perspective(45.0_deg, aspect, 0.1f, 100.0f);

  Mat4f mvp = proj * view * model;

  glUseProgram(app->shaderProgram);

  int loc = glGetUniformLocation(app->shaderProgram, "uTransform");
  glUniformMatrix4fv(loc, 1, GL_TRUE, mvp.data());

  glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glBindVertexArray(app->vao);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

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