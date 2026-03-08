add_library(gazeshot_renderer STATIC
	src/opengl/GLRenderer.cpp
)

target_include_directories(gazeshot_renderer PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gazeshot_renderer
	PUBLIC gazeshot_core
	PRIVATE SDL3::Headers
)

if (NOT EMSCRIPTEN)
	find_package(OpenGL REQUIRED)
	target_link_libraries(gazeshot_renderer PRIVATE OpenGL::GL)
endif()
