#include <gazeshot/core/Event.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/engine/GameClock.hpp>
#include <gazeshot/engine/Input.hpp>
#include <gazeshot/engine/Mesh.hpp>
#include <gazeshot/engine/MeshGen.hpp>
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

#include <SDL3/SDL_scancode.h>

#include <cstdio>

using namespace gazeshot;
using namespace gazeshot::core;
using namespace gazeshot::core::math;
using namespace gazeshot::core::math::literals;

const char* VERT_SRC = R"(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uTransform;
out vec3 vNormal;

void main() {
  gl_Position = uTransform * vec4(aPos, 1.0);
  vNormal = aNormal;
}
)";
const char* FRAG_SRC = R"(
in vec3 vNormal;
out vec4 FragColor;
void main() {
  // 법선을 색으로: (-1,1) → (0,1)
  vec3 color = abs(vNormal);
  FragColor = vec4(color, 1.0);
}
)";

struct App {
  platform::Window window;
  std::unique_ptr<renderer::Renderer> renderer;
  std::unique_ptr<renderer::ShaderProgram> shader;
  std::unique_ptr<renderer::VertexBuffer> vbo;
  std::unique_ptr<renderer::IndexBuffer> ibo;
  u32 vao = 0;

  engine::Input input;
  engine::GameClock clock;

  std::array<engine::Mesh, 4> meshes;

  Quatf rotation{};         // 현재 회전 (보간됨), identity = {1, 0, 0, 0}
  Quatf targetRotation{};   // 목표 회전
  Vec3f position{0, 0, 0};  // 모델 위치

  Vec4f clearColor{0.12f, 0.12f, 0.15f, 1.0f};
  i32 colorIndex = 0;

  bool isDragging = false;
};

void init(App& app) {
  app.renderer = renderer::createRenderer();
  app.renderer->init();
  app.renderer->setDepthTest(true);
  app.renderer->setCullFace(true);

  app.meshes[0] = engine::MeshGen::box(1, 1, 1);
  app.meshes[1] = engine::MeshGen::sphere(0.6f, 32, 16);
  app.meshes[2] = engine::MeshGen::cylinder(0.4f, 1.2f, 32);
  app.meshes[3] = engine::MeshGen::plane(1.5f, 1.5f, 4);

  for (auto& mesh : app.meshes) {
    mesh.upload(*app.renderer);
  }

  app.shader = app.renderer->createShaderProgram(VERT_SRC, FRAG_SRC);
}

// 입력 처리 - 프레임마다 1번 호출
void handleInput(App& app) {
  // R 키: 리셋
  if (app.input.isKeyPressed(SDL_SCANCODE_R)) {
    app.targetRotation = Quatf{};   // identity
    app.position = Vec3f{0, 0, 0};  // 위치도 리셋
  }

  // Space 키: 색상 변경
  if (app.input.isKeyPressed(SDL_SCANCODE_SPACE)) {
    constexpr Vec4f colors[] = {
        {0.12f, 0.12f, 0.15f, 1.0f},
        {0.15f, 0.08f, 0.08f, 1.0f},
        {0.08f, 0.15f, 0.08f, 1.0f},
        {0.08f, 0.08f, 0.15f, 1.0f},
    };
    app.colorIndex = (app.colorIndex + 1) % 4;
    app.clearColor = colors[app.colorIndex];
  }

  // 방향키: 이동 (누르고 있는 동안 계속 이동)
  constexpr f32 moveSpeed = 2.0f;  // 초당 2 유닛
  f32 dt = engine::GameClock::FIXED_DT;

  if (app.input.isKeyHeld(SDL_SCANCODE_LEFT)) {
    app.position.x -= moveSpeed * dt;
  }
  if (app.input.isKeyHeld(SDL_SCANCODE_RIGHT)) {
    app.position.x += moveSpeed * dt;
  }
  if (app.input.isKeyHeld(SDL_SCANCODE_UP)) {
    app.position.y += moveSpeed * dt;
  }
  if (app.input.isKeyHeld(SDL_SCANCODE_DOWN)) {
    app.position.y -= moveSpeed * dt;
  }

  app.isDragging = app.input.isMouseButtonHeld(MouseButton::Left);
}

// 물리 업데이트 - 고정 스텝으로 0~N번 호출
void update(App& app, f32 dt) {
  if (app.isDragging) {
    auto delta = app.input.mouseDelta();

    // 월드 공간 축 기준으로 회전 누적 (Quaternion)
    Quatf deltaY = Quatf::fromAxisAngle(Vec3f{0, 1, 0}, delta.x * 0.01f);
    Quatf deltaX = Quatf::fromAxisAngle(Vec3f{1, 0, 0}, delta.y * 0.01f);

    app.targetRotation = deltaY * deltaX * app.targetRotation;
  }

  // 부드러운 보간 (slerp)
  app.rotation = slerp(app.rotation, app.targetRotation, 10.0f * dt);
}

void render(App& app, f32 alpha) {
  app.renderer->clear(app.clearColor);

  // Translation matrix 생성
  Mat4f translation = translate(app.position);
  ;
  Mat4f rotation = app.rotation.toMat4();  // Quaternion → Matrix

  f32 aspect = static_cast<f32>(app.window.width()) /
               static_cast<f32>(app.window.height());
  Mat4f view = lookAt(Vec3f{0, 0, 5}, Vec3f{0, 0, 0}, Vec3f{0, 1, 0});
  Mat4f proj = perspective(45.0_deg, aspect, 0.1f, 100.0f);

  f32 offsets[] = {-3.0f, -1.0f, 1.0f, 3.0f};

  app.shader->bind();
  for (i32 i = 0; i < 4; ++i) {
    Mat4f model = translation * translate(Vec3f{offsets[i], 0, 0}) * rotation;
    Mat4f mvp = proj * view * model;
    app.shader->setMat4("uTransform", mvp);
    app.meshes[i].draw(*app.renderer);
  }

  app.window.swapBuffers();
}

void oneFrame(void* arg) {
  auto* app = static_cast<App*>(arg);

  // 1. 이벤트 수집
  auto events = app->window.pollEvents();
  for (auto& event : events) {
    app->input.processEvent(event);
  }

  if (app->window.shouldClose()) {
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    return;
  }

  // 2. 입력 처리 (프레임마다 1번) - 고정 스텝과 무관
  handleInput(*app);

  // 3. 고정 스텝 업데이트 (물리/게임 로직만)
  auto frame = app->clock.tick();
  for (i32 i = 0; i < frame.updateCount; ++i) {
    update(*app, engine::GameClock::FIXED_DT);
  }

  // 4. 렌더링 (보간값 alpha 전달)
  render(*app, frame.alpha);

  // 5. 프레임 마무리
  app->input.endFrame();
  app->clock.updateFPS(frame.frameTime);

  // 6. 디버그 출력 (1초마다)
  static f32 logTimer = 0.0f;
  logTimer += frame.frameTime;
  if (logTimer >= 1.0f) {
    auto pos = app->input.mousePosition();
    std::printf(
        "FPS: %.2f, Mouse: (%.1f, %.1f)\n", app->clock.fps(), pos.x, pos.y
    );
    logTimer = 0.0f;
  }
}

int main(int argc, char* argv[]) {
  App app{
      .window = gazeshot::platform::Window{gazeshot::platform::WindowConfig{
          .title = "Gazeshot", .width = 1280, .height = 720
      }}
  };

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