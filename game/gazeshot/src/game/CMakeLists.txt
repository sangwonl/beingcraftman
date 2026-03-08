add_executable(gazeshot_game
	src/main.cpp
)

target_link_libraries(gazeshot_game PRIVATE
	gazeshot_platform
	gazeshot_engine
	SDL3::Headers
)

if (EMSCRIPTEN)
	set_target_properties(gazeshot_game PROPERTIES
		SUFFIX ".html"
	)

	target_link_options(gazeshot_game PRIVATE
		"SHELL:-s USE_SDL=0"	# 시스템 SDL 사용 안 함 (직접 빌드한 SDL 사용)
		"SHELL:-s MIN_WEBGL_VERSION=2" # WebGL 2.0 이상 요구
		"SHELL:-s MAX_WEBGL_VERSION=2"
		"SHELL:-s ALLOW_MEMORY_GROWTH=1"
		"SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
	)
endif()
