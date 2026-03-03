#include <gazeshot/platform/Window.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <GLES3/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

struct App {
	gazeshot::platform::Window window;
	bool running = true;
};

void oneFrame(void* arg) {
	auto* app = static_cast<App*>(arg);

	app->window.pollEvents();

	if (app->window.shouldClose()) {
		app->running = false;
#ifdef __EMSCRIPTEN__
		emscripten_cancel_main_loop();
#endif
		return;
	}

	glClearColor(0.18f, 0.55f, 0.54f, 1.0f);  // teal color
	glClear(GL_COLOR_BUFFER_BIT);

	app->window.swapBuffers();
}

int main(int argc, char* argv[]) {
	App app{
		.window = gazeshot::platform::Window{
			gazeshot::platform::WindowConfig{
				.title = "Gazeshot",
				.width = 1280,
				.height = 720
			}
		}
	};

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(oneFrame, &app, 0, true);
#else
	while (!app.window.shouldClose()) {
		oneFrame(&app);
	}
#endif

	return 0;
}